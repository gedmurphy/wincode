/*!

    @file Core/Tables.hpp

    @brief The lookup tables generated from the Windows SDK headers.

    @details Generated into Core/Tables/*.g.cpp by tools/generate_tables.py,
             each sorted by value. Where names share a value the primary one
             comes first and the rest follow as aliases, so a lookup binary
             searches to the first entry and walks forward.

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
#include <vector>

//
// Another name for the value of the entry before it.
//
constexpr uint32_t k_TableEntryAlias = 0x1;

//
// Not in the SDK headers, we only know it from tools/wm_messages.txt.
//
constexpr uint32_t k_TableEntryUndocumented = 0x2;


/*!

    @brief One name for a value.

    @details Narrow rather than UTF-16 because these two are nearly all of the
             tables' size, and it halves it. Names are macro names so always
             ASCII, and Text is UTF-8.

             Text is the English message captured at generation, for when the
             Windows we run on has none of its own. It's nullptr where Windows
             had no text, and on aliases, which share the primary's.

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

    //
    // Each entry's own header, for a table built from many, like the IOCTLs.
    // Null when Source covers the lot.
    //
    const PCWSTR* Headers;
};


extern const TableSpan g_WinErrorTable;
extern const TableSpan g_HResultTable;
extern const TableSpan g_HResultFacilityTable;
extern const TableSpan g_NtStatusTable;
extern const TableSpan g_NtFacilityTable;
extern const TableSpan g_BugCheckTable;
extern const TableSpan g_WindowMessageTable;
extern const TableSpan g_KnownValueTable;
extern const TableSpan g_HResultWindowsUpdateTable;
extern const TableSpan g_HResultAudioTable;
extern const TableSpan g_HResultShadowCopyTable;
extern const TableSpan g_HResultClrTable;
extern const TableSpan g_DeviceProblemTable;
extern const TableSpan g_UsbStatusTable;
extern const TableSpan g_LdapErrorTable;
extern const TableSpan g_PoolTagTable;
extern const TableSpan g_IoctlSdkTable;
extern const TableSpan g_IoctlWdkTable;
extern const TableSpan g_IoctlDeviceTypeTable;


/*!

    @brief A generated family of flags (Flags.g.cpp).

*/
struct FlagFamily
{
    PCWSTR Key;
    PCWSTR Name;

    //
    // The SDK header the flags come from.
    //
    PCWSTR Source;

    //
    // Whole values where one of them's meant, rather than bits.
    //
    bool Enumerated;

    const TableEntry* Entries;
    size_t Count;
};

extern const FlagFamily g_FlagFamilies[];
extern const size_t g_FlagFamilyCount;

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

    //
    // How settings.json knows it, and what the settings call it.
    //
    PCWSTR Key;
    PCWSTR Name;

    //
    // How it's shown until you choose otherwise. Quiet for codes that overlap
    // much more common ones, like USB statuses sharing NTSTATUS values.
    //
    SourceShow Default;

    //
    // A table from one of your own headers (UserCodes.cpp).
    //
    bool User;
};

//
// In CodeKind order. HRESULTs get one table per header, so a match can say
// where it came from. Facility tables name parts of codes, so aren't here.
//
constexpr size_t k_KindTableCount = 16;
extern const KindTable g_KindTables[k_KindTableCount];


const std::vector<KindTable>&
AllKindTables (
    void
    );

SourceShow
SourceShowFor (
    _In_z_ PCWSTR Key
    );

FieldLayout
IoctlFields (
    _In_ uint32_t Value
    );

const TableEntry*
TableFindFirst (
    _In_ const TableSpan& Table,
    _In_ uint32_t Value
    );

std::wstring
AsciiToWide (
    _In_z_ PCSTR Text
    );
