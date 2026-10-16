/*!

    @file Tests/WinCodeTests.cpp

    @brief Tests for the WinCode core.

    @details A plain exe rather than a test framework, so there's nothing to
             install. Each check prints its result, and the exit code is the
             failure count, which is what ctest goes by.

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

#include <stdio.h>
#include <string>
#include <unordered_set>


static int g_Failures;


/*!

    @brief Records and prints the result of one check.

    @param[in] Condition - True if the check passed.

    @param[in] What - What was checked.

*/
static
void
Check (
    _In_ bool Condition,
    _In_z_ PCSTR What
    )
{
    printf("  [%s] %s\n", Condition ? "ok  " : "FAIL", What);

    if (!Condition)
    {
        g_Failures++;
    }
}


/*!

    @brief Finds a table entry by name.

    @details A linear search, which is fine for tests. Real lookups go by
             value, which is what the tables are sorted for.

    @param[in] Table - The table to search.

    @param[in] Name - The symbolic name.

    @return The entry, or nullptr if the name isn't in the table.

*/
static
const TableEntry*
FindName (
    _In_ const TableSpan& Table,
    _In_z_ PCSTR Name
    )
{
    for (size_t i = 0; i < Table.Count; i++)
    {
        if (strcmp(Table.Entries[i].Name, Name) == 0)
        {
            return &Table.Entries[i];
        }
    }

    return nullptr;
}


/*!

    @brief Finds the primary entry for a value.

    @param[in] Table - The table to search.

    @param[in] Value - The value.

    @return The first entry with the value, or nullptr if there is none.

*/
static
const TableEntry*
FindPrimary (
    _In_ const TableSpan& Table,
    _In_ uint32_t Value
    )
{
    for (size_t i = 0; i < Table.Count; i++)
    {
        if (Table.Entries[i].Value == Value)
        {
            return &Table.Entries[i];
        }
    }

    return nullptr;
}


/*!

    @brief Checks a name is in a table with the expected value.

    @param[in] Table - The table.

    @param[in] Name - The symbolic name.

    @param[in] Value - The value it should have.

    @param[in] What - What is being checked, for the report.

*/
static
void
CheckName (
    _In_ const TableSpan& Table,
    _In_z_ PCSTR Name,
    _In_ uint32_t Value,
    _In_z_ PCSTR What
    )
{
    const TableEntry* entry;

    entry = FindName(Table, Name);
    Check((entry != nullptr) && (entry->Value == Value), What);
}


/*!

    @brief Checks the invariants every generated table must hold.

    @details Lookups binary search by value and then walk forward over the
             aliases, so the order and the alias flags have to agree exactly.

    @param[in] Label - The table's name, for the report.

    @param[in] Table - The table.

*/
static
void
CheckTableShape (
    _In_z_ PCSTR Label,
    _In_ const TableSpan& Table
    )
{
    std::unordered_set<std::string> names;
    bool sorted;
    bool aliasesConsistent;
    bool namesUnique;

    printf("Table %s (%zu entries)\n", Label, Table.Count);

    sorted = true;
    aliasesConsistent = true;
    namesUnique = true;

    for (size_t i = 0; i < Table.Count; i++)
    {
        const TableEntry& entry = Table.Entries[i];
        bool sameAsPrevious = (i > 0) && (Table.Entries[i - 1].Value == entry.Value);
        bool isAlias = (entry.Flags & k_TableEntryAlias) != 0;

        if ((i > 0) && (entry.Value < Table.Entries[i - 1].Value))
        {
            sorted = false;
        }

        if ((isAlias != sameAsPrevious) || (isAlias && (entry.Text != nullptr)))
        {
            aliasesConsistent = false;
        }

        if (!names.insert(entry.Name).second)
        {
            namesUnique = false;
        }
    }

    Check(Table.Count > 0, "not empty");
    Check(sorted, "sorted by value");
    Check(aliasesConsistent, "aliases follow their primary and carry no text");
    Check(namesUnique, "no name appears twice");
}


/*!

    @brief Checks the version string is there.

*/
static
void
TestVersion (
    void
    )
{
    PCWSTR version;

    printf("Version\n");

    version = WinCodeVersion();
    Check((version != nullptr) && (version[0] != L'\0'), "version string is not empty");
}


/*!

    @brief Checks every table's shape.

*/
static
void
TestTableShapes (
    void
    )
{
    CheckTableShape("WinError", g_WinErrorTable);
    CheckTableShape("HResult", g_HResultTable);
    CheckTableShape("HResultFacility", g_HResultFacilityTable);
    CheckTableShape("NtStatus", g_NtStatusTable);
    CheckTableShape("NtFacility", g_NtFacilityTable);
    CheckTableShape("BugCheck", g_BugCheckTable);
    CheckTableShape("WindowMessage", g_WindowMessageTable);
    CheckTableShape("KnownValue", g_KnownValueTable);
    CheckTableShape("HResultWindowsUpdate", g_HResultWindowsUpdateTable);
    CheckTableShape("HResultAudio", g_HResultAudioTable);
    CheckTableShape("HResultShadowCopy", g_HResultShadowCopyTable);
    CheckTableShape("HResultClr", g_HResultClrTable);
    CheckTableShape("DeviceProblem", g_DeviceProblemTable);
    CheckTableShape("UsbStatus", g_UsbStatusTable);
    CheckTableShape("LdapError", g_LdapErrorTable);
    CheckTableShape("PoolTag", g_PoolTagTable);
}


/*!

    @brief Checks known Win32 errors and HRESULTs, including the cases that
           need the compiler rather than a text scan to get right.

*/
static
void
TestWinErrorAndHResult (
    void
    )
{
    const TableEntry* entry;

    printf("Win32 errors and HRESULTs\n");

    CheckName(g_WinErrorTable, "ERROR_NO_MORE_FILES", 18, "ERROR_NO_MORE_FILES is 18");
    CheckName(g_WinErrorTable, "WSAECONNRESET", 10054, "WSAECONNRESET is 10054");

    entry = FindPrimary(g_WinErrorTable, 18);
    Check((entry != nullptr) && (entry->Text != nullptr), "ERROR_NO_MORE_FILES has captured text");

    entry = FindPrimary(g_WinErrorTable, 0);
    Check((entry != nullptr) && (strcmp(entry->Name, "ERROR_SUCCESS") == 0), "0 is ERROR_SUCCESS first");

    CheckName(g_HResultTable, "E_INVALIDARG", 0x80070057, "E_INVALIDARG takes the live half of its #if");
    CheckName(g_HResultTable, "E_FAIL", 0x80004005, "E_FAIL is 0x80004005");
    CheckName(g_HResultTable, "E_NOT_SET", 0x80070490, "E_NOT_SET, an HRESULT_FROM_WIN32 expression, is evaluated");
    Check(FindName(g_HResultTable, "DRAGDROP_E_FIRST") == nullptr, "range markers are left out");

    entry = FindPrimary(g_HResultTable, 0);
    Check((entry != nullptr) && (strcmp(entry->Name, "S_OK") == 0), "0 is S_OK first");

    CheckName(g_HResultFacilityTable, "FACILITY_WIN32", 7, "FACILITY_WIN32 is 7");
}


/*!

    @brief Checks known NTSTATUS codes and bugchecks.

*/
static
void
TestNtStatusAndBugCheck (
    void
    )
{
    const TableEntry* entry;

    printf("NTSTATUS codes and bugchecks\n");

    entry = FindPrimary(g_NtStatusTable, 0);
    Check((entry != nullptr) && (strcmp(entry->Name, "STATUS_SUCCESS") == 0), "0 is STATUS_SUCCESS first");

    entry = FindName(g_NtStatusTable, "STATUS_WAIT_0");
    Check((entry != nullptr) && ((entry->Flags & k_TableEntryAlias) != 0), "STATUS_WAIT_0 is an alias");

    entry = FindPrimary(g_NtStatusTable, 0xC0000005);
    Check((entry != nullptr) && (strcmp(entry->Name, "STATUS_ACCESS_VIOLATION") == 0),
          "0xC0000005 is STATUS_ACCESS_VIOLATION");
    Check((entry != nullptr) && (entry->Text != nullptr), "STATUS_ACCESS_VIOLATION has captured text");

    CheckName(g_NtFacilityTable, "FACILITY_DEBUGGER", 0x1, "FACILITY_DEBUGGER is 1");

    CheckName(g_BugCheckTable, "IRQL_NOT_LESS_OR_EQUAL", 0xA, "IRQL_NOT_LESS_OR_EQUAL is 0xA");
    CheckName(g_BugCheckTable, "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED_M", 0x1000007E, "_M variants are kept");
    CheckName(g_BugCheckTable, "MANUALLY_INITIATED_CRASH1", 0xDEADDEAD, "MANUALLY_INITIATED_CRASH1 is 0xDEADDEAD");
    Check(FindName(g_BugCheckTable, "WINDOWS_NT_BANNER") == nullptr, "bugcheck screen text IDs are left out");
}


