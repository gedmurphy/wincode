/*!

    @file Core/Lookup.cpp

    @brief Looks a value up in every table, and works out what else it says.

    @details Message text comes from the Windows the tool is running on where
             it has any, since that text is current and in the reader's own
             language. Otherwise it falls back to the English captured when the
             tables were generated. Either way, "text" that is nothing but a
             symbolic name is dropped. ntdll's message table has plenty of
             those, and they say nothing the name doesn't.

             Beyond the tables, a match carries its fields and the codes it
             converts to or wraps, and a value that is in no table can still be
             worth reporting from its shape alone. Each of those is shown only
             when it is telling, since a field breakdown of an arbitrary number
             is noise that looks like information.

    @author Ged Murphy

    @copyright Copyright (c) 2026 Ged Murphy

    This file is part of WinCode.

    WinCode is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    WinCode is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with WinCode. If not, see <https://www.gnu.org/licenses/>.

*/

#include "WinCode.hpp"
#include "Tables.hpp"

#include <algorithm>
#include <cwctype>
#include <format>


//
// HRESULT and NTSTATUS bits. The top two are one severity field in an
// NTSTATUS, but in an HRESULT the top is the failure bit and the next is R,
// which is clear in any HRESULT HRESULT_FROM_NT didn't make.
//
static constexpr uint32_t k_SeverityBit = 0x80000000;
static constexpr uint32_t k_ReservedBit = 0x40000000;
static constexpr uint32_t k_CustomerBit = 0x20000000;
static constexpr uint32_t k_NtBit = 0x10000000;

static constexpr uint32_t k_FacilityWin32 = 7;
static constexpr uint32_t k_HResultFromWin32 = 0x80070000;

//
// Above WM_USER, messages are private to a window class; above WM_APP, to an
// application; from 0xC000, handed out by RegisterWindowMessage.
//
static constexpr uint32_t k_RegisteredMessageFirst = 0xC000;
static constexpr uint32_t k_MessageLast = 0xFFFF;


typedef ULONG (NTAPI* RtlNtStatusToDosErrorRoutine)(LONG Status);

//
// Resolved on first use rather than imported, which keeps ntdll.lib out of the
// link. Two threads racing to resolve it store the same pointer, so that race
// is harmless.
//
static RtlNtStatusToDosErrorRoutine g_RtlNtStatusToDosError;
static bool g_RtlNtStatusToDosErrorResolved;


const KindTable g_KindTables[k_KindTableCount] =
{
    { CodeKind::WinError, &g_WinErrorTable },
    { CodeKind::HResult, &g_HResultTable },
    { CodeKind::NtStatus, &g_NtStatusTable },
    { CodeKind::BugCheck, &g_BugCheckTable },
    { CodeKind::WindowMessage, &g_WindowMessageTable }
};


const TableEntry*
TableFindFirst (
    _In_ const TableSpan& Table,
    _In_ uint32_t Value
    )
{
    const TableEntry* end;
    const TableEntry* entry;

    end = Table.Entries + Table.Count;
    entry = std::lower_bound(Table.Entries,
                             end,
                             Value,
                             [](const TableEntry& Entry, uint32_t Wanted)
                             {
                                 return Entry.Value < Wanted;
                             });

    if ((entry == end) || (entry->Value != Value))
    {
        return nullptr;
    }

    return entry;
}


std::wstring
AsciiToWide (
    _In_z_ PCSTR Text
    )
{
    std::wstring result;

    for (PCSTR c = Text; *c != '\0'; c++)
    {
        result += SCAST(WCHAR)(*c);
    }

    return result;
}


/*!

    @brief The primary name for a value in a table.

    @param[in] Table - The table.

    @param[in] Value - The value.

    @return The name, or an empty string if the value isn't in the table.

*/
static
std::wstring
PrimaryName (
    _In_ const TableSpan& Table,
    _In_ uint32_t Value
    )
{
    const TableEntry* entry;

    entry = TableFindFirst(Table, Value);
    if (entry == nullptr)
    {
        return std::wstring();
    }

    return AsciiToWide(entry->Name);
}


/*!

    @brief Converts captured UTF-8 text to UTF-16.

    @param[in] Text - The text.

    @return The text, or an empty string if it won't convert.

*/
static
std::wstring
Utf8ToWide (
    _In_z_ PCSTR Text
    )
{
    std::wstring result;
    int length;

    length = MultiByteToWideChar(CP_UTF8, 0, Text, -1, nullptr, 0);
    if (length <= 1)
    {
        return result;
    }

    result.resize(SCAST(size_t)(length));
    MultiByteToWideChar(CP_UTF8, 0, Text, -1, result.data(), length);
    result.resize(SCAST(size_t)(length) - 1);
    return result;
}


