/*!

    @file Core/Lookup.cpp

    @brief Looks a value up in every table, and works out what else it says.

    @details Message text comes from the Windows we're running on where it has
             any, since that's current and in your own language. Otherwise we
             fall back to the English captured when the tables were generated.
             Either way, "text" that's just a symbolic name gets dropped.
             ntdll's message table has plenty, and they say nothing the name
             doesn't.

             Beyond the tables, a match carries its fields and the codes it
             converts to or wraps, and a value in no table can still be worth
             reporting from its shape. We only show those when they're telling,
             since a field breakdown of any old number is noise that looks like
             information.

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
#include <cstring>
#include <cwctype>
#include <format>


//
// The top two are one severity field in an NTSTATUS. In an HRESULT they're
// failure and R, which is clear unless HRESULT_FROM_NT made it.
//
static constexpr uint32_t k_SeverityBit = 0x80000000;
static constexpr uint32_t k_ReservedBit = 0x40000000;
static constexpr uint32_t k_CustomerBit = 0x20000000;
static constexpr uint32_t k_NtBit = 0x10000000;

static constexpr uint32_t k_FacilityWin32 = 7;
static constexpr uint32_t k_HResultFromWin32 = 0x80070000;

//
// Above WM_USER, messages belong to a window class, above WM_APP to an app,
// and from 0xC000 they're handed out by RegisterWindowMessage.
//
static constexpr uint32_t k_RegisteredMessageFirst = 0xC000;
static constexpr uint32_t k_MessageLast = 0xFFFF;


typedef ULONG (NTAPI* RtlNtStatusToDosErrorRoutine)(LONG Status);

//
// Resolved on first use, which keeps ntdll.lib out of the link. Two threads
// racing here store the same pointer, so it's harmless.
//
static RtlNtStatusToDosErrorRoutine g_RtlNtStatusToDosError;
static bool g_RtlNtStatusToDosErrorResolved;


const KindTable g_KindTables[k_KindTableCount] =
{
    { CodeKind::KnownValue, &g_KnownValueTable, L"known", L"Known values: fill patterns and exception codes", SourceShow::Always },
    { CodeKind::WinError, &g_WinErrorTable, L"win32", L"Win32 errors", SourceShow::Always },
    { CodeKind::HResult, &g_HResultTable, L"hresult", L"HRESULTs (winerror.h)", SourceShow::Always },
    { CodeKind::HResult, &g_HResultWindowsUpdateTable, L"hresult-windowsupdate", L"Windows Update HRESULTs", SourceShow::Always },
    { CodeKind::HResult, &g_HResultAudioTable, L"hresult-audio", L"Audio HRESULTs", SourceShow::Always },
    { CodeKind::HResult, &g_HResultShadowCopyTable, L"hresult-shadowcopy", L"Volume Shadow Copy HRESULTs", SourceShow::Always },
    { CodeKind::HResult, &g_HResultClrTable, L"hresult-clr", L".NET runtime (CLR) HRESULTs", SourceShow::Always },
    { CodeKind::NtStatus, &g_NtStatusTable, L"ntstatus", L"NTSTATUS codes", SourceShow::Always },
    { CodeKind::BugCheck, &g_BugCheckTable, L"bugcheck", L"Bugchecks", SourceShow::Always },
    { CodeKind::WindowMessage, &g_WindowMessageTable, L"message", L"Window messages", SourceShow::Always },
    { CodeKind::DeviceProblem, &g_DeviceProblemTable, L"device", L"Device problem codes", SourceShow::Always },
    { CodeKind::UsbStatus, &g_UsbStatusTable, L"usb", L"USB statuses", SourceShow::Quiet },
    { CodeKind::LdapError, &g_LdapErrorTable, L"ldap", L"LDAP results", SourceShow::Quiet },
    { CodeKind::PoolTag, &g_PoolTagTable, L"pooltag", L"Pool tags", SourceShow::Always },
    { CodeKind::Ioctl, &g_IoctlSdkTable, L"ioctl", L"IOCTLs and FSCTLs", SourceShow::Always },
    { CodeKind::Ioctl, &g_IoctlWdkTable, L"ioctl", L"IOCTLs and FSCTLs", SourceShow::Always }
};


/*!

    @brief Finds the primary entry for a value.

    @param[in] Table - The table.

    @param[in] Value - The value.

    @return The first entry with the value, which is the primary one, or
            nullptr if the value isn't in the table.

*/
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


