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

#include "Json.hpp"
#include "Resource.h"


//
// The notification the tray icon sends its owner (Tray.cpp).
//
constexpr UINT k_TrayMessage = WM_APP + 1;


//
// The popup's hotkey (Hotkey.cpp, and registered by MainWindow.cpp).
//

/*!

    @brief A hotkey: MOD_* modifiers, not including MOD_NOREPEAT, and a
           virtual key.

*/
struct Hotkey
{
    UINT Modifiers;
    UINT Key;
};

//
// Win+Shift+E: free on the dev box when chosen, and clear of Command Palette
// (Win+Alt+Space), PowerToys Run (Alt+Space) and Visual Studio's Exception
// Settings (Ctrl+Alt+E).
//
constexpr Hotkey k_DefaultHotkey = { MOD_WIN | MOD_SHIFT, 'E' };

/*!

    @brief Every key a hotkey can use, in the order the settings dialog lists
           them: letters, digits, F1 to F24, then a few named keys.

    @return The virtual keys.

*/
std::vector<UINT>
HotkeyKeys (
    void
    );

/*!

    @brief How a hotkey's key is written, as in "E", "F5" or "Space".

    @param[in] Key - The virtual key.

    @return The name, or an empty string for a key hotkeys don't use.

*/
std::wstring
HotkeyKeyName (
    _In_ UINT Key
    );

/*!

    @brief How a hotkey is written, as in "Win+Shift+E".

    @param[in] Key - The hotkey.

    @return The text.

*/
std::wstring
HotkeyFormat (
    _In_ const Hotkey& Key
    );

/*!

    @brief Reads a hotkey written as HotkeyFormat writes it. Case and spaces
           around the + signs are ignored.

    @param[in] Text - The text.

    @param[out] Key - Receives the hotkey, or zeroes on failure.

    @return True if it's a hotkey, with at least one modifier.

*/
bool
HotkeyParse (
    _In_z_ PCWSTR Text,
    _Out_ Hotkey* Key
    );

/*!

    @brief The hotkey in settings.json, or the default if there isn't one.

*/
Hotkey
HotkeyLoad (
    void
    );

/*!

    @brief Saves the hotkey to settings.json.

    @param[in] Key - The hotkey.

*/
void
HotkeySave (
    _In_ const Hotkey& Key
    );

/*!

    @brief The popup's hotkey, whether or not it's registered.

*/
Hotkey
MainWindowHotkey (
    void
    );

/*!

    @brief Whether the popup's hotkey is registered, and so works.

*/
bool
MainWindowHotkeyRegistered (
    void
    );

/*!

    @brief Changes the popup's hotkey, and saves it.

    @param[in] Key - The new hotkey.

    @return True if it registered. If not, because Windows or another program
            has it, the old hotkey stays.

*/
bool
MainWindowSetHotkey (
    _In_ const Hotkey& Key
    );


//
// Settings (Settings.cpp).
//

/*!

    @brief Reads settings.json, once. Everything else calls it as needed.

*/
void
SettingsLoad (
    void
    );

/*!

    @brief Writes settings.json.

    @return True if it was written. False if there's nowhere to write it, or
            the file on disk won't parse and is being left for fixing.

*/
bool
SettingsSave (
    void
    );

/*!

    @brief Where settings.json is, or an empty string if nowhere will do.

*/
std::wstring
SettingsPath (
    void
    );

/*!

    @brief Whether settings.json is there but won't parse, in which case
           nothing is saved over it.

*/
bool
SettingsIsUnreadable (
    void
    );

/*!

    @brief The settings, for lists and objects the typed accessors don't
           cover.

*/
JsonValue&
SettingsRoot (
    void
    );

//
// Typed access by dotted path, as in "window.width". A missing value, or one
// of the wrong type, reads as the default.
//
int SettingsGetInt(_In_ PCWSTR Path, _In_ int Default);
void SettingsSetInt(_In_ PCWSTR Path, _In_ int Value);
bool SettingsGetBool(_In_ PCWSTR Path, _In_ bool Default);
void SettingsSetBool(_In_ PCWSTR Path, _In_ bool Value);
std::wstring SettingsGetString(_In_ PCWSTR Path, _In_ PCWSTR Default);
void SettingsSetString(_In_ PCWSTR Path, _In_ PCWSTR Value);

