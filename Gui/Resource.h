/*!

    @file Gui/Resource.h

    @brief Resource and control IDs for WinCodeUI.exe.

    @details Plain #defines, since WinCodeUI.rc includes this too and the
             resource compiler understands nothing more.

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

#ifndef WINCODE_RESOURCE_H
#define WINCODE_RESOURCE_H

#ifndef IDC_STATIC
#define IDC_STATIC              (-1)
#endif

#define IDI_WINCODE             100

#define IDD_SETTINGS            200
#define IDD_NOTE                201
#define IDD_USERCODES           202

//
// The main window.
//
#define IDC_BACK                1001
#define IDC_FORWARD             1002
#define IDC_SEARCH              1003
#define IDC_PINBUTTON           1004    // Not IDC_PIN, which winuser.h has for a cursor
#define IDC_COUNT               1005
#define IDC_RESULTS             1006
#define IDC_DETAILS             1007
#define IDC_SETTINGSBUTTON      1008
#define IDC_FILTER              1009

//
// The settings dialog.
//
#define IDC_HOTKEY_WIN          1101
#define IDC_HOTKEY_CTRL         1102
#define IDC_HOTKEY_ALT          1103
#define IDC_HOTKEY_SHIFT        1104
#define IDC_HOTKEY_KEY          1105
#define IDC_HOTKEY_STATUS       1106
#define IDC_STARTUP             1107
#define IDC_CLEAR_RECENT        1108
#define IDC_SETTINGS_PATH       1109
#define IDC_SETTINGS_HEADING    1110
#define IDC_UPDATECHECK         1111
#define IDC_SOURCES_HEADING     1112
#define IDC_SOURCES_LIST        1113
#define IDC_SOURCE_SHOW         1114
#define IDC_SOURCES_RESET       1115
#define IDC_USERCODES           1116

//
// The note dialog.
//
#define IDC_NOTE_LABEL          1301
#define IDC_NOTE_TEXT           1302

//
// The Your codes dialog.
//
#define IDC_UC_HEADING          1401
#define IDC_UC_LIST             1402
#define IDC_UC_TREATAS          1403
#define IDC_UC_REMOVE           1404
#define IDC_UC_DETAILS          1405
#define IDC_UC_ADD              1406
#define IDC_UC_FOLDER           1407
#define IDC_UC_RELOAD           1408

//
// The tray icon's menu.
//
#define IDM_TRAY_OPEN           1201
#define IDM_TRAY_POPUP          1202
#define IDM_TRAY_SETTINGS       1203
#define IDM_TRAY_STARTUP        1204
#define IDM_TRAY_EXIT           1205

#endif