/*!

    @brief Finds a table's pool tag patterns, like Gh?5.

    @param[in] Table - The table.

    @return Every entry of four characters or fewer with a ? or * in it.

*/
static
std::vector<const TableEntry*>
PoolTagPatterns (
    _In_ const TableSpan& Table
    )
{
    std::vector<const TableEntry*> patterns;

    for (size_t i = 0; i < Table.Count; i++)
    {
        if ((strlen(Table.Entries[i].Name) <= 4) && (strpbrk(Table.Entries[i].Name, "?*") != nullptr))
        {
            patterns.push_back(&Table.Entries[i]);
        }
    }

    return patterns;
}


/*!

    @brief Finds the pooltag.txt pattern that covers a tag, like Gh?5 for Gh05.

    @details pooltag.txt writes a family of tags with ? for any character and a
             trailing * for any rest, so Gh05 is only there as Gh?5. When more
             than one fits, the one with the most fixed characters wins, so
             Gh05 is Gh?5 rather than Gh??.

    @param[in] Table - The table.

    @param[in] Tag - The tag's four characters, padded with spaces.

    @param[in] IgnoreCase - True for a tag someone typed, which a search
                            matches whatever its case.

    @return The pattern's entry, or nullptr if none covers the tag.

*/
const TableEntry*
FindPoolTagPattern (
    _In_ const TableSpan& Table,
    _In_ const std::string& Tag,
    _In_ bool IgnoreCase
    )
{
    //
    // Only about a hundred of pooltag.txt's tags are patterns, so find them
    // once rather than on every lookup.
    //
    static const std::vector<const TableEntry*> g_SdkPatterns = PoolTagPatterns(g_PoolTagTable);

    std::vector<const TableEntry*> patterns;
    const std::vector<const TableEntry*>* candidates;
    const TableEntry* best;
    size_t bestFixed;
    size_t fixed;
    size_t length;
    char wanted;
    char have;
    bool fits;

    best = nullptr;
    bestFixed = 0;

    if (Tag.size() != 4)
    {
        return nullptr;
    }

    candidates = &g_SdkPatterns;
    if (&Table != &g_PoolTagTable)
    {
        patterns = PoolTagPatterns(Table);
        candidates = &patterns;
    }

    for (const TableEntry* candidate : *candidates)
    {
        length = strlen(candidate->Name);
        fits = true;
        fixed = 0;

        for (size_t j = 0; j < 4; j++)
        {
            //
            // The table drops a tag's trailing spaces, so a short pattern
            // ends in them.
            //
            wanted = (j < length) ? candidate->Name[j] : ' ';
            have = Tag[j];

            if (wanted == '*')
            {
                break;
            }

            if (wanted == '?')
            {
                continue;
            }

            if (IgnoreCase ? (_strnicmp(&wanted, &have, 1) != 0) : (wanted != have))
            {
                fits = false;
                break;
            }

            fixed++;
        }

        if (fits && (fixed > bestFixed))
        {
            best = candidate;
            bestFixed = fixed;
        }
    }

    return best;
}


/*!

    @brief A pool tag's characters, its bytes as they sit in memory, which is
           how a debugger shows it.

    @param[in] Value - The value.

    @param[out] Tag - Receives the four characters.

    @return True if every byte is a printable character, as in a tag, and the
            first isn't a space.

*/
static
bool
PoolTagChars (
    _In_ uint32_t Value,
    _Out_ std::string* Tag
    )
{
    char c;

    Tag->clear();

    for (int i = 0; i < 4; i++)
    {
        c = SCAST(char)((Value >> (8 * i)) & 0xFF);
        if ((c < 0x20) || (c > 0x7E))
        {
            return false;
        }

        *Tag += c;
    }

    return (*Tag)[0] != ' ';
}


