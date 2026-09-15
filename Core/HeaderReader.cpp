/*!

    @file Core/HeaderReader.cpp

    @brief Reads the user's own header files for the codes they define,
           without a compiler.

    @details An installed WinCode has no compiler, so we evaluate #defines
             ourselves. That covers what code headers hold, so numbers, casts,
             character constants, simple arithmetic, the SDK's code macros
             (MAKE_HRESULT, _HRESULT_TYPEDEF_ and so on), your own function
             like macros built on them, and names from your headers or the SDK.

             Conditionals aren't evaluated. Where a name's defined twice, as in
             both halves of an #if, the first one wins, as in the generator.

             Headers are read together and names evaluated when first needed,
             so one header can use another's facility, and a name can be used
             before it's defined.

             We only report failures for definitions that look like codes.
             Include guards, sizes and IOCTLs aren't, and reporting them all
             would bury what matters.

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

#include <algorithm>
#include <cwctype>
#include <format>
#include <unordered_map>


//
// How deep macros can nest before we give up, which also stops one that
// expands to itself.
//
static constexpr int k_MaxDepth = 32;

//
// The largest header read. Anything bigger isn't a header of codes.
//
static constexpr LONGLONG k_MaxFileSize = 16 * 1024 * 1024;

static constexpr uint32_t k_FacilityWin32 = 7;
static constexpr uint32_t k_NtBit = 0x10000000;


/*!

    @brief What a definition says about which kind of code it is.

*/
enum class Hint
{
    None,
    NtStatus,
    HResult,
    WinError,
    PoolTag,
    Ioctl
};


/*!

    @brief The kinds of token an expression is made of.

*/
enum class TokenKind
{
    Number,
    Character,
    Identifier,
    Punctuation
};


/*!

    @brief One token of an expression.

*/
struct Token
{
    TokenKind Kind;
    uint64_t Value;
    std::wstring Text;

    //
    // A number written with an L suffix, as Win32 errors are in winerror.h.
    //
    bool Long;
};


/*!

    @brief A value, and what was learnt on the way about its kind.

*/
struct Evaluated
{
    uint64_t Value;
    Hint Kind;
};


/*!

    @brief One #define, as read from a header.

*/
struct Define
{
    size_t Header;
    uint32_t Line;
    std::wstring Name;
    bool FunctionLike;
    std::vector<std::wstring> Parameters;
    std::wstring Body;

    //
    // The comment on the definition's line, which is its text if there's no
    // MessageText block for it.
    //
    std::wstring Comment;
};


/*!

    @brief What's known of a name while the headers are evaluated.

*/
struct Resolution
{
    bool InProgress;
    bool Done;
    bool Ok;
    Evaluated Result;
    std::wstring Error;
};


/*!

    @brief Everything read from the headers, and what's been evaluated so far.

*/
struct HeaderContext
{
    std::vector<Define> Defines;

    //
    // The first definition of each name, by index into Defines.
    //
    std::unordered_map<std::wstring, size_t> ByName;

    std::unordered_map<std::wstring, std::wstring> MessageTexts;
    std::unordered_map<std::wstring, Resolution> Resolved;
};


/*!

    @brief Names every expression can use, as the SDK defines them, beyond the
           ones in the generated tables.

*/
struct BuiltinConstant
{
    PCWSTR Name;
    uint32_t Value;
};

static const BuiltinConstant k_Builtins[] =
{
    { L"SEVERITY_SUCCESS", 0 },
    { L"SEVERITY_ERROR", 1 },
    { L"ERROR_SEVERITY_SUCCESS", 0x00000000 },
    { L"ERROR_SEVERITY_INFORMATIONAL", 0x40000000 },
    { L"ERROR_SEVERITY_WARNING", 0x80000000 },
    { L"ERROR_SEVERITY_ERROR", 0xC0000000 },
    { L"STATUS_SEVERITY_SUCCESS", 0 },
    { L"STATUS_SEVERITY_INFORMATIONAL", 1 },
    { L"STATUS_SEVERITY_WARNING", 2 },
    { L"STATUS_SEVERITY_ERROR", 3 },
    { L"FACILITY_NT_BIT", 0x10000000 },
    { L"FACILITY_NTWIN32", 7 },
    { L"METHOD_BUFFERED", 0 },
    { L"METHOD_IN_DIRECT", 1 },
    { L"METHOD_OUT_DIRECT", 2 },
    { L"METHOD_NEITHER", 3 },
    { L"FILE_ANY_ACCESS", 0 },
    { L"FILE_SPECIAL_ACCESS", 0 },
    { L"FILE_READ_ACCESS", 1 },
    { L"FILE_WRITE_ACCESS", 2 },
    { L"FILE_DEVICE_CONTROLLER", 0x04 },
    { L"FILE_DEVICE_DISK", 0x07 },
    { L"FILE_DEVICE_FILE_SYSTEM", 0x09 },
    { L"FILE_DEVICE_KEYBOARD", 0x0B },
    { L"FILE_DEVICE_MOUSE", 0x0F },
    { L"FILE_DEVICE_NETWORK", 0x12 },
    { L"FILE_DEVICE_NETWORK_FILE_SYSTEM", 0x14 },
    { L"FILE_DEVICE_SERIAL_PORT", 0x1B },
    { L"FILE_DEVICE_UNKNOWN", 0x22 },
    { L"FILE_DEVICE_VIDEO", 0x23 },
    { L"FILE_DEVICE_BUS_EXTENDER", 0x2A },
    { L"TRUE", 1 },
    { L"FALSE", 0 },
};

//
// Types a cast can name. The ones that say what kind of code follows are
// picked out in CastHint.
//
static const PCWSTR k_TypeNames[] =
{
    L"NTSTATUS", L"HRESULT", L"SCODE", L"NDIS_STATUS", L"USBD_STATUS",
    L"LONG", L"ULONG", L"DWORD", L"INT", L"UINT", L"BYTE", L"WORD", L"SHORT", L"USHORT",
    L"LONG32", L"ULONG32", L"INT32", L"UINT32", L"LONG_PTR", L"ULONG_PTR",
    L"int", L"unsigned", L"signed", L"long", L"short", L"char",
    L"int32_t", L"uint32_t", L"__int32", L"size_t",
};