/*!

    @brief Checks window messages from both sources.

*/
static
void
TestWindowMessages (
    void
    )
{
    const TableEntry* entry;

    printf("Window messages\n");

    entry = FindName(g_WindowMessageTable, "WM_CLOSE");
    Check((entry != nullptr) && (entry->Value == 0x10) && ((entry->Flags & k_TableEntryUndocumented) == 0),
          "WM_CLOSE is 0x10, from the headers");

    CheckName(g_WindowMessageTable, "WM_USER", 0x400, "WM_USER is 0x400");
    CheckName(g_WindowMessageTable, "WM_APP", 0x8000, "WM_APP is 0x8000, not the 0x32768 the old script produced");
    CheckName(g_WindowMessageTable, "CB_ADDSTRING", 0x143, "control messages below WM_USER are included");

    entry = FindName(g_WindowMessageTable, "WM_SIZEWAIT");
    Check((entry != nullptr) && (entry->Value == 0x4) && ((entry->Flags & k_TableEntryUndocumented) != 0),
          "WM_SIZEWAIT comes from the list, marked undocumented");

    Check(FindName(g_WindowMessageTable, "WM_KEYFIRST") == nullptr, "range markers are left out");
}


/*!

    @brief Checks a query yields exactly one number, read as expected.

    @param[in] Query - The query.

    @param[in] Value - The value it should yield.

    @param[in] Form - How it should have been read.

    @param[in] What - What is being checked, for the report.

*/
static
void
CheckParsesTo (
    _In_z_ PCWSTR Query,
    _In_ uint32_t Value,
    _In_ NumberForm Form,
    _In_z_ PCSTR What
    )
{
    std::vector<ParsedNumber> numbers;

    numbers = ParseQuery(Query).Numbers;
    Check((numbers.size() == 1) && (numbers[0].Value == Value) && (numbers[0].Form == Form), What);
}


/*!

    @brief Checks every form a number can be written in, and the text that
           must not be mistaken for one.

*/
static
void
TestParseQuery (
    void
    )
{
    std::vector<ParsedNumber> numbers;

    printf("Parsing\n");

    CheckParsesTo(L"18", 18, NumberForm::Decimal, "decimal");
    CheckParsesTo(L"0x12", 0x12, NumberForm::Hex, "0x hex");
    CheckParsesTo(L"C0000005", 0xC0000005, NumberForm::Hex, "bare hex");
    CheckParsesTo(L"0C0000005h", 0xC0000005, NumberForm::Hex, "assembler style hex");
    CheckParsesTo(L"-2147024891", 0x80070005, NumberForm::Negative, "negative decimal, as a signed 32-bit value");
    CheckParsesTo(L"0xFFFFFFFF80070005", 0x80070005, NumberForm::SignExtended, "sign-extended 64-bit value");
    CheckParsesTo(L"0x80004005L", 0x80004005, NumberForm::Hex, "C integer suffix");
    CheckParsesTo(L"(HRESULT: 0x80070005)", 0x80070005, NumberForm::Hex, "label and brackets");
    CheckParsesTo(L"Status=0xC0000022.", 0xC0000022, NumberForm::Hex, "label and a full stop");
    CheckParsesTo(L"A", 0xA, NumberForm::Hex, "a lone hex word is hex");
    CheckParsesTo(L"code DEADDEAD here", 0xDEADDEAD, NumberForm::Hex, "eight hex letters count inside text");

    numbers = ParseQuery(L"80070005").Numbers;
    Check((numbers.size() == 2) &&
          (numbers[0].Form == NumberForm::Decimal) &&
          (numbers[1].Form == NumberForm::HexGuess) &&
          (numbers[1].Value == 0x80070005),
          "eight decimal digits are also read as hex");

    Check(ParseQuery(L"it failed with a bad code").Numbers.empty(), "words that happen to be hex aren't numbers");
    Check(ParseQuery(L"ALL").Numbers.empty(), "a word doesn't lose letters to suffix stripping");
    Check(ParseQuery(L"0x100000000").Numbers.empty(), "hex wider than 32 bits isn't a code");
    Check(ParseQuery(L"4294967296").Numbers.empty(), "decimal wider than 32 bits isn't a code");

    numbers = ParseQuery(L"5 and 5 again, then 0x5").Numbers;
    Check(numbers.size() == 1, "repeated values are dropped");
}


/*!

    @brief Checks which tokens are taken as names to search for.

*/
static
void
TestParseNames (
    void
    )
{
    ParsedQuery query;

    printf("Names in queries\n");

    query = ParseQuery(L"wm_close");
    Check((query.Names.size() == 1) && (query.Names[0] == L"wm_close") && query.Numbers.empty(),
          "a lone name is a name");

    query = ParseQuery(L"STATUS_ACCESS_DENIED returned 0x5");
    Check((query.Names.size() == 1) && (query.Numbers.size() == 1), "names and numbers mix");

    query = ParseQuery(L"CreateFile failed with an error");
    Check(query.Names.empty(), "ordinary words in text aren't searched for");
}


/*!

    @brief Checks name search and its grading.

*/
static
void
TestSearchNames (
    void
    )
{
    std::vector<NameMatch> results;
    bool foundError;
    bool foundStatus;

    printf("Name search\n");

    results = SearchNames(L"wm_close");
    Check(!results.empty() &&
          (results[0].Quality == NameQuality::Exact) &&
          (results[0].Name == L"WM_CLOSE") &&
          (results[0].Value == 0x10),
          "wm_close finds WM_CLOSE exactly, ignoring case");

    results = SearchNames(L"ACCESS_DENIED");
    foundError = false;
    foundStatus = false;
    for (const NameMatch& result : results)
    {
        if ((result.Name == L"ERROR_ACCESS_DENIED") && (result.Quality == NameQuality::Words))
        {
            foundError = true;
        }

        if ((result.Name == L"STATUS_ACCESS_DENIED") && (result.Quality == NameQuality::Words))
        {
            foundStatus = true;
        }
    }
    Check(foundError && foundStatus, "ACCESS_DENIED finds ERROR_ and STATUS_ACCESS_DENIED as whole words");
    Check(!results.empty() && (results[0].Quality == NameQuality::Words), "... with whole word matches first");

    results = SearchNames(L"ACCESS_DEN");
    Check(!results.empty() && (results[0].Quality == NameQuality::Partial), "part of a word is a partial match");

    results = SearchNames(L"NOT_A_REAL_NAME_AT_ALL");
    Check(results.empty(), "nonsense finds nothing");
}


/*!

    @brief Finds the match of one kind in a lookup's results.

    @param[in] Matches - The results.

    @param[in] Kind - The kind wanted.

    @return The match, or nullptr if there is none of that kind.

*/
static
const CodeMatch*
FindMatch (
    _In_ const std::vector<CodeMatch>& Matches,
    _In_ CodeKind Kind
    )
{
    for (const CodeMatch& match : Matches)
    {
        if (match.Kind == Kind)
        {
            return &match;
        }
    }

    return nullptr;
}


/*!

    @brief Checks lookups by value across the tables.

*/
static
void
TestLookupValue (
    void
    )
{
    std::vector<CodeMatch> matches;
    const CodeMatch* match;

    printf("Lookup\n");

    matches = LookupValue(0xC0000005);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) && (match->Names[0] == L"STATUS_ACCESS_VIOLATION"), "0xC0000005 is STATUS_ACCESS_VIOLATION");
    Check((match != nullptr) && !match->Text.empty() && (match->Source != TextSource::None), "... with message text");

    matches = LookupValue(0x80070005);
    match = FindMatch(matches, CodeKind::HResult);
    Check((match != nullptr) && (match->Names[0] == L"E_ACCESSDENIED"), "0x80070005 is E_ACCESSDENIED");

    matches = LookupValue(18);
    match = FindMatch(matches, CodeKind::WinError);
    Check((match != nullptr) && (match->Names[0] == L"ERROR_NO_MORE_FILES"), "18 is ERROR_NO_MORE_FILES");
    match = FindMatch(matches, CodeKind::WindowMessage);
    Check((match != nullptr) && (match->Names[0] == L"WM_QUIT"), "... and WM_QUIT, since every kind is looked up");

    matches = LookupValue(0);
    match = FindMatch(matches, CodeKind::WinError);
    Check((match != nullptr) && (match->Names.size() > 1) && (match->Names[0] == L"ERROR_SUCCESS"),
          "0 is ERROR_SUCCESS, with its aliases after it");

    matches = LookupValue(1);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) && match->Text.empty() && (match->Source == TextSource::None),
          "STATUS_WAIT_1's text, which is only its name, is dropped");

    matches = LookupValue(4);
    match = FindMatch(matches, CodeKind::WindowMessage);
    Check((match != nullptr) && match->Undocumented, "WM_SIZEWAIT is flagged undocumented");

    Check(LookupValue(0x12345678).empty(), "a value that is no code matches nothing");
}


/*!

    @brief Finds the related code of one kind in a match.

    @param[in] Match - The match.

    @param[in] Kind - The kind wanted.

    @return The related code, or nullptr if there is none of that kind.

*/
static
const RelatedCode*
FindRelated (
    _In_ const CodeMatch& Match,
    _In_ CodeKind Kind
    )
{
    for (const RelatedCode& related : Match.Related)
    {
        if (related.Kind == Kind)
        {
            return &related;
        }
    }

    return nullptr;
}


