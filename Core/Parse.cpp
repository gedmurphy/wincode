/*!

    @file Core/Parse.cpp

    @brief Finds the numbers and names in a query, and works out how to read
           the numbers.

    @details Queries get pasted from logs and debuggers as often as typed, so a
             number can come wrapped in a label, brackets or punctuation, in
             any form those tools print.

             Bare hex without 0x is easy to mistake for a word ("add", "face").
             A query that's a single token is taken at its word, since there's
             no doubt. Inside longer text, bare hex needs a digit, or all eight
             characters, to count.

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


//
// Characters that separate tokens. Labels such as "HRESULT:" and "Status=" fall
// away as tokens of their own that aren't numbers.
//
static constexpr WCHAR k_Separators[] = L" \t\r\n()[]{}<>\"'`,;:=|#";

//
// Punctuation that ends a sentence rather than belonging to the number.
//
static constexpr WCHAR k_TrailingPunctuation[] = L".!?";


/*!

    @brief The value of a hex digit.

    @param[in] Char - The character.

    @return 0 to 15, or -1 if it isn't a hex digit.

*/
static
int
HexDigitValue (
    _In_ WCHAR Char
    )
{
    if ((Char >= L'0') && (Char <= L'9'))
    {
        return Char - L'0';
    }

    if ((Char >= L'a') && (Char <= L'f'))
    {
        return Char - L'a' + 10;
    }

    if ((Char >= L'A') && (Char <= L'F'))
    {
        return Char - L'A' + 10;
    }

    return -1;
}


/*!

    @brief Whether a character is a decimal digit.

    @param[in] Char - The character.

    @return True for 0 to 9.

*/
static
bool
IsDecimalDigit (
    _In_ WCHAR Char
    )
{
    return (Char >= L'0') && (Char <= L'9');
}


/*!

    @brief Reads a run of digits in a given base.

    @param[in] Digits - The digits, with no prefix or suffix.

    @param[in] Base - 10 or 16.

    @param[out] Value - Receives the value.

    @return True if every character is a digit in the base and the value fits
            in 64 bits.

*/
static
bool
ReadDigits (
    _In_ const std::wstring& Digits,
    _In_ uint32_t Base,
    _Out_ uint64_t* Value
    )
{
    uint64_t result;
    int digit;

    *Value = 0;

    if (Digits.empty())
    {
        return false;
    }

    result = 0;
    for (WCHAR c : Digits)
    {
        digit = HexDigitValue(c);
        if ((digit < 0) || (SCAST(uint32_t)(digit) >= Base))
        {
            return false;
        }

        if (result > ((UINT64_MAX - SCAST(uint64_t)(digit)) / Base))
        {
            return false;
        }

        result = (result * Base) + SCAST(uint64_t)(digit);
    }

    *Value = result;
    return true;
}


/*!

    @brief Removes a C integer suffix, as on values copied out of a header
           (0x80004005L, 5UL).

    @details Only where it starts with a digit, as a C literal must. Otherwise
             words lose letters, and "ALL" would become "A", which is hex.

    @param[in] Text - The text.

    @return The text without its suffix.

*/
static
std::wstring
StripIntegerSuffix (
    _In_ const std::wstring& Text
    )
{
    std::wstring result;

    result = Text;

    if (result.empty() || !IsDecimalDigit(result[0]))
    {
        return result;
    }

    while ((result.size() > 1) &&
           ((result.back() == L'u') || (result.back() == L'U') ||
            (result.back() == L'l') || (result.back() == L'L')))
    {
        result.pop_back();
    }

    return result;
}


