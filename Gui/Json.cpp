/*!

    @file Gui/Json.cpp

    @brief The JSON parser and writer.

    @details Copied from FileMon++.

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

#include "WinCodeUI.hpp"

#include <stdio.h>
#include <stdlib.h>


bool
JsonValue::AsBool (
    _In_ bool Default
    ) const
{
    return (m_Type == Type::Bool) ? m_Bool : Default;
}

double
JsonValue::AsNumber (
    _In_ double Default
    ) const
{
    return (m_Type == Type::Number) ? m_Number : Default;
}

int
JsonValue::AsInt (
    _In_ int Default
    ) const
{
    return (m_Type == Type::Number) ? SCAST(int)(m_Number) : Default;
}

std::wstring
JsonValue::AsString (
    _In_ PCWSTR Default
    ) const
{
    return (m_Type == Type::String) ? m_String : std::wstring(Default);
}

const JsonValue*
JsonValue::Find (
    _In_ PCWSTR Name
    ) const
{
    size_t i;

    if (m_Type != Type::Object)
    {
        return nullptr;
    }

    for (i = 0; i < m_Members.size(); i++)
    {
        if (m_Members[i].first == Name)
        {
            return &m_Members[i].second;
        }
    }

    return nullptr;
}

const JsonValue*
JsonValue::Resolve (
    _In_ PCWSTR Path
    ) const
{
    const JsonValue* current;
    std::wstring segment;
    std::wstring path;
    size_t start;
    size_t dot;

    current = this;
    path = Path;
    start = 0;

    for (;;)
    {
        dot = path.find(L'.', start);

        segment = (dot == std::wstring::npos)
            ? path.substr(start)
            : path.substr(start, dot - start);

        current = current->Find(segment.c_str());
        if (current == nullptr)
        {
            return nullptr;
        }

        if (dot == std::wstring::npos)
        {
            return current;
        }

        start = dot + 1;
    }
}

JsonValue&
JsonValue::ResolveOrCreate (
    _In_ PCWSTR Path
    )
{
    JsonValue* current;
    std::wstring segment;
    std::wstring path;
    size_t start;
    size_t dot;

    current = this;
    path = Path;
    start = 0;

    for (;;)
    {
        dot = path.find(L'.', start);

        segment = (dot == std::wstring::npos)
            ? path.substr(start)
            : path.substr(start, dot - start);

        current = &current->Member(segment.c_str());

        if (dot == std::wstring::npos)
        {
            return *current;
        }

        start = dot + 1;
    }
}

void
JsonValue::SetNull (
    void
    )
{
    m_Type = Type::Null;
    m_Elements.clear();
    m_Members.clear();
    m_String.clear();
}

void
JsonValue::SetBool (
    _In_ bool Value
    )
{
    SetNull();
    m_Type = Type::Bool;
    m_Bool = Value;
}

void
JsonValue::SetNumber (
    _In_ double Value
    )
{
    SetNull();
    m_Type = Type::Number;
    m_Number = Value;
}

void
JsonValue::SetInt (
    _In_ int Value
    )
{
    SetNumber(SCAST(double)(Value));
}

void
JsonValue::SetString (
    _In_ PCWSTR Value
    )
{
    SetNull();
    m_Type = Type::String;
    m_String = Value;
}

void
JsonValue::SetArray (
    void
    )
{
    SetNull();
    m_Type = Type::Array;
}

void
JsonValue::SetObject (
    void
    )
{
    SetNull();
    m_Type = Type::Object;
}

JsonValue&
JsonValue::Append (
    void
    )
{
    if (m_Type != Type::Array)
    {
        SetArray();
    }

    m_Elements.emplace_back();

    return m_Elements.back();
}

JsonValue&
JsonValue::Member (
    _In_ PCWSTR Name
    )
{
    size_t i;

    if (m_Type != Type::Object)
    {
        SetObject();
    }

    for (i = 0; i < m_Members.size(); i++)
    {
        if (m_Members[i].first == Name)
        {
            return m_Members[i].second;
        }
    }

    m_Members.emplace_back(std::wstring(Name), JsonValue());

    return m_Members.back().second;
}


/////////////////////////////////////////////////////////////
//
//  Parsing
//
////////////////////////////////////////////////////////////

/*!

    @brief Parser state: the text and how far through it we are.

*/
struct JsonParser
{
    const std::wstring* Text;
    size_t Position;
};

static bool ParseValue(_Inout_ JsonParser& Parser, _Out_ JsonValue& Value);

/*!

    @brief Steps over whitespace.

*/
static
void
SkipWhitespace (
    _Inout_ JsonParser& Parser
    )
{
    const std::wstring& text = *Parser.Text;

    while (Parser.Position < text.size())
    {
        WCHAR c = text[Parser.Position];

        if ((c != L' ') && (c != L'\t') && (c != L'\r') && (c != L'\n'))
        {
            return;
        }

        Parser.Position++;
    }
}

