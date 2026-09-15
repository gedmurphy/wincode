/*!

    @file Core/Search.cpp

    @brief Searches the tables by name.

    @details A search is usually for part of a name, remembered or seen in a
             log, so the text is looked for anywhere in a name rather than only
             at the start. Matches are graded so that the likely one comes
             first: the whole name, then whole underscore separated words
             (ACCESS_DENIED in ERROR_ACCESS_DENIED), then anything else.

             The tables are small enough to scan in full on every search, so no
             index is kept.

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


/*!

    @brief Upper cases an ASCII character.

    @param[in] Char - The character.

    @return The character, upper cased if it is a lower case letter.

*/
static
char
AsciiUpper (
    _In_ char Char
    )
{
    if ((Char >= 'a') && (Char <= 'z'))
    {
        return SCAST(char)(Char - 'a' + 'A');
    }

    return Char;
}


/*!

    @brief Whether a name contains the query at a given position, ignoring case.

    @param[in] Name - The name.

    @param[in] Start - Where in the name to compare.

    @param[in] Query - The query, already upper cased.

    @return True if the characters match.

*/
static
bool
MatchesAt (
    _In_z_ PCSTR Name,
    _In_ size_t Start,
    _In_ const std::string& Query
    )
{
    for (size_t i = 0; i < Query.size(); i++)
    {
        if (AsciiUpper(Name[Start + i]) != Query[i])
        {
            return false;
        }
    }

    return true;
}


/*!

    @brief Grades how well a name matches a query.

    @param[in] Name - The name.

    @param[in] Query - The query, already upper cased.

    @param[out] Quality - Receives the grade.

    @return True if the name contains the query at all.

*/
static
bool
GradeName (
    _In_z_ PCSTR Name,
    _In_ const std::string& Query,
    _Out_ NameQuality* Quality
    )
{
    size_t nameLength;
    size_t end;
    bool found;
    bool startsWord;
    bool endsWord;

    *Quality = NameQuality::Partial;

    nameLength = strlen(Name);
    found = false;

    for (size_t start = 0; (start + Query.size()) <= nameLength; start++)
    {
        if (!MatchesAt(Name, start, Query))
        {
            continue;
        }

        found = true;
        end = start + Query.size();

        if ((start == 0) && (end == nameLength))
        {
            *Quality = NameQuality::Exact;
            return true;
        }

        startsWord = (start == 0) || (Name[start - 1] == '_');
        endsWord = (end == nameLength) || (Name[end] == '_');

        if (startsWord && endsWord)
        {
            *Quality = NameQuality::Words;
            return true;
        }
    }

    return found;
}


std::vector<NameMatch>
SearchNames (
    _In_z_ PCWSTR Text
    )
{
    std::vector<NameMatch> results;
    std::string query;
    const TableEntry* entry;
    NameQuality quality;

    //
    // Every name in the tables is ASCII, so a query that isn't can't match.
    //
    for (PCWSTR c = Text; *c != L'\0'; c++)
    {
        if (*c > 0x7F)
        {
            return results;
        }

        query += AsciiUpper(SCAST(char)(*c));
    }

    if (query.empty())
    {
        return results;
    }

    for (const KindTable& kindTable : g_KindTables)
    {
        for (size_t i = 0; i < kindTable.Table->Count; i++)
        {
            entry = &kindTable.Table->Entries[i];

            if (!GradeName(entry->Name, query, &quality))
            {
                continue;
            }

            results.push_back({ kindTable.Kind,
                                entry->Value,
                                AsciiToWide(entry->Name),
                                quality,
                                (entry->Flags & k_TableEntryUndocumented) != 0 });
        }
    }

    std::stable_sort(results.begin(),
                     results.end(),
                     [](const NameMatch& A, const NameMatch& B)
                     {
                         if (A.Quality != B.Quality)
                         {
                             return A.Quality < B.Quality;
                         }

                         if (A.Name.size() != B.Name.size())
                         {
                             return A.Name.size() < B.Name.size();
                         }

                         return A.Kind < B.Kind;
                     });

    return results;
}
