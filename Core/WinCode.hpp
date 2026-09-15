/*!

    @file Core/WinCode.hpp

    @brief The project-wide header, included by every WinCode binary.

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

//
// N.B. UNICODE, _UNICODE, WIN32_LEAN_AND_MEAN, NOMINMAX and _WIN32_WINNT are
// defined on the compiler command line by CMakeLists.txt rather than here.
//
#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

//
// Cast macros. Every binary includes this header, so they're defined exactly
// once for the whole project.
//
#define CCAST(type) const_cast<type>
#define SCAST(type) static_cast<type>
#define RCAST(type) reinterpret_cast<type>


/*!

    @brief The WinCode version, for example 2.0.0.

    @return A static, never null, string.

*/
PCWSTR
WinCodeVersion (
    void
    );


/*!

    @brief How a number in a query was read.

    @details Reported alongside the value, so a front end can say how it read
             anything ambiguous rather than leave the reader to guess.

*/
enum class NumberForm
{
    Decimal,        // 1234
    Hex,            // 0x4D2, 4D2h, or bare hex such as C0000005
    Negative,       // -2147024891, a signed 32-bit value
    SignExtended,   // 0xFFFFFFFF80070005, a 32-bit value widened to 64 bits
    HexGuess        // 80070005: eight decimal digits, also read as hex
};

/*!

    @brief One reading of one number found in a query.

*/
struct ParsedNumber
{
    uint32_t Value;
    NumberForm Form;

    //
    // The text it was read from, so that two readings of the same token can be
    // told apart from two tokens.
    //
    std::wstring Token;
};

/*!

    @brief Everything found in a query.

*/
struct ParsedQuery
{
    //
    // Every reading of every number, in the order found, with repeated values
    // dropped.
    //
    std::vector<ParsedNumber> Numbers;

    //
    // Tokens to search for as symbolic names, in the order found.
    //
    std::vector<std::wstring> Names;
};

/*!

    @brief Finds every number and name in a query.

    @details A query may be typed, or pasted from a log or a debugger, so a
             number can come with a label, brackets or trailing punctuation,
             as in "(HRESULT: 0x80070005)" or "Status=0xC0000022.".

             A token counts as a name if it is letters, digits and underscores
             and has an underscore in it, as nearly every code's name does, or
             if it is the whole query. That keeps the ordinary words of a pasted
             log line from being searched for.

    @param[in] Query - The text to search.

    @return The numbers and names found, either of which may be empty.

*/
ParsedQuery
ParseQuery (
    _In_z_ PCWSTR Query
    );


/*!

    @brief The kinds of code a value can be.

*/
enum class CodeKind
{
    WinError,
    HResult,
    NtStatus,
    BugCheck,
    WindowMessage
};

/*!

    @brief Where a match's message text came from.

*/
enum class TextSource
{
    None,       // No text anywhere
    System,     // The Windows the tool is running on
    Captured    // The English captured when the tables were generated
};

/*!

    @brief Which layout a value is taken apart with.

*/
enum class FieldLayoutKind
{
    HResult,
    NtStatus
};

/*!

    @brief A value taken apart into the fields of an HRESULT or an NTSTATUS.

    @details The two are alike but not the same. An HRESULT has a one bit
             severity, an eleven bit facility, and the N bit that
             HRESULT_FROM_NT sets. An NTSTATUS has a two bit severity and a
             twelve bit facility. The facility is only named when the customer
             bit is clear, since a customer facility means whatever its owner
             says it does.

*/
struct FieldLayout
{
    FieldLayoutKind Layout;
    uint32_t Severity;          // HRESULT: 0 or 1. NTSTATUS: 0 to 3.
    bool Customer;
    bool NtBit;                 // HRESULT only
    uint32_t Facility;
    std::wstring FacilityName;  // Empty if it has none
    uint32_t Code;
};

/*!

    @brief The name of a layout's severity, for display.

    @param[in] Fields - The layout.

    @return A static, never null, name: success or failure for an HRESULT;
            success, informational, warning or error for an NTSTATUS.

*/
PCWSTR
SeverityName (
    _In_ const FieldLayout& Fields
    );

/*!

    @brief How a related code relates to the match it belongs to.

*/
enum class RelationKind
{
    AsHResult,  // The same error, expressed as an HRESULT
    MapsTo,     // The Win32 error Windows converts an NTSTATUS to
    Wraps,      // The code carried inside an HRESULT
    MappedFrom  // An NTSTATUS that Windows converts to a Win32 error
};

/*!

    @brief Another code that a match converts to, or wraps.

*/
struct RelatedCode
{
    RelationKind Relation;