/*!

    @brief Text with the white space at either end removed.

    @param[in] Text - The text.

    @return The trimmed text.

*/
static
std::wstring
Trim (
    _In_ const std::wstring& Text
    )
{
    size_t first;
    size_t last;

    first = Text.find_first_not_of(L" \t\r\n\f\v");
    if (first == std::wstring::npos)
    {
        return std::wstring();
    }

    last = Text.find_last_not_of(L" \t\r\n\f\v");
    return Text.substr(first, last - first + 1);
}


/*!

    @brief Whether a character can be part of an identifier.

*/
static
bool
IsIdentifierChar (
    _In_ WCHAR Char
    )
{
    return ((Char >= L'A') && (Char <= L'Z')) ||
           ((Char >= L'a') && (Char <= L'z')) ||
           ((Char >= L'0') && (Char <= L'9')) ||
           (Char == L'_');
}


/*!

    @brief The SDK's names from the generated tables, every code and facility
           with its kind where it has one. Built the first time a header needs
           one.

*/
struct SdkName
{
    uint32_t Value;
    Hint Kind;
};

static
const std::unordered_map<std::wstring, SdkName>&
SdkNames (
    void
    )
{
    static std::unordered_map<std::wstring, SdkName> names;
    static bool built;
    Hint kind;

    if (built)
    {
        return names;
    }

    for (const KindTable& table : g_KindTables)
    {
        switch (table.Kind)
        {
        case CodeKind::NtStatus:
            kind = Hint::NtStatus;
            break;

        case CodeKind::HResult:
            kind = Hint::HResult;
            break;

        case CodeKind::WinError:
            kind = Hint::WinError;
            break;

        case CodeKind::Ioctl:
            kind = Hint::Ioctl;
            break;

        default:
            kind = Hint::None;
            break;
        }

        for (size_t i = 0; i < table.Table->Count; i++)
        {
            names.emplace(AsciiToWide(table.Table->Entries[i].Name), SdkName{ table.Table->Entries[i].Value, kind });
        }
    }

    for (const TableSpan* facilities : { &g_HResultFacilityTable, &g_NtFacilityTable, &g_IoctlDeviceTypeTable })
    {
        for (size_t i = 0; i < facilities->Count; i++)
        {
            names.emplace(AsciiToWide(facilities->Entries[i].Name), SdkName{ facilities->Entries[i].Value, Hint::None });
        }
    }

    built = true;
    return names;
}


/*!

    @brief Splits an expression into tokens.

    @param[in] Text - The expression.

    @param[out] Tokens - Receives the tokens.

    @param[out] Error - Receives why it couldn't be, on failure.

    @return True if every character was understood.

*/
static
bool
Tokenize (
    _In_ const std::wstring& Text,
    _Out_ std::vector<Token>* Tokens,
    _Out_ std::wstring* Error
    )
{
    static constexpr WCHAR k_Single[] = L"(),|&^~!+-*/%";

    std::wstring digits;
    uint64_t value;
    uint32_t base;
    size_t i;
    size_t j;
    size_t count;
    int digit;
    WCHAR c;
    bool isLong;

    Tokens->clear();
    Error->clear();

    for (i = 0; i < Text.size();)
    {
        c = Text[i];

        if (iswspace(c))
        {
            i++;
            continue;
        }

        if ((c >= L'0') && (c <= L'9'))
        {
            base = 10;
            j = i;

            if ((c == L'0') && ((i + 1) < Text.size()) && ((Text[i + 1] == L'x') || (Text[i + 1] == L'X')))
            {
                base = 16;
                j = i + 2;
            }
            else if ((c == L'0') && ((i + 1) < Text.size()) && (Text[i + 1] >= L'0') && (Text[i + 1] <= L'7'))
            {
                base = 8;
                j = i + 1;
            }

            value = 0;
            count = 0;
            for (; j < Text.size(); j++)
            {
                c = Text[j];
                digit = ((c >= L'0') && (c <= L'9')) ? (c - L'0')
                      : ((c >= L'a') && (c <= L'f')) ? (c - L'a' + 10)
                      : ((c >= L'A') && (c <= L'F')) ? (c - L'A' + 10)
                      : -1;

                if ((digit < 0) || (SCAST(uint32_t)(digit) >= base))
                {
                    break;
                }

                if (value > ((UINT64_MAX - SCAST(uint64_t)(digit)) / base))
                {
                    *Error = L"a number too big for 64 bits";
                    return false;
                }

                value = (value * base) + SCAST(uint64_t)(digit);
                count++;
            }

            if ((base == 16) && (count == 0))
            {
                *Error = L"0x with no digits";
                return false;
            }

            isLong = false;
            while ((j < Text.size()) && IsIdentifierChar(Text[j]))
            {
                c = Text[j];
                if ((c == L'l') || (c == L'L'))
                {
                    isLong = true;
                }
                else if ((c != L'u') && (c != L'U') && (c != L'i') && (c != L'6') && (c != L'4'))
                {
                    *Error = std::format(L"a number with an odd suffix, {}", Text.substr(i, j - i + 1));
                    return false;
                }
                j++;
            }

            Tokens->push_back({ TokenKind::Number, value, Text.substr(i, j - i), isLong });
            i = j;
            continue;
        }

        if (IsIdentifierChar(c))
        {
            for (j = i; (j < Text.size()) && IsIdentifierChar(Text[j]); j++)
            {
            }

            Tokens->push_back({ TokenKind::Identifier, 0, Text.substr(i, j - i), false });
            i = j;
            continue;
        }

        if (c == L'\'')
        {
            //
            // A character constant, as pool tags are written. C puts the first
            // character in the top byte, so 'gaTM' is 0x6761544D.
            //
            value = 0;
            count = 0;

            for (j = i + 1; (j < Text.size()) && (Text[j] != L'\''); j++)
            {
                c = Text[j];

                if (c == L'\\')
                {
                    if ((j + 1) >= Text.size())
                    {
                        break;
                    }

                    j++;
                    switch (Text[j])
                    {
                    case L'n':  c = L'\n'; break;
                    case L't':  c = L'\t'; break;
                    case L'r':  c = L'\r'; break;
                    case L'0':  c = L'\0'; break;
                    default:    c = Text[j]; break;
                    }
                }

                if (c > 0xFF)
                {
                    *Error = L"a character constant that isn't ASCII";
                    return false;
                }

                value = (value << 8) | SCAST(uint64_t)(c);
                count++;
            }

            if ((j >= Text.size()) || (count == 0) || (count > 4))
            {
                *Error = L"a character constant WinCode can't read";
                return false;
            }

            Tokens->push_back({ TokenKind::Character, value, Text.substr(i, j - i + 1), false });
            i = j + 1;
            continue;
        }

        if (((c == L'<') || (c == L'>')) && ((i + 1) < Text.size()) && (Text[i + 1] == c))
        {
            Tokens->push_back({ TokenKind::Punctuation, 0, Text.substr(i, 2), false });
            i += 2;
            continue;
        }

        if (wcschr(k_Single, c) != nullptr)
        {
            //
            // && and || are logic, not the bit arithmetic codes are made with.
            //
            if (((c == L'&') || (c == L'|')) && ((i + 1) < Text.size()) && (Text[i + 1] == c))
            {
                *Error = std::format(L"{}{}, which WinCode doesn't evaluate", c, c);
                return false;
            }

            Tokens->push_back({ TokenKind::Punctuation, 0, std::wstring(1, c), false });
            i++;
            continue;
        }

        *Error = (c == L'"') ? L"a string" : std::format(L"'{}', which WinCode doesn't evaluate", c);
        return false;
    }

    return true;
}


