/*!

    @file Core/Flags.cpp

    @brief Decodes a value into the named bits of a flag family, like access
           masks, page protections, file flags and window styles.

    @details The families come from the SDK (Flags.g.cpp). A family has
             single-bit flags and composites like FILE_GENERIC_READ that name
             several bits at once. A decode names every bit set by its
             single-bit flag, then the composites the value holds in full,
             keeping only the largest, so FILE_ALL_ACCESS isn't followed by
             everything in it.

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

#include "Tables.hpp"

#include <bit>
#include <format>


//
// The access rights in the low 16 bits mean something different for each type
// of object, so the generic family can only say so.
//
static constexpr uint32_t k_SpecificRights = 0x0000FFFF;


/*!

    @brief Finds a flag family.

    @param[in] Family - Its key or its name, ignoring case.

    @return The family, or nullptr.

*/
static
const FlagFamily*
FindFamily (
    _In_z_ PCWSTR Family
    )
{
    for (size_t i = 0; i < g_FlagFamilyCount; i++)
    {
        if ((_wcsicmp(g_FlagFamilies[i].Key, Family) == 0) || (_wcsicmp(g_FlagFamilies[i].Name, Family) == 0))
        {
            return &g_FlagFamilies[i];
        }
    }

    return nullptr;
}


/*!

    @brief The name for an entry, with any other names for the same value
           after it in brackets.

    @param[in] Family - The family.

    @param[in] Index - The entry, a primary one.

    @return For example "FILE_READ_DATA (FILE_LIST_DIRECTORY)".

*/
static
std::wstring
EntryName (
    _In_ const FlagFamily& Family,
    _In_ size_t Index
    )
{
    std::wstring name;
    std::wstring others;

    name = AsciiToWide(Family.Entries[Index].Name);

    for (size_t i = Index + 1; (i < Family.Count) && ((Family.Entries[i].Flags & k_TableEntryAlias) != 0); i++)
    {
        others += others.empty() ? L"" : L", ";
        others += AsciiToWide(Family.Entries[i].Name);
    }

    return others.empty() ? name : (name + L" (" + others + L")");
}


/*!

    @brief Joins names into one line.

    @param[in] Names - The names.

    @param[in] Separator - What goes between them.

    @return The line.

*/
static
std::wstring
Join (
    _In_ const std::vector<std::wstring>& Names,
    _In_z_ PCWSTR Separator
    )
{
    std::wstring text;

    for (const std::wstring& name : Names)
    {
        text += text.empty() ? L"" : Separator;
        text += name;
    }

    return text;
}


/*!

    @brief Every flag family, in the order they're listed.

    @return The families.

*/
std::vector<FlagFamilyName>
FlagFamilyNames (
    void
    )
{
    std::vector<FlagFamilyName> names;

    for (size_t i = 0; i < g_FlagFamilyCount; i++)
    {
        names.push_back({ g_FlagFamilies[i].Key, g_FlagFamilies[i].Name });
    }

    return names;
}


/*!

    @brief Reads a query asking for a value decoded as flags, which is a
           family's key and one number, as in "file 0x120089" or "page:0x40".

    @details Only a query that's exactly that counts, so text pasted from a
             log like "access denied 5" never gets taken for one.

    @param[in] Query - The query.

    @param[out] Family - Receives the family's key.

    @param[out] Value - Receives the value.

    @return True if the query asks for flags.

*/
bool
ParseFlagQuery (
    _In_z_ PCWSTR Query,
    _Out_ std::wstring* Family,
    _Out_ uint32_t* Value
    )
{
    ParsedQuery parsed;
    std::wstring query;
    std::wstring number;
    size_t start;
    size_t split;
    size_t end;

    Family->clear();
    *Value = 0;

    query = Query;
    start = query.find_first_not_of(L" \t");
    end = query.find_last_not_of(L" \t");
    if (start == std::wstring::npos)
    {
        return false;
    }

    query = query.substr(start, end - start + 1);

    split = query.find_first_of(L" \t:");
    if (split == std::wstring::npos)
    {
        return false;
    }

    number = query.substr(query.find_first_not_of(L" \t:", split) == std::wstring::npos
                              ? query.size()
                              : query.find_first_not_of(L" \t:", split));

    //
    // Exactly one more word, and that word a number.
    //
    if (number.empty() || (number.find_first_of(L" \t") != std::wstring::npos))
    {
        return false;
    }

    if (FindFamily(query.substr(0, split).c_str()) == nullptr)
    {
        return false;
    }

    parsed = ParseQuery(number.c_str());
    if (parsed.Numbers.empty() || !parsed.Names.empty())
    {
        return false;
    }

    //
    // Eight decimal digits read two ways. Flags are written in hex, so a hex
    // reading is the one meant.
    //
    *Value = parsed.Numbers[0].Value;
    for (const ParsedNumber& reading : parsed.Numbers)
    {
        if (reading.Form == NumberForm::HexGuess)
        {
            *Value = reading.Value;
        }
    }

    *Family = query.substr(0, split);
    return true;
}