/*!

    @brief Tests for a character at the current position and consumes it.

*/
static
bool
Accept (
    _Inout_ JsonParser& Parser,
    _In_ WCHAR Character
    )
{
    SkipWhitespace(Parser);

    if ((Parser.Position < Parser.Text->size()) &&
        ((*Parser.Text)[Parser.Position] == Character))
    {
        Parser.Position++;
        return true;
    }

    return false;
}

/*!

    @brief Parses a quoted string, resolving the escapes JSON defines.

*/
static
bool
ParseString (
    _Inout_ JsonParser& Parser,
    _Out_ std::wstring& Value
    )
{
    const std::wstring& text = *Parser.Text;

    Value.clear();

    if (!Accept(Parser, L'"'))
    {
        return false;
    }

    while (Parser.Position < text.size())
    {
        WCHAR c = text[Parser.Position++];

        if (c == L'"')
        {
            return true;
        }

        if (c != L'\\')
        {
            Value.push_back(c);
            continue;
        }

        if (Parser.Position >= text.size())
        {
            return false;
        }

        c = text[Parser.Position++];

        switch (c)
        {
            case L'"':  Value.push_back(L'"'); break;
            case L'\\': Value.push_back(L'\\'); break;
            case L'/':  Value.push_back(L'/'); break;
            case L'b':  Value.push_back(L'\b'); break;
            case L'f':  Value.push_back(L'\f'); break;
            case L'n':  Value.push_back(L'\n'); break;
            case L'r':  Value.push_back(L'\r'); break;
            case L't':  Value.push_back(L'\t'); break;

            case L'u':
            {
                WCHAR digits[5];
                int i;

                if ((text.size() - Parser.Position) < 4)
                {
                    return false;
                }

                for (i = 0; i < 4; i++)
                {
                    digits[i] = text[Parser.Position + i];
                }

                digits[4] = L'\0';
                Parser.Position += 4;

                Value.push_back(SCAST(WCHAR)(wcstoul(digits, nullptr, 16)));
                break;
            }

            default:
                return false;
        }
    }

    return false;
}

/*!

    @brief Parses a number.

*/
static
bool
ParseNumber (
    _Inout_ JsonParser& Parser,
    _Out_ double& Value
    )
{
    const std::wstring& text = *Parser.Text;
    std::wstring number;

    SkipWhitespace(Parser);

    while (Parser.Position < text.size())
    {
        WCHAR c = text[Parser.Position];

        if (((c >= L'0') && (c <= L'9')) ||
            (c == L'-') || (c == L'+') || (c == L'.') ||
            (c == L'e') || (c == L'E'))
        {
            number.push_back(c);
            Parser.Position++;
            continue;
        }

        break;
    }

    if (number.empty())
    {
        return false;
    }

    Value = _wtof(number.c_str());

    return true;
}

/*!

    @brief Tests for a bare word such as true, false or null.

*/
static
bool
AcceptWord (
    _Inout_ JsonParser& Parser,
    _In_ PCWSTR Word
    )
{
    size_t length;

    SkipWhitespace(Parser);

    length = wcslen(Word);

    if ((Parser.Text->size() - Parser.Position) < length)
    {
        return false;
    }

    if (Parser.Text->compare(Parser.Position, length, Word) != 0)
    {
        return false;
    }

    Parser.Position += length;

    return true;
}

static
bool
ParseValue (
    _Inout_ JsonParser& Parser,
    _Out_ JsonValue& Value
    )
{
    std::wstring text;
    double number;

    SkipWhitespace(Parser);

    if (Parser.Position >= Parser.Text->size())
    {
        return false;
    }

    if (Accept(Parser, L'{'))
    {
        Value.SetObject();

        if (Accept(Parser, L'}'))
        {
            return true;
        }

        for (;;)
        {
            if (!ParseString(Parser, text))
            {
                return false;
            }

            if (!Accept(Parser, L':'))
            {
                return false;
            }

            if (!ParseValue(Parser, Value.Member(text.c_str())))
            {
                return false;
            }

            if (Accept(Parser, L','))
            {
                continue;
            }

            return Accept(Parser, L'}');
        }
    }

    if (Accept(Parser, L'['))
    {
        Value.SetArray();

        if (Accept(Parser, L']'))
        {
            return true;
        }

        for (;;)
        {
            if (!ParseValue(Parser, Value.Append()))
            {
                return false;
            }

            if (Accept(Parser, L','))
            {
                continue;
            }

            return Accept(Parser, L']');
        }
    }

    if ((*Parser.Text)[Parser.Position] == L'"')
    {
        if (!ParseString(Parser, text))
        {
            return false;
        }

        Value.SetString(text.c_str());
        return true;
    }

    if (AcceptWord(Parser, L"true"))
    {
        Value.SetBool(true);
        return true;
    }

    if (AcceptWord(Parser, L"false"))
    {
        Value.SetBool(false);
        return true;
    }

    if (AcceptWord(Parser, L"null"))
    {
        Value.SetNull();
        return true;
    }

    if (ParseNumber(Parser, number))
    {
        Value.SetNumber(number);
        return true;
    }

    return false;
}