/*!

    @brief Converts a table name, which is always ASCII, to UTF-16.

    @param[in] Text - The name.

    @return The name.

*/
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
             so we drop \r and both come out the same.

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

    @details Uses ntdll's RtlNtStatusToDosError rather than a table of ours, so
             the answer's whatever Windows actually does.

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
    // It doesn't always admit there's no mapping, and hands the status back.
    // So unchanged or wider than 16 bits means none (STATUS_ABANDONED is 128).
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
        // HRESULT_FROM_NT is for Windows' own statuses. On a customer code it
        // makes a number that means nothing.
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
             they'd only repeat their primary. The table's sorted by value and
             the sort's stable, so each error's statuses stay in value order.

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
             quick but worth doing once. A function local static is only
             initialised once even when threads race, so the GUI's threads can
             share it.

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

    @brief Adds unnamed HRESULT and NTSTATUS matches for a value in neither
           table, when its shape is telling.

    @details Where both readings are plausible the HRESULT one wins, so an
             unknown 0x8007xxxx isn't also offered as an NTSTATUS warning.

    @param[in] Value - The value.

    @param[in,out] Matches - Matches are appended here.

*/
static
void
AddUnnamedCodes (
    _In_ uint32_t Value,
    _In_ bool AllowHResult,
    _In_ bool AllowNtStatus,
    _Inout_ std::vector<CodeMatch>& Matches
    )
{
    CodeMatch match;
    uint32_t win32;
    bool hresultShown;
    bool telling;

    hresultShown = false;

    //
    // R is clear in every HRESULT except the ones HRESULT_FROM_NT makes, which
    // carry N, so R set without N is some other kind of code.
    //
    if (AllowHResult && (((Value & k_ReservedBit) == 0) || ((Value & k_NtBit) != 0)))
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

    if (hresultShown || !AllowNtStatus)
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
        // RegisterWindowMessage shares RegisterClipboardFormat's atom table, so
        // this finds the name it was registered under in this session.
        //
        match.Description = std::format(L"registered as \"{}\" (RegisterWindowMessage)", name);
    }
    else
    {
        match.Description = L"in the RegisterWindowMessage range, but not registered in this session";
    }

    Matches.push_back(match);
}


/*!

    @brief Whether a value no table names may be taken apart as a kind of code.
           It follows the kind's main source, so an unnamed HRESULT follows
           winerror.h's HRESULTs.

    @param[in] Key - The kind's main source.

    @param[in] AlwaysNamed - Whether a source set to Always has named the
                             value.

    @return True if it may.

*/
static
bool
UnnamedAllowed (
    _In_z_ PCWSTR Key,
    _In_ bool AlwaysNamed
    )
{
    switch (SourceShowFor(Key))
    {
    case SourceShow::Never:
        return false;

    case SourceShow::Quiet:
        return !AlwaysNamed;

    default:
        return true;
    }
}


/*!

    @brief The Windows SDK version the tables were generated from, as in
           10.0.28000.0.

    @return A static, never null, string.

*/
PCWSTR
TablesSdkVersion (
    void
    )
{
    return g_TablesSdkVersion;
}


/*!

    @brief The name of a layout's severity, for display.

    @param[in] Fields - The layout.

    @return A static, never null, name: success or failure for an HRESULT;
            success, informational, warning or error for an NTSTATUS.

*/
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


/*!

    @brief The name of a kind of code, for display.

    @param[in] Kind - The kind.

    @return A static, never null, name.

*/
PCWSTR
CodeKindName (
    _In_ CodeKind Kind
    )
{
    switch (Kind)
    {
    case CodeKind::KnownValue:
        return L"Known value";

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

    case CodeKind::DeviceProblem:
        return L"Device problem";

    case CodeKind::UsbStatus:
        return L"USB status";

    case CodeKind::LdapError:
        return L"LDAP error";

    case CodeKind::PoolTag:
        return L"Pool tag";

    case CodeKind::Ioctl:
        return L"IOCTL";

    case CodeKind::Flags:
        return L"Flags";
    }

    return L"Unknown";
}


