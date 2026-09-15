/*!

    @file Gui/Results.cpp

    @brief Turns a query into the rows the main window and the popup list.

    @details A row holds only enough to show it and find it again. The full
             match is looked up afresh when a row is selected, which keeps a
             name search with thousands of rows cheap.

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

#include "WinCodeUI.hpp"

#include <cwctype>
#include <format>


/*!

    @brief Finds text inside other text, ignoring case.

    @param[in] Text - The text to search.

    @param[in] Part - The text to find.

    @return Where Part starts in Text, or std::wstring::npos.

*/
static
size_t
FindIgnoreCase (
    _In_ const std::wstring& Text,
    _In_ const std::wstring& Part
    )
{
    size_t j;

    if (Part.empty() || (Part.size() > Text.size()))
    {
        return std::wstring::npos;
    }

    for (size_t i = 0; (i + Part.size()) <= Text.size(); i++)
    {
        for (j = 0; j < Part.size(); j++)
        {
            if (towupper(Text[i + j]) != towupper(Part[j]))
            {
                break;
            }
        }

        if (j == Part.size())
        {
            return i;
        }
    }

    return std::wstring::npos;
}


/*!

    @brief Adds a row unless the same code is already listed.

    @param[in,out] Rows - The rows.

    @param[in] CheckCount - How many rows, from the start, to check for a
                            duplicate. Name search results are unique among
                            themselves, so only the rows before them need
                            checking, which keeps a search with thousands of
                            names from turning quadratic.

    @param[in] Kind - The kind of code.

    @param[in] Value - Its value.

    @param[in] Name - Its name as listed.

    @param[in] MatchStart - Where a name search matched in Name.

    @param[in] MatchLength - How long the match is, or zero for none.

*/
static
void
AddRow (
    _Inout_ std::vector<ResultRow>& Rows,
    _In_ size_t CheckCount,
    _In_ CodeKind Kind,
    _In_ uint32_t Value,
    _In_ const std::wstring& Name,
    _In_ size_t MatchStart,
    _In_ size_t MatchLength
    )
{
    for (size_t i = 0; (i < CheckCount) && (i < Rows.size()); i++)
    {
        if ((Rows[i].Kind == Kind) && (Rows[i].Value == Value) && (Rows[i].Name == Name))
        {
            return;
        }
    }

    Rows.push_back({ Kind, Value, Name, MatchStart, MatchLength });
}


ResultSet
BuildResults (
    _In_z_ PCWSTR Query
    )
{
    ResultSet results = {};
    ParsedQuery parsed;
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    size_t start;

    parsed = ParseQuery(Query);
    results.HasQuery = !parsed.Numbers.empty() || !parsed.Names.empty();

    for (const ParsedNumber& number : parsed.Numbers)
    {
        matches = LookupValue(number.Value);

        for (const CodeMatch& match : matches)
        {
            AddRow(results.Rows,
                   results.Rows.size(),
                   match.Kind,
                   match.Value,
                   match.Names.empty() ? match.Description : match.Names[0],
                   0,
                   0);
        }
    }

    results.NumberRows = results.Rows.size();

    for (const std::wstring& name : parsed.Names)
    {
        names = SearchNames(name.c_str());

        for (const NameMatch& found : names)
        {
            start = FindIgnoreCase(found.Name, name);

            AddRow(results.Rows,
                   results.NumberRows,
                   found.Kind,
                   found.Value,
                   found.Name,
                   (start == std::wstring::npos) ? 0 : start,
                   (start == std::wstring::npos) ? 0 : name.size());
        }
    }

    return results;
}


std::wstring
ResultCountText (
    _In_ const ResultSet& Results
    )
{
    size_t nameRows;

    nameRows = Results.Rows.size() - Results.NumberRows;

    if (!Results.HasQuery)
    {
        return std::wstring();
    }

    if (Results.Rows.empty())
    {
        return L"No match";
    }

    if (nameRows == 0)
    {
        return std::format(L"{} {}", Results.NumberRows, (Results.NumberRows == 1) ? L"match" : L"matches");
    }

    if (Results.NumberRows == 0)
    {
        return std::format(L"{} {}", nameRows, (nameRows == 1) ? L"name" : L"names");
    }

    return std::format(L"{} results", Results.Rows.size());
}


bool
FindMatch (
    _In_ CodeKind Kind,
    _In_ uint32_t Value,
    _Out_ CodeMatch* Match
    )
{
    std::vector<CodeMatch> matches;

    matches = LookupValue(Value);

    for (CodeMatch& match : matches)
    {
        if (match.Kind == Kind)
        {
            *Match = std::move(match);
            return true;
        }
    }

    *Match = CodeMatch();
    return false;
}