/*!

    @brief Checks fields, the codes a match converts to or wraps, and the
           decoding of values that are in no table.

*/
static
void
TestCrossReferences (
    void
    )
{
    std::vector<CodeMatch> matches;
    const CodeMatch* match;
    const RelatedCode* related;

    printf("Cross references and decoding\n");

    matches = LookupValue(5);
    match = FindMatch(matches, CodeKind::WinError);
    related = (match != nullptr) ? FindRelated(*match, CodeKind::HResult) : nullptr;
    Check((related != nullptr) && (related->Value == 0x80070005) && (related->Name == L"E_ACCESSDENIED"),
          "Win32 5 is 0x80070005 E_ACCESSDENIED as an HRESULT");
    Check((related != nullptr) &&
          (related->Relation == RelationKind::AsHResult) &&
          (std::wstring(related->Via) == L"HRESULT_FROM_WIN32"),
          "... by HRESULT_FROM_WIN32");

    matches = LookupValue(0x80070005);
    match = FindMatch(matches, CodeKind::HResult);
    Check((match != nullptr) &&
          match->Fields.has_value() &&
          (match->Fields->FacilityName == L"FACILITY_WIN32") &&
          (match->Fields->Code == 5),
          "0x80070005's fields are FACILITY_WIN32, code 5");
    related = (match != nullptr) ? FindRelated(*match, CodeKind::WinError) : nullptr;
    Check((related != nullptr) && (related->Value == 5) && (related->Name == L"ERROR_ACCESS_DENIED"),
          "... and it wraps ERROR_ACCESS_DENIED");
    Check((related != nullptr) && (related->Relation == RelationKind::Wraps), "... marked as wrapped");

    matches = LookupValue(0xC0000022);
    match = FindMatch(matches, CodeKind::NtStatus);
    related = (match != nullptr) ? FindRelated(*match, CodeKind::WinError) : nullptr;
    Check((related != nullptr) && (related->Value == ERROR_ACCESS_DENIED),
          "STATUS_ACCESS_DENIED maps to ERROR_ACCESS_DENIED");
    Check((related != nullptr) &&
          (related->Relation == RelationKind::MapsTo) &&
          (std::wstring(related->Via) == L"RtlNtStatusToDosError"),
          "... by RtlNtStatusToDosError");
    related = (match != nullptr) ? FindRelated(*match, CodeKind::HResult) : nullptr;
    Check((related != nullptr) && (related->Value == 0xD0000022), "... and is 0xD0000022 as an HRESULT");

    matches = LookupValue(0xD0000022);
    match = FindMatch(matches, CodeKind::HResult);
    related = (match != nullptr) ? FindRelated(*match, CodeKind::NtStatus) : nullptr;
    Check((related != nullptr) && (related->Name == L"STATUS_ACCESS_DENIED"),
          "0xD0000022, in no table, wraps STATUS_ACCESS_DENIED");

    matches = LookupValue(0x8007FFFF);
    match = FindMatch(matches, CodeKind::HResult);
    Check((match != nullptr) &&
          match->Names.empty() &&
          match->Fields.has_value() &&
          (match->Fields->FacilityName == L"FACILITY_WIN32"),
          "an unnamed 0x8007xxxx is still decoded as an HRESULT");
    Check(FindMatch(matches, CodeKind::NtStatus) == nullptr, "... and not also offered as an NTSTATUS");

    //
    // Not 0xE06D7363, the MSVC C++ exception, since that's a known value now
    // and gets named.
    //
    matches = LookupValue(0xE0AB1234);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) &&
          match->Names.empty() &&
          match->Fields.has_value() &&
          match->Fields->Customer &&
          (match->Fields->Facility == 0x0AB) &&
          (match->Fields->Code == 0x1234),
          "0xE0AB1234, in no table, decodes as a customer code");
    Check((match != nullptr) &&
          (FindRelated(*match, CodeKind::WinError) == nullptr) &&
          (FindRelated(*match, CodeKind::HResult) == nullptr),
          "... with no Win32 mapping or HRESULT form, which mean nothing for a customer code");
}


/*!

    @brief Checks window messages outside the tables.

*/
static
void
TestMessageRanges (
    void
    )
{
    std::vector<CodeMatch> matches;
    const CodeMatch* match;
    UINT registered;

    printf("Window message ranges\n");

    matches = LookupValue(0x401);
    match = FindMatch(matches, CodeKind::WindowMessage);
    Check((match != nullptr) && match->Names.empty() && (match->Description.find(L"WM_USER + 1") == 0),
          "0x401 is WM_USER + 1");

    matches = LookupValue(0x8001);
    match = FindMatch(matches, CodeKind::WindowMessage);
    Check((match != nullptr) && (match->Description.find(L"WM_APP + 1") == 0), "0x8001 is WM_APP + 1");

    registered = RegisterWindowMessageW(L"WinCodeTestMessage");
    matches = LookupValue(registered);
    match = FindMatch(matches, CodeKind::WindowMessage);
    Check((registered != 0) &&
          (match != nullptr) &&
          (match->Description.find(L"WinCodeTestMessage") != std::wstring::npos),
          "a registered message is named");
}


/*!

    @brief Checks reverse mapping, and that it agrees with the forward one.

*/
static
void
TestReverseMapping (
    void
    )
{
    std::vector<CodeMatch> matches;
    std::vector<CodeMatch> forward;
    const CodeMatch* match;
    const CodeMatch* status;
    const RelatedCode* related;
    bool hasAccessDenied;
    bool allAgree;
    bool hasAbandoned;

    printf("Reverse mapping\n");

    matches = LookupValue(ERROR_ACCESS_DENIED);
    match = FindMatch(matches, CodeKind::WinError);
    Check((match != nullptr) && (match->MappedFrom.size() > 1), "several NTSTATUS codes map to ERROR_ACCESS_DENIED");

    hasAccessDenied = false;
    allAgree = (match != nullptr);

    if (match != nullptr)
    {
        for (const RelatedCode& from : match->MappedFrom)
        {
            if (from.Name == L"STATUS_ACCESS_DENIED")
            {
                hasAccessDenied = true;
            }

            forward = LookupValue(from.Value);
            status = FindMatch(forward, CodeKind::NtStatus);
            related = (status != nullptr) ? FindRelated(*status, CodeKind::WinError) : nullptr;

            if ((from.Kind != CodeKind::NtStatus) ||
                (from.Relation != RelationKind::MappedFrom) ||
                (related == nullptr) ||
                (related->Value != ERROR_ACCESS_DENIED))
            {
                allAgree = false;
            }
        }
    }

    Check(hasAccessDenied, "... including STATUS_ACCESS_DENIED");
    Check(allAgree, "... and each of them maps back to it");

    matches = LookupValue(0x80);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) && (FindRelated(*match, CodeKind::WinError) == nullptr),
          "STATUS_ABANDONED doesn't map to Win32 128: Windows only passes it through");

    match = FindMatch(matches, CodeKind::WinError);
    hasAbandoned = false;

    if (match != nullptr)
    {
        for (const RelatedCode& from : match->MappedFrom)
        {
            if (from.Name == L"STATUS_ABANDONED")
            {
                hasAbandoned = true;
            }
        }
    }

    Check((match != nullptr) && !hasAbandoned, "... so it isn't listed under ERROR_WAIT_NO_CHILDREN either");
}


/*!

    @brief Checks the text the front ends share for values, fields and related
           codes.

*/
static
void
TestDescribe (
    void
    )
{
    std::vector<CodeMatch> matches;
    const CodeMatch* match;

    printf("Describing codes\n");

    Check(FormatCodeValue(CodeKind::WinError, 5) == L"5", "a Win32 error is written in decimal");
    Check(FormatCodeValue(CodeKind::WindowMessage, 0x10) == L"0x0010", "a window message in four hex digits");
    Check(FormatCodeValue(CodeKind::NtStatus, 0xC0000022) == L"0xC0000022", "anything else in eight");

    matches = LookupValue(0x80070005);
    match = FindMatch(matches, CodeKind::HResult);
    Check((match != nullptr) &&
          match->Fields.has_value() &&
          (DescribeFields(*match->Fields) == L"failure, FACILITY_WIN32 (7), code 0x0005 (5)"),
          "an HRESULT's fields");
    Check((match != nullptr) &&
          !match->Related.empty() &&
          (DescribeRelated(match->Related[0]) == L"Win32 error 5 ERROR_ACCESS_DENIED (HRESULT_FROM_WIN32)"),
          "the code it wraps");
    Check(std::wstring(RelationLabel(RelationKind::MappedFrom)) == L"mapped from", "a relation's label");

    printf("Where names come from\n");

    Check(std::wstring(TablesSdkVersion()).starts_with(L"10.0."), "the tables name the SDK they came from");

    matches = LookupValue(5);
    match = FindMatch(matches, CodeKind::WinError);
    Check((match != nullptr) && (match->Header == L"winerror.h"),
          "ERROR_ACCESS_DENIED comes from winerror.h");

    match = FindMatch(matches, CodeKind::WindowMessage);
    Check((match != nullptr) && (match->Header == L"winuser.h"),
          "WM_SIZE comes from winuser.h");

    matches = LookupValue(0xE0AB1234);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) && match->Header.empty(), "a value in no table has no header");
}


/*!

    @brief Runs every test.

    @return The number of failed checks, so zero means everything passed.

*/
/*!

    @brief The next number from a small, fixed pseudo-random sequence, so the
           odd-input test is the same on every run.

    @param[in,out] State - The sequence's state.

    @return The number.

*/
static
uint32_t
NextRandom (
    _Inout_ uint32_t* State
    )
{
    uint32_t x;

    //
    // xorshift32, plenty for picking characters and the same everywhere.
    //
    x = *State;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *State = x;
    return x;
}