bool
JsonValue::Parse (
    _In_ const std::wstring& Text,
    _Out_ JsonValue& Value
    )
{
    JsonParser parser;

    Value.SetNull();

    parser.Text = &Text;
    parser.Position = 0;

    if (!ParseValue(parser, Value))
    {
        Value.SetNull();
        return false;
    }

    return true;
}


/////////////////////////////////////////////////////////////
//
//  Writing
//
////////////////////////////////////////////////////////////

/*!

    @brief Appends a string with the escapes JSON requires.

*/
static
void
FormatString (
    _Inout_ std::wstring& Text,
    _In_ const std::wstring& Value
    )
{
    size_t i;

    Text.push_back(L'"');

    for (i = 0; i < Value.size(); i++)
    {
        WCHAR c = Value[i];

        switch (c)
        {
            case L'"':  Text.append(L"\\\""); break;
            case L'\\': Text.append(L"\\\\"); break;
            case L'\b': Text.append(L"\\b"); break;
            case L'\f': Text.append(L"\\f"); break;
            case L'\n': Text.append(L"\\n"); break;
            case L'\r': Text.append(L"\\r"); break;
            case L'\t': Text.append(L"\\t"); break;

            default:

                if (c < 0x20)
                {
                    WCHAR escape[8];

                    swprintf_s(escape, ARRAYSIZE(escape), L"\\u%04X", c);
                    Text.append(escape);
                    break;
                }

                Text.push_back(c);
                break;
        }
    }

    Text.push_back(L'"');
}

/*!

    @brief Appends a number, without a decimal point when it does not need one.

*/
static
void
FormatNumber (
    _Inout_ std::wstring& Text,
    _In_ double Value
    )
{
    WCHAR number[64];

    //
    // Settings are overwhelmingly whole numbers, and "1280" reads better in a
    // file somebody might edit than "1280.0".
    //
    if ((Value == SCAST(double)(SCAST(long long)(Value))) &&
        (Value < 1.0e15) && (Value > -1.0e15))
    {
        swprintf_s(number, ARRAYSIZE(number), L"%lld", SCAST(long long)(Value));
    }
    else
    {
        swprintf_s(number, ARRAYSIZE(number), L"%.17g", Value);
    }

    Text.append(number);
}

/*!

    @brief Appends indentation.

*/
static
void
Indent (
    _Inout_ std::wstring& Text,
    _In_ int Depth
    )
{
    int i;

    for (i = 0; i < Depth; i++)
    {
        Text.append(L"  ");
    }
}

void
JsonValue::FormatInto (
    _Inout_ std::wstring& Text,
    _In_ int Depth
    ) const
{
    size_t i;

    switch (m_Type)
    {
        case Type::Null:
            Text.append(L"null");
            break;

        case Type::Bool:
            Text.append(m_Bool ? L"true" : L"false");
            break;

        case Type::Number:
            FormatNumber(Text, m_Number);
            break;

        case Type::String:
            FormatString(Text, m_String);
            break;

        case Type::Array:

            if (m_Elements.empty())
            {
                Text.append(L"[]");
                break;
            }

            Text.append(L"[\n");

            for (i = 0; i < m_Elements.size(); i++)
            {
                Indent(Text, Depth + 1);
                m_Elements[i].FormatInto(Text, Depth + 1);

                if ((i + 1) < m_Elements.size())
                {
                    Text.push_back(L',');
                }

                Text.push_back(L'\n');
            }

            Indent(Text, Depth);
            Text.push_back(L']');
            break;

        case Type::Object:

            if (m_Members.empty())
            {
                Text.append(L"{}");
                break;
            }

            Text.append(L"{\n");

            for (i = 0; i < m_Members.size(); i++)
            {
                Indent(Text, Depth + 1);
                FormatString(Text, m_Members[i].first);
                Text.append(L": ");
                m_Members[i].second.FormatInto(Text, Depth + 1);

                if ((i + 1) < m_Members.size())
                {
                    Text.push_back(L',');
                }

                Text.push_back(L'\n');
            }

            Indent(Text, Depth);
            Text.push_back(L'}');
            break;

        default:
            break;
    }
}

std::wstring
JsonValue::Format (
    void
    ) const
{
    std::wstring text;

    FormatInto(text, 0);
    text.push_back(L'\n');

    return text;
}
