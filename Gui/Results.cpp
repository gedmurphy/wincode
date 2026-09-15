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

#include <algorithm>
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


/*!

    @brief Looks a query up.

    @param[in] Query - The query, typed or pasted.

    @return What it found.

*/
ResultSet
BuildResults (
    _In_z_ PCWSTR Query
    )
{
    ResultSet results = {};
    ParsedQuery parsed;
    CodeMatch decode;
    std::wstring family;
    uint32_t flagValue;
    uint32_t ioctlValue;
    std::vector<CodeMatch> matches;
    std::vector<NameMatch> names;
    size_t start;

    //
    // "ioctl 0x2D1400" decodes the value as an IOCTL whether or not anything
    // names it.
    //
    if (ParseIoctlQuery(Query, &ioctlValue))
    {
        DecodeIoctl(ioctlValue, &decode);
        results.HasQuery = true;
        results.Rows.push_back({ CodeKind::Ioctl, ioctlValue, decode.Names[0], 0, 0 });
        results.NumberRows = 1;
        return results;
    }

    //
    // A family and a value, as in "file 0x120089", is a request to decode
    // flags, which gives one result rather than a lookup.
    //
    if (ParseFlagQuery(Query, &family, &flagValue) && DecodeFlags(family.c_str(), flagValue, &decode))
    {
        results.HasQuery = true;
        results.Rows.push_back({ CodeKind::Flags, flagValue, decode.Names[0], 0, 0 });
        results.NumberRows = 1;
        return results;
    }

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


/*!

    @brief How many results there are, in words, as in "3 matches" or
           "42 names".

    @param[in] Results - The results.

    @return The text, or an empty string when there was no query.

*/
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


/*!

    @brief Looks up everything known about one code.

    @param[in] Kind - The kind of code.

    @param[in] Value - Its value.

    @param[in] Name - Its name, as listed, which picks between two codes of
                      one kind with the same value. Empty for the first.

    @param[out] Match - Receives the match.

    @return True if the value is that kind of code.

*/
bool
FindMatch (
    _In_ CodeKind Kind,
    _In_ uint32_t Value,
    _In_ const std::wstring& Name,
    _Out_ CodeMatch* Match
    )
{
    std::vector<CodeMatch> matches;
    CodeMatch* first;

    //
    // A flags decode isn't a lookup, its name is the family it was decoded as.
    //
    if (Kind == CodeKind::Flags)
    {
        return DecodeFlags(Name.c_str(), Value, Match);
    }

    matches = LookupValue(Value);
    first = nullptr;

    //
    // A value can be two codes of one kind, as when the CLR reuses an OLE
    // Automation HRESULT, so the one with the row's own name is the one meant.
    //
    for (CodeMatch& match : matches)
    {
        if (match.Kind != Kind)
        {
            continue;
        }

        if (first == nullptr)
        {
            first = &match;
        }

        if (!Name.empty() && (std::find(match.Names.begin(), match.Names.end(), Name) != match.Names.end()))
        {
            *Match = std::move(match);
            return true;
        }
    }

    if (first != nullptr)
    {
        *Match = std::move(*first);
        return true;
    }

    //
    // An IOCTL nothing names only came from asking, so decode it again.
    //
    if (Kind == CodeKind::Ioctl)
    {
        DecodeIoctl(Value, Match);
        return true;
    }

    *Match = CodeMatch();
    return false;
}


/*!

    @brief The name a kind of code is saved under in settings.json, as in
           L"win32".

    @param[in] Kind - The kind.

    @return A static, never null, name.

*/
PCWSTR
KindKey (
    _In_ CodeKind Kind
    )
{
    switch (Kind)
    {
    case CodeKind::KnownValue:
        return L"known";

    case CodeKind::WinError:
        return L"win32";

    case CodeKind::HResult:
        return L"hresult";

    case CodeKind::NtStatus:
        return L"ntstatus";

    case CodeKind::BugCheck:
        return L"bugcheck";

    case CodeKind::DeviceProblem:
        return L"device";

    case CodeKind::UsbStatus:
        return L"usb";

    case CodeKind::LdapError:
        return L"ldap";

    case CodeKind::PoolTag:
        return L"pooltag";

    case CodeKind::Ioctl:
        return L"ioctl";

    case CodeKind::Flags:
        return L"flags";

    default:
        return L"message";
    }
}


/*!

    @brief The kind of code a name from settings.json means.

    @param[in] Key - The name, as KindKey gives it.

    @param[out] Kind - Receives the kind.

    @return True if the name is one KindKey gives.

*/
bool
KindFromKey (
    _In_ const std::wstring& Key,
    _Out_ CodeKind* Kind
    )
{
    static const CodeKind kinds[] =
    {
        CodeKind::KnownValue,
        CodeKind::WinError,
        CodeKind::HResult,
        CodeKind::NtStatus,
        CodeKind::BugCheck,
        CodeKind::WindowMessage,
        CodeKind::DeviceProblem,
        CodeKind::UsbStatus,
        CodeKind::LdapError,
        CodeKind::PoolTag,
        CodeKind::Ioctl,
        CodeKind::Flags
    };

    for (CodeKind kind : kinds)
    {
        if (Key == KindKey(kind))
        {
            *Kind = kind;
            return true;
        }
    }

    *Kind = CodeKind::WinError;
    return false;
}