/*!

    @brief Runs one input through everything that reads a query.

    @param[in] Query - The input.

    @param[in] Search - Whether to search for its names too, which reads every
                        table and so is kept to some of the inputs.

*/
static
void
Exercise (
    _In_ const std::wstring& Query,
    _In_ bool Search
    )
{
    std::wstring family;
    ParsedQuery parsed;
    CodeMatch match;
    uint32_t value;

    parsed = ParseQuery(Query.c_str());

    for (const ParsedNumber& number : parsed.Numbers)
    {
        SCAST(void)(LookupValue(number.Value));
    }

    if (Search && !parsed.Names.empty())
    {
        SCAST(void)(SearchNames(parsed.Names[0].c_str()));
    }

    if (ParseFlagQuery(Query.c_str(), &family, &value))
    {
        SCAST(void)(DecodeFlags(family.c_str(), value, &match));
    }
}


/*!

    @brief Throws odd input at everything that reads a query, thousands of
           generated strings and the edge cases, none of which may crash.

    @details A fixed, small cousin of the libFuzzer target (WinCodeFuzz.cpp),
             so every test run gets some of its benefit.

*/
static
void
TestOddInput (
    void
    )
{
    static constexpr WCHAR k_Alphabet[] =
        L"0123456789abcdefABCDEFxXhHlLuUvV-_ .:,;()[]{}=\"'\t\r\n"
        L"\x00E9\x4E2D\xFFFF\xD83D\xDE00\xD800";

    static constexpr PCWSTR k_EdgeCases[] =
    {
        L"",
        L"0x",
        L"-",
        L"--1",
        L"0h",
        L"h",
        L"0xFFFFFFFFFFFFFFFFFFFFFFFF",
        L"99999999999999999999999999",
        L"-99999999999999999999",
        L"-0",
        L"0x-1",
        L"file",
        L"file 0x",
        L"file:",
        L":0x5",
        L"access 0x1 0x2",
        L"style 0xFFFFFFFF",
        L"disposition 0",
        L"\xD800",
        L"_",
        L"___",
    };

    std::wstring input;
    uint32_t state;
    size_t length;
    int count;

    printf("Odd input\n");

    for (PCWSTR edge : k_EdgeCases)
    {
        Exercise(edge, true);
    }

    Exercise(std::wstring(100000, L'9'), true);
    Exercise(std::wstring(100000, L'_'), true);
    Exercise(std::wstring(50000, L' ') + L"0x5", true);

    state = 0x57434F44;
    count = 20000;

    for (int i = 0; i < count; i++)
    {
        length = NextRandom(&state) % 48;
        input.clear();

        for (size_t j = 0; j < length; j++)
        {
            input += k_Alphabet[NextRandom(&state) % (ARRAYSIZE(k_Alphabet) - 1)];
        }

        Exercise(input, (i % 50) == 0);
    }

    Check(true, "the edge cases and 20,000 generated inputs ran without a crash");
}


/*!

    @brief Checks choosing which sources of codes are shown.

*/
static
void
TestSourceShows (
    void
    )
{
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    std::vector<CodeSource> sources;
    bool found;

    printf("Choosing sources\n");

    ResetSourceShows();

    sources = CodeSources();
    Check((sources.size() == 15) && (sources[0].Key == L"known"), "every table is a source, in order");
    Check(GetSourceShow(L"ldap") == SourceShow::Quiet, "LDAP results are quiet by default");

    matches = LookupValue(18);
    Check(FindMatch(matches, CodeKind::WindowMessage) != nullptr, "window messages show by default");

    SetSourceShow(L"message", SourceShow::Never);

    matches = LookupValue(18);
    Check(FindMatch(matches, CodeKind::WindowMessage) == nullptr, "a source set to Never isn't shown");

    matches = LookupValue(WM_USER + 1);
    Check(FindMatch(matches, CodeKind::WindowMessage) == nullptr, "... nor are its unnamed values");

    names = SearchNames(L"WM_CLOSE");
    found = false;
    for (const NameMatch& name : names)
    {
        found = found || (name.Kind == CodeKind::WindowMessage);
    }
    Check(!found, "... nor found by name");

    SetSourceShow(L"message", SourceShow::Always);
    SetSourceShow(L"ldap", SourceShow::Always);

    matches = LookupValue(49);
    Check(FindMatch(matches, CodeKind::LdapError) != nullptr, "a quiet source set to Always shows beside the rest");

    SetSourceShow(L"hresult", SourceShow::Never);

    matches = LookupValue(0xD0000022);
    Check(FindMatch(matches, CodeKind::HResult) == nullptr, "an unnamed HRESULT follows winerror.h's setting");

    ResetSourceShows();

    matches = LookupValue(49);
    Check(FindMatch(matches, CodeKind::LdapError) == nullptr, "restoring the defaults makes LDAP quiet again");

    ShowEverySource(true);
    matches = LookupValue(49);
    Check(FindMatch(matches, CodeKind::LdapError) != nullptr, "--all shows every source");
    ShowEverySource(false);
}


