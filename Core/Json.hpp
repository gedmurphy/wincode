/*!

    @file Core/Json.hpp

    @brief A small JSON value type, enough for a settings file.

    @details Copied from FileMon++ rather than shared, so each project stands
             alone. Hand written rather than pulled in, since it only ever
             parses the settings file, the grammar's tiny, and otherwise we
             link nothing but system libraries.

             It's a real tree rather than a flat map of dotted keys, because
             the recent lookups are a list of objects.

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

#include <string>
#include <utility>
#include <vector>


/*!

    @brief One JSON value.

    @details Members live in a vector rather than a map, so the file keeps the
             same order between saves and diffs nicely. Settings objects are
             small, so the linear lookup costs nothing.

*/
class JsonValue
{
public:

    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    JsonValue (
        void
        ) : m_Type(Type::Null),
            m_Bool(false),
            m_Number(0.0)
    {
    }

    Type
    GetType (
        void
        ) const
    {
        return m_Type;
    }

    bool
    IsNull (
        void
        ) const
    {
        return m_Type == Type::Null;
    }

    //
    // These return the default if the value's missing or the wrong type, so a
    // typo in a hand edited file costs one setting, not the whole file.
    //
    bool
    AsBool (
        _In_ bool Default
        ) const;

    double
    AsNumber (
        _In_ double Default
        ) const;

    int
    AsInt (
        _In_ int Default
        ) const;

    std::wstring
    AsString (
        _In_ PCWSTR Default
        ) const;

    const JsonValue*
    Find (
        _In_ PCWSTR Name
        ) const;

    const JsonValue*
    Resolve (
        _In_ PCWSTR Path
        ) const;

    JsonValue&
    ResolveOrCreate (
        _In_ PCWSTR Path
        );

    //
    // Writing.
    //
    void SetNull(void);
    void SetBool(_In_ bool Value);
    void SetNumber(_In_ double Value);
    void SetInt(_In_ int Value);
    void SetString(_In_ PCWSTR Value);

    void
    SetArray (
        void
        );

    void
    SetObject (
        void
        );

    JsonValue&
    Append (
        void
        );

    JsonValue&
    Member (
        _In_ PCWSTR Name
        );

    void
    RemoveMember (
        _In_ PCWSTR Name
        );

    const std::vector<JsonValue>&
    Elements (
        void
        ) const
    {
        return m_Elements;
    }

    const std::vector<std::pair<std::wstring, JsonValue>>&
    Members (
        void
        ) const
    {
        return m_Members;
    }

    static
    bool
    Parse (
        _In_ const std::wstring& Text,
        _Out_ JsonValue& Value
        );

    std::wstring
    Format (
        void
        ) const;

private:

    void
    FormatInto (
        _Inout_ std::wstring& Text,
        _In_ int Depth
        ) const;

    Type m_Type;
    bool m_Bool;
    double m_Number;
    std::wstring m_String;
    std::vector<JsonValue> m_Elements;
    std::vector<std::pair<std::wstring, JsonValue>> m_Members;
};
