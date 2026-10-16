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

#include <cwctype>


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

    @brief A token, and where it was in the query.

*/
struct QueryToken
{
    std::wstring Text;

    //
    // Offsets into the query, from the token's first character to just past
    // its last, before trailing punctuation was taken off.
    //
    size_t Start;
    size_t End;
};


/*!

    @brief Splits a query into tokens.

    @param[in] Query - The query.

    @return The tokens, with trailing sentence punctuation removed.

*/
static
std::vector<QueryToken>
SplitTokens (
    _In_z_ PCWSTR Query
    )
{
    std::vector<QueryToken> tokens;
    std::wstring current;
    size_t start;

    start = 0;

    for (PCWSTR c = Query; ; c++)
    {
        if ((*c != L'\0') && (wcschr(k_Separators, *c) == nullptr))
        {
            if (current.empty())
            {
                start = SCAST(size_t)(c - Query);
            }

            current += *c;
            continue;
        }

        while (!current.empty() && (wcschr(k_TrailingPunctuation, current.back()) != nullptr))
        {
            current.pop_back();
        }

        if (!current.empty())
        {
            tokens.push_back({ current, start, SCAST(size_t)(c - Query) });
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

    @brief Whether text could be a pool tag.

    @param[in] Text - The text, without trailing spaces.

    @return True if it's one to four printable characters, starting with
            something other than a space, with a letter or digit in it. A tag
            of digits alone needs all four, so "tag 5" stays a number.

*/
static
bool
IsTagText (
    _In_ const std::wstring& Text
    )
{
    bool hasLetter;
    bool hasDigit;

    if (Text.empty() || (Text.size() > 4) || (Text[0] == L' '))
    {
        return false;
    }

    hasLetter = false;
    hasDigit = false;

    for (WCHAR c : Text)
    {
        if ((c < 0x20) || (c > 0x7E) || (c == L'\'') || (c == L'"'))
        {
            return false;
        }

        if (((c >= L'A') && (c <= L'Z')) || ((c >= L'a') && (c <= L'z')))
        {
            hasLetter = true;
        }
        else if (IsDecimalDigit(c))
        {
            hasDigit = true;
        }
    }

    return hasLetter || (hasDigit && (Text.size() == 4));
}


/*!

    @brief Whether a token says a pool tag comes next.

    @details As in "Pool tag: Ntfx", "Pooltag MmSt" from !pool, "!poolfind
             MmSt", or "(Allocated) *MmSt", where !pool puts the tag after the
             block's state.

    @param[in] Query - The query.

    @param[in] Token - The token.

    @return True if the token is one of those labels.

*/
static
bool
IsTagLabel (
    _In_z_ PCWSTR Query,
    _In_ const QueryToken& Token
    )
{
    PCWSTR text;
    bool bracketed;

    text = Token.Text.c_str();
    if (text[0] == L'!')
    {
        text++;
    }

    if ((_wcsicmp(text, L"tag") == 0) ||
        (_wcsicmp(text, L"pooltag") == 0) ||
        (_wcsicmp(text, L"poolfind") == 0))
    {
        return true;
    }

    bracketed = (Token.Start > 0) && (Query[Token.Start - 1] == L'(') && (Query[Token.End] == L')');

    return bracketed && ((_wcsicmp(text, L"Allocated") == 0) || (_wcsicmp(text, L"Free") == 0));
}


/*!

    @brief Reads the pool tag after a label.

    @details We read the query itself rather than its tokens, which would split
             or trim a tag with punctuation in it, like Lr!! or Gh?:.

    @param[in] Query - The query.

    @param[in] From - Where the label ends.

    @param[out] Tag - Receives the tag, without trailing spaces.

    @param[in,out] End - Receives where the tag ends, so the tokens it covers
                         are skipped. Left alone if no tag follows.

    @return True if a tag follows.

*/
static
bool
ReadMarkedTag (
    _In_z_ PCWSTR Query,
    _In_ size_t From,
    _Out_ std::wstring* Tag,
    _Inout_ size_t* End
    )
{
    size_t pos;
    size_t start;
    size_t end;
    WCHAR quote;

    Tag->clear();

    pos = From;
    while ((Query[pos] != L'\0') && (wcschr(L" \t):=*", Query[pos]) != nullptr))
    {
        pos++;
    }

    //
    // In quotes, a tag can end in spaces, like 'Irp '.
    //
    if ((Query[pos] == L'\'') || (Query[pos] == L'"'))
    {
        quote = Query[pos];
        start = ++pos;

        while ((Query[pos] != L'\0') && (Query[pos] != quote) && ((pos - start) <= 4))
        {
            pos++;
        }

        if (Query[pos] != quote)
        {
            return false;
        }

        Tag->assign(Query + start, pos - start);
        end = pos + 1;
    }
    else
    {
        start = pos;
        while ((Query[pos] != L'\0') && !iswspace(Query[pos]))
        {
            pos++;
        }

        Tag->assign(Query + start, pos - start);
        end = pos;

        //
        // Punctuation after a tag in a sentence, but only past four characters,
        // since a tag like DCa. can end in a full stop.
        //
        while ((Tag->size() > 4) && (wcschr(L",;.)]", Tag->back()) != nullptr))
        {
            Tag->pop_back();
        }

        if (!Tag->empty() && (Tag->back() == L','))
        {
            Tag->pop_back();
        }
    }

    while (!Tag->empty() && (Tag->back() == L' '))
    {
        Tag->pop_back();
    }

    if (!IsTagText(*Tag))
    {
        return false;
    }

    *End = end;
    return true;
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

    @brief Adds a name to search for, unless it's there already.

    @param[in,out] Names - The names so far.

    @param[in] Name - The name.

*/
static
void
AddName (
    _Inout_ std::vector<std::wstring>& Names,
    _In_ const std::wstring& Name
    )
{
    for (const std::wstring& name : Names)
    {
        if (_wcsicmp(name.c_str(), Name.c_str()) == 0)
        {
            return;
        }
    }

    Names.push_back(Name);
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

             Pool tags have no underscores, and plenty are words, like File and
             Pool, so inside longer text a tag only counts after a label that
             says it's one.

    @param[in] Query - The text to search.

    @return The numbers and names found, either of which may be empty.

*/
ParsedQuery
ParseQuery (
    _In_z_ PCWSTR Query
    )
{
    std::vector<QueryToken> tokens;
    std::vector<ParsedNumber> readings;
    ParsedQuery parsed;
    std::wstring tag;
    std::wstring trimmed;
    size_t before;
    size_t tagEnd;
    bool alone;
    bool seen;

    tokens = SplitTokens(Query);
    alone = (tokens.size() == 1);
    tagEnd = 0;

    for (const QueryToken& token : tokens)
    {
        //
        // Part of a tag we've already read.
        //
        if (token.Start < tagEnd)
        {
            continue;
        }

        if (IsTagLabel(Query, token) && ReadMarkedTag(Query, token.End, &tag, &tagEnd))
        {
            AddName(parsed.Names, tag);
            continue;
        }

        before = readings.size();
        ParseToken(token.Text, alone, readings);

        if ((readings.size() == before) && IsNameToken(token.Text, alone))
        {
            AddName(parsed.Names, token.Text);
        }

        //
        // A tag like Abc or 8042 reads as a number, so it's a name too, but
        // only if it really is a tag, or "dead" would find every DEADLOCK.
        //
        if ((readings.size() != before) && alone && IsTagText(token.Text) && IsPoolTagName(token.Text.c_str()))
        {
            AddName(parsed.Names, token.Text);
        }
    }

    //
    // A tag with punctuation, like Lr!! or Gh?:, is only a name when it's the
    // whole query, and the tokens would have split or trimmed it.
    //
    trimmed = Query;
    trimmed.erase(0, trimmed.find_first_not_of(L" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(L" \t\r\n") + 1);

    if (readings.empty() && IsTagText(trimmed) && !IsNameToken(trimmed, true))
    {
        parsed.Names = { trimmed };
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