/*!

    @brief Checks reading your own headers.

*/
static
void
TestUserCodes (
    void
    )
{
    static constexpr WCHAR k_Driver[] =
        L"#ifndef MYDRV_STATUS_H\n"
        L"#define MYDRV_STATUS_H\n"
        L"#define FACILITY_MYDRV 0x123\n"
        L"#define MYDRV_BUFFER_SIZE 256\n"
        L"#define MYDRV_STATUS_NOT_READY ((NTSTATUS)0xE0010001L) // the device isn't ready\n"
        L"#define MYDRV_E_BAD_CONFIG _HRESULT_TYPEDEF_(0x80AB0001L)\n"
        L"#define MYDRV_MAKE(n) MAKE_HRESULT(SEVERITY_ERROR, FACILITY_MYDRV, n)\n"
        L"#define MYDRV_E_TIMEOUT MYDRV_MAKE(2)\n"
        L"#define MYDRV_ERROR_BUSY 61234L\n"
        L"#define MYDRV_TAG 'gaTM' /* request context */\n"
        L"#define MYDRV_E_LATER MYDRV_E_EARLIER\n"
        L"#define MYDRV_E_EARLIER MAKE_HRESULT(1, FACILITY_ITF, 0x0201)\n"
        L"#define MYDRV_E_LOOP (MYDRV_E_LOOP + 1)\n"
        L"#define MYDRV_E_UNKNOWN SOMETHING_UNDEFINED\n"
        L"#define MYDRV_E_WRAPPED \\\n"
        L"    _HRESULT_TYPEDEF_(0x80AB0003L)\n"
        L"#define MYDRV_STATUS_DENIED STATUS_ACCESS_DENIED\n"
        L"//\n"
        L"// MessageId: MYDRV_STATUS_RETRY\n"
        L"//\n"
        L"// MessageText:\n"
        L"//\n"
        L"// Try the request\n"
        L"// again.\n"
        L"//\n"
        L"#define MYDRV_STATUS_RETRY ((NTSTATUS)0x60010002L)\n"
        L"#define MYDRV_CANCEL_TAG '!!rL'\n"
        L"#endif\n";

    static constexpr WCHAR k_Other[] =
        L"#define OTHER_FAILED 0xE0020001\n"
        L"#define OTHER_E_MINE MAKE_HRESULT(1, FACILITY_MYDRV, 5)\n";

    std::vector<HeaderSource> headers;
    std::vector<HeaderRead> reads;
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    std::vector<CodeSource> sources;
    std::vector<UserHeader> status;
    const HeaderCode* code;
    const CodeMatch* match;
    std::wstring key;
    size_t ntStatusCount;
    bool found;

    printf("Your own codes\n");

    headers.push_back({ L"C:\\src\\mydrv.h", k_Driver, UserTreatAs::Automatic });
    headers.push_back({ L"C:\\src\\other.h", k_Other, UserTreatAs::NtStatus });

    reads = ReadHeaders(headers);
    Check(reads.size() == 2, "each header is read");

    auto FindCode = [&reads](size_t Header, PCWSTR Name) -> const HeaderCode*
    {
        for (const HeaderCode& candidate : reads[Header].Codes)
        {
            if (candidate.Name == Name)
            {
                return &candidate;
            }
        }
        return nullptr;
    };

    code = FindCode(0, L"MYDRV_STATUS_NOT_READY");
    Check((code != nullptr) && (code->Kind == CodeKind::NtStatus) && (code->Value == 0xE0010001) && (code->Line == 5),
          "an NTSTATUS cast is an NTSTATUS, on its line");
    Check((code != nullptr) && (code->Text == L"The device isn't ready."), "... with its comment as its text");

    code = FindCode(0, L"MYDRV_E_BAD_CONFIG");
    Check((code != nullptr) && (code->Kind == CodeKind::HResult) && (code->Value == 0x80AB0001), "_HRESULT_TYPEDEF_ is an HRESULT");

    code = FindCode(0, L"MYDRV_E_TIMEOUT");
    Check((code != nullptr) && (code->Kind == CodeKind::HResult) && (code->Value == 0x81230002),
          "a macro of the header's own, using its own facility, is evaluated");

    code = FindCode(0, L"MYDRV_ERROR_BUSY");
    Check((code != nullptr) && (code->Kind == CodeKind::WinError) && (code->Value == 61234), "a bare L number is a Win32 error");

    code = FindCode(0, L"MYDRV_TAG");
    Check((code != nullptr) && (code->Kind == CodeKind::PoolTag) && (code->Value == 0x6761544D) && (code->Tag == L"MTag"),
          "a character constant is a pool tag, named as a debugger shows it");
    Check((code != nullptr) && (code->Text == L"Request context."), "... with a block comment as its text");

    code = FindCode(0, L"MYDRV_E_LATER");
    Check((code != nullptr) && (code->Value == 0x80040201), "a name defined later, and an SDK facility, are used");

    code = FindCode(0, L"MYDRV_E_WRAPPED");
    Check((code != nullptr) && (code->Value == 0x80AB0003), "a definition carried on with a backslash is read whole");

    code = FindCode(0, L"MYDRV_STATUS_DENIED");
    Check((code != nullptr) && (code->Kind == CodeKind::NtStatus) && (code->Value == 0xC0000022), "an SDK code's name is used, kind and all");

    code = FindCode(0, L"MYDRV_STATUS_RETRY");
    Check((code != nullptr) && (code->Text == L"Try the request again."), "a MessageText block is a code's text");

    Check((FindCode(0, L"MYDRV_BUFFER_SIZE") == nullptr) && (FindCode(0, L"FACILITY_MYDRV") == nullptr) &&
          (FindCode(0, L"MYDRV_STATUS_H") == nullptr),
          "sizes, facilities and include guards aren't codes");

    found = false;
    for (const HeaderSkip& skip : reads[0].Skipped)
    {
        found = found || ((skip.Name == L"MYDRV_E_UNKNOWN") && (skip.Line == 14) && (skip.Reason.find(L"SOMETHING_UNDEFINED") != std::wstring::npos));
    }
    Check(found, "a code using an unknown name is reported, with its line and why");

    found = false;
    for (const HeaderSkip& skip : reads[0].Skipped)
    {
        found = found || (skip.Name == L"MYDRV_E_LOOP");
    }
    Check(found && (reads[0].Skipped.size() == 2), "a code defined in terms of itself is reported, and nothing else is");

    code = FindCode(1, L"OTHER_FAILED");
    Check((code != nullptr) && (code->Kind == CodeKind::NtStatus), "Treat as says what a plain number is");

    code = FindCode(1, L"OTHER_E_MINE");
    Check((code != nullptr) && (code->Kind == CodeKind::HResult) && (code->Value == 0x81230005),
          "... but not a code that says otherwise, and one header can use another's names");

    ResetSourceShows();
    LoadUserCodeText(headers);

    matches = LookupValue(0xE0010001);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) && match->UserCode && (match->Names[0] == L"MYDRV_STATUS_NOT_READY") && (match->Header == L"mydrv.h"),
          "a lookup finds the user's code, from its header");
    Check((match != nullptr) && (match->Text == L"The device isn't ready.") && (match->Source == TextSource::Reference),
          "... with the header's text");

    matches = LookupValue(0xC0000022);
    ntStatusCount = 0;
    for (const CodeMatch& candidate : matches)
    {
        ntStatusCount += (candidate.Kind == CodeKind::NtStatus) ? 1 : 0;
    }
    Check(ntStatusCount == 2, "a value that's also an SDK code shows both");

    matches = LookupValue(0x6761544D);
    match = FindMatch(matches, CodeKind::PoolTag);
    Check((match != nullptr) && (match->Names.size() == 2) && (match->Names[0] == L"MTag") && (match->Names[1] == L"MYDRV_TAG"),
          "a pool tag is named by its tag, then the header's name for it");

    names = SearchNames(L"Lr!!");
    Check((names.size() == 1) && (names[0].Name == L"Lr!!"), "a tag both Windows and your header have is found once");

    names = SearchNames(L"MYDRV_E_TIMEOUT");
    Check(!names.empty() && (names[0].Kind == CodeKind::HResult) && (names[0].Value == 0x81230002), "a name search finds the user's codes");

    sources = CodeSources();
    Check((sources.size() == 17) && sources[15].User && (sources[15].Name == L"Your codes: mydrv.h"), "each header is one source");

    status = UserHeaders();
    Check((status.size() == 2) && (status[0].CodeCount == reads[0].Codes.size()) && (status[0].Skipped.size() == 2),
          "each header's codes and skips are counted");

    //
    // Hidden, the value still shows as an unnamed customer NTSTATUS, so it's
    // the user's code that has to be missing.
    //
    key = sources[15].Key;
    SetSourceShow(key.c_str(), SourceShow::Never);
    matches = LookupValue(0xE0010001);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) && !match->UserCode, "a header set to Never isn't shown");

    LoadUserCodeText(headers);
    matches = LookupValue(0xE0010001);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) && !match->UserCode, "... and stays hidden when it's read again");

    ResetSourceShows();
    LoadUserCodeText({});
    Check(CodeSources().size() == 15, "removing the headers removes their sources");
}


/*!

    @brief Checks IOCTLs: named ones from the SDK and WDK, their fields, the
           ioctl query, and a CTL_CODE in your own header.

*/
static
void
TestIoctls (
    void
    )
{
    static constexpr WCHAR k_Driver[] =
        L"#define MYDRV_DEVICE 0x8123\n"
        L"#define IOCTL_MYDRV_RESET CTL_CODE(MYDRV_DEVICE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS) // resets it\n";

    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    const CodeMatch* match;
    CodeMatch decode;
    uint32_t value;

    printf("IOCTLs\n");

    ResetSourceShows();

    matches = LookupValue(0x00070000);
    match = FindMatch(matches, CodeKind::Ioctl);
    Check((match != nullptr) && (match->Names[0] == L"IOCTL_DISK_GET_DRIVE_GEOMETRY"), "0x70000 is IOCTL_DISK_GET_DRIVE_GEOMETRY");
    Check((match != nullptr) && match->Fields.has_value() &&
          (DescribeFields(*match->Fields) == L"FILE_DEVICE_DISK (0x7), function 0x0 (0), METHOD_BUFFERED, FILE_ANY_ACCESS"),
          "... with its fields named");
    Check((match != nullptr) && (match->Header == L"ntdddisk.h"), "... from the header that defines it");
    Check(FindMatch(matches, CodeKind::HResult) == nullptr, "... and no unnamed HRESULT guessed beside it");

    matches = LookupValue(0x00090018);
    match = FindMatch(matches, CodeKind::Ioctl);
    Check((match != nullptr) && (match->Names[0] == L"FSCTL_LOCK_VOLUME"), "FSCTLs are named too");

    matches = LookupValue(0x006D0008);
    match = FindMatch(matches, CodeKind::Ioctl);
    Check((match != nullptr) && (match->Names[0] == L"IOCTL_MOUNTMGR_QUERY_POINTS") && (match->Header == L"mountmgr.h"),
          "so are the WDK's, from mountmgr.h");

    matches = LookupValue(0x00170002);
    match = FindMatch(matches, CodeKind::Ioctl);
    Check((match != nullptr) && (match->Names[0] == L"IOCTL_NDIS_QUERY_GLOBAL_STATS"), "and NDIS's, made with its own macro");

    names = SearchNames(L"IOCTL_STORAGE_QUERY_PROPERTY");
    Check(!names.empty() && (names[0].Kind == CodeKind::Ioctl) && (names[0].Value == 0x002D1400), "a name search finds IOCTLs");

    Check(ParseIoctlQuery(L"ioctl 0x2D1400", &value) && (value == 0x002D1400), "\"ioctl 0x2D1400\" asks for a decode");
    Check(ParseIoctlQuery(L"FSCTL: 0x90018", &value) && (value == 0x00090018), "... as does \"FSCTL: 0x90018\"");
    Check(!ParseIoctlQuery(L"ioctl", &value) && !ParseIoctlQuery(L"ioctl 1 2", &value) && !ParseIoctlQuery(L"0x2D1400", &value),
          "... but not without exactly one value");

    DecodeIoctl(0x002D1400, &decode);
    Check((decode.Kind == CodeKind::Ioctl) && (decode.Names[0] == L"IOCTL_STORAGE_QUERY_PROPERTY"), "a decode of a named IOCTL names it");

    DecodeIoctl(0x8000E00F, &decode);
    Check((decode.Names[0] == L"CTL_CODE(0x8000, 0x803, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)"),
          "an unnamed one is called by its CTL_CODE");
    Check(decode.Fields.has_value() &&
          (DescribeFields(*decode.Fields) ==
           L"device type 0x8000, vendor device type, function 0x803 (2051), vendor function, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS"),
          "... with vendor ranges said so");

    matches = LookupValue(0x0000E00F);
    Check(FindMatch(matches, CodeKind::Ioctl) == nullptr, "a plain lookup doesn't guess at IOCTLs");

    LoadUserCodeText({ { L"C:\\src\\mydrv.h", k_Driver, UserTreatAs::Automatic } });
    matches = LookupValue(0x81232004);
    match = FindMatch(matches, CodeKind::Ioctl);
    Check((match != nullptr) && match->UserCode && (match->Names[0] == L"IOCTL_MYDRV_RESET") && (match->Text == L"Resets it."),
          "a CTL_CODE in your own header is an IOCTL");

    LoadUserCodeText({});
    ResetSourceShows();
}