/*!

    @brief Shows the settings dialog, or brings it forward if it's open.

    @param[in] Owner - The window that owns it, or nullptr.

*/
void
SettingsDialogShow (
    _In_opt_ HWND Owner
    );


//
// Starting at sign-in (Startup.cpp).
//

/*!

    @brief Whether this copy of WinCode starts when the user signs in.

    @return True only if the Run key names this executable and Task Manager
            hasn't switched it off.

*/
bool
StartupIsEnabled (
    void
    );

/*!

    @brief Turns starting at sign-in on or off.

    @param[in] Enabled - True to start at sign-in.

    @return True on success.

*/
bool
StartupSetEnabled (
    _In_ bool Enabled
    );


//
// The tray icon (Tray.cpp).
//

/*!

    @brief Adds the tray icon, or adds it again after Explorer restarts.

    @param[in] Owner - The window that receives k_TrayMessage.

    @param[in] Tip - Its tooltip.

    @return True if the icon is there.

*/
bool
TrayAdd (
    _In_ HWND Owner,
    _In_z_ PCWSTR Tip
    );

/*!

    @brief Changes the tray icon's tooltip.

    @param[in] Tip - The tooltip.

*/
void
TraySetTip (
    _In_z_ PCWSTR Tip
    );

/*!

    @brief Shows a notification from the tray icon.

    @param[in] Title - Its title.

    @param[in] Text - Its text.

*/
void
TrayNotify (
    _In_z_ PCWSTR Title,
    _In_z_ PCWSTR Text
    );

/*!

    @brief Removes the tray icon.

*/
void
TrayRemove (
    void
    );

/*!

    @brief The message Explorer broadcasts when the taskbar is created, after
           it starts or restarts.

*/
UINT
TrayCreatedMessage (
    void
    );

/*!

    @brief Shows the tray icon's menu and waits for a choice.

    @param[in] Owner - The icon's owner.

    @param[in] At - Where, in screen coordinates.

    @param[in] StartAtSignIn - Whether to tick "Start when I sign in".

    @param[in] HotkeyText - The hotkey, to show beside "Look up", or an empty
                            string when there isn't one.

    @return The IDM_TRAY_* chosen, or 0 for none.

*/
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

/*!

    @brief Whether there are any recent lookups.

*/
bool
PopupHasRecent (
    void
    );

/*!

    @brief Forgets every recent lookup, and saves that.

*/
void
PopupClearRecent (
    void
    );


//
// The main window (MainWindow.cpp).
//

/*!

    @brief Registers the main window's class.

    @param[in] Instance - The module instance.

    @return True on success.

*/
bool
MainWindowRegister (
    _In_ HINSTANCE Instance
    );

/*!

    @brief Creates the main window, sized and centred for its monitor, and
           registers the popup's hotkey.

    @param[in] Instance - The module instance.

    @param[in] InitialQuery - Something to look up straight away, or an empty
                              string for none.

    @param[in] ShowCommand - How to show it, as passed to wWinMain, or SW_HIDE
                             to start with only the popup.

    @return The window, or nullptr if it couldn't be created.

*/
HWND
MainWindowCreate (
    _In_ HINSTANCE Instance,
    _In_z_ PCWSTR InitialQuery,
    _In_ int ShowCommand
    );

/*!

    @brief Handles the main window's keys that work wherever the focus is, such
           as Alt+Left for Back. Call for every message, before IsDialogMessage.

    @param[in] Msg - The message.

    @return True if the message was handled, and needs nothing more.

*/
bool
MainWindowPreTranslate (
    _In_ const MSG* Msg
    );

/*!

    @brief Brings the main window to the front, looking something up in it.

    @details What the window showed before goes onto its Back history, so
             nothing is lost by opening something from the popup.

    @param[in] Query - What to look up, or an empty string to leave the window
                       as it is.

    @param[in] HasSelection - True to select a particular result.

    @param[in] Kind - The result's kind, if HasSelection.

    @param[in] Value - Its value, if HasSelection.

*/
void
MainWindowOpen (
    _In_z_ PCWSTR Query,
    _In_ bool HasSelection,
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    );

