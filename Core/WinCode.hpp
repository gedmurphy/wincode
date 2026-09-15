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
// UNICODE, _UNICODE, WIN32_LEAN_AND_MEAN, NOMINMAX and _WIN32_WINNT come from
// CMakeLists.txt, not here.
//
#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

//
// Every binary includes this, so they're defined once for the whole project.
//
#define CCAST(type) const_cast<type>
#define SCAST(type) static_cast<type>
#define RCAST(type) reinterpret_cast<type>


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
    Hex,            // 0x4D2, 4D2h, or bare hex like C0000005
    Negative,       // -2147024891, a signed 32-bit value
    SignExtended,   // 0xFFFFFFFF80070005, a 32-bit value widened to 64 bits
    HexGuess,       // 80070005, eight decimal digits also read as hex
    Repeated        // 0xCCCCCCCCCCCCCCCC, a 32-bit fill pattern at 64 bits
};

/*!

    @brief One reading of one number found in a query.

*/
struct ParsedNumber
{
    uint32_t Value;
    NumberForm Form;

    //
    // The text it was read from, so two readings of one token can be told
    // apart from two tokens.
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

ParsedQuery
ParseQuery (
    _In_z_ PCWSTR Query
    );


/*!

    @brief The kinds of code a value can be.

*/
enum class CodeKind
{
    //
    // First, so it sorts first. A known value's the most specific answer.
    //
    KnownValue,
    WinError,
    HResult,
    NtStatus,
    BugCheck,
    WindowMessage,
    DeviceProblem,      // Device Manager's "Code 43"
    UsbStatus,
    LdapError,
    PoolTag,
    Ioctl,              // An I/O control code, CTL_CODE's or FSCTL's

    //
    // Not a code but a value decoded into the named bits of a flag family,
    // by DecodeFlags.
    //
    Flags
};

/*!

    @brief Where a match's message text came from.

*/
enum class TextSource
{
    None,       // No text anywhere
    System,     // The Windows the tool is running on
    Captured,   // The English captured when the tables were generated
    Curated,    // WinCode's own list of known values
    Reference   // The SDK's own files, for a kind Windows has no text for
};

/*!

    @brief Which layout a value is taken apart with.

*/
enum class FieldLayoutKind
{
    HResult,
    NtStatus,
    Ioctl
};

/*!

    @brief A value taken apart into the fields of an HRESULT, an NTSTATUS or
           an IOCTL.

    @details Alike but not the same. An HRESULT has a one bit severity, an
             eleven bit facility and the N bit HRESULT_FROM_NT sets. An NTSTATUS
             has a two bit severity and a twelve bit facility. We only name the
             facility when the customer bit's clear, since a customer facility
             means whatever its owner says.

             An IOCTL reuses the same slots, with its device type as the
             facility and its function as the code. Customer is set for a
             vendor's own device type.

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
    uint32_t Method;            // IOCTL only
    uint32_t Access;            // IOCTL only
};

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
    // The primary name first, then any aliases. Empty for a value in no table of
    // this kind that still means something as one, see LookupValue.
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
    // The SDK header the name's from, or nullptr if it's in no table or a known
    // value. Undocumented messages still say winuser.h, so check that first.
    //
    std::wstring Header;

    //
    // Whether it's one of the user's own codes, from a header of theirs.
    //
    bool UserCode;
};

PCWSTR
CodeKindName (
    _In_ CodeKind Kind
    );

PCWSTR
TablesSdkVersion (
    void
    );

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

std::vector<NameMatch>
SearchNames (
    _In_z_ PCWSTR Text
    );


std::wstring
FormatCodeValue (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    );

std::wstring
DescribeFields (
    _In_ const FieldLayout& Fields
    );

PCWSTR
RelationLabel (
    _In_ RelationKind Relation
    );

std::wstring
DescribeRelated (
    _In_ const RelatedCode& Related
    );


/*!

    @brief A family of flags a value can be decoded with.

*/
struct FlagFamilyName
{
    //
    // What it's asked for by, as in L"file".
    //
    PCWSTR Key;