/*!

    @brief Checks the other ways files write codes: macro calls, enum members
           and constants, Microsoft's rxpooltg.h, tag lists, and Treat as.

*/
static
void
TestCodeFormats (
    void
    )
{
    //
    // The macro's defined after its first use, as can happen when it lives
    // in a shared header read later.
    //
    static constexpr WCHAR k_Tags[] =
        L"POOLTAG_DEF(TEST_TAG_, 'gatT') // optional comment describing the pooltag\n"
        L"#define POOLTAG_DEF(Name, Value) Name = Value\n"
        L"enum MY_TAGS\n"
        L"{\n"
        L"    POOLTAG_DEF(ENUM_CALL_TAG, 'llaC'),\n"
        L"    PLAIN_ENUM_TAG = 'nmuE', // a plain member\n"
        L"    NOT_A_TAG = 5,\n"
        L"};\n"
        L"constexpr ULONG k_ConstTag = 'tsnC';\n"
        L"inline void Example() { ULONG status = 'xxxx'; }\n";

    static constexpr WCHAR k_Shared[] =
        L"#define SHARED_DEF(Name, Value) Name = Value,\n";

    static constexpr WCHAR k_UsesShared[] =
        L"enum { SHARED_DEF(SHARED_TAG, 'srhS') };\n";

    static constexpr WCHAR k_CallsOnly[] =
        L"POOLTAG_DEF(TEST_TAG_, 'gatT') // comment\n"
        L"extern \"C\" {\n"
        L"POOLTAG_DEF(EXTERN_TAG, 'ntxE')\n"
        L"}\n"
        L"LONG_DEF(LONG_TAG, 'gnoL', \"a description\")\n"
        L"DEFINE_GUID(MY_GUID, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB);\n"
        L"inline void Clear() { RtlZeroMemory(Buffer, 'xxxx'); }\n";

    static constexpr WCHAR k_Rx[] =
        L"#define RX_MISC_POOLTAG         ('sMxR')\n"
        L"extern ULONG RxExplodePoolTags;\n"
        L"#define RX_DEFINE_POOLTAG(ExplodedPoolTag,DefaultPoolTag)  \\\n"
        L"        ((RxExplodePoolTags == 0) ? (DefaultPoolTag) : (ExplodedPoolTag))\n"
        L"#define RX_TIMER_POOLTAG          RX_DEFINE_POOLTAG('mTxR',RX_MISC_POOLTAG)\n"
        L"#define PICKED_TAG (1 == 1 ? 'aaaA' : 'bbbB')\n";

    static constexpr WCHAR k_List[] =
        L"rem My driver's tags\n"
        L"\n"
        L"LstT - mydrv.sys - request context\n"
        L"Ls2  - <unknown>  - a short one\n";

    static constexpr WCHAR k_Plain[] =
        L"#define PLAIN_TAG 0x67617450\n"
        L"#define PLAIN_SIZE 256\n";

    std::vector<HeaderRead> reads;
    std::vector<CodeMatch> matches;
    const CodeMatch* match;

    printf("Code file formats\n");

    auto FindCode = [&reads](size_t File, PCWSTR Name) -> const HeaderCode*
    {
        for (const HeaderCode& candidate : reads[File].Codes)
        {
            if (candidate.Name == Name)
            {
                return &candidate;
            }
        }

        return nullptr;
    };

    reads = ReadHeaders({ { L"tags.h", k_Tags, UserTreatAs::Automatic } });
    Check((FindCode(0, L"TEST_TAG_") != nullptr) && (FindCode(0, L"TEST_TAG_")->Tag == L"Ttag") &&
          (FindCode(0, L"TEST_TAG_")->Text == L"Optional comment describing the pooltag."),
          "a macro call that comes down to Name = 'tag' is a pool tag, with its comment, before the macro's defined");
    Check((FindCode(0, L"ENUM_CALL_TAG") != nullptr) && (FindCode(0, L"ENUM_CALL_TAG")->Tag == L"Call"),
          "... and inside an enum");
    Check((FindCode(0, L"PLAIN_ENUM_TAG") != nullptr) && (FindCode(0, L"PLAIN_ENUM_TAG")->Text == L"A plain member."),
          "a plain enum member is read");
    Check((FindCode(0, L"k_ConstTag") != nullptr) && (FindCode(0, L"k_ConstTag")->Tag == L"Cnst"), "so is a constexpr constant");
    Check((FindCode(0, L"NOT_A_TAG") == nullptr) && (FindCode(0, L"status") == nullptr),
          "but not a plain number, or an assignment in an inline function");
    Check((reads[0].Format != nullptr) && (wcscmp(reads[0].Format, L"C/C++ header") == 0), "it's read as a C/C++ header");

    reads = ReadHeaders({ { L"uses.h", k_UsesShared, UserTreatAs::Automatic }, { L"shared.h", k_Shared, UserTreatAs::Automatic } });
    Check((FindCode(0, L"SHARED_TAG") != nullptr) && (FindCode(0, L"SHARED_TAG")->Tag == L"Shrs"),
          "a macro from another header works, even with its own comma");

    reads = ReadHeaders({ { L"tags.h", k_CallsOnly, UserTreatAs::Automatic } });
    Check((FindCode(0, L"TEST_TAG_") != nullptr) && (FindCode(0, L"TEST_TAG_")->Tag == L"Ttag") &&
          (FindCode(0, L"TEST_TAG_")->Text == L"Comment."),
          "a call to a macro from a header we don't have is read as Name = Value");
    Check((FindCode(0, L"EXTERN_TAG") != nullptr) && (FindCode(0, L"EXTERN_TAG")->Tag == L"Extn"), "... in an extern \"C\" block too");
    Check((FindCode(0, L"Buffer") == nullptr) && (FindCode(0, L"Size") == nullptr), "but not a call inside a function");
    Check((FindCode(0, L"LONG_TAG") == nullptr) && (reads[0].Skipped.size() == 1) && (reads[0].Skipped[0].Name == L"LONG_TAG") &&
          (reads[0].Skipped[0].Reason == L"LONG_DEF isn't defined in any header WinCode reads"),
          "and a longer call with a tag in it is skipped, saying why");

    reads = ReadHeaders({ { L"rxpooltg.h", k_Rx, UserTreatAs::Automatic } });
    Check((FindCode(0, L"RX_TIMER_POOLTAG") != nullptr) && (FindCode(0, L"RX_TIMER_POOLTAG")->Tag == L"RxTm"),
          "rxpooltg.h's RX_DEFINE_POOLTAG is its own tag, though it picks at run time");
    Check((FindCode(0, L"PICKED_TAG") != nullptr) && (FindCode(0, L"PICKED_TAG")->Tag == L"Aaaa"), "a ?: that can be worked out is");
    Check(reads[0].Skipped.empty(), "... and nothing's skipped");

    reads = ReadHeaders({ { L"tags.txt", k_List, UserTreatAs::Automatic } });
    Check((reads[0].Format != nullptr) && (wcscmp(reads[0].Format, L"pool tag list") == 0), "pooltag.txt's format is a tag list");
    Check((reads[0].Codes.size() == 2) && (reads[0].Codes[0].Value == 0x5474734C) && (reads[0].Codes[0].Text == L"mydrv.sys: request context") &&
          (reads[0].Codes[1].Value == 0x2032734C) && (reads[0].Codes[1].Text == L"a short one"),
          "... with each tag as a debugger shows it, and its binary and description");

    reads = ReadHeaders({ { L"plain.h", k_Plain, UserTreatAs::PoolTag } });
    Check((FindCode(0, L"PLAIN_TAG") != nullptr) && (FindCode(0, L"PLAIN_TAG")->Tag == L"Ptag") &&
          (FindCode(0, L"PLAIN_SIZE") == nullptr) && reads[0].Skipped.empty(),
          "Treat as pool tags makes plain numbers tags, and quietly leaves out ones that can't be");

    reads = ReadHeaders({ { L"plain.h", L"#define MY_IOCTL 0x222004\n", UserTreatAs::Ioctl } });
    Check((FindCode(0, L"MY_IOCTL") != nullptr) && (FindCode(0, L"MY_IOCTL")->Kind == CodeKind::Ioctl), "Treat as IOCTLs makes them IOCTLs");

    Check(IsUserCodeFileName(L"tags.TXT") && IsUserCodeFileName(L"mydrv.h") && !IsUserCodeFileName(L"notes.doc"),
          "the codes folder reads every format's files");

    //
    // FMPP is fltmgr's FM?? to pooltag.txt, which your own name for it beats.
    //
    LoadUserCodeText({ { L"C:\\src\\mydrv.h", L"#define MY_FLT_TAG 'PPMF'\n", UserTreatAs::Automatic } });
    matches = LookupValue(0x50504D46);
    match = FindMatch(matches, CodeKind::PoolTag);
    Check((match != nullptr) && match->UserCode, "your exact tag beats a pooltag.txt pattern");

    LoadUserCodeText({});
    matches = LookupValue(0x50504D46);
    match = FindMatch(matches, CodeKind::PoolTag);
    Check((match != nullptr) && (match->Names.size() == 2) && (match->Names[1] == L"FM??"), "... which is back without it");
}


