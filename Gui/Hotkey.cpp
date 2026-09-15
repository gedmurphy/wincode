/*!

    @file Gui/Hotkey.cpp

    @brief The popup's hotkey: how it's written, which keys it can use, and
           where it's kept.

    @details Kept in settings.json as text such as "Win+Shift+E" rather than as
             numbers, so the file can be read and edited by hand.

             A hotkey always needs a modifier. A bare key would be taken from
             every other program, and typing the letter anywhere would open
             the popup.

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


static constexpr WCHAR k_SettingName[] = L"hotkey";


/*!

    @brief A modifier, and how it's written.

*/
struct ModifierName
{
    UINT Modifier;
    PCWSTR Name;
};


/*!

    @brief A key that isn't a letter, a digit or a function key, and how it's
           written.

*/
struct KeyName
{
    UINT Key;
    PCWSTR Name;
};


//
// In the order they're written, which is the order Windows writes them in its
// own keyboard shortcuts.
//
static const ModifierName k_Modifiers[] =
{
    { MOD_WIN, L"Win" },
    { MOD_CONTROL, L"Ctrl" },
    { MOD_ALT, L"Alt" },
    { MOD_SHIFT, L"Shift" }
};

//
// The keys offered beyond letters, digits and F1 to F24, which are worked out
// rather than listed.
//
static const KeyName k_NamedKeys[] =
{
    { VK_SPACE, L"Space" },
    { VK_INSERT, L"Insert" },
    { VK_DELETE, L"Delete" },
    { VK_HOME, L"Home" },
    { VK_END, L"End" },
    { VK_PRIOR, L"PageUp" },
    { VK_NEXT, L"PageDown" },
    { VK_PAUSE, L"Pause" }
};


/*!

    @brief Text with the spaces at either end removed.

    @param[in] Text - The text.

    @return The trimmed text.

*/
static
std::wstring
TrimSpaces (
    _In_ const std::wstring& Text
    )
{
    size_t first;
    size_t last;

    first = Text.find_first_not_of(L" \t");
    if (first == std::wstring::npos)
    {
        return std::wstring();
    }

    last = Text.find_last_not_of(L" \t");
    return Text.substr(first, last - first + 1);
}


std::vector<UINT>
HotkeyKeys (
    void
    )
{
    std::vector<UINT> keys;

    for (UINT key = 'A'; key <= 'Z'; key++)
    {
        keys.push_back(key);
    }

    for (UINT key = '0'; key <= '9'; key++)
    {
        keys.push_back(key);
    }

    for (UINT key = VK_F1; key <= VK_F24; key++)
    {
        keys.push_back(key);
    }

    for (const KeyName& named : k_NamedKeys)
    {
        keys.push_back(named.Key);
    }

    return keys;
}


std::wstring
HotkeyKeyName (
    _In_ UINT Key
    )
{
    if (((Key >= 'A') && (Key <= 'Z')) || ((Key >= '0') && (Key <= '9')))
    {
        return std::wstring(1, SCAST(WCHAR)(Key));
    }

    if ((Key >= VK_F1) && (Key <= VK_F24))
    {
        return std::format(L"F{}", Key - VK_F1 + 1);
    }

    for (const KeyName& named : k_NamedKeys)
    {
        if (named.Key == Key)
        {
            return named.Name;
        }
    }

    return std::wstring();
}


std::wstring
HotkeyFormat (
    _In_ const Hotkey& Key
    )
{
    std::wstring text;

    for (const ModifierName& modifier : k_Modifiers)
    {
        if ((Key.Modifiers & modifier.Modifier) != 0)
        {
            text += modifier.Name;
            text += L'+';
        }
    }

    return text + HotkeyKeyName(Key.Key);
}


bool
HotkeyParse (
    _In_z_ PCWSTR Text,
    _Out_ Hotkey* Key
    )
{
    std::vector<std::wstring> parts;
    std::wstring text;
    std::wstring part;
    size_t start;
    size_t plus;
    bool found;

    *Key = {};

    text = Text;
    start = 0;

    for (;;)
    {
        plus = text.find(L'+', start);
        parts.push_back(TrimSpaces(text.substr(start, (plus == std::wstring::npos) ? std::wstring::npos : plus - start)));

        if (plus == std::wstring::npos)
        {
            break;
        }

        start = plus + 1;
    }

    //
    // Every part but the last is a modifier; the last is the key.
    //
    for (size_t i = 0; (i + 1) < parts.size(); i++)
    {
        found = false;

        for (const ModifierName& modifier : k_Modifiers)
        {
            if (_wcsicmp(parts[i].c_str(), modifier.Name) == 0)
            {
                Key->Modifiers |= modifier.Modifier;
                found = true;
            }
        }

        //
        // Windows itself writes Control as well as Ctrl.
        //
        if (!found && (_wcsicmp(parts[i].c_str(), L"Control") == 0))
        {
            Key->Modifiers |= MOD_CONTROL;
            found = true;
        }

        if (!found)
        {
            *Key = {};
            return false;
        }
    }

    for (UINT candidate : HotkeyKeys())
    {
        if (_wcsicmp(parts.back().c_str(), HotkeyKeyName(candidate).c_str()) == 0)
        {
            Key->Key = candidate;
        }
    }

    if ((Key->Key == 0) || (Key->Modifiers == 0))
    {
        *Key = {};
        return false;
    }

    return true;
}


Hotkey
HotkeyLoad (
    void
    )
{
    std::wstring text;
    Hotkey key;

    //
    // A hotkey that's missing, or that a hand edit has made unreadable, gets
    // the default rather than none.
    //
    text = SettingsGetString(k_SettingName, L"");
    if (!HotkeyParse(text.c_str(), &key))
    {
        return k_DefaultHotkey;
    }

    return key;
}


void
HotkeySave (
    _In_ const Hotkey& Key
    )
{
    SettingsSetString(k_SettingName, HotkeyFormat(Key).c_str());
    SettingsSave();
}
