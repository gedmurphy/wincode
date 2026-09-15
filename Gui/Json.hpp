/*!

    @file Gui/Json.hpp

    @brief A small JSON value type, enough for a settings file.

    @details Copied from FileMon++, not shared, so each project stays
             self-contained. Hand written rather than taken as a dependency:
             the settings file is the only thing parsed here, the grammar is
             tiny, and the project otherwise links nothing beyond the system
             libraries.

             It is a real tree rather than a flat map of dotted keys, because
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

#include <string>
#include <utility>
#include <vector>


/*!

    @brief One JSON value.

    @details Object members are kept in a vector rather than a map so that the
             file keeps a stable order between saves and stays readable in a
             diff. Settings objects are small enough that the linear lookup
             costs nothing.

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
    // Reading. Each returns the supplied default when the value is absent or is
    // not of the type asked for, so a hand edited file with a mistake in it
    // costs one setting rather than the whole file.
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

    /*!

        @brief Finds a member of an object.

        @param[in] Name - The member name.

        @return The member, or nullptr.

    */
    const JsonValue*
    Find (
        _In_ PCWSTR Name
        ) const;

    /*!

        @brief Finds a value by a dotted path, for example "window.width".

        @param[in] Path - The path.

        @return The value, or nullptr.

    */
    const JsonValue*
    Resolve (
        _In_ PCWSTR Path
        ) const;

    /*!

        @brief Finds or creates a value by dotted path, making objects on the
               way as needed.

        @param[in] Path - The path.

        @return The value.

    */
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

    /*!

        @brief Turns this value into an empty array.

    */
    void
    SetArray (
        void
        );

    /*!

        @brief Turns this value into an empty object.

    */
    void
    SetObject (
        void
        );

    /*!

        @brief Appends to an array, making this an array if it is not one.

        @return The appended element.

    */
    JsonValue&
    Append (
        void
        );

    /*!

        @brief Gets or creates a member, making this an object if it is not one.

        @param[in] Name - The member name.

        @return The member.

    */
    JsonValue&
    Member (
        _In_ PCWSTR Name
        );

    const std::vector<JsonValue>&
    Elements (
        void
        ) const
    {
        return m_Elements;
    }

    /*!

        @brief Parses JSON text.

        @param[in] Text - The text to parse.

        @param[out] Value - Receives the parsed value.

        @return True if the text parsed.

    */
    static
    bool
    Parse (
        _In_ const std::wstring& Text,
        _Out_ JsonValue& Value
        );

    /*!

        @brief Formats this value as indented JSON text.

        @return The text.

    */
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
