/*!

    @file Core/Tables.hpp

    @brief The lookup tables generated from the Windows SDK headers.

    @details The tables are in Core/Tables/*.g.cpp, written by
             tools/generate_tables.py. Each is sorted by value. Where several
             names share a value the primary one comes first and the rest follow
             it marked as aliases, so a lookup binary searches to the first
             entry for a value and walks forward.

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


#pragma once

#include "WinCode.hpp"

#include <cstdint>

//
// Another name for the value of the entry before it.
//
constexpr uint32_t k_TableEntryAlias = 0x1;

//
// Not in the SDK headers: known only from tools/wm_messages.txt.
//
constexpr uint32_t k_TableEntryUndocumented = 0x2;


/*!

    @brief One name for a value.

    @details Name and Text are narrow rather than UTF-16 because together they
             are nearly all of the tables' size, and this halves it. Names are
             macro names, so always ASCII; Text is UTF-8.

             Text is the English message captured when the table was generated,
             for when the Windows the tool runs on has none of its own. It is
             nullptr where Windows had no text, and on aliases, which share the
             text of the primary entry.

*/
struct TableEntry
{
    uint32_t Value;
    uint32_t Flags;
    PCSTR Name;
    PCSTR Text;
};

/*!

    @brief A generated table.

*/
struct TableSpan
{
    const TableEntry* Entries;
    size_t Count;

    //
    // The SDK header the table was generated from, as in L"winerror.h".
    //
    PCWSTR Source;
};


extern const TableSpan g_WinErrorTable;
extern const TableSpan g_HResultTable;
extern const TableSpan g_HResultFacilityTable;
extern const TableSpan g_NtStatusTable;
extern const TableSpan g_NtFacilityTable;
extern const TableSpan g_BugCheckTable;
extern const TableSpan g_WindowMessageTable;

//
// The Windows SDK version the tables were generated from, as in
// L"10.0.28000.0".
//
extern const WCHAR g_TablesSdkVersion[];


/*!

    @brief A table and the kind of code it holds.

*/
struct KindTable
{
    CodeKind Kind;
    const TableSpan* Table;
};

//
// The tables of codes, in CodeKind order. The facility tables name parts of
// codes rather than codes, so aren't among them.
//
constexpr size_t k_KindTableCount = 5;
extern const KindTable g_KindTables[k_KindTableCount];


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
    );

/*!

    @brief Converts a table name, which is always ASCII, to UTF-16.

    @param[in] Text - The name.

    @return The name.

*/
std::wstring
AsciiToWide (
    _In_z_ PCSTR Text
    );