/*!

    @brief Narrows a value read as hex to the 32 bits every code has.

    @details A 64-bit debugger prints a sign extended 32-bit value with the top
             half all ones, like 0xFFFFFFFF80070005, so we take the value it
             came from. Anything else over 32 bits isn't a code.

    @param[in] Wide - The value as read.

    @param[out] Value - Receives the 32-bit value.

    @param[out] Form - Receives Hex, SignExtended if it was widened, or
                       Repeated for a 64-bit fill pattern.

    @return True if the value narrows.

*/
static
bool
NarrowHex (
    _In_ uint64_t Wide,
    _Out_ uint32_t* Value,
    _Out_ NumberForm* Form
    )
{
    *Value = 0;
    *Form = NumberForm::Hex;

    if (Wide <= 0xFFFFFFFFull)
    {
        *Value = SCAST(uint32_t)(Wide);
        return true;
    }

    if (((Wide >> 32) == 0xFFFFFFFFull) && ((Wide & 0x80000000ull) != 0))
    {
        *Value = SCAST(uint32_t)(Wide);
        *Form = NumberForm::SignExtended;
        return true;
    }

    //
    // A 64-bit fill like 0xCCCCCCCCCCCCCCCC is the 32-bit one it repeats. The
    // top bit has to be set, so 0x0000000100000001 isn't read as 1.
    //
    if (((Wide >> 32) == (Wide & 0xFFFFFFFFull)) && ((Wide & 0x80000000ull) != 0))
    {
        *Value = SCAST(uint32_t)(Wide);
        *Form = NumberForm::Repeated;
        return true;
    }

    return false;
}


/*!

    @brief Reads one token as a number, in every way it can sensibly be read.

    @details Usually there's one. Eight decimal digits get a second reading as
             hex, since that's a code with its 0x dropped, which logs do all the
             time. Both come back, and the lookup shows which means something.

    @param[in] Token - The token, already free of separators.

    @param[in] Alone - True if the token is the whole query, which lets bare hex
                       with no digits count.

    @param[in,out] Numbers - Readings are appended here.

*/
static
void
ParseToken (
    _In_ const std::wstring& Token,
    _In_ bool Alone,
    _Inout_ std::vector<ParsedNumber>& Numbers
    )
{
    std::wstring body;
    uint64_t wide;
    uint32_t value;
    NumberForm form;
    bool hasDigit;
    bool hasHexLetter;

    //
    // A negative decimal is a signed 32-bit value, which is how .NET and plenty
    // of logs print an HRESULT.
    //
    if ((Token.size() > 1) && (Token[0] == L'-'))
    {
        body = StripIntegerSuffix(Token.substr(1));
        if (ReadDigits(body, 10, &wide) && (wide <= 0x80000000ull))
        {
            Numbers.push_back({ SCAST(uint32_t)(0ull - wide), NumberForm::Negative, Token });
        }
        return;
    }

    if ((Token.size() > 2) && (Token[0] == L'0') && ((Token[1] == L'x') || (Token[1] == L'X')))
    {
        //
        // The digits after 0x can start with a letter (0xC0000005L), which the
        // suffix rule would refuse. A leading 0 fixes that, same value.
        //
        body = StripIntegerSuffix(L"0" + Token.substr(2));
        if (ReadDigits(body, 16, &wide) && NarrowHex(wide, &value, &form))
        {
            Numbers.push_back({ value, form, Token });
        }
        return;
    }

    //
    // Assembler and debugger style, as in 0C0000005h. The leading digit it
    // requires is what keeps words ending in h out.
    //
    if ((Token.size() > 1) && IsDecimalDigit(Token[0]) && ((Token.back() == L'h') || (Token.back() == L'H')))
    {
        body = Token.substr(0, Token.size() - 1);
        if (ReadDigits(body, 16, &wide) && NarrowHex(wide, &value, &form))
        {
            Numbers.push_back({ value, form, Token });
        }
        return;
    }

    body = StripIntegerSuffix(Token);

    hasDigit = false;
    hasHexLetter = false;
    for (WCHAR c : body)
    {
        if (IsDecimalDigit(c))
        {
            hasDigit = true;
        }
        else if (HexDigitValue(c) >= 0)
        {
            hasHexLetter = true;
        }
    }

    if (!hasHexLetter)
    {
        if (ReadDigits(body, 10, &wide) && (wide <= 0xFFFFFFFFull))
        {
            Numbers.push_back({ SCAST(uint32_t)(wide), NumberForm::Decimal, Token });
        }

        if ((body.size() == 8) && ReadDigits(body, 16, &wide))
        {
            Numbers.push_back({ SCAST(uint32_t)(wide), NumberForm::HexGuess, Token });
        }
        return;
    }

    if (!Alone && !hasDigit && (body.size() < 8))
    {
        return;
    }

    if (ReadDigits(body, 16, &wide) && NarrowHex(wide, &value, &form))
    {
        Numbers.push_back({ value, form, Token });
    }
}


