/*!

    @file Gui/WinCodeUI.hpp

    @brief Shared declarations for WinCodeUI.exe, the GUI.

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

#include <commctrl.h>

#include "Settings.hpp"
#include "Resource.h"


//
// The notification the tray icon sends its owner (Tray.cpp).
//
constexpr UINT k_TrayMessage = WM_APP + 1;

//
// Posted when the update check's done (Update.cpp), with an UpdateResult or
// nothing in LPARAM. WM_APP + 2 is the main window's own.
//
constexpr UINT k_UpdateMessage = WM_APP + 3;

//
// What the watch on the user's headers posts the main window when one of
// them changes (UserCodesDialog.cpp).
//
constexpr UINT k_UserCodesMessage = WM_APP + 4;


//
// The update check (Update.cpp).
//

void
UpdateCheckIfDue (
    _In_ HWND Owner
    );

void
UpdateOnResult (
    _In_ LPARAM Result
    );

bool
UpdateOpenPending (
    void
    );

bool
UpdateCheckEnabled (
    void
    );

void
UpdateSetCheckEnabled (
    _In_ bool Enabled
    );


//
// The popup's hotkey (Hotkey.cpp, and registered by MainWindow.cpp).
//

/*!

    @brief A hotkey, MOD_* modifiers (not MOD_NOREPEAT) and a virtual key.

*/
struct Hotkey
{
    UINT Modifiers;
    UINT Key;
};

//
// Win+Shift+E, which was free when we picked it and doesn't clash with Command
// Palette (Win+Alt+Space), PowerToys Run (Alt+Space) or VS (Ctrl+Alt+E).
//
constexpr Hotkey k_DefaultHotkey = { MOD_WIN | MOD_SHIFT, 'E' };

std::vector<UINT>
HotkeyKeys (
    void
    );

std::wstring
HotkeyKeyName (
    _In_ UINT Key
    );

std::wstring
HotkeyFormat (
    _In_ const Hotkey& Key
    );

bool
HotkeyParse (
    _In_z_ PCWSTR Text,
    _Out_ Hotkey* Key
    );

Hotkey
HotkeyLoad (
    void
    );

void
HotkeySave (
    _In_ const Hotkey& Key
    );

Hotkey
MainWindowHotkey (
    void
    );

bool
MainWindowHotkeyRegistered (
    void
    );

bool
MainWindowSetHotkey (
    _In_ const Hotkey& Key
    );


//
// The settings dialog (SettingsDialog.cpp).
//

void
SettingsDialogShow (
    _In_opt_ HWND Owner
    );


//
// Starting at sign-in (Startup.cpp).
//

bool
StartupIsEnabled (
    void
    );

bool
StartupSetEnabled (
    _In_ bool Enabled
    );


//
// The tray icon (Tray.cpp).
//

bool
TrayAdd (
    _In_ HWND Owner,
    _In_z_ PCWSTR Tip
    );

void
TraySetTip (
    _In_z_ PCWSTR Tip
    );

void
TrayNotify (
    _In_z_ PCWSTR Title,
    _In_z_ PCWSTR Text
    );

void
TrayRemove (
    void
    );

UINT
TrayCreatedMessage (
    void
    );

UINT
TrayShowMenu (
    _In_ HWND Owner,
    _In_ POINT At,
    _In_ bool StartAtSignIn,
    _In_z_ PCWSTR HotkeyText
    );


//
// Recent lookups, kept by the popup (Popup.cpp).
//

bool
PopupHasRecent (
    void
    );

void
PopupClearRecent (
    void
    );


//
// The main window (MainWindow.cpp).
//

bool
MainWindowRegister (
    _In_ HINSTANCE Instance
    );

HWND
MainWindowCreate (
    _In_ HINSTANCE Instance,
    _In_z_ PCWSTR InitialQuery,
    _In_ int ShowCommand
    );

bool
MainWindowPreTranslate (
    _In_ const MSG* Msg
    );

void
MainWindowOpen (
    _In_z_ PCWSTR Query,
    _In_ bool HasSelection,
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    );

bool
MainWindowForward (
    _In_z_ PCWSTR Query,
    _In_ bool Popup
    );

bool
MainWindowExitRunning (
    void
    );

void
MainWindowRefresh (
    void
    );

void
SettingsDialogOnCodesReloaded (
    void
    );

//
// The user's own codes (UserCodesDialog.cpp).
//

void
UserCodesDialogShow (
    _In_opt_ HWND Owner
    );

void
UserCodesReload (
    void
    );

void
UserCodesWatchStart (
    _In_opt_ HWND Owner
    );

void
UserCodesWatchStop (
    void
    );


//
// The launcher popup (Popup.cpp).
//

bool
PopupRegister (
    _In_ HINSTANCE Instance
    );

bool
PopupCreate (
    _In_ HINSTANCE Instance
    );

void
PopupShow (
    _In_opt_z_ PCWSTR Query
    );

void
PopupToggle (
    void
    );


//
// Sent to the details pane's parent as WM_NOTIFY when a link to another code
// is followed, with a DetailsLinkNotify.
//
constexpr UINT k_DetailsFollowLink = 1;