/*!

    @brief Checks a header of POOLTAG_DEF calls against what the compiler's
           preprocessor makes of it.

    @details The build runs Tests/Data/PoolTagDef.h through cl /EP, so a
             header that doesn't preprocess fails the build. Each Name = 'tag'
             it comes out with has to be one of ours, with the same value.

*/
static
void
TestPoolTagHeader (
    void
    )
{
    std::wstring header;
    std::wstring expanded;
    std::vector<HeaderRead> reads;
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    const CodeMatch* match;
    size_t compared;
    size_t agreed;

    printf("A header of pool tag macros\n");

    if (!ReadHeaderFile(WINCODE_TEST_DATA L"/PoolTagDef.h", &header) ||
        !ReadHeaderFile(WINCODE_TEST_PREPROCESSED, &expanded))
    {
        Check(false, "the test header and its preprocessed copy can be read");
        return;
    }

    reads = ReadHeaders({ { L"PoolTagDef.h", header, UserTreatAs::Automatic } });

    auto FindCode = [&reads](const std::wstring& Name) -> const HeaderCode*
    {
        for (const HeaderCode& candidate : reads[0].Codes)
        {
            if (candidate.Name == Name)
            {
                return &candidate;
            }
        }

        return nullptr;
    };

    compared = 0;
    agreed = 0;

    for (size_t start = 0; start < expanded.size(); )
    {
        size_t end = expanded.find(L'\n', start);
        std::wstring line;
        size_t equals;
        size_t open;
        uint32_t value;
        const HeaderCode* code;

        if (end == std::wstring::npos)
        {
            end = expanded.size();
        }

        line = expanded.substr(start, end - start);
        start = end + 1;

        equals = line.find(L" = '");
        if (equals == std::wstring::npos)
        {
            continue;
        }

        //
        // A multi-character constant puts its first character in the top
        // byte, which is how cl works out each tag's value.
        //
        open = equals + 4;
        if ((open + 5 > line.size()) || (line[open + 4] != L'\''))
        {
            continue;
        }

        value = 0;
        for (size_t i = 0; i < 4; i++)
        {
            value = (value << 8) | SCAST(uint8_t)(line[open + i]);
        }

        compared++;
        code = FindCode(line.substr(line.find_first_not_of(L" \t"), equals - line.find_first_not_of(L" \t")));
        if ((code != nullptr) && (code->Kind == CodeKind::PoolTag) && (code->Value == value))
        {
            agreed++;
        }
    }

    Check(compared == 10, "the preprocessor expands all ten calls");
    Check((agreed == compared) && (reads[0].Codes.size() == compared) && reads[0].Skipped.empty(),
          "... and we read each one as the same pool tag, with nothing skipped");
    Check((FindCode(L"TEST_TAG1") != nullptr) && (FindCode(L"TEST_TAG1")->Tag == L"Tta1") &&
          (FindCode(L"TEST_TAG1")->Text == L"Optional comment describing the pooltag."),
          "a tag is named as a debugger shows it, with its comment");
    Check((FindCode(L"TEST_TAG2") != nullptr) && (FindCode(L"TEST_TAG2")->Text == L"Comment 2.") &&
          (FindCode(L"TEST_TAG7") != nullptr) && (FindCode(L"TEST_TAG7")->Text == L"A block comment.") &&
          (FindCode(L"TEST_TAG3") != nullptr) && FindCode(L"TEST_TAG3")->Text.empty(),
          "... however far away the comment is, a block comment too, and no comment's fine");
    Check((FindCode(L"TEST_TAG8") != nullptr) && (FindCode(L"TEST_TAG8")->Tag == L"Tt8"),
          "a tag can end in a space, which is dropped from its name as Irp's is");

    LoadUserCodeText({ { L"C:\\src\\PoolTagDef.h", header, UserTreatAs::Automatic } });
    matches = LookupValue(0x31617454);
    match = FindMatch(matches, CodeKind::PoolTag);
    Check((match != nullptr) && match->UserCode && (match->Names.size() == 2) && (match->Names[1] == L"TEST_TAG1"),
          "it's found as one of your codes by value");

    names = SearchNames(L"Tta1");
    Check(!names.empty() && (names[0].Kind == CodeKind::PoolTag) && (names[0].Value == 0x31617454), "... by the tag");

    names = SearchNames(L"TEST_TAG2");
    Check(!names.empty() && (names[0].Kind == CodeKind::PoolTag) && (names[0].Value == 0x32617454), "... and by its name");

    LoadUserCodeText({});
}


/*!

    @brief Checks comparing versions, which the update check relies on.

*/
static
void
TestVersions (
    void
    )
{
    printf("Versions\n");

    Check(CompareVersions(L"2.1.0", L"2.0.0") > 0, "2.1.0 is newer than 2.0.0");
    Check(CompareVersions(L"v2.0.1", L"2.0.0") > 0, "a leading v is ignored");
    Check(CompareVersions(L"2.1", L"2.1.0") == 0, "a missing part is zero");
    Check(CompareVersions(L"2.0.10", L"2.0.9") > 0, "parts compare as numbers, not text");
    Check(CompareVersions(L"1.9.9", L"2.0.0") < 0, "1.9.9 is older than 2.0.0");
    Check(CompareVersions(L"2.1.0-beta", L"2.1.0") == 0, "anything after the numbers is ignored");
}


/*!

    @brief Checks decoding values into flag families.

*/
static
void
TestFlags (
    void
    )
{
    std::wstring family;
    CodeMatch match;
    uint32_t value;

    printf("Flags\n");

    Check(DecodeFlags(L"file", 0x00120089, &match) && (match.Kind == CodeKind::Flags) &&
              (match.Names[0] == L"File access rights"),
          "0x00120089 decodes as file access rights");
    Check((match.Text.find(L"FILE_READ_DATA") != std::wstring::npos) &&
              (match.Text.find(L"SYNCHRONIZE") != std::wstring::npos),
          "... naming each bit");
    Check(match.Text.find(L"Includes FILE_GENERIC_READ") != std::wstring::npos, "... and the composite it holds in full");
    Check(match.Text.find(L"isn't named") == std::wstring::npos, "... with nothing left over");

    Check(DecodeFlags(L"page", 0x104, &match) && (match.Text.find(L"PAGE_READWRITE | PAGE_GUARD") == 0),
          "0x104 is PAGE_READWRITE | PAGE_GUARD");

    Check(DecodeFlags(L"disposition", 3, &match) && (match.Text == L"OPEN_EXISTING"), "disposition 3 is OPEN_EXISTING");

    Check(DecodeFlags(L"style", 0x14CF0000, &match) &&
              (match.Text.find(L"WS_VISIBLE") != std::wstring::npos) &&
              (match.Text.find(L"Includes WS_OVERLAPPEDWINDOW") != std::wstring::npos) &&
              (match.Text.find(L"WS_CAPTION,") == std::wstring::npos),
          "0x14CF0000 is a visible overlapped window, without the composites inside it");

    Check(DecodeFlags(L"key", 0x40, &match) && (match.Text.find(L"isn't named") != std::wstring::npos),
          "a bit no flag names is shown, not dropped");

    Check(DecodeFlags(L"access", 0x00100001, &match) &&
              (match.Text.find(L"specific to the type of object") != std::wstring::npos),
          "the generic family says what the low 16 bits are");

    Check(!DecodeFlags(L"nosuch", 1, &match), "a family that doesn't exist isn't decoded");

    Check(ParseFlagQuery(L"file 0x120089", &family, &value) && (family == L"file") && (value == 0x120089),
          "\"file 0x120089\" asks for flags");
    Check(ParseFlagQuery(L"page:0x40", &family, &value) && (family == L"page") && (value == 0x40), "... as does \"page:0x40\"");
    Check(!ParseFlagQuery(L"access denied 5", &family, &value), "log text doesn't");
    Check(!ParseFlagQuery(L"0xC0000022", &family, &value), "... nor does a plain value");
}


/*!

    @brief Checks kernel pool tags, by the DWORD a debugger shows and by the
           tag itself.

*/
static
void
TestPoolTags (
    void
    )
{
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    const CodeMatch* match;
    ParsedQuery parsed;

    printf("Pool tags\n");

    matches = LookupValue(0x20707249);
    match = FindMatch(matches, CodeKind::PoolTag);
    Check((match != nullptr) && (match->Names[0] == L"Irp"), "0x20707249 is the pool tag 'Irp '");
    Check((match != nullptr) && !match->Text.empty() && (match->Source == TextSource::Reference),
          "... described from pooltag.txt, with no note that Windows lacks text");

    names = SearchNames(L"MmSt");
    Check(!names.empty() && (names[0].Kind == CodeKind::PoolTag) && (names[0].Name == L"MmSt"),
          "the tag MmSt is found by name");

    matches = LookupValue(0x35306847);
    match = FindMatch(matches, CodeKind::PoolTag);
    Check((match != nullptr) && (match->Names.size() == 2) && (match->Names[0] == L"Gh05") && (match->Names[1] == L"Gh?5"),
          "Gh05 is pooltag.txt's Gh?5, the most specific pattern that fits");

    names = SearchNames(L"gh05");
    Check(!names.empty() && (names[0].Kind == CodeKind::PoolTag) && (names[0].Name == L"Gh05") &&
          (names[0].Value == 0x35306847) && (names[0].Quality == NameQuality::Exact),
          "... and typing gh05 finds it, as Gh05");

    names = SearchNames(L"Ntfx");
    Check(!names.empty() && (names[0].Name == L"Ntfx") && (names[0].Value == 0x7866744E),
          "tags are case sensitive, so Ntfx is ntfs.sys's Nt?? rather than the tag NtFX");

    names = SearchNames(L"Lr!!");
    Check(!names.empty() && (names[0].Name == L"Lr!!"), "a tag with punctuation is found by name");

    parsed = ParseQuery(L"Lr!!");
    Check((parsed.Names.size() == 1) && (parsed.Names[0] == L"Lr!!"), "... and keeps its punctuation as a query");

    parsed = ParseQuery(L"Pool tag: Ntfx");
    Check(parsed.Numbers.empty() && (parsed.Names.size() == 1) && (parsed.Names[0] == L"Ntfx"),
          "a tag after a label is a name in longer text");

    parsed = ParseQuery(L"ffffa50f`6a2d1000 size: 60 previous size: 0  (Allocated) *MmSt");
    Check((parsed.Names.size() == 1) && (parsed.Names[0] == L"MmSt"), "... as is the tag in a line from !pool");

    parsed = ParseQuery(L"Pooltag Irp+ : I/O verifier allocated IRP packets");
    Check((parsed.Names.size() == 1) && (parsed.Names[0] == L"Irp+"), "... and in !pool's Pooltag line");

    parsed = ParseQuery(L"tag 'Irp '");
    Check((parsed.Names.size() == 1) && (parsed.Names[0] == L"Irp"), "... and in quotes, with its space");

    parsed = ParseQuery(L"AfdA");
    Check((parsed.Names.size() == 1) && (parsed.Names[0] == L"AfdA") && !parsed.Numbers.empty(),
          "a tag that reads as hex, like AfdA, is a name as well as a number");

    parsed = ParseQuery(L"8042");
    Check((parsed.Names.size() == 1) && (parsed.Names[0] == L"8042"), "... as is one of digits, like 8042");

    parsed = ParseQuery(L"dead");
    Check(parsed.Names.empty(), "... but a hex word that isn't a tag isn't");

    parsed = ParseQuery(L"tag 5");
    Check(parsed.Names.empty() && !parsed.Numbers.empty(), "\"tag 5\" is still a number");

    parsed = ParseQuery(L"File not found (0x80070002)");
    Check(parsed.Names.empty(), "a word that's also a tag, like File, isn't one unless it's labelled");

    matches = LookupValue(43);
    match = FindMatch(matches, CodeKind::DeviceProblem);
    Check((match != nullptr) && (match->Source == TextSource::Reference), "a device problem's text is from cfg.h, not standing in");
}