/*!

    @brief Splits a query into tokens.

    @param[in] Query - The query.

    @return The tokens, with trailing sentence punctuation removed.

*/
static
std::vector<std::wstring>
SplitTokens (
    _In_z_ PCWSTR Query
    )
{
    std::vector<std::wstring> tokens;
    std::wstring current;

    for (PCWSTR c = Query; ; c++)
    {
        if ((*c != L'\0') && (wcschr(k_Separators, *c) == nullptr))
        {
            current += *c;
            continue;
        }

        while (!current.empty() && (wcschr(k_TrailingPunctuation, current.back()) != nullptr))
        {
            current.pop_back();
        }

        if (!current.empty())
        {
            tokens.push_back(current);
            current.clear();
        }

        if (*c == L'\0')
        {
            break;
        }
    }

    return tokens;
}


/*!

    @brief Whether a token that isn't a number should be searched for as a
           name.

    @param[in] Token - The token.

    @param[in] Alone - True if the token is the whole query.

    @return True if it is letters, digits and underscores with at least one
            letter, and either has an underscore or is the whole query.

*/
static
bool
IsNameToken (
    _In_ const std::wstring& Token,
    _In_ bool Alone
    )
{
    bool hasLetter;
    bool hasUnderscore;

    if (Token.size() < 2)
    {
        return false;
    }

    hasLetter = false;
    hasUnderscore = false;

    for (WCHAR c : Token)
    {
        if (c == L'_')
        {
            hasUnderscore = true;
        }
        else if (((c >= L'A') && (c <= L'Z')) || ((c >= L'a') && (c <= L'z')))
        {
            hasLetter = true;
        }
        else if (!IsDecimalDigit(c))
        {
            return false;
        }
    }

    return hasLetter && (hasUnderscore || Alone);
}


/*!

    @brief Finds every number and name in a query.

    @details Typed, or pasted from a log or debugger, so a number can come with
             a label, brackets or trailing punctuation, as in
             "(HRESULT: 0x80070005)" or "Status=0xC0000022.".

             A token counts as a name if it's letters, digits and underscores
             with an underscore in it, like nearly every code's name, or if it's
             the whole query. That keeps a pasted log line's ordinary words out
             of the search.

    @param[in] Query - The text to search.

    @return The numbers and names found, either of which may be empty.

*/
ParsedQuery
ParseQuery (
    _In_z_ PCWSTR Query
    )
{
    std::vector<std::wstring> tokens;
    std::vector<ParsedNumber> readings;
    ParsedQuery parsed;
    size_t before;
    bool alone;
    bool seen;

    tokens = SplitTokens(Query);
    alone = (tokens.size() == 1);

    for (const std::wstring& token : tokens)
    {
        before = readings.size();
        ParseToken(token, alone, readings);

        if ((readings.size() != before) || !IsNameToken(token, alone))
        {
            continue;
        }

        seen = false;
        for (const std::wstring& name : parsed.Names)
        {
            if (_wcsicmp(name.c_str(), token.c_str()) == 0)
            {
                seen = true;
                break;
            }
        }

        if (!seen)
        {
            parsed.Names.push_back(token);
        }
    }

    //
    // The same value twice says nothing new, whether it's the same code
    // repeated or a hex guess that came out equal to the decimal.
    //
    for (const ParsedNumber& reading : readings)
    {
        seen = false;
        for (const ParsedNumber& number : parsed.Numbers)
        {
            if (number.Value == reading.Value)
            {
                seen = true;
                break;
            }
        }

        if (!seen)
        {
            parsed.Numbers.push_back(reading);
        }
    }

    return parsed;
}