/*!

    @brief A followed link, and the code it goes to.

*/
struct DetailsLinkNotify
{
    NMHDR Header;
    CodeKind Kind;
    uint32_t Value;

    //
    // Its name, or an empty string. Only valid during the notification.
    //
    PCWSTR Name;
};


/*!

    @brief The fonts a window draws in, all made for one DPI.

*/
struct UiFonts
{
    //
    // Running text and links, in the system message font.
    //
    HFONT Message;

    //
    // Kinds and section headings, smaller and semibold.
    //
    HFONT Label;

    //
    // Names in lists, semibold.
    //
    HFONT Name;

    //
    // Notes, hints and the source line.
    //
    HFONT Small;

    //
    // The name at the top of the details pane.
    //
    HFONT Heading;

    //
    // The popup's search box.
    //
    HFONT Query;

    //
    // Values.
    //
    HFONT Mono;

    //
    // Segoe Fluent Icons glyphs.
    //
    HFONT Icon;
};


//
// The details pane (DetailsPane.cpp).
//

bool
DetailsPaneRegister (
    _In_ HINSTANCE Instance
    );

HWND
DetailsPaneCreate (
    _In_ HWND Parent,
    _In_ int Id,
    _In_ HINSTANCE Instance
    );

void
DetailsPaneSetFonts (
    _In_ const UiFonts& Fonts,
    _In_ UINT Dpi
    );

void
DetailsPaneShow (
    _In_opt_ const CodeMatch* Match,
    _In_opt_z_ PCWSTR Hint
    );


//
// Results (Results.cpp).
//

/*!

    @brief One result row.

*/
struct ResultRow
{
    CodeKind Kind;
    uint32_t Value;

    //
    // The name as listed, so a symbolic name, a description like "WM_USER + 1",
    // or empty for an unnamed code.
    //
    std::wstring Name;

    //
    // For a row a name search found, the part of Name that matched, to
    // highlight. MatchLength is zero for any other row.
    //
    size_t MatchStart;
    size_t MatchLength;
};

/*!

    @brief Everything a query found.

*/
struct ResultSet
{
    //
    // Every match for the query's numbers, then every name found for its
    // names.
    //
    std::vector<ResultRow> Rows;

    //
    // How many of the rows, from the start, came from numbers.
    //
    size_t NumberRows;

    //
    // False when the query held nothing to look up at all.
    //
    bool HasQuery;
};

ResultSet
BuildResults (
    _In_z_ PCWSTR Query
    );

std::wstring
ResultCountText (
    _In_ const ResultSet& Results
    );

bool
FindMatch (
    _In_ CodeKind Kind,
    _In_ uint32_t Value,
    _In_ const std::wstring& Name,
    _Out_ CodeMatch* Match
    );


PCWSTR
KindKey (
    _In_ CodeKind Kind
    );

bool
KindFromKey (
    _In_ const std::wstring& Key,
    _Out_ CodeKind* Kind
    );


//
// Notes on codes (Notes.cpp).
//

std::wstring
NoteGet (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    );

void
NoteSet (
    _In_ CodeKind Kind,
    _In_ uint32_t Value,
    _In_ const std::wstring& Text
    );


//
// Fonts and drawing (Draw.cpp).
//

bool
FontsCreate (
    _In_ UINT Dpi,
    _Out_ UiFonts* Fonts
    );

void
FontsDelete (
    _Inout_ UiFonts* Fonts
    );

int
FontLineHeight (
    _In_opt_ HDC Dc,
    _In_ HFONT Font
    );

int
TextWidth (
    _In_ HDC Dc,
    _In_ HFONT Font,
    _In_ const std::wstring& Text
    );

void
FillRounded (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour,
    _In_ int Radius
    );

void
FrameRounded (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour,
    _In_ int Radius
    );

void
FillDot (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour
    );

void
FillSolid (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour
    );

bool
CopyTextToClipboard (
    _In_ HWND Owner,
    _In_ const std::wstring& Text
    );

std::wstring
DescribeValue (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    );


//
// The theme (Theme.cpp).
//

void
ThemeInitialise (
    void
    );

void
ThemeRefresh (
    void
    );

bool
ThemeIsDark (
    void
    );

//
// Background's the window, Control is content and input, Muted and Faint are
// secondary text, Accent's links, Selection the selected row and Match a hit.
//
COLORREF ThemeTextColour (void);
COLORREF ThemeMutedColour (void);
COLORREF ThemeFaintColour (void);
COLORREF ThemeAccentColour (void);
COLORREF ThemeSelectionColour (void);
COLORREF ThemeMatchColour (void);
COLORREF ThemeBackgroundColour (void);
COLORREF ThemeControlColour (void);
HBRUSH ThemeBackgroundBrush (void);
HBRUSH ThemeControlBrush (void);

COLORREF
ThemeKindColour (
    _In_ CodeKind Kind
    );

INT_PTR
ThemeOnCtlColor (
    _In_ WPARAM WParam,
    _In_ bool Content
    );

void
ThemeApplyToWindow (
    _In_ HWND Window
    );

void
ThemeApplyToPopup (
    _In_ HWND Popup
    );