/*!

    @brief Hands a command line to a WinCodeUI that's already running, so there's
           only ever one, and one owner of the hotkey.

    @param[in] Query - What to look up, or an empty string.

    @param[in] Popup - True to show it in the popup, false in the main window.

    @return True if a running WinCodeUI took it.

*/
bool
MainWindowForward (
    _In_z_ PCWSTR Query,
    _In_ bool Popup
    );

/*!

    @brief Tells a WinCodeUI that's already running to quit, and waits until
           it has, as the installer needs before it replaces the files.

    @return True if none was running, or the running one has quit.

*/
bool
MainWindowExitRunning (
    void
    );


//
// The launcher popup (Popup.cpp).
//

/*!

    @brief Registers the popup's window class.

    @param[in] Instance - The module instance.

    @return True on success.

*/
bool
PopupRegister (
    _In_ HINSTANCE Instance
    );

/*!

    @brief Creates the popup, hidden. There's only ever one.

    @param[in] Instance - The module instance.

    @return True on success.

*/
bool
PopupCreate (
    _In_ HINSTANCE Instance
    );

/*!

    @brief Shows the popup on the monitor with the focused window, and gives it
           the keyboard.

    @param[in] Query - What to look up, or nullptr to take it from the
                       clipboard when that holds a code or a name.

*/
void
PopupShow (
    _In_opt_z_ PCWSTR Query
    );

/*!

    @brief What the hotkey does: shows the popup, or hides it if it's in front.

*/
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

    @brief A followed link: the code it goes to.

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
    // Running text and links: the system message font.
    //
    HFONT Message;

    //
    // Kinds and section headings: smaller, and semibold.
    //
    HFONT Label;

    //
    // Names in lists: semibold.
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

/*!

    @brief Registers the details pane's window classes.

    @param[in] Instance - The module instance.

    @return True on success.

*/
bool
DetailsPaneRegister (
    _In_ HINSTANCE Instance
    );

/*!

    @brief Creates the details pane. There's only ever one.

    @param[in] Parent - Its parent, which receives its notifications.

    @param[in] Id - Its control ID.

    @param[in] Instance - The module instance.

    @return The pane, or nullptr if it couldn't be created.

*/
HWND
DetailsPaneCreate (
    _In_ HWND Parent,
    _In_ int Id,
    _In_ HINSTANCE Instance
    );

/*!

    @brief Gives the pane its fonts, after it's created and whenever the DPI
           changes, and lays it out again.

    @param[in] Fonts - The fonts. The caller owns them, and keeps them alive
                       while the pane uses them.

    @param[in] Dpi - The DPI they were made for.

*/
void
DetailsPaneSetFonts (
    _In_ const UiFonts& Fonts,
    _In_ UINT Dpi
    );

/*!

    @brief Shows a match, or a hint when there's none.

    @param[in] Match - The match, or nullptr.

    @param[in] Hint - What to show instead of a match, or nullptr for nothing.

*/
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
    // The name as listed: a symbolic name, a description such as "WM_USER + 1",
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

/*!

    @brief Looks a query up.

    @param[in] Query - The query, typed or pasted.

    @return What it found.

*/
ResultSet
BuildResults (
    _In_z_ PCWSTR Query
    );

/*!

    @brief How many results there are, in words, as in "3 matches" or
           "42 names".

    @param[in] Results - The results.

    @return The text, or an empty string when there was no query.

*/
std::wstring
ResultCountText (
    _In_ const ResultSet& Results
    );

/*!

    @brief Looks up everything known about one code.

    @param[in] Kind - The kind of code.

    @param[in] Value - Its value.

    @param[out] Match - Receives the match.

    @return True if the value is that kind of code.

*/
bool
FindMatch (
    _In_ CodeKind Kind,
    _In_ uint32_t Value,
    _Out_ CodeMatch* Match
    );


//
// Fonts and drawing (Draw.cpp).
//

/*!

    @brief Makes the fonts for a DPI.

    @param[in] Dpi - The DPI.

    @param[out] Fonts - Receives the fonts, which FontsDelete deletes.

    @return True if every font was made. On failure, none are left.

*/
bool
FontsCreate (
    _In_ UINT Dpi,
    _Out_ UiFonts* Fonts
    );

/*!

    @brief Deletes the fonts FontsCreate made.

    @param[in,out] Fonts - The fonts, which are set to nullptr.

*/
void
FontsDelete (
    _Inout_ UiFonts* Fonts
    );