static bool EvaluateTokens(HeaderContext& Context, const std::vector<Token>& Tokens, int Depth, Evaluated* Result, std::wstring* Error);
static bool ResolveName(HeaderContext& Context, const std::wstring& Name, int Depth, Evaluated* Result, std::wstring* Error);


/*!

    @brief A recursive descent parser over one expression's tokens.

*/
struct Parser
{
    HeaderContext* Context;
    const std::vector<Token>* Tokens;
    size_t Position;
    int Depth;
    std::wstring Error;
};


/*!

    @brief Whether the next token is a piece of punctuation.

*/
static
bool
Peek (
    _In_ const Parser& State,
    _In_z_ PCWSTR Text
    )
{
    return (State.Position < State.Tokens->size()) &&
           ((*State.Tokens)[State.Position].Kind == TokenKind::Punctuation) &&
           ((*State.Tokens)[State.Position].Text == Text);
}


static bool ParseOr(Parser& State, Evaluated* Result);
static bool ParseUnary(Parser& State, Evaluated* Result);


/*!

    @brief The kind a cast's type says, if it says one.

    @param[in] Type - The type's name.

    @return The kind, or Hint::None.

*/
static
Hint
CastHint (
    _In_ const std::wstring& Type
    )
{
    if ((Type == L"NTSTATUS") || (Type == L"NDIS_STATUS"))
    {
        return Hint::NtStatus;
    }

    if ((Type == L"HRESULT") || (Type == L"SCODE"))
    {
        return Hint::HResult;
    }

    return Hint::None;
}


/*!

    @brief Whether a name is a type a cast can use.

*/
static
bool
IsTypeName (
    _In_ const std::wstring& Name
    )
{
    for (PCWSTR type : k_TypeNames)
    {
        if (Name == type)
        {
            return true;
        }
    }

    return false;
}


/*!

    @brief Reads the arguments of a macro call, as runs of tokens, up to the
           closing bracket. The opening bracket has been read.

    @param[in,out] State - The parser.

    @param[out] Arguments - Receives the arguments.

    @return True if the call is well formed.

*/
static
bool
ReadArguments (
    _Inout_ Parser& State,
    _Out_ std::vector<std::vector<Token>>* Arguments
    )
{
    std::vector<Token> current;
    const Token* token;
    int depth;

    Arguments->clear();
    depth = 0;

    while (State.Position < State.Tokens->size())
    {
        token = &(*State.Tokens)[State.Position++];

        if ((token->Kind == TokenKind::Punctuation) && (token->Text == L"("))
        {
            depth++;
        }
        else if ((token->Kind == TokenKind::Punctuation) && (token->Text == L")"))
        {
            if (depth == 0)
            {
                if (!current.empty() || !Arguments->empty())
                {
                    Arguments->push_back(current);
                }
                return true;
            }
            depth--;
        }
        else if ((token->Kind == TokenKind::Punctuation) && (token->Text == L",") && (depth == 0))
        {
            Arguments->push_back(current);
            current.clear();
            continue;
        }

        current.push_back(*token);
    }

    State.Error = L"a macro call with no closing bracket";
    return false;
}


/*!

    @brief Evaluates a macro's arguments.

    @param[in,out] State - The parser.

    @param[in] Name - The macro, for the error.

    @param[in] Arguments - Its arguments.

    @param[in] Count - How many it takes.

    @param[out] Values - Receives their values.

    @return True if there are that many, and each evaluates.

*/
static
bool
EvaluateArguments (
    _Inout_ Parser& State,
    _In_ const std::wstring& Name,
    _In_ const std::vector<std::vector<Token>>& Arguments,
    _In_ size_t Count,
    _Out_ std::vector<Evaluated>* Values
    )
{
    Evaluated value;

    Values->clear();

    if (Arguments.size() != Count)
    {
        State.Error = std::format(L"{} given {} arguments rather than {}", Name, Arguments.size(), Count);
        return false;
    }

    for (const std::vector<Token>& argument : Arguments)
    {
        if (!EvaluateTokens(*State.Context, argument, State.Depth + 1, &value, &State.Error))
        {
            return false;
        }

        Values->push_back(value);
    }

    return true;
}


