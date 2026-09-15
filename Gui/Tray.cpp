/*!

    @file Gui/Tray.cpp

    @brief WinCode's icon in the notification area.

    @details WinCode keeps running once its window is closed, so the hotkey
             always works, and the tray icon is how it's opened again and how
             it's told to exit. The main window owns the icon and acts on what's
             chosen from its menu; this file only talks to the shell.

             Explorer forgets every icon when it restarts, and broadcasts
             TaskbarCreated once it's back, so the owner adds the icon again
             then.

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

#include <shellapi.h>
#include <strsafe.h>


static constexpr UINT k_TrayIconId = 1;

static HWND g_TrayOwner;
static HICON g_TrayIcon;
static bool g_TrayAdded;


/*!

    @brief Fills in the parts of a tray request that every request needs.

    @param[out] Data - The request.

*/
static
void
InitialiseData (
    _Out_ NOTIFYICONDATAW* Data
    )
{
    *Data = {};
    Data->cbSize = sizeof(*Data);
    Data->hWnd = g_TrayOwner;
    Data->uID = k_TrayIconId;
}


bool
TrayAdd (
    _In_ HWND Owner,
    _In_z_ PCWSTR Tip
    )
{
    NOTIFYICONDATAW data;

    g_TrayOwner = Owner;

    //
    // LoadIconMetric picks the size the tray wants at this DPI, rather than
    // the 32 pixel icon scaled down.
    //
    if (g_TrayIcon == nullptr)
    {
        LoadIconMetric(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_WINCODE), LIM_SMALL, &g_TrayIcon);
    }

    InitialiseData(&data);
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = k_TrayMessage;
    data.hIcon = g_TrayIcon;
    StringCchCopyW(data.szTip, ARRAYSIZE(data.szTip), Tip);

    //
    // After Explorer restarts the icon is gone and adding is right. If it's
    // somehow still there, updating it does the same job.
    //
    if (!Shell_NotifyIconW(NIM_ADD, &data) && !Shell_NotifyIconW(NIM_MODIFY, &data))
    {
        g_TrayAdded = false;
        return false;
    }

    //
    // Version 4 reports clicks as NIN_SELECT and WM_CONTEXTMENU, with the
    // keyboard's equivalents, rather than raw mouse messages.
    //
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);

    g_TrayAdded = true;
    return true;
}


void
TraySetTip (
    _In_z_ PCWSTR Tip
    )
{
    NOTIFYICONDATAW data;

    if (!g_TrayAdded)
    {
        return;
    }

    InitialiseData(&data);
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    StringCchCopyW(data.szTip, ARRAYSIZE(data.szTip), Tip);

    Shell_NotifyIconW(NIM_MODIFY, &data);
}


void
TrayNotify (
    _In_z_ PCWSTR Title,
    _In_z_ PCWSTR Text
    )
{
    NOTIFYICONDATAW data;

    if (!g_TrayAdded)
    {
        return;
    }

    InitialiseData(&data);
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    StringCchCopyW(data.szInfoTitle, ARRAYSIZE(data.szInfoTitle), Title);
    StringCchCopyW(data.szInfo, ARRAYSIZE(data.szInfo), Text);

    Shell_NotifyIconW(NIM_MODIFY, &data);
}


void
TrayRemove (
    void
    )
{
    NOTIFYICONDATAW data;

    if (g_TrayAdded)
    {
        InitialiseData(&data);
        Shell_NotifyIconW(NIM_DELETE, &data);
        g_TrayAdded = false;
    }

    if (g_TrayIcon != nullptr)
    {
        DestroyIcon(g_TrayIcon);
        g_TrayIcon = nullptr;
    }
}


UINT
TrayCreatedMessage (
    void
    )
{
    static UINT message = RegisterWindowMessageW(L"TaskbarCreated");

    return message;
}


UINT
TrayShowMenu (
    _In_ HWND Owner,
    _In_ POINT At,
    _In_ bool StartAtSignIn,
    _In_z_ PCWSTR HotkeyText
    )
{
    std::wstring lookUp;
    HMENU menu;
    UINT alignment;
    BOOL command;

    menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return 0;
    }

    lookUp = L"&Look up...";
    if (HotkeyText[0] != L'\0')
    {
        lookUp += L'\t';
        lookUp += HotkeyText;
    }

    AppendMenuW(menu, MF_STRING, IDM_TRAY_OPEN, L"&Open WinCode");
    AppendMenuW(menu, MF_STRING, IDM_TRAY_POPUP, lookUp.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_SETTINGS, L"&Settings...");
    AppendMenuW(menu, MF_STRING | (StartAtSignIn ? MF_CHECKED : MF_UNCHECKED), IDM_TRAY_STARTUP, L"Start when I sign &in");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, L"E&xit");
    SetMenuDefaultItem(menu, IDM_TRAY_OPEN, FALSE);

    alignment = (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;

    //
    // A tray menu only closes when the user clicks away if its owner is in
    // the foreground, which TrackPopupMenuEx doesn't arrange; and the posted
    // WM_NULL is the documented way to make a second click on the icon work.
    //
    SetForegroundWindow(Owner);
    command = TrackPopupMenuEx(menu,
                               TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | alignment,
                               At.x,
                               At.y,
                               Owner,
                               nullptr);
    PostMessageW(Owner, WM_NULL, 0, 0);

    DestroyMenu(menu);
    return SCAST(UINT)(command);
}