/*!

    @brief The height of a font's lines.

    @param[in] Dc - A device context to measure with, or nullptr for the
                    screen's. Its font is left as it was.

    @param[in] Font - The font.

    @return The height in pixels.

*/
int
FontLineHeight (
    _In_opt_ HDC Dc,
    _In_ HFONT Font
    );

/*!

    @brief The width of a line of text, as DrawText draws it.

    @param[in] Dc - A device context to measure with. Its font is left as it
                    was.

    @param[in] Font - The font.

    @param[in] Text - The text.

    @return The width in pixels.

*/
int
TextWidth (
    _In_ HDC Dc,
    _In_ HFONT Font,
    _In_ const std::wstring& Text
    );

/*!

    @brief Fills a rectangle with rounded corners.

    @param[in] Dc - The device context.

    @param[in] Rect - The rectangle.

    @param[in] Colour - The fill.

    @param[in] Radius - The width and height of the ellipse that rounds each
                        corner, so twice the corners' radius.

*/
void
FillRounded (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour,
    _In_ int Radius
    );

/*!

    @brief Outlines a rectangle with rounded corners, one pixel wide.

    @param[in] Dc - The device context.

    @param[in] Rect - The rectangle.

    @param[in] Colour - The line.

    @param[in] Radius - As for FillRounded.

*/
void
FrameRounded (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour,
    _In_ int Radius
    );

/*!

    @brief Fills a circle, as a kind's dot.

    @param[in] Dc - The device context.

    @param[in] Rect - The circle's bounds.

    @param[in] Colour - The fill.

*/
void
FillDot (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour
    );

/*!

    @brief Fills a rectangle with a colour.

    @param[in] Dc - The device context.

    @param[in] Rect - The rectangle.

    @param[in] Colour - The fill.

*/
void
FillSolid (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour
    );

/*!

    @brief Puts text on the clipboard.

    @param[in] Owner - The window that owns the clipboard while it's open.

    @param[in] Text - The text.

    @return True if it was copied.

*/
bool
CopyTextToClipboard (
    _In_ HWND Owner,
    _In_ const std::wstring& Text
    );

/*!

    @brief A value written every way it's likely to be asked for.

    @param[in] Kind - The kind of code, which decides which form comes first.

    @param[in] Value - The value.

    @return For example "5 · 0x00000005", or "0xC0000022 · 3221225506 · signed
            -1073741790".

*/
std::wstring
DescribeValue (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    );


//
// The theme (Theme.cpp).
//

/*!

    @brief Sets up dark mode support for the process and reads the Windows
           setting.

    @details Call once, before any window is created.

*/
void
ThemeInitialise (
    void
    );

/*!

    @brief Reads the Windows light/dark setting again. Call when Windows
           broadcasts WM_SETTINGCHANGE with "ImmersiveColorSet".

*/
void
ThemeRefresh (
    void
    );

/*!

    @brief Whether WinCode is currently dark.

*/
bool
ThemeIsDark (
    void
    );

//
// The palette. Background is the window's ground, Control the ground for
// content and input, Muted and Faint for secondary text, Accent for links,
// Selection for the selected row and Match behind the text a search matched.
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

/*!

    @brief The colour a kind of code is labelled in.

    @param[in] Kind - The kind.

    @return The colour, for the current theme.

*/
COLORREF
ThemeKindColour (
    _In_ CodeKind Kind
    );

/*!

    @brief Handles a WM_CTLCOLOR* message.

    @param[in] WParam - The device context, as the message passes it.

    @param[in] Content - True for a control holding content or input, which
                         gets the content ground in either theme.

    @return The brush to return from the message, or 0 to let the default
            handling have it.

*/
INT_PTR
ThemeOnCtlColor (
    _In_ WPARAM WParam,
    _In_ bool Content
    );

/*!

    @brief Applies the theme to a top-level window, its title bar and every
           control in it.

    @param[in] Window - The window.

*/
void
ThemeApplyToWindow (
    _In_ HWND Window
    );

/*!

    @brief Applies the theme to a popup, such as a tooltip, which isn't a child
           and so isn't reached by ThemeApplyToWindow.

    @param[in] Popup - The popup.

*/
void
ThemeApplyToPopup (
    _In_ HWND Popup
    );