/*!

    @brief Tidies message text, and drops it if it says nothing.

    @details Windows ends lines with \r\n and the captured text with \n alone,
             so \r goes, so both sources come out the same.

    @param[in,out] Text - The text.

*/
static
void
NormaliseText (
    _Inout_ std::wstring& Text
    )
{
    bool identifier;

    std::erase(Text, L'\r');

    while (!Text.empty() && iswspace(Text.back()))
    {
        Text.pop_back();
    }

    identifier = !Text.empty();
    for (WCHAR c : Text)
    {
        if (!iswalnum(c) && (c != L'_'))
        {
            identifier = false;
            break;
        }
    }

    if (identifier)
    {
        Text.clear();
    }
}


/*!

    @brief Asks the running Windows for a code's message text.

    @param[in] Kind - The kind of code, which decides where the text lives.

    @param[in] Value - The code.

    @return The text, or an empty string if Windows has none.

*/
static
std::wstring
SystemText (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    )
{
    std::wstring result;
    PWSTR buffer;
    DWORD flags;
    HMODULE module;

    flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;
    module = nullptr;

    switch (Kind)
    {
    case CodeKind::WinError:
    case CodeKind::HResult:
        flags |= FORMAT_MESSAGE_FROM_SYSTEM;
        break;

    case CodeKind::NtStatus:
        flags |= FORMAT_MESSAGE_FROM_HMODULE;
        module = GetModuleHandleW(L"ntdll.dll");
        break;

    default:
        return result;
    }

    buffer = nullptr;
    if (FormatMessageW(flags, module, Value, 0, RCAST(PWSTR)(&buffer), 0, nullptr) == 0)
    {
        return result;
    }

    result = buffer;
    LocalFree(buffer);

    NormaliseText(result);
    return result;
}


/*!

    @brief Takes a value apart as an HRESULT.

    @param[in] Value - The value.

    @return The fields.

*/
static
FieldLayout
HResultFields (
    _In_ uint32_t Value
    )
{
    FieldLayout fields;

    fields.Layout = FieldLayoutKind::HResult;
    fields.Severity = Value >> 31;
    fields.Customer = (Value & k_CustomerBit) != 0;
    fields.NtBit = (Value & k_NtBit) != 0;
    fields.Facility = (Value >> 16) & 0x7FF;
    fields.Code = Value & 0xFFFF;

    if (!fields.Customer)
    {
        fields.FacilityName = PrimaryName(g_HResultFacilityTable, fields.Facility);
    }

    return fields;
}


/*!

    @brief Takes a value apart as an NTSTATUS.

    @param[in] Value - The value.

    @return The fields.

*/
static
FieldLayout
NtStatusFields (
    _In_ uint32_t Value
    )
{
    FieldLayout fields;

    fields.Layout = FieldLayoutKind::NtStatus;
    fields.Severity = Value >> 30;
    fields.Customer = (Value & k_CustomerBit) != 0;
    fields.NtBit = false;
    fields.Facility = (Value >> 16) & 0xFFF;
    fields.Code = Value & 0xFFFF;

    if (!fields.Customer)
    {
        fields.FacilityName = PrimaryName(g_NtFacilityTable, fields.Facility);
    }

    return fields;
}