/*!

    @brief Checks device problem codes and the quiet tables, USB statuses and
           LDAP results, which only show when nothing else names the value.

*/
static
void
TestDeviceUsbLdap (
    void
    )
{
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    std::vector<ParsedNumber> numbers;
    const CodeMatch* match;

    printf("Device problems, USB and LDAP\n");

    matches = LookupValue(43);
    match = FindMatch(matches, CodeKind::DeviceProblem);
    Check((match != nullptr) && (match->Names[0] == L"CM_PROB_FAILED_POST_START"), "43 is Device Manager's Code 43");
    Check((match != nullptr) && !match->Text.empty(), "... with the text from cfg.h");

    numbers = ParseQuery(L"Code 43").Numbers;
    Check((numbers.size() == 1) && (numbers[0].Value == 43), "\"Code 43\" reads as 43");

    Check(FormatCodeValue(CodeKind::DeviceProblem, 43) == L"43", "a device problem in decimal, as Device Manager shows it");

    matches = LookupValue(0xC0000005);
    Check(FindMatch(matches, CodeKind::UsbStatus) == nullptr, "a USB status that shares an NTSTATUS value stays out of the way");

    matches = LookupValue(0x80000200);
    match = FindMatch(matches, CodeKind::UsbStatus);
    Check((match != nullptr) && (match->Names[0] == L"USBD_STATUS_INVALID_URB_FUNCTION"),
          "a USB status nothing else names is shown");

    names = SearchNames(L"USBD_STATUS_CRC");
    Check(!names.empty() && (names[0].Kind == CodeKind::UsbStatus), "a quiet USB status is still found by name");

    names = SearchNames(L"LDAP_INVALID_CREDENTIALS");
    Check(!names.empty() && (names[0].Kind == CodeKind::LdapError) && (names[0].Value == 49),
          "LDAP_INVALID_CREDENTIALS is 49, found by name");

    matches = LookupValue(49);
    Check(FindMatch(matches, CodeKind::LdapError) == nullptr, "... but 49 doesn't gain an LDAP row");
}


/*!

    @brief Finds the HRESULT match with a given name.

    @param[in] Matches - The matches.

    @param[in] Name - The name.

    @return The match, or nullptr.

*/
static
const CodeMatch*
FindHResult (
    _In_ const std::vector<CodeMatch>& Matches,
    _In_z_ PCWSTR Name
    )
{
    for (const CodeMatch& match : Matches)
    {
        if ((match.Kind == CodeKind::HResult) && !match.Names.empty() && (match.Names[0] == Name))
        {
            return &match;
        }
    }

    return nullptr;
}


/*!

    @brief Checks the HRESULT tables from headers beyond winerror.h, each of
           which says where it came from.

*/
static
void
TestMoreTables (
    void
    )
{
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    const CodeMatch* match;

    printf("More tables\n");

    matches = LookupValue(0x80240001);
    match = FindHResult(matches, L"WU_E_NO_SERVICE");
    Check((match != nullptr) && (match->Header == L"wuerror.h"), "0x80240001 is WU_E_NO_SERVICE, from wuerror.h");
    Check((match != nullptr) && !match->Text.empty(), "... with the text from the header's comments");

    matches = LookupValue(0x80042301);
    match = FindHResult(matches, L"VSS_E_BAD_STATE");
    Check((match != nullptr) && (match->Header == L"vsserror.h"), "0x80042301 is VSS_E_BAD_STATE, from vsserror.h");

    names = SearchNames(L"AUDCLNT_E_NOT_INITIALIZED");
    Check(!names.empty() && (names[0].Kind == CodeKind::HResult) && (names[0].Value == 0x88890001),
          "AUDCLNT_E_NOT_INITIALIZED is 0x88890001, through AUDCLNT_ERR");

    matches = LookupValue(0x80131013);
    match = FindHResult(matches, L"COR_E_TYPEUNLOADED");
    Check((match != nullptr) && (match->Header == L"corerror.h"), "0x80131013 is COR_E_TYPEUNLOADED, from corerror.h");

    matches = LookupValue(0x8002000E);
    Check((FindHResult(matches, L"DISP_E_BADPARAMCOUNT") != nullptr) && (FindHResult(matches, L"COR_E_TARGETPARAMCOUNT") != nullptr),
          "a value two headers define is both, each under its own name");
}


/*!

    @brief Checks the known values, fill patterns at every width and the
           exception codes no header names.

*/
static
void
TestKnownValues (
    void
    )
{
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;

    printf("Known values\n");

    matches = LookupValue(0xCCCCCCCC);
    Check(!matches.empty() && (matches[0].Kind == CodeKind::KnownValue) &&
              (matches[0].Names[0].find(L"Uninitialised stack") == 0),
          "0xCCCCCCCC is uninitialised stack, listed first");
    Check(!matches.empty() && !matches[0].Text.empty() && (matches[0].Source == TextSource::Curated),
          "... with WinCode's own text");

    matches = LookupValue(0xCCCC);
    Check(FindMatch(matches, CodeKind::KnownValue) != nullptr, "the 16-bit 0xCCCC is too");

    CheckParsesTo(L"0xCCCCCCCCCCCCCCCC", 0xCCCCCCCC, NumberForm::Repeated, "a 64-bit fill pattern is the 32-bit one it repeats");
    Check(ParseQuery(L"0x0000000100000001").Numbers.empty(), "a repeat without the top bit isn't a fill pattern");

    matches = LookupValue(0xE06D7363);
    Check(!matches.empty() && (matches[0].Kind == CodeKind::KnownValue) && (matches[0].Names[0] == L"EH_EXCEPTION_NUMBER"),
          "0xE06D7363 is an MSVC C++ exception");
    Check(FindMatch(matches, CodeKind::NtStatus) == nullptr, "... with no guess at it as an unnamed NTSTATUS beside it");

    matches = LookupValue(0xC06D007E);
    Check(FindMatch(matches, CodeKind::KnownValue) != nullptr, "0xC06D007E is a delay-load failure");

    names = SearchNames(L"EXCEPTION_COMPLUS");
    Check(!names.empty() && (names[0].Kind == CodeKind::KnownValue) && (names[0].Value == 0xE0434352),
          "EXCEPTION_COMPLUS is found by name");

    Check(FormatCodeValue(CodeKind::KnownValue, 0xCCCC) == L"0xCCCC", "a 16-bit known value in four hex digits");
    Check(FormatCodeValue(CodeKind::KnownValue, 0xCCCCCCCC) == L"0xCCCCCCCC", "a 32-bit one in eight");
}


int
wmain (
    void
    )
{
    TestVersion();
    TestTableShapes();
    TestWinErrorAndHResult();
    TestNtStatusAndBugCheck();
    TestWindowMessages();
    TestParseQuery();
    TestParseNames();
    TestLookupValue();
    TestSearchNames();
    TestCrossReferences();
    TestMessageRanges();
    TestReverseMapping();
    TestDescribe();
    TestKnownValues();
    TestMoreTables();
    TestDeviceUsbLdap();
    TestPoolTags();
    TestFlags();
    TestVersions();
    TestOddInput();
    TestSourceShows();
    TestUserCodes();
    TestIoctls();
    TestCodeFormats();
    TestPoolTagHeader();

    printf("\n%s (%d failures)\n", (g_Failures == 0) ? "PASS" : "FAILURES", g_Failures);
    return g_Failures;
}