/*!

    @brief Evaluates a call to a function-like macro, one of the SDK's code
           macros or one your headers define.

    @param[in,out] State - The parser, just past the macro's name.

    @param[in] Name - The macro.

    @param[out] Result - Receives the value.

    @return True if it evaluates.

*/
static
bool
ParseCall (
    _Inout_ Parser& State,
    _In_ const std::wstring& Name,
    _Out_ Evaluated* Result
    )
{
    std::vector<std::vector<Token>> arguments;
    std::vector<Evaluated> values;
    std::vector<Token> body;
    std::vector<Token> expanded;
    std::unordered_map<std::wstring, size_t>::const_iterator found;
    const Define* define;
    uint64_t x;
    size_t index;
    bool substituted;

    *Result = { 0, Hint::None };

    State.Position++;
    if (!ReadArguments(State, &arguments))
    {
        return false;
    }

    if ((Name == L"_HRESULT_TYPEDEF_") || (Name == L"_NDIS_ERROR_TYPEDEF_"))
    {
        if (!EvaluateArguments(State, Name, arguments, 1, &values))
        {
            return false;
        }

        //
        // winerror.h's NDIS errors are HRESULTs in shape, 0x8034xxxx, as the
        // generated tables have them.
        //
        *Result = { values[0].Value, Hint::HResult };
        return true;
    }

    if ((Name == L"MAKE_HRESULT") || (Name == L"MAKE_SCODE"))
    {
        if (!EvaluateArguments(State, Name, arguments, 3, &values))
        {
            return false;
        }

        *Result = { (values[0].Value << 31) | (values[1].Value << 16) | values[2].Value, Hint::HResult };
        return true;
    }

    if ((Name == L"HRESULT_FROM_WIN32") || (Name == L"NTSTATUS_FROM_WIN32"))
    {
        if (!EvaluateArguments(State, Name, arguments, 1, &values))
        {
            return false;
        }

        //
        // Zero and negative values pass through, as in the SDK's macros.
        //
        x = values[0].Value & 0xFFFFFFFF;
        if ((x != 0) && ((x & 0x80000000) == 0))
        {
            x = (x & 0xFFFF) | (k_FacilityWin32 << 16) | ((Name == L"HRESULT_FROM_WIN32") ? 0x80000000 : 0xC0000000);
        }

        *Result = { x, (Name == L"HRESULT_FROM_WIN32") ? Hint::HResult : Hint::NtStatus };
        return true;
    }

    if (Name == L"HRESULT_FROM_NT")
    {
        if (!EvaluateArguments(State, Name, arguments, 1, &values))
        {
            return false;
        }

        *Result = { values[0].Value | k_NtBit, Hint::HResult };
        return true;
    }

    if (Name == L"CTL_CODE")
    {
        if (!EvaluateArguments(State, Name, arguments, 4, &values))
        {
            return false;
        }

        *Result = { (values[0].Value << 16) | (values[3].Value << 14) | (values[1].Value << 2) | values[2].Value, Hint::Ioctl };
        return true;
    }

    found = State.Context->ByName.find(Name);
    if ((found == State.Context->ByName.end()) || !State.Context->Defines[found->second].FunctionLike)
    {
        State.Error = std::format(L"uses {}(), which WinCode doesn't know", Name);
        return false;
    }

    define = &State.Context->Defines[found->second];

    if (arguments.size() != define->Parameters.size())
    {
        State.Error = std::format(L"{} given {} arguments rather than {}", Name, arguments.size(), define->Parameters.size());
        return false;
    }

    if (!Tokenize(define->Body, &body, &State.Error))
    {
        State.Error = std::format(L"{} holds {}", Name, State.Error);
        return false;
    }

    //
    // Each parameter becomes its argument, bracketed so the argument's own
    // operators can't bind to the macro's.
    //
    for (const Token& token : body)
    {
        substituted = false;

        if (token.Kind == TokenKind::Identifier)
        {
            for (index = 0; index < define->Parameters.size(); index++)
            {
                if (token.Text == define->Parameters[index])
                {
                    expanded.push_back({ TokenKind::Punctuation, 0, L"(", false });
                    expanded.insert(expanded.end(), arguments[index].begin(), arguments[index].end());
                    expanded.push_back({ TokenKind::Punctuation, 0, L")", false });
                    substituted = true;
                    break;
                }
            }
        }

        if (!substituted)
        {
            expanded.push_back(token);
        }
    }

    return EvaluateTokens(*State.Context, expanded, State.Depth + 1, Result, &State.Error);
}


/*!

    @brief Parses a number, a character constant, a name, a macro call, a
           cast, or a bracketed expression.

*/
static
bool
ParsePrimary (
    _Inout_ Parser& State,
    _Out_ Evaluated* Result
    )
{
    const Token* token;
    std::wstring name;
    Evaluated operand;
    Hint cast;
    size_t next;

    *Result = { 0, Hint::None };

    if (State.Position >= State.Tokens->size())
    {
        State.Error = L"an expression that ends too soon";
        return false;
    }

    token = &(*State.Tokens)[State.Position];

    if (token->Kind == TokenKind::Number)
    {
        State.Position++;
        *Result = { token->Value, Hint::None };
        return true;
    }

    //
    // Only four characters make a pool tag, as drivers write them. A shorter
    // constant, such as a drive letter, is just a number.
    //
    if (token->Kind == TokenKind::Character)
    {
        State.Position++;
        *Result = { token->Value, ((token->Text.size() == 6) && (token->Text.find(L'\\') == std::wstring::npos)) ? Hint::PoolTag : Hint::None };
        return true;
    }

    if (token->Kind == TokenKind::Identifier)
    {
        name = token->Text;
        State.Position++;

        if (Peek(State, L"("))
        {
            return ParseCall(State, name, Result);
        }

        return ResolveName(*State.Context, name, State.Depth + 1, Result, &State.Error);
    }

    if (!Peek(State, L"("))
    {
        State.Error = std::format(L"an unexpected {}", token->Text);
        return false;
    }

    //
    // A cast, so one or more type names in brackets before an operand.
    //
    next = State.Position + 1;
    cast = Hint::None;
    while ((next < State.Tokens->size()) &&
           ((*State.Tokens)[next].Kind == TokenKind::Identifier) &&
           IsTypeName((*State.Tokens)[next].Text))
    {
        if (CastHint((*State.Tokens)[next].Text) != Hint::None)
        {
            cast = CastHint((*State.Tokens)[next].Text);
        }
        next++;
    }

    if ((next > (State.Position + 1)) &&
        (next < State.Tokens->size()) &&
        ((*State.Tokens)[next].Kind == TokenKind::Punctuation) &&
        ((*State.Tokens)[next].Text == L")"))
    {
        State.Position = next + 1;
        if (!ParseUnary(State, &operand))
        {
            return false;
        }

        *Result = { operand.Value & 0xFFFFFFFF, (cast != Hint::None) ? cast : operand.Kind };
        return true;
    }

    State.Position++;
    if (!ParseOr(State, Result))
    {
        return false;
    }

    if (!Peek(State, L")"))
    {
        State.Error = L"a bracket that isn't closed";
        return false;
    }

    State.Position++;
    return true;
}


