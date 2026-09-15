/*!

    @file Gui/WinCodeUI.cpp

    @brief WinCodeUI.exe's entry point.

    @details Only one WinCodeUI runs at a time, since only one can own the
             hotkey. A second one hands its command line to the first and
             quits.

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

#include <objbase.h>
#include <shellapi.h>


//
// Held for the life of the process, so a second WinCodeUI can tell one is
// already running. In the Local namespace, so each signed-in user has their
// own.
//
constexpr WCHAR k_InstanceMutex[] = L"Local\\WinCodeUI.Instance";

//
// The options, each only as the first argument.
//
constexpr WCHAR k_PopupOption[] = L"--popup";
constexpr WCHAR k_BackgroundOption[] = L"--background";
constexpr WCHAR k_ExitOption[] = L"--exit";


/*!

    @brief How WinCodeUI was asked to start.

*/
enum class LaunchMode
{
    Window,     // Show the window, as when started from a shortcut
    Popup,      // Show the popup
    Background, // Show nothing but the tray icon, as at sign-in
    Exit        // Tell the one running to quit, as the installer does
};


/*!

    @brief Reads the command line: an option first, then anything else as one
           query.

    @param[out] Mode - Receives how to start.

    @return The query, or an empty string if there was none.

*/
static
std::wstring
ParseCommandLine (
    _Out_ LaunchMode* Mode
    )
{
    std::wstring query;
    PWSTR* args;
    int count;
    int first;

    *Mode = LaunchMode::Window;

    args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (args == nullptr)
    {
        return query;
    }

    first = 1;
    if (count > 1)
    {
        if (_wcsicmp(args[1], k_PopupOption) == 0)
        {
            *Mode = LaunchMode::Popup;
            first = 2;
        }
        else if (_wcsicmp(args[1], k_BackgroundOption) == 0)
        {
            *Mode = LaunchMode::Background;
            first = 2;
        }
        else if (_wcsicmp(args[1], k_ExitOption) == 0)
        {
            *Mode = LaunchMode::Exit;
            first = 2;
        }
    }

    for (int i = first; i < count; i++)
    {
        if (i > first)
        {
            query += L' ';
        }
        query += args[i];
    }

    LocalFree(args);
    return query;
}


/*!

    @brief Entry point.

    @param[in] Instance - The module instance.

    @param[in] PrevInstance - Always null.

    @param[in] CommandLine - Unused; the arguments are read with
                             CommandLineToArgvW instead, so quoting works.

    @param[in] ShowCommand - How to show the window.

    @return The exit code.

*/
int
WINAPI
wWinMain (
    _In_ HINSTANCE Instance,
    _In_opt_ HINSTANCE PrevInstance,
    _In_ PWSTR CommandLine,
    _In_ int ShowCommand
    )
{
    INITCOMMONCONTROLSEX controls;
    std::wstring query;
    HANDLE instanceMutex;
    LaunchMode mode;
    HWND window;
    MSG msg;
    bool running;
    bool exited;

    UNREFERENCED_PARAMETER(PrevInstance);
    UNREFERENCED_PARAMETER(CommandLine);

    query = ParseCommandLine(&mode);

    instanceMutex = CreateMutexW(nullptr, FALSE, k_InstanceMutex);
    running = (instanceMutex != nullptr) && (GetLastError() == ERROR_ALREADY_EXISTS);

    //
    // --exit never starts WinCode, whether or not one was running to tell.
    //
    if (mode == LaunchMode::Exit)
    {
        exited = !running || MainWindowExitRunning();

        if (instanceMutex != nullptr)
        {
            CloseHandle(instanceMutex);
        }

        return exited ? 0 : 1;
    }

    if (running)
    {
        //
        // Starting in the background when WinCode is already running asks for
        // nothing it isn't already doing.
        //
        if (mode == LaunchMode::Background)
        {
            CloseHandle(instanceMutex);
            return 0;
        }

        //
        // If the one already running doesn't answer, this one carries on
        // rather than do nothing at all. It won't get the hotkey, and will say
        // so.
        //
        if (MainWindowForward(query.c_str(), mode == LaunchMode::Popup))
        {
            CloseHandle(instanceMutex);
            return 0;
        }
    }

    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);

    ThemeInitialise();
    SettingsLoad();

    //
    // COM, for the accessible names of the icon buttons.
    //
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        return 1;
    }

    //
    // The popup first, since the hotkey can fire as soon as the main window
    // registers it.
    //
    if (!MainWindowRegister(Instance) || !DetailsPaneRegister(Instance) || !PopupRegister(Instance) ||
        !PopupCreate(Instance))
    {
        return 1;
    }

    window = MainWindowCreate(Instance,
                              (mode == LaunchMode::Window) ? query.c_str() : L"",
                              (mode == LaunchMode::Window) ? ShowCommand : SW_HIDE);
    if (window == nullptr)
    {
        return 1;
    }

    if (mode == LaunchMode::Popup)
    {
        PopupShow(query.empty() ? nullptr : query.c_str());
    }

    //
    // IsDialogMessage gives the window dialog-style keyboard handling: Tab
    // moves between the controls, Enter is IDOK and Esc is IDCANCEL. It passes
    // over the popup's messages, since the popup isn't the window's child.
    //
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (MainWindowPreTranslate(&msg))
        {
            continue;
        }

        if (!IsDialogMessageW(window, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    CoUninitialize();

    if (instanceMutex != nullptr)
    {
        CloseHandle(instanceMutex);
    }

    return SCAST(int)(msg.wParam);
}