/*!

    @brief Maps an NTSTATUS to the Win32 error Windows itself converts it to.

    @details Uses ntdll's own RtlNtStatusToDosError rather than a table of our
             own, so the answer is whatever Windows actually does.

    @param[in] Status - The status.

    @param[out] Win32 - Receives the Win32 error.

    @return True if Windows has a mapping for the status.

*/
static
bool
NtStatusToWin32 (
    _In_ uint32_t Status,
    _Out_ uint32_t* Win32
    )
{
    ULONG result;

    *Win32 = 0;

    if (!g_RtlNtStatusToDosErrorResolved)
    {
        g_RtlNtStatusToDosError = RCAST(RtlNtStatusToDosErrorRoutine)(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlNtStatusToDosError"));
        g_RtlNtStatusToDosErrorResolved = true;
    }

    if (g_RtlNtStatusToDosError == nullptr)
    {
        return false;
    }

    result = g_RtlNtStatusToDosError(SCAST(LONG)(Status));

    //
    // RtlNtStatusToDosError doesn't always admit to having no mapping. Over a
    // thousand statuses, customer codes among them, come back unchanged, and a
    // small one that does is a coincidence rather than a mapping:
    // STATUS_ABANDONED returns 128, which isn't ERROR_WAIT_NO_CHILDREN. So an
    // unchanged result is no mapping, and so is anything wider than a Win32
    // error's sixteen bits.
    //
    if ((result == ERROR_MR_MID_NOT_FOUND) || (result > 0xFFFF) || ((result == Status) && (Status != 0)))
    {
        return false;
    }

    *Win32 = result;
    return true;
}


/*!

    @brief Adds the code an HRESULT wraps, if it wraps one.

    @param[in] Value - The HRESULT.

    @param[in,out] Related - The code, if any, is appended here.

*/
static
void
AddWrappedCode (
    _In_ uint32_t Value,
    _Inout_ std::vector<RelatedCode>& Related
    )
{
    uint32_t inner;

    if (((Value & k_SeverityBit) == 0) || ((Value & k_CustomerBit) != 0))
    {
        return;
    }

    if ((Value & k_NtBit) != 0)
    {
        inner = Value & ~k_NtBit;
        Related.push_back({ RelationKind::Wraps,
                            L"HRESULT_FROM_NT",
                            CodeKind::NtStatus,
                            inner,
                            PrimaryName(g_NtStatusTable, inner) });
        return;
    }

    if (((Value >> 16) & 0x7FF) == k_FacilityWin32)
    {
        inner = Value & 0xFFFF;
        Related.push_back({ RelationKind::Wraps,
                            L"HRESULT_FROM_WIN32",
                            CodeKind::WinError,
                            inner,
                            PrimaryName(g_WinErrorTable, inner) });
    }
}


/*!

    @brief Adds the codes a match converts to or wraps.

    @param[in,out] Match - The match, whose Related list is filled in.

*/
static
void
AddRelated (
    _Inout_ CodeMatch& Match
    )
{
    uint32_t other;

    switch (Match.Kind)
    {
    case CodeKind::WinError:
        if ((Match.Value != 0) && (Match.Value <= 0xFFFF))
        {
            other = k_HResultFromWin32 | Match.Value;
            Match.Related.push_back({ RelationKind::AsHResult,
                                      L"HRESULT_FROM_WIN32",
                                      CodeKind::HResult,
                                      other,
                                      PrimaryName(g_HResultTable, other) });
        }
        break;

    case CodeKind::HResult:
        AddWrappedCode(Match.Value, Match.Related);
        break;

    case CodeKind::NtStatus:
        if ((Match.Value != 0) && NtStatusToWin32(Match.Value, &other))
        {
            Match.Related.push_back({ RelationKind::MapsTo,
                                      L"RtlNtStatusToDosError",
                                      CodeKind::WinError,
                                      other,
                                      PrimaryName(g_WinErrorTable, other) });
        }

        //
        // HRESULT_FROM_NT is for Windows' own statuses. Applied to a customer
        // code it produces a number that means nothing.
        //
        if (((Match.Value & k_SeverityBit) != 0) &&
            ((Match.Value & k_NtBit) == 0) &&
            ((Match.Value & k_CustomerBit) == 0))
        {
            other = Match.Value | k_NtBit;
            Match.Related.push_back({ RelationKind::AsHResult,
                                      L"HRESULT_FROM_NT",
                                      CodeKind::HResult,
                                      other,
                                      PrimaryName(g_HResultTable, other) });
        }
        break;

    default:
        break;
    }
}


/*!

    @brief One NTSTATUS and the Win32 error Windows converts it to.

*/
struct ReverseEntry
{
    uint32_t Win32;
    const TableEntry* Status;
};


/*!

    @brief Builds the Win32 error to NTSTATUS index.

    @details Every primary NTSTATUS goes through the same mapping the forward
             direction uses, so the two always agree. Aliases are left out, as
             they would only repeat their primary. The NTSTATUS table is sorted
             by value and the sort is stable, so each error's statuses stay in
             value order.

    @return The index, sorted by Win32 error.

*/
static
std::vector<ReverseEntry>
BuildReverseMap (
    void
    )
{
    std::vector<ReverseEntry> map;
    const TableEntry* entry;
    uint32_t win32;

    for (size_t i = 0; i < g_NtStatusTable.Count; i++)
    {
        entry = &g_NtStatusTable.Entries[i];

        if (((entry->Flags & k_TableEntryAlias) != 0) || (entry->Value == 0))
        {
            continue;
        }

        if (NtStatusToWin32(entry->Value, &win32))
        {
            map.push_back({ win32, entry });
        }
    }

    std::stable_sort(map.begin(),
                     map.end(),
                     [](const ReverseEntry& A, const ReverseEntry& B)
                     {
                         return A.Win32 < B.Win32;
                     });

    return map;
}


/*!

    @brief The Win32 error to NTSTATUS index, built on first use.

    @details Building it asks Windows about three thousand statuses, which is
             quick but worth doing once. A function-local static is initialised
             exactly once even when threads race to it, so the GUI's threads
             can share it safely.

    @return The index.

*/
static
const std::vector<ReverseEntry>&
ReverseMap (
    void
    )
{
    static const std::vector<ReverseEntry> map = BuildReverseMap();

    return map;
}


/*!

    @brief Fills in every NTSTATUS that Windows converts to a Win32 error.

    @param[in,out] Match - A Win32 error match, whose MappedFrom list is filled
                           in.

*/
static
void
AddMappedFrom (
    _Inout_ CodeMatch& Match
    )
{
    const std::vector<ReverseEntry>& map = ReverseMap();
    std::vector<ReverseEntry>::const_iterator first;

    first = std::lower_bound(map.begin(),
                             map.end(),
                             Match.Value,
                             [](const ReverseEntry& Entry, uint32_t Wanted)
                             {
                                 return Entry.Win32 < Wanted;
                             });

    for (std::vector<ReverseEntry>::const_iterator it = first; (it != map.end()) && (it->Win32 == Match.Value); it++)
    {
        Match.MappedFrom.push_back({ RelationKind::MappedFrom,
                                     L"RtlNtStatusToDosError",
                                     CodeKind::NtStatus,
                                     it->Status->Value,
                                     AsciiToWide(it->Status->Name) });
    }
}


/*!

    @brief Adds unnamed HRESULT and NTSTATUS matches for a value that is
           neither in any table, when its shape is telling.

    @details Where both readings are plausible the HRESULT one wins, so an
             unknown 0x8007xxxx isn't also offered as an NTSTATUS warning.

    @param[in] Value - The value.

    @param[in,out] Matches - Matches are appended here.

*/
static
void
AddUnnamedCodes (
    _In_ uint32_t Value,
    _Inout_ std::vector<CodeMatch>& Matches
    )
{
    CodeMatch match;
    uint32_t win32;
    bool hresultShown;
    bool telling;

    hresultShown = false;

    //
    // The R bit is clear in every HRESULT except the ones HRESULT_FROM_NT makes
    // out of an NTSTATUS, which carry the N bit, so a value with R set and no N
    // is some other kind of code.
    //
    if (((Value & k_ReservedBit) == 0) || ((Value & k_NtBit) != 0))
    {
        match = CodeMatch();
        match.Kind = CodeKind::HResult;
        match.Value = Value;
        match.Fields = HResultFields(Value);
        AddWrappedCode(Value, match.Related);

        //
        // FACILITY_NULL has a name, but plenty of values that are no HRESULT
        // at all have a zero facility, so it counts for nothing.
        //
        telling = match.Fields->Customer ||
                  !match.Related.empty() ||
                  ((match.Fields->Facility != 0) && !match.Fields->FacilityName.empty());

        if (telling)
        {
            Matches.push_back(match);
            hresultShown = true;
        }
    }

    if (hresultShown)
    {
        return;
    }

    match = CodeMatch();
    match.Kind = CodeKind::NtStatus;
    match.Value = Value;
    match.Fields = NtStatusFields(Value);
    AddRelated(match);

    //
    // Windows only maps statuses it knows, so a mapping is good evidence.
    //
    telling = match.Fields->Customer ||
              NtStatusToWin32(Value, &win32) ||
              ((match.Fields->Facility != 0) && !match.Fields->FacilityName.empty());

    if (telling)
    {
        Matches.push_back(match);
    }
}


/*!

    @brief Adds an unnamed window message match for a value in the class,
           application or registered message ranges.

    @param[in] Value - The value.

    @param[in,out] Matches - The match, if any, is appended here.

*/
static
void
AddMessageRange (
    _In_ uint32_t Value,
    _Inout_ std::vector<CodeMatch>& Matches
    )
{
    CodeMatch match;
    WCHAR name[256];

    if ((Value < WM_USER) || (Value > k_MessageLast))
    {
        return;
    }

    match = CodeMatch();
    match.Kind = CodeKind::WindowMessage;
    match.Value = Value;

    if (Value < WM_APP)
    {
        match.Description = std::format(L"WM_USER + {} (private to a window class)", Value - WM_USER);
    }
    else if (Value < k_RegisteredMessageFirst)
    {
        match.Description = std::format(L"WM_APP + {} (private to an application)", Value - WM_APP);
    }
    else if (GetClipboardFormatNameW(Value, name, ARRAYSIZE(name)) > 0)
    {
        //
        // RegisterWindowMessage and RegisterClipboardFormat share one atom
        // table, so this finds the name a message was registered under, if it
        // has been registered in this session.
        //
        match.Description = std::format(L"registered as \"{}\" (RegisterWindowMessage)", name);
    }
    else
    {
        match.Description = L"in the RegisterWindowMessage range, but not registered in this session";
    }

    Matches.push_back(match);
}


PCWSTR
TablesSdkVersion (
    void
    )
{
    return g_TablesSdkVersion;
}


PCWSTR
SeverityName (
    _In_ const FieldLayout& Fields
    )
{
    static constexpr PCWSTR k_NtSeverities[] = { L"success", L"informational", L"warning", L"error" };

    if (Fields.Layout == FieldLayoutKind::HResult)
    {
        return (Fields.Severity != 0) ? L"failure" : L"success";
    }

    return k_NtSeverities[Fields.Severity & 3];
}


PCWSTR
CodeKindName (
    _In_ CodeKind Kind
    )
{
    switch (Kind)
    {
    case CodeKind::WinError:
        return L"Win32 error";

    case CodeKind::HResult:
        return L"HRESULT";

    case CodeKind::NtStatus:
        return L"NTSTATUS";

    case CodeKind::BugCheck:
        return L"Bugcheck";

    case CodeKind::WindowMessage:
        return L"Window message";
    }

    return L"Unknown";
}


std::vector<CodeMatch>
LookupValue (
    _In_ uint32_t Value
    )
{
    std::vector<CodeMatch> matches;
    const TableEntry* first;
    const TableEntry* end;
    CodeMatch match;
    bool haveHResultOrNtStatus;
    bool haveWindowMessage;

    haveHResultOrNtStatus = false;
    haveWindowMessage = false;

    for (const KindTable& kindTable : g_KindTables)
    {
        first = TableFindFirst(*kindTable.Table, Value);
        if (first == nullptr)
        {
            continue;
        }

        match = CodeMatch();
        match.Kind = kindTable.Kind;
        match.Value = Value;
        match.Undocumented = (first->Flags & k_TableEntryUndocumented) != 0;
        match.Header = kindTable.Table->Source;

        end = kindTable.Table->Entries + kindTable.Table->Count;
        for (const TableEntry* entry = first; (entry != end) && (entry->Value == Value); entry++)
        {
            match.Names.push_back(AsciiToWide(entry->Name));
        }

        match.Text = SystemText(kindTable.Kind, Value);
        match.Source = TextSource::System;

        if (match.Text.empty() && (first->Text != nullptr))
        {
            match.Text = Utf8ToWide(first->Text);
            NormaliseText(match.Text);
            match.Source = TextSource::Captured;
        }

        if (match.Text.empty())
        {
            match.Source = TextSource::None;
        }

        //
        // The fields of a 16-bit value are all zero but the code, which the
        // value already says.
        //
        if ((kindTable.Kind == CodeKind::HResult) && (Value > 0xFFFF))
        {
            match.Fields = HResultFields(Value);
        }
        else if ((kindTable.Kind == CodeKind::NtStatus) && (Value > 0xFFFF))
        {
            match.Fields = NtStatusFields(Value);
        }

        AddRelated(match);

        if (kindTable.Kind == CodeKind::WinError)
        {
            AddMappedFrom(match);
        }

        if ((kindTable.Kind == CodeKind::HResult) || (kindTable.Kind == CodeKind::NtStatus))
        {
            haveHResultOrNtStatus = true;
        }

        if (kindTable.Kind == CodeKind::WindowMessage)
        {
            haveWindowMessage = true;
        }

        matches.push_back(match);
    }

    if (!haveHResultOrNtStatus && (Value > 0xFFFF))
    {
        AddUnnamedCodes(Value, matches);
    }

    if (!haveWindowMessage)
    {
        AddMessageRange(Value, matches);
    }

    std::stable_sort(matches.begin(),
                     matches.end(),
                     [](const CodeMatch& A, const CodeMatch& B)
                     {
                         return A.Kind < B.Kind;
                     });

    return matches;
}