    //
    // What it's called, as in L"File access rights".
    //
    PCWSTR Name;
};

std::vector<FlagFamilyName>
FlagFamilyNames (
    void
    );

bool
ParseFlagQuery (
    _In_z_ PCWSTR Query,
    _Out_ std::wstring* Family,
    _Out_ uint32_t* Value
    );

bool
DecodeFlags (
    _In_z_ PCWSTR Family,
    _In_ uint32_t Value,
    _Out_ CodeMatch* Match
    );

bool
ParseIoctlQuery (
    _In_z_ PCWSTR Query,
    _Out_ uint32_t* Value
    );

void
DecodeIoctl (
    _In_ uint32_t Value,
    _Out_ CodeMatch* Match
    );

std::wstring
DescribeIoctlFields (
    _In_ const FieldLayout& Fields
    );


int
CompareVersions (
    _In_z_ PCWSTR A,
    _In_z_ PCWSTR B
    );


/*!

    @brief How the codes from one source are shown.

*/
enum class SourceShow
{
    Always,     // Whenever they match
    Quiet,      // Only for a value no source set to Always names
    Never       // Left out of lookups and name search
};

/*!

    @brief One source of codes, as the settings list it.

*/
struct CodeSource
{
    //
    // How settings.json knows it, as in L"message".
    //
    std::wstring Key;

    //
    // What it's called, as in L"Window messages".
    //
    std::wstring Name;

    CodeKind Kind;

    //
    // How it's shown until you choose otherwise.
    //
    SourceShow Default;

    //
    // Whether it's one of the user's own headers.
    //
    bool User;
};

std::vector<CodeSource>
CodeSources (
    void
    );

SourceShow
GetSourceShow (
    _In_z_ PCWSTR Key
    );

void
SetSourceShow (
    _In_z_ PCWSTR Key,
    _In_ SourceShow Show
    );

void
ResetSourceShows (
    void
    );

void
ShowEverySource (
    _In_ bool All
    );

void
LoadSourceShows (
    void
    );

void
SaveSourceShows (
    void
    );


/*!

    @brief What a header's plain numbers are, where how they're written
           doesn't say.

*/
enum class UserTreatAs
{
    Automatic,  // Decided by how each code is written
    NtStatus,
    HResult,
    WinError
};

/*!

    @brief One header to read.

*/
struct HeaderSource
{
    std::wstring Path;
    std::wstring Text;
    UserTreatAs TreatAs;
};

/*!

    @brief One code a header defines.

*/
struct HeaderCode
{
    CodeKind Kind;
    uint32_t Value;
    std::wstring Name;

    //
    // For a pool tag, the tag as a debugger shows it.
    //
    std::wstring Tag;

    std::wstring Text;
    uint32_t Line;
};

/*!

    @brief A definition that looks like a code, but couldn't be evaluated.

*/
struct HeaderSkip
{
    uint32_t Line;
    std::wstring Name;
    std::wstring Reason;
};

/*!

    @brief What was read from one header.

*/
struct HeaderRead
{
    std::vector<HeaderCode> Codes;
    std::vector<HeaderSkip> Skipped;
};

/*!

    @brief One of the user's headers, as the settings show it.

*/
struct UserHeader
{
    std::wstring Path;

    //
    // Its source's key in Codes to show.
    //
    std::wstring Key;

    //
    // Added in settings, rather than found in the codes folder.
    //
    bool Added;

    //
    // False if the file couldn't be read, as when it's been deleted.
    //
    bool Readable;

    UserTreatAs TreatAs;
    size_t CodeCount;
    std::vector<HeaderSkip> Skipped;
};

std::vector<HeaderRead>
ReadHeaders (
    _In_ const std::vector<HeaderSource>& Headers
    );

bool
ReadHeaderFile (
    _In_ const std::wstring& Path,
    _Out_ std::wstring* Text
    );

std::wstring
UserCodesFolder (
    void
    );

void
LoadUserCodes (
    void
    );

void
LoadUserCodeText (
    _In_ const std::vector<HeaderSource>& Headers
    );

std::vector<UserHeader>
UserHeaders (
    void
    );

void
UserCodesAddFile (
    _In_z_ PCWSTR Path
    );

void
UserCodesRemoveFile (
    _In_z_ PCWSTR Path
    );

void
UserCodesSetTreatAs (
    _In_z_ PCWSTR Path,
    _In_ UserTreatAs TreatAs
    );

std::vector<std::wstring>
UserCodesWatchFolders (
    void
    );
