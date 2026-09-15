/*!

    @file Gui/Startup.cpp

    @brief Starting WinCode when the user signs in.

    @details An entry in the current user's Run key, which needs no admin
             rights and is what Task Manager's Startup apps page lists. The
             entry starts WinCodeUI with --background, so all that appears is
             the tray icon, and the hotkey works from sign-in.

             Whether it's on is read from the registry every time rather than
             kept in settings.json, since Task Manager can switch it off
             without WinCode knowing. It does that by marking the entry in
             StartupApproved rather than removing it, so both are checked.

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


static constexpr WCHAR k_RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static constexpr WCHAR k_ApprovedKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
static constexpr WCHAR k_ValueName[] = L"WinCode";

//
// Room for a long path to the executable, quoted, and the option after it.
//
static constexpr DWORD k_CommandLength = MAX_PATH + 32;


/*!

    @brief The command the Run key should hold: this executable, quoted, with
           --background.

    @return The command, or an empty string if the executable's path can't be
            found.

*/
static
std::wstring
StartupCommand (
    void
    )
{
    WCHAR module[MAX_PATH];
    DWORD length;

    length = GetModuleFileNameW(nullptr, module, ARRAYSIZE(module));
    if ((length == 0) || (length >= ARRAYSIZE(module)))
    {
        return std::wstring();
    }

    return std::wstring(L"\"") + module + L"\" --background";
}


bool
StartupIsEnabled (
    void
    )
{
    WCHAR command[k_CommandLength];
    BYTE approved[12];
    DWORD size;
    std::wstring expected;

    expected = StartupCommand();
    if (expected.empty())
    {
        return false;
    }

    size = sizeof(command);
    if (RegGetValueW(HKEY_CURRENT_USER, k_RunKey, k_ValueName, RRF_RT_REG_SZ, nullptr, command, &size) != ERROR_SUCCESS)
    {
        return false;
    }

    //
    // An entry for a copy somewhere else isn't this copy starting at sign-in.
    // Turning it on here replaces it.
    //
    if (_wcsicmp(command, expected.c_str()) != 0)
    {
        return false;
    }

    //
    // Task Manager marks an entry it has switched off with an odd first byte.
    //
    size = sizeof(approved);
    if ((RegGetValueW(HKEY_CURRENT_USER, k_ApprovedKey, k_ValueName, RRF_RT_REG_BINARY, nullptr, approved, &size) == ERROR_SUCCESS) &&
        (size >= 1) &&
        ((approved[0] & 0x01) != 0))
    {
        return false;
    }

    return true;
}


bool
StartupSetEnabled (
    _In_ bool Enabled
    )
{
    std::wstring command;
    LSTATUS status;

    if (!Enabled)
    {
        status = RegDeleteKeyValueW(HKEY_CURRENT_USER, k_RunKey, k_ValueName);
        return (status == ERROR_SUCCESS) || (status == ERROR_FILE_NOT_FOUND);
    }

    command = StartupCommand();
    if (command.empty())
    {
        return false;
    }

    status = RegSetKeyValueW(HKEY_CURRENT_USER,
                             k_RunKey,
                             k_ValueName,
                             REG_SZ,
                             command.c_str(),
                             SCAST(DWORD)((command.size() + 1) * sizeof(WCHAR)));
    if (status != ERROR_SUCCESS)
    {
        return false;
    }

    //
    // Asked for here, so a switch-off left in Task Manager is cleared.
    // Otherwise the tick would say WinCode starts at sign-in when it won't.
    //
    RegDeleteKeyValueW(HKEY_CURRENT_USER, k_ApprovedKey, k_ValueName);
    return true;
}