/*!

    @brief Parses the unary operators -, +, ~ and !.

*/
static
bool
ParseUnary (
    _Inout_ Parser& State,
    _Out_ Evaluated* Result
    )
{
    WCHAR op;

    if (Peek(State, L"-") || Peek(State, L"+") || Peek(State, L"~") || Peek(State, L"!"))
    {
        op = (*State.Tokens)[State.Position].Text[0];

        State.Position++;
        if (!ParseUnary(State, Result))
        {
            return false;
        }

        switch (op)
        {
        case L'-':
            Result->Value = 0 - Result->Value;
            break;

        case L'~':
            Result->Value = ~Result->Value;
            break;

        case L'!':
            Result->Value = (Result->Value == 0) ? 1 : 0;
            break;

        default:
            break;
        }

        return true;
    }

    return ParsePrimary(State, Result);
}


/*!

    @brief Combines what two operands say about their kind, the left one's
           if it says anything.

*/
static
Hint
Combine (
    _In_ Hint Left,
    _In_ Hint Right
    )
{
    return (Left != Hint::None) ? Left : Right;
}


/*!

    @brief Parses one level of binary operators.

    @param[in,out] State - The parser.

    @param[out] Result - Receives the value.

    @param[in] Operators - The level's operators.

    @param[in] Next - The next level down.

    @return True if it parses.

*/
static
bool
ParseLevel (
    _Inout_ Parser& State,
    _Out_ Evaluated* Result,
    _In_ std::initializer_list<PCWSTR> Operators,
    _In_ bool (*Next)(Parser&, Evaluated*)
    )
{
    Evaluated right;
    PCWSTR op;

    if (!Next(State, Result))
    {
        return false;
    }

    for (;;)
    {
        op = nullptr;
        for (PCWSTR candidate : Operators)
        {
            if (Peek(State, candidate))
            {
                op = candidate;
                break;
            }
        }

        if (op == nullptr)
        {
            return true;
        }

        State.Position++;
        if (!Next(State, &right))
        {
            return false;
        }

        switch (op[0])
        {
        case L'|':
            Result->Value |= right.Value;
            break;

        case L'^':
            Result->Value ^= right.Value;
            break;

        case L'&':
            Result->Value &= right.Value;
            break;

        case L'<':
            Result->Value = (right.Value < 64) ? (Result->Value << right.Value) : 0;
            break;

        case L'>':
            Result->Value = (right.Value < 64) ? (Result->Value >> right.Value) : 0;
            break;

        case L'+':
            Result->Value += right.Value;
            break;

        case L'-':
            Result->Value -= right.Value;
            break;

        case L'*':
            Result->Value *= right.Value;
            break;

        case L'/':
        case L'%':
            if (right.Value == 0)
            {
                State.Error = L"a division by zero";
                return false;
            }

            Result->Value = (op[0] == L'/') ? (Result->Value / right.Value) : (Result->Value % right.Value);
            break;
        }

        Result->Kind = Combine(Result->Kind, right.Kind);
    }
}

static bool ParseMultiply(Parser& State, Evaluated* Result) { return ParseLevel(State, Result, { L"*", L"/", L"%" }, ParseUnary); }
static bool ParseAdd(Parser& State, Evaluated* Result) { return ParseLevel(State, Result, { L"+", L"-" }, ParseMultiply); }
static bool ParseShift(Parser& State, Evaluated* Result) { return ParseLevel(State, Result, { L"<<", L">>" }, ParseAdd); }
static bool ParseAnd(Parser& State, Evaluated* Result) { return ParseLevel(State, Result, { L"&" }, ParseShift); }
static bool ParseXor(Parser& State, Evaluated* Result) { return ParseLevel(State, Result, { L"^" }, ParseAnd); }
static bool ParseOr(Parser& State, Evaluated* Result) { return ParseLevel(State, Result, { L"|" }, ParseXor); }


/*!

    @brief Evaluates an expression's tokens.

    @param[in,out] Context - The headers.

    @param[in] Tokens - The expression.

    @param[in] Depth - How deep in macro expansion this is.

    @param[out] Result - Receives the value.

    @param[out] Error - Receives why it couldn't be evaluated.

    @return True if it evaluates.

*/
static
bool
EvaluateTokens (
    _Inout_ HeaderContext& Context,
    _In_ const std::vector<Token>& Tokens,
    _In_ int Depth,
    _Out_ Evaluated* Result,
    _Out_ std::wstring* Error
    )
{
    Parser state;

    *Result = { 0, Hint::None };

    if (Depth > k_MaxDepth)
    {
        *Error = L"macros nested too deeply";
        return false;
    }

    if (Tokens.empty())
    {
        *Error = L"nothing to evaluate";
        return false;
    }

    state.Context = &Context;
    state.Tokens = &Tokens;
    state.Position = 0;
    state.Depth = Depth;

    if (!ParseOr(state, Result))
    {
        *Error = state.Error;
        return false;
    }

    if (state.Position != Tokens.size())
    {
        *Error = std::format(L"an unexpected {}", Tokens[state.Position].Text);
        return false;
    }

    return true;
}