    //
    // The macro or routine that relates the two, as in HRESULT_FROM_WIN32.
    //
    PCWSTR Via;

    CodeKind Kind;
    uint32_t Value;

    //
    // Empty if the value is in no table.
    //
    std::wstring Name;
};

/*!

    @brief What a value means as one kind of code.

*/
struct CodeMatch
{
    CodeKind Kind;
    uint32_t Value;

    //
    // The primary name first, then any aliases. Empty for a value that is in no
    // table of this kind but still says something as one, see LookupValue.
    //
    std::vector<std::wstring> Names;

    //
    // For an unnamed window message, what it is, as in "WM_USER + 1". Empty
    // otherwise.
    //
    std::wstring Description;

    //
    // Known only from tools/wm_messages.txt, not the SDK headers.
    //
    bool Undocumented;

    //
    // Lines are separated by \n alone.
    //
    std::wstring Text;
    TextSource Source;

    //
    // The value's fields, for HRESULTs and NTSTATUS codes wider than 16 bits.
    //
    std::optional<FieldLayout> Fields;

    std::vector<RelatedCode> Related;

    //
    // For a Win32 error, every NTSTATUS that Windows converts to it, in value
    // order. Kept apart from Related because there can be dozens.
    //
    std::vector<RelatedCode> MappedFrom;

    //
    // The SDK header the name comes from, as in L"winerror.h", or nullptr for a
    // value in no table. An undocumented window message still names
    // winuser.h, so check Undocumented before quoting it.
    //
    PCWSTR Header;
};

/*!

    @brief The name of a kind of code, for display.

    @param[in] Kind - The kind.

    @return A static, never null, name.

*/
PCWSTR
CodeKindName (
    _In_ CodeKind Kind
    );

/*!

    @brief The Windows SDK version the tables were generated from, as in
           10.0.28000.0.

    @return A static, never null, string.

*/
PCWSTR
TablesSdkVersion (
    void
    );

/*!

    @brief Looks a value up as every kind of code.

    @details A value that is in no table of a kind can still match it, with no
             name, when its shape says something: an HRESULT whose facility or
             wrapped code is known, an NTSTATUS that Windows maps to a Win32
             error, a customer code, or a window message in the WM_USER, WM_APP
             or registered ranges.

    @param[in] Value - The value.

    @return One match for each kind of code the value is, in CodeKind order.
            Empty if it is none.

*/
std::vector<CodeMatch>
LookupValue (
    _In_ uint32_t Value
    );


/*!

    @brief How well a name matched a search.

*/
enum class NameQuality
{
    Exact,      // The whole name
    Words,      // Whole underscore separated words: ACCESS_DENIED in ERROR_ACCESS_DENIED
    Partial     // Anywhere, even inside a word
};

/*!

    @brief One name found by a search.

*/
struct NameMatch
{
    CodeKind Kind;
    uint32_t Value;
    std::wstring Name;
    NameQuality Quality;
    bool Undocumented;
};

/*!

    @brief Searches every table's names.

    @param[in] Text - What to search for. Case is ignored.

    @return Every name containing the text, best matches first: by quality,
            then shortest name, then kind. Empty if there are none.

*/
std::vector<NameMatch>
SearchNames (
    _In_z_ PCWSTR Text
    );


/*!

    @brief Formats a value the way its kind is usually written.

    @param[in] Kind - The kind of code.

    @param[in] Value - The value.

    @return Decimal for a Win32 error, four hex digits for a window message,
            eight for anything else.

*/
std::wstring
FormatCodeValue (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    );

/*!

    @brief Describes the fields of an HRESULT or NTSTATUS, as in "failure,
           FACILITY_WIN32 (7), code 0x0005 (5)".

    @param[in] Fields - The fields.

    @return One line.

*/
std::wstring
DescribeFields (
    _In_ const FieldLayout& Fields
    );

/*!

    @brief The label for how a code relates, in lower case, as in "maps to".

    @param[in] Relation - How it relates.

    @return A static, never null, label.

*/
PCWSTR
RelationLabel (
    _In_ RelationKind Relation
    );

/*!

    @brief Describes a related code, as in "Win32 error 5 ERROR_ACCESS_DENIED
           (HRESULT_FROM_WIN32)".

    @details The kind is given except for an HRESULT form, where the label
             already says it. Without it an unnamed code inside an HRESULT would
             be a bare number.

    @param[in] Related - The related code.

    @return One line.

*/
std::wstring
DescribeRelated (
    _In_ const RelatedCode& Related
    );