/*!

    @brief Decodes a value into the named bits of a flag family.

    @details The match is of kind Flags, named after the family, with the
             decode as its text. That's the bits set by name, the composites
             the value holds in full, and any bits no flag names, which get
             shown rather than dropped.

    @param[in] Family - The family, by key or by name.

    @param[in] Value - The value.

    @param[out] Match - Receives the decode.

    @return True if the family is known.

*/
bool
DecodeFlags (
    _In_z_ PCWSTR Family,
    _In_ uint32_t Value,
    _Out_ CodeMatch* Match
    )
{
    const FlagFamily* family;
    std::vector<std::wstring> flags;
    std::vector<std::wstring> composites;
    std::vector<uint32_t> held;
    std::wstring text;
    uint32_t named;
    uint32_t leftover;
    uint32_t entry;
    bool larger;

    *Match = CodeMatch();

    family = FindFamily(Family);
    if (family == nullptr)
    {
        return false;
    }

    Match->Kind = CodeKind::Flags;
    Match->Value = Value;
    Match->Names.push_back(family->Name);
    Match->Header = (family->Source != nullptr) ? family->Source : L"";
    Match->Source = TextSource::Reference;

    if (family->Enumerated)
    {
        for (size_t i = 0; i < family->Count; i++)
        {
            if ((family->Entries[i].Value == Value) && ((family->Entries[i].Flags & k_TableEntryAlias) == 0))
            {
                Match->Text = EntryName(*family, i);
                return true;
            }
        }

        Match->Text = std::format(L"{} isn't one of the {}.", Value, family->Name);
        return true;
    }

    named = 0;

    for (size_t i = 0; i < family->Count; i++)
    {
        entry = family->Entries[i].Value;
        if (((family->Entries[i].Flags & k_TableEntryAlias) != 0) || (entry == 0))
        {
            continue;
        }

        if (std::popcount(entry) == 1)
        {
            if ((Value & entry) != 0)
            {
                flags.push_back(EntryName(*family, i));
                named |= entry;
            }
            continue;
        }

        if ((Value & entry) == entry)
        {
            held.push_back(entry);
        }
    }

    //
    // Only the largest composites. One inside another the value also holds
    // says nothing more.
    //
    for (size_t i = 0; i < family->Count; i++)
    {
        entry = family->Entries[i].Value;
        if (((family->Entries[i].Flags & k_TableEntryAlias) != 0) ||
            (std::popcount(entry) < 2) ||
            ((Value & entry) != entry))
        {
            continue;
        }

        larger = false;
        for (uint32_t other : held)
        {
            if ((other != entry) && ((other & entry) == entry))
            {
                larger = true;
                break;
            }
        }

        if (!larger)
        {
            composites.push_back(EntryName(*family, i));
        }
    }

    if (Value == 0)
    {
        for (size_t i = 0; i < family->Count; i++)
        {
            if ((family->Entries[i].Value == 0) && ((family->Entries[i].Flags & k_TableEntryAlias) == 0))
            {
                Match->Text = EntryName(*family, i) + L": no flags set.";
                return true;
            }
        }

        Match->Text = L"No flags set.";
        return true;
    }

    leftover = Value & ~named;
    if (leftover != 0)
    {
        flags.push_back(std::format(L"0x{:X}", leftover));
    }

    text = Join(flags, L" | ");

    if (!composites.empty())
    {
        text += L"\nIncludes " + Join(composites, L", ") + L".";
    }

    if (leftover != 0)
    {
        text += std::format(L"\n0x{:08X} isn't named by any flag in this family.", leftover);

        if ((_wcsicmp(family->Key, L"access") == 0) && ((leftover & k_SpecificRights) != 0))
        {
            text += L" The low 16 bits are rights specific to the type of object: try file, key, process or thread.";
        }
    }

    Match->Text = text;
    return true;
}