/*!

    @brief Evaluates a name, one your headers define, one of the SDK's, or one
           we know.

    @param[in,out] Context - The headers.

    @param[in] Name - The name.

    @param[in] Depth - How deep in macro expansion this is.

    @param[out] Result - Receives the value.

    @param[out] Error - Receives why it couldn't be evaluated.

    @return True if it evaluates.

*/
static
bool
ResolveName (
    _Inout_ HeaderContext& Context,
    _In_ const std::wstring& Name,
    _In_ int Depth,
    _Out_ Evaluated* Result,
    _Out_ std::wstring* Error
    )
{
    std::unordered_map<std::wstring, size_t>::const_iterator define;
    std::unordered_map<std::wstring, SdkName>::const_iterator sdk;
    std::vector<Token> tokens;
    Resolution* resolution;
    Evaluated value;
    std::wstring error;
    size_t first;
    size_t last;
    bool ok;

    *Result = { 0, Hint::None };
    value = { 0, Hint::None };

    define = Context.ByName.find(Name);
    if ((define != Context.ByName.end()) && !Context.Defines[define->second].FunctionLike)
    {
        resolution = &Context.Resolved[Name];

        if (resolution->Done)
        {
            *Result = resolution->Result;
            *Error = resolution->Error;
            return resolution->Ok;
        }

        if (resolution->InProgress)
        {
            *Error = std::format(L"{} is defined in terms of itself", Name);
            return false;
        }

        resolution->InProgress = true;

        ok = Tokenize(Context.Defines[define->second].Body, &tokens, &error) &&
             EvaluateTokens(Context, tokens, Depth, &value, &error);

        //
        // A lone number with an L suffix is how winerror.h writes a Win32
        // error. Brackets around it don't change that.
        //
        if (ok && (value.Kind == Hint::None))
        {
            first = 0;
            last = tokens.size();

            while (((last - first) >= 3) &&
                   (tokens[first].Text == L"(") && (tokens[first].Kind == TokenKind::Punctuation) &&
                   (tokens[last - 1].Text == L")") && (tokens[last - 1].Kind == TokenKind::Punctuation))
            {
                first++;
                last--;
            }

            //
            // Flags get written that way too, so only a name that says it's
            // an error makes one. Treat as covers the rest.
            //
            if (((last - first) == 1) &&
                (tokens[first].Kind == TokenKind::Number) &&
                tokens[first].Long &&
                (Name.find(L"ERROR") != std::wstring::npos))
            {
                value.Kind = Hint::WinError;
            }
        }

        //
        // Looked up again, since evaluating may have added to the map.
        //
        resolution = &Context.Resolved[Name];
        resolution->InProgress = false;
        resolution->Done = true;
        resolution->Ok = ok;
        resolution->Result = ok ? value : Evaluated{ 0, Hint::None };
        resolution->Error = ok ? std::wstring() : error;

        *Result = resolution->Result;
        *Error = resolution->Error;
        return ok;
    }

    for (const BuiltinConstant& builtin : k_Builtins)
    {
        if (Name == builtin.Name)
        {
            *Result = { builtin.Value, Hint::None };
            return true;
        }
    }

    sdk = SdkNames().find(Name);
    if (sdk != SdkNames().end())
    {
        *Result = { sdk->second.Value, sdk->second.Kind };
        return true;
    }

    *Error = std::format(L"uses {}, which WinCode doesn't know", Name);
    return false;
}


/*!

    @brief Splits one line into its code and its comment, carrying whether a
           block comment is still open to the next line.

    @param[in] Line - The line.

    @param[in,out] InBlock - Whether a /* comment is open.

    @param[out] Code - Receives the code.

    @param[out] Comment - Receives the comment's text.

*/
static
void
SplitComment (
    _In_ const std::wstring& Line,
    _Inout_ bool* InBlock,
    _Out_ std::wstring* Code,
    _Out_ std::wstring* Comment
    )
{
    WCHAR quote;
    size_t i;

    Code->clear();
    Comment->clear();
    quote = 0;

    for (i = 0; i < Line.size(); i++)
    {
        if (*InBlock)
        {
            if ((Line[i] == L'*') && ((i + 1) < Line.size()) && (Line[i + 1] == L'/'))
            {
                *InBlock = false;
                Comment->push_back(L' ');
                i++;
                continue;
            }

            Comment->push_back(Line[i]);
            continue;
        }

        if (quote != 0)
        {
            Code->push_back(Line[i]);

            if ((Line[i] == L'\\') && ((i + 1) < Line.size()))
            {
                Code->push_back(Line[++i]);
            }
            else if (Line[i] == quote)
            {
                quote = 0;
            }
            continue;
        }

        if ((Line[i] == L'"') || (Line[i] == L'\''))
        {
            quote = Line[i];
            Code->push_back(Line[i]);
            continue;
        }

        if ((Line[i] == L'/') && ((i + 1) < Line.size()) && (Line[i + 1] == L'/'))
        {
            Comment->append(Line, i + 2, std::wstring::npos);
            return;
        }

        if ((Line[i] == L'/') && ((i + 1) < Line.size()) && (Line[i + 1] == L'*'))
        {
            *InBlock = true;
            Code->push_back(L' ');
            i++;
            continue;
        }

        Code->push_back(Line[i]);
    }
}


/*!

    @brief Turns a comment into a sentence, with its doc markers gone, a
           capital first and a full stop at the end.

    @param[in] Comment - The comment.

    @return The text, or an empty string if there's nothing in it.

*/
static
std::wstring
CommentText (
    _In_ const std::wstring& Comment
    )
{
    std::wstring text;
    size_t start;

    text = Trim(Comment);

    //
    // Doxygen's ///< and //!< and their block forms.
    //
    start = text.find_first_not_of(L"/*!<");
    text = (start == std::wstring::npos) ? std::wstring() : Trim(text.substr(start));

    while (!text.empty() && ((text.back() == L'*') || (text.back() == L'/')))
    {
        text.pop_back();
    }

    text = Trim(text);
    if (text.empty())
    {
        return text;
    }

    text[0] = SCAST(WCHAR)(towupper(text[0]));
    if ((text.back() != L'.') && (text.back() != L'!') && (text.back() != L'?'))
    {
        text += L'.';
    }

    return text;
}