/*!

    @brief Looks a value up as every kind of code.

    @details A value in no table of a kind can still match it with no name, when
             its shape says something. That's an HRESULT whose facility or
             wrapped code is known, an NTSTATUS Windows maps to a Win32 error, a
             customer code, or a window message in the WM_USER, WM_APP or
             registered ranges.

    @param[in] Value - The value.

    @return One match for each kind of code the value is, in CodeKind order.
            Empty if it is none.

*/
std::vector<CodeMatch>
LookupValue (
    _In_ uint32_t Value
    )
{
    std::vector<CodeMatch> matches;
    std::vector<CodeMatch> quiet;
    const TableEntry* first;
    const TableEntry* end;
    const TableEntry* pattern;
    std::string tag;
    CodeMatch match;
    SourceShow show;
    bool alwaysNamed;
    bool haveHResultOrNtStatus;
    bool haveWindowMessage;
    bool haveSpecific;

    haveHResultOrNtStatus = false;
    haveWindowMessage = false;
    haveSpecific = false;

    for (const KindTable& kindTable : AllKindTables())
    {
        show = SourceShowFor(kindTable.Key);
        if (show == SourceShow::Never)
        {
            continue;
        }

        first = TableFindFirst(*kindTable.Table, Value);
        pattern = nullptr;

        if ((first == nullptr) && (kindTable.Kind == CodeKind::PoolTag) && PoolTagChars(Value, &tag))
        {
            pattern = FindPoolTagPattern(*kindTable.Table, tag, false);
            first = pattern;
        }

        if (first == nullptr)
        {
            continue;
        }

        match = CodeMatch();
        match.Kind = kindTable.Kind;
        match.Value = Value;
        match.Undocumented = (first->Flags & k_TableEntryUndocumented) != 0;
        match.Header = (kindTable.Table->Source != nullptr) ? kindTable.Table->Source : L"";
        if (kindTable.Table->Headers != nullptr)
        {
            match.Header = kindTable.Table->Headers[first - kindTable.Table->Entries];
        }

        match.UserCode = kindTable.User;

        //
        // A tag pooltag.txt only has as a pattern is named as itself, with the
        // pattern after it.
        //
        if (pattern != nullptr)
        {
            match.Names.push_back(AsciiToWide(tag.c_str()));
            while (match.Names[0].back() == L' ')
            {
                match.Names[0].pop_back();
            }

            match.Names.push_back(AsciiToWide(pattern->Name));
        }
        else
        {
            end = kindTable.Table->Entries + kindTable.Table->Count;
            for (const TableEntry* entry = first; (entry != end) && (entry->Value == Value); entry++)
            {
                match.Names.push_back(AsciiToWide(entry->Name));
            }
        }

        //
        // The user's codes aren't Windows', so whatever text Windows has for
        // the same value is about something else.
        //
        match.Text = kindTable.User ? std::wstring() : SystemText(kindTable.Kind, Value);
        match.Source = TextSource::System;

        if (match.Text.empty() && (first->Text != nullptr))
        {
            match.Text = Utf8ToWide(first->Text);
            NormaliseText(match.Text);
            //
            // Only Win32, HRESULT and NTSTATUS text comes from Windows. Any
            // other kind's is from the SDK's own files and needs no excuse.
            //
            match.Source = (!kindTable.User &&
                            ((kindTable.Kind == CodeKind::WinError) ||
                             (kindTable.Kind == CodeKind::HResult) ||
                             (kindTable.Kind == CodeKind::NtStatus))) ? TextSource::Captured : TextSource::Reference;
        }

        //
        // A known value's text is WinCode's own, not English standing in for
        // text this Windows lacks.
        //
        if (kindTable.Kind == CodeKind::KnownValue)
        {
            match.Source = TextSource::Curated;
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
        else if (kindTable.Kind == CodeKind::Ioctl)
        {
            match.Fields = IoctlFields(Value);
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

        if ((kindTable.Kind == CodeKind::KnownValue) ||
            (kindTable.Kind == CodeKind::PoolTag) ||
            (kindTable.Kind == CodeKind::Ioctl))
        {
            haveSpecific = true;
        }

        if (show == SourceShow::Quiet)
        {
            quiet.push_back(match);
            continue;
        }

        matches.push_back(match);
    }

    //
    // A quiet source's codes are only offered when no source set to Always
    // names the value.
    //
    alwaysNamed = !matches.empty();
    if (!alwaysNamed)
    {
        matches = std::move(quiet);
    }

    //
    // Unnamed HRESULT and NTSTATUS matches are guesses, and just noise once
    // something's named it. A three letter pool tag's space sets the C bit.
    //
    if (!haveHResultOrNtStatus && !haveSpecific && (Value > 0xFFFF))
    {
        AddUnnamedCodes(Value,
                        UnnamedAllowed(L"hresult", alwaysNamed),
                        UnnamedAllowed(L"ntstatus", alwaysNamed),
                        matches);
    }

    if (!haveWindowMessage && UnnamedAllowed(L"message", alwaysNamed))
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
