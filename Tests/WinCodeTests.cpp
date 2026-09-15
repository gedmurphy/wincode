/*!

    @file Tests/WinCodeTests.cpp

    @brief Tests for the WinCode core.

    @details A plain executable rather than a test framework, so there is
             nothing to install. Each check prints its result, and the exit
             code is the number of failures, which is what ctest goes by.

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

    @details A linear search, which is fine for tests. Lookups proper go by
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

    matches = LookupValue(0xE06D7363);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) &&
          match->Names.empty() &&
          match->Fields.has_value() &&
          match->Fields->Customer &&
          (match->Fields->Facility == 0x6D) &&
          (match->Fields->Code == 0x7363),
          "0xE06D7363, the MSVC C++ exception, decodes as a customer code");
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
    Check((match != nullptr) && (match->Header != nullptr) && (std::wstring(match->Header) == L"winerror.h"),
          "ERROR_ACCESS_DENIED comes from winerror.h");

    match = FindMatch(matches, CodeKind::WindowMessage);
    Check((match != nullptr) && (match->Header != nullptr) && (std::wstring(match->Header) == L"winuser.h"),
          "WM_SIZE comes from winuser.h");

    matches = LookupValue(0xE06D7363);
    match = FindMatch(matches, CodeKind::NtStatus);
    Check((match != nullptr) && (match->Header == nullptr), "a value in no table has no header");
}


/*!

    @brief Runs every test.

    @return The number of failed checks, so zero means everything passed.

*/
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

    printf("\n%s (%d failures)\n", (g_Failures == 0) ? "PASS" : "FAILURES", g_Failures);
    return g_Failures;
}
