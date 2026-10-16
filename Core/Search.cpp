/*!

    @file Core/Search.cpp

    @brief Searches the tables by name.

    @details You're usually searching for part of a name you remember or saw in
             a log, so we look anywhere in the name, not just the start. The
             likely match comes first, so the whole name, then whole words
             (ACCESS_DENIED in ERROR_ACCESS_DENIED), then anything else.

             The tables are small enough to scan every time, so there's no
             index.

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
#include <set>
#include <tuple>


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


/*!

    @brief Finds the pooltag.txt pattern that covers a typed tag, and the tag
           it means.

    @details The tag takes the pattern's case where the pattern fixes it and
             yours where it doesn't, so gh05 is Gh05, the tag with a meaning.

    @param[in] Typed - The tag as typed, padded with spaces to four.

    @param[in] IgnoreCase - True to match whatever the case.

    @param[out] Found - Receives the tag, as an exact match.

    @return True if a pattern covers the tag.

*/
static
bool
FindPatternTag (
    _In_ const std::string& Typed,
    _In_ bool IgnoreCase,
    _Out_ NameMatch* Found
    )
{
    const TableEntry* pattern;
    std::string tag;
    std::wstring name;
    size_t length;
    uint32_t value;
    char c;

    *Found = NameMatch();

    for (const KindTable& kindTable : AllKindTables())
    {
        if ((kindTable.Kind != CodeKind::PoolTag) || (SourceShowFor(kindTable.Key) == SourceShow::Never))
        {
            continue;
        }

        pattern = FindPoolTagPattern(*kindTable.Table, Typed, IgnoreCase);
        if (pattern == nullptr)
        {
            continue;
        }

        length = strlen(pattern->Name);
        tag.clear();

        for (size_t i = 0; i < 4; i++)
        {
            c = (i < length) ? pattern->Name[i] : ' ';
            if (c == '*')
            {
                tag += Typed.substr(i);
                break;
            }

            tag += (c == '?') ? Typed[i] : c;
        }

        value = 0;
        for (size_t i = 0; i < 4; i++)
        {
            value |= SCAST(uint32_t)(SCAST(uint8_t)(tag[i])) << (8 * i);
        }

        name = AsciiToWide(tag.c_str());
        while (name.back() == L' ')
        {
            name.pop_back();
        }

        *Found = { CodeKind::PoolTag,
                   value,
                   name,
                   NameQuality::Exact,
                   (pattern->Flags & k_TableEntryUndocumented) != 0 };
        return true;
    }

    return false;
}


/*!

    @brief Finds a typed pool tag that pooltag.txt only has as a pattern, like
           Gh05 for Gh?5, since no name contains it.

    @details Tags are case sensitive, so Ntfx is ntfs.sys's Nt?? and not the
             tag NtFX, even though the search found NtFX ignoring case. A
             pattern only matches whatever the case when no tag did.

    @param[in] Text - What was searched for.

    @param[in,out] Results - The tag is added here as an exact match, and a tag
                             that only matched ignoring case drops behind it.

*/
static
void
AddPatternTag (
    _In_z_ PCWSTR Text,
    _Inout_ std::vector<NameMatch>& Results
    )
{
    NameMatch found;
    std::string typed;
    std::wstring wide;
    bool exactSame;
    bool exactOther;

    for (PCWSTR t = Text; *t != L'\0'; t++)
    {
        if ((*t < 0x20) || (*t > 0x7E) || (typed.size() == 4))
        {
            return;
        }

        typed += SCAST(char)(*t);
    }

    if (typed.empty() || (typed[0] == ' '))
    {
        return;
    }

    wide = AsciiToWide(typed.c_str());
    exactSame = false;
    exactOther = false;

    for (const NameMatch& result : Results)
    {
        if ((result.Kind != CodeKind::PoolTag) || (result.Quality != NameQuality::Exact))
        {
            continue;
        }

        if (result.Name == wide)
        {
            exactSame = true;
        }
        else
        {
            exactOther = true;
        }
    }

    if (exactSame)
    {
        return;
    }

    typed.resize(4, ' ');

    if (FindPatternTag(typed, false, &found))
    {
        for (NameMatch& result : Results)
        {
            if ((result.Kind == CodeKind::PoolTag) && (result.Quality == NameQuality::Exact))
            {
                result.Quality = NameQuality::Words;
            }
        }

        Results.push_back(found);
        return;
    }

    if (!exactOther && FindPatternTag(typed, true, &found))
    {
        Results.push_back(found);
    }
}


/*!

    @brief Searches every table's names.

    @param[in] Text - What to search for. Case is ignored.

    @return Every name containing the text, best matches first: by quality,
            then shortest name, then kind. Empty if there are none.

*/
std::vector<NameMatch>
SearchNames (
    _In_z_ PCWSTR Text
    )
{
    std::vector<NameMatch> results;
    std::set<std::tuple<CodeKind, uint32_t, std::wstring>> seen;
    std::string query;
    const TableEntry* entry;
    NameQuality quality;
    bool checking;

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

    checking = false;

    for (const KindTable& kindTable : AllKindTables())
    {
        //
        // A source the user has hidden is hidden from searches too.
        //
        if (SourceShowFor(kindTable.Key) == SourceShow::Never)
        {
            continue;
        }

        for (size_t i = 0; i < kindTable.Table->Count; i++)
        {
            entry = &kindTable.Table->Entries[i];

            if (!GradeName(entry->Name, query, &quality))
            {
                continue;
            }

            //
            // Your header can define a code Windows already has, or another
            // header does, and that's still one code.
            //
            if (kindTable.User)
            {
                if (!checking)
                {
                    for (const NameMatch& result : results)
                    {
                        seen.insert({ result.Kind, result.Value, result.Name });
                    }

                    checking = true;
                }

                if (!seen.insert({ kindTable.Kind, entry->Value, AsciiToWide(entry->Name) }).second)
                {
                    continue;
                }
            }

            results.push_back({ kindTable.Kind,
                                entry->Value,
                                AsciiToWide(entry->Name),
                                quality,
                                (entry->Flags & k_TableEntryUndocumented) != 0 });
        }
    }

    AddPatternTag(Text, results);

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


/*!

    @brief Whether text is exactly a pool tag, in any table being shown.

    @param[in] Text - The text. Case is ignored, as in a search.

    @return True if a search for it would find a tag by its whole name.

*/
bool
IsPoolTagName (
    _In_z_ PCWSTR Text
    )
{
    NameMatch found;
    std::string typed;

    for (PCWSTR t = Text; *t != L'\0'; t++)
    {
        if ((*t < 0x20) || (*t > 0x7E) || (typed.size() == 4))
        {
            return false;
        }

        typed += SCAST(char)(*t);
    }

    if (typed.empty())
    {
        return false;
    }

    for (const KindTable& kindTable : AllKindTables())
    {
        if ((kindTable.Kind != CodeKind::PoolTag) || (SourceShowFor(kindTable.Key) == SourceShow::Never))
        {
            continue;
        }

        for (size_t i = 0; i < kindTable.Table->Count; i++)
        {
            if (_stricmp(kindTable.Table->Entries[i].Name, typed.c_str()) == 0)
            {
                return true;
            }
        }
    }

    typed.resize(4, ' ');

    return FindPatternTag(typed, true, &found);
}