/*!

    @brief Reads the #defines and the message compiler's MessageText blocks
           from one header.

    @param[in,out] Context - Where they go.

    @param[in] Header - The header's index.

    @param[in] Text - The header.

*/
static
void
CollectDefines (
    _Inout_ HeaderContext& Context,
    _In_ size_t Header,
    _In_ const std::wstring& Text
    )
{
    std::wstring line;
    std::wstring code;
    std::wstring comment;
    std::wstring logical;
    std::wstring logicalComment;
    std::wstring messageId;
    std::wstring messageText;
    std::wstring rest;
    std::wstring parameters;
    Define define;
    size_t from;
    size_t comma;
    size_t start;
    size_t end;
    size_t position;
    size_t close;
    uint32_t lineNumber;
    uint32_t logicalLine;
    bool inBlock;
    bool continued;
    bool inMessageText;

    inBlock = false;
    continued = false;
    inMessageText = false;
    lineNumber = 0;
    logicalLine = 0;

    for (start = 0; start <= Text.size(); start = end + 1)
    {
        end = Text.find(L'\n', start);
        if (end == std::wstring::npos)
        {
            end = Text.size();
        }

        line = Text.substr(start, end - start);
        if (!line.empty() && (line.back() == L'\r'))
        {
            line.pop_back();
        }

        lineNumber++;
        SplitComment(line, &inBlock, &code, &comment);

        //
        // The message compiler's MessageId and MessageText blocks are comment
        // only lines, and even a bare // in one is part of the text.
        //
        if (!continued && Trim(code).empty())
        {
            rest = Trim(comment);

            if (rest.rfind(L"MessageId:", 0) == 0)
            {
                messageId = Trim(rest.substr(10));
                messageText.clear();
                inMessageText = false;
                continue;
            }

            if (!messageId.empty() && (rest.rfind(L"MessageText:", 0) == 0))
            {
                inMessageText = true;
                continue;
            }

            if (inMessageText && !rest.empty())
            {
                messageText += messageText.empty() ? L"" : L" ";
                messageText += rest;
            }
            continue;
        }

        if (inMessageText)
        {
            if (!messageText.empty())
            {
                Context.MessageTexts.emplace(messageId, messageText);
            }
            inMessageText = false;
            messageId.clear();
        }

        if (!continued)
        {
            logical.clear();
            logicalComment.clear();
            logicalLine = lineNumber;
        }

        logical += code;
        logicalComment += comment;

        //
        // A backslash at the end carries the line on to the next.
        //
        rest = Trim(logical);
        continued = !rest.empty() && (rest.back() == L'\\');
        if (continued)
        {
            logical = rest.substr(0, rest.size() - 1) + L" ";
            continue;
        }

        rest = Trim(logical);
        if (rest.empty() || (rest[0] != L'#'))
        {
            continue;
        }

        rest = Trim(rest.substr(1));
        if ((rest.rfind(L"define", 0) != 0) || (rest.size() <= 6) || !iswspace(rest[6]))
        {
            continue;
        }

        rest = Trim(rest.substr(6));
        for (position = 0; (position < rest.size()) && IsIdentifierChar(rest[position]); position++)
        {
        }

        if (position == 0)
        {
            continue;
        }

        define = Define();
        define.Header = Header;
        define.Line = logicalLine;
        define.Name = rest.substr(0, position);
        define.Comment = logicalComment;

        //
        // A bracket straight after the name, with no space, makes it a
        // function-like macro.
        //
        if ((position < rest.size()) && (rest[position] == L'('))
        {
            close = rest.find(L')', position);
            if (close == std::wstring::npos)
            {
                continue;
            }

            define.FunctionLike = true;

            parameters = rest.substr(position + 1, close - position - 1);
            from = 0;

            while (!Trim(parameters).empty())
            {
                comma = parameters.find(L',', from);
                define.Parameters.push_back(Trim(parameters.substr(from, (comma == std::wstring::npos) ? std::wstring::npos : comma - from)));
                if (comma == std::wstring::npos)
                {
                    break;
                }
                from = comma + 1;
            }

            define.Body = Trim(rest.substr(close + 1));
        }
        else
        {
            define.Body = Trim(rest.substr(position));
        }

        //
        // The first definition wins, as with the two halves of an #if.
        //
        if (Context.ByName.find(define.Name) == Context.ByName.end())
        {
            Context.ByName.emplace(define.Name, Context.Defines.size());
        }

        Context.Defines.push_back(define);
    }
}


/*!

    @brief Whether a name's a building block codes are made from rather than
           a code, like a severity, a facility or a range marker.

*/
static
bool
IsBuildingBlock (
    _In_ const std::wstring& Name
    )
{
    static constexpr PCWSTR k_Endings[] = { L"_BASE", L"_FIRST", L"_LAST", L"_MASK", L"_MIN", L"_MAX", L"_BIT", L"_SHIFT" };

    size_t length;

    if ((Name.find(L"SEVERITY") != std::wstring::npos) || (Name.rfind(L"FACILITY_", 0) == 0))
    {
        return true;
    }

    for (PCWSTR ending : k_Endings)
    {
        length = wcslen(ending);

        if ((Name.size() > length) && (Name.compare(Name.size() - length, length, ending) == 0))
        {
            return true;
        }
    }

    return false;
}


/*!

    @brief Whether a definition looks like a code, and so is worth reporting
           when it can't be evaluated.

*/
static
bool
LooksLikeCode (
    _In_ const Define& Definition,
    _In_ UserTreatAs TreatAs
    )
{
    static constexpr PCWSTR k_Prefixes[] = { L"STATUS_", L"ERROR_", L"E_", L"S_" };
    static constexpr PCWSTR k_Infixes[] = { L"_E_", L"_S_", L"_STATUS_", L"_ERROR_" };
    static constexpr PCWSTR k_Makers[] = { L"(NTSTATUS)", L"(HRESULT)", L"(SCODE)", L"_HRESULT_TYPEDEF_", L"MAKE_HRESULT", L"MAKE_SCODE", L"CTL_CODE" };

    //
    // Strings and sizes are never codes, whatever they're called, as with
    // ntdef.h's ERROR_LOG_MESSAGE_LIMIT_SIZE.
    //
    if ((Definition.Body.find(L'"') != std::wstring::npos) || (Definition.Body.find(L"sizeof") != std::wstring::npos))
    {
        return false;
    }

    for (PCWSTR prefix : k_Prefixes)
    {
        if (Definition.Name.rfind(prefix, 0) == 0)
        {
            return true;
        }
    }

    for (PCWSTR infix : k_Infixes)
    {
        if (Definition.Name.find(infix) != std::wstring::npos)
        {
            return true;
        }
    }

    for (PCWSTR maker : k_Makers)
    {
        if (Definition.Body.find(maker) != std::wstring::npos)
        {
            return true;
        }
    }

    return (TreatAs != UserTreatAs::Automatic) &&
           (Definition.Body.find_first_of(L"0123456789") != std::wstring::npos);
}


/*!

    @brief A pool tag's text, its bytes as they sit in memory, which is how a
           debugger shows it.

    @param[in] Value - The tag, as C evaluates its character constant.

    @return The tag, without trailing spaces.

*/
static
std::wstring
PoolTagText (
    _In_ uint32_t Value
    )
{
    std::wstring tag;
    WCHAR c;

    for (int i = 0; i < 4; i++)
    {
        c = SCAST(WCHAR)((Value >> (8 * i)) & 0xFF);
        if (c == 0)
        {
            break;
        }
        tag += c;
    }

    while (!tag.empty() && (tag.back() == L' '))
    {
        tag.pop_back();
    }

    return tag;
}


