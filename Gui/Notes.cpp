/*!

    @file Gui/Notes.cpp

    @brief Your own notes on codes, like "in our driver this means surprise
           removal", kept in settings.json.

    @details Keyed by the code's kind and value, as in "win32:0x00000005", so a
             note follows the code whichever name it's found by.

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

#include <format>


static constexpr WCHAR k_NotesSetting[] = L"notes";


/*!

    @brief The key a code's note is kept under.

    @param[in] Kind - The code's kind.

    @param[in] Value - Its value.

    @return The key.

*/
static
std::wstring
NoteKey (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    )
{
    return std::format(L"{}:0x{:08X}", KindKey(Kind), Value);
}


/*!

    @brief Your note on a code.

    @param[in] Kind - The code's kind.

    @param[in] Value - Its value.

    @return The note, or an empty string for none.

*/
std::wstring
NoteGet (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    )
{
    const JsonValue* notes;
    const JsonValue* note;

    notes = SettingsRoot().Find(k_NotesSetting);
    if (notes == nullptr)
    {
        return std::wstring();
    }

    note = notes->Find(NoteKey(Kind, Value).c_str());
    return (note != nullptr) ? note->AsString(L"") : std::wstring();
}


/*!

    @brief Sets or removes your note on a code, and saves it.

    @param[in] Kind - The code's kind.

    @param[in] Value - Its value.

    @param[in] Text - The note. Empty, or only white space, removes it.

*/
void
NoteSet (
    _In_ CodeKind Kind,
    _In_ uint32_t Value,
    _In_ const std::wstring& Text
    )
{
    std::wstring text;
    JsonValue* notes;
    size_t first;
    size_t last;

    first = Text.find_first_not_of(L" \t\r\n");
    last = Text.find_last_not_of(L" \t\r\n");
    if (first != std::wstring::npos)
    {
        text = Text.substr(first, last - first + 1);
    }

    notes = &SettingsRoot().Member(k_NotesSetting);

    if (text.empty())
    {
        notes->RemoveMember(NoteKey(Kind, Value).c_str());
    }
    else
    {
        notes->Member(NoteKey(Kind, Value).c_str()).SetString(text.c_str());
    }

    SettingsSave();
}