/*!

    @brief Reads headers for the codes they define, without a compiler.

    @details They're read together, so one can use names another defines.

    @param[in] Headers - The headers.

    @return What was read from each, in the same order.

*/
std::vector<HeaderRead>
ReadHeaders (
    _In_ const std::vector<HeaderSource>& Headers
    )
{
    std::vector<HeaderRead> results;
    std::unordered_map<std::wstring, std::wstring>::const_iterator message;
    HeaderContext context;
    HeaderCode code;
    Evaluated value;
    std::wstring error;
    UserTreatAs treatAs;
    Hint kind;

    results.resize(Headers.size());

    for (size_t i = 0; i < Headers.size(); i++)
    {
        CollectDefines(context, i, Headers[i].Text);
    }

    for (size_t i = 0; i < context.Defines.size(); i++)
    {
        const Define& define = context.Defines[i];

        //
        // Only the first definition of a name counts, and only object-like
        // ones are values. Building blocks are used, but aren't codes.
        //
        if (define.FunctionLike ||
            (context.ByName[define.Name] != i) ||
            IsBuildingBlock(define.Name))
        {
            continue;
        }

        treatAs = Headers[define.Header].TreatAs;

        if (!ResolveName(context, define.Name, 0, &value, &error))
        {
            //
            // Failing on a size or a string, even one a name it uses holds,
            // says it was never a code.
            //
            if (LooksLikeCode(define, treatAs) &&
                !define.Body.empty() &&
                (error.find(L"sizeof") == std::wstring::npos) &&
                (error != L"a string"))
            {
                results[define.Header].Skipped.push_back({ define.Line, define.Name, error });
            }
            continue;
        }

        kind = value.Kind;

        if ((kind == Hint::None) && (define.Name.rfind(L"STATUS_", 0) == 0))
        {
            kind = Hint::NtStatus;
        }

        //
        // Treat as says what the header's plain numbers are, and so overrides
        // the guess that an L-suffixed number is a Win32 error.
        //
        if (((kind == Hint::None) || (kind == Hint::WinError)) && (treatAs != UserTreatAs::Automatic))
        {
            kind = (treatAs == UserTreatAs::NtStatus) ? Hint::NtStatus
                 : (treatAs == UserTreatAs::HResult) ? Hint::HResult
                 : Hint::WinError;
        }

        if (kind == Hint::None)
        {
            continue;
        }

        code = HeaderCode();
        code.Name = define.Name;
        code.Value = SCAST(uint32_t)(value.Value & 0xFFFFFFFF);
        code.Line = define.Line;

        switch (kind)
        {
        case Hint::NtStatus:
            code.Kind = CodeKind::NtStatus;
            break;

        case Hint::HResult:
            code.Kind = CodeKind::HResult;
            break;

        case Hint::WinError:
            code.Kind = CodeKind::WinError;
            break;

        case Hint::Ioctl:
            code.Kind = CodeKind::Ioctl;
            break;

        default:
            code.Kind = CodeKind::PoolTag;
            code.Tag = PoolTagText(code.Value);
            if (code.Tag.empty() ||
                (std::find_if(code.Tag.begin(), code.Tag.end(), [](WCHAR C) { return (C < 0x20) || (C > 0x7E); }) != code.Tag.end()))
            {
                results[define.Header].Skipped.push_back({ define.Line, define.Name, L"a pool tag that isn't printable" });
                continue;
            }
            break;
        }

        message = context.MessageTexts.find(define.Name);
        code.Text = (message != context.MessageTexts.end()) ? message->second : CommentText(define.Comment);

        results[define.Header].Codes.push_back(code);
    }

    return results;
}


/*!

    @brief Reads a header file as text, whether UTF-8, UTF-16 with a byte
           order mark, or the ANSI code page.

    @param[in] Path - The file.

    @param[out] Text - Receives its text.

    @return True if it was read.

*/
bool
ReadHeaderFile (
    _In_ const std::wstring& Path,
    _Out_ std::wstring* Text
    )
{
    std::vector<char> bytes;
    LARGE_INTEGER size;
    HANDLE file;
    DWORD read;
    size_t offset;
    UINT codePage;
    DWORD flags;
    int characters;

    Text->clear();

    file = CreateFileW(Path.c_str(),
                       GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    if (!GetFileSizeEx(file, &size) || (size.QuadPart < 0) || (size.QuadPart > k_MaxFileSize))
    {
        CloseHandle(file);
        return false;
    }

    bytes.resize(SCAST(size_t)(size.QuadPart));

    if (!bytes.empty() &&
        (!ReadFile(file, bytes.data(), SCAST(DWORD)(bytes.size()), &read, nullptr) || (read != bytes.size())))
    {
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);

    if (bytes.empty())
    {
        return true;
    }

    //
    // UTF-16 with its byte order mark, as Visual Studio sometimes saves.
    //
    if ((bytes.size() >= 2) && (SCAST(unsigned char)(bytes[0]) == 0xFF) && (SCAST(unsigned char)(bytes[1]) == 0xFE))
    {
        Text->assign(RCAST(const WCHAR*)(bytes.data() + 2), (bytes.size() - 2) / sizeof(WCHAR));
        return true;
    }

    offset = 0;
    if ((bytes.size() >= 3) &&
        (SCAST(unsigned char)(bytes[0]) == 0xEF) &&
        (SCAST(unsigned char)(bytes[1]) == 0xBB) &&
        (SCAST(unsigned char)(bytes[2]) == 0xBF))
    {
        offset = 3;
    }

    //
    // UTF-8 if it is, which ASCII is too, otherwise the ANSI code page an
    // older editor saved it in.
    //
    codePage = CP_UTF8;
    flags = MB_ERR_INVALID_CHARS;

    characters = MultiByteToWideChar(codePage, flags, bytes.data() + offset, SCAST(int)(bytes.size() - offset), nullptr, 0);
    if (characters <= 0)
    {
        codePage = CP_ACP;
        flags = 0;
        characters = MultiByteToWideChar(codePage, flags, bytes.data() + offset, SCAST(int)(bytes.size() - offset), nullptr, 0);
    }

    if (characters <= 0)
    {
        return false;
    }

    Text->resize(SCAST(size_t)(characters));
    MultiByteToWideChar(codePage, flags, bytes.data() + offset, SCAST(int)(bytes.size() - offset), Text->data(), characters);
    return true;
}
