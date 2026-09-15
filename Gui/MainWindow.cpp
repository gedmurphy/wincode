/*!

    @file Gui/MainWindow.cpp

    @brief The main window: a search box, the results, and the details of the
           selected one.

    @details Plain Win32, laid out by hand on WM_SIZE in units scaled to the
             window's DPI. Everything shown comes from the core; this file only
             arranges it.

             The results list is a virtual ListView (LVS_OWNERDATA), which asks
             for each row as it paints. A short name search that matches
             thousands of names therefore costs nothing to show. Its rows are
             owner drawn in two lines, the kind above the name, as in the
             mockups, and the ListView still supplies each row's text to screen
             readers.

             The details pane (DetailsPane.cpp) shows the selected row. Its
             links to related codes come back here: following one looks the
             code up as if it had been typed, and remembers where it came from,
             so Back and Forward work as in a browser. Only links are
             remembered. Typing replaces the results without adding to the
             history, so the history stays a trail of links followed.

             The window is also what keeps WinCode running. It owns the popup's
             hotkey and the tray icon, and closing it only hides it, so the
             hotkey keeps working; Exit on the tray icon's menu is what quits.
             A second WinCodeUI finds it by its class, hands over its command
             line in a WM_COPYDATA, and quits, so there's only ever one owner
             of the hotkey. The installer uses the same route to ask it to
             quit before replacing its files.

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
#include <uxtheme.h>
#include <windowsx.h>

//
// initguid.h first, so oleacc.h defines the PROPID_ACC_* GUIDs here.
//
#include <initguid.h>
#include <oleacc.h>

#include <algorithm>
#include <format>


constexpr WCHAR k_MainWindowClass[] = L"WinCodeMainWindow";

//
// Layout, in 96 DPI units, scaled to the window's DPI when used.
//
static constexpr int k_Margin = 12;
static constexpr int k_ToolbarTop = 8;
static constexpr int k_ButtonWidth = 36;
static constexpr int k_ButtonHeight = 30;
static constexpr int k_EditHeight = 26;
static constexpr int k_Gap = 8;
static constexpr int k_ListWidth = 400;
static constexpr int k_CountHeight = 22;
static constexpr int k_DefaultWidth = 1040;
static constexpr int k_DefaultHeight = 640;
static constexpr int k_MinWidth = 640;
static constexpr int k_MinHeight = 400;

//
// A result row: where its text starts, after the selection marker, the space
// above and below its two lines, the kind's dot, and the selection's corners.
//
static constexpr int k_RowIndent = 18;
static constexpr int k_RowPadding = 6;
static constexpr int k_KindDot = 7;
static constexpr int k_RowRadius = 8;

//
// How many links followed Back remembers.
//
static constexpr size_t k_HistoryLimit = 100;

//
// The popup's hotkey's ID. MOD_NOREPEAT is added to whatever the hotkey is, so
// a held key doesn't toggle the popup over and over.
//
static constexpr int k_HotkeyId = 1;

//
// What a second WinCodeUI hands over in WM_COPYDATA: a query for the window,
// a query for the popup, or a request to quit. How long it waits for the first
// to have a window, and to answer.
//
static constexpr ULONG_PTR k_ForwardWindow = 0x57430001;
static constexpr ULONG_PTR k_ForwardPopup = 0x57430002;
static constexpr ULONG_PTR k_ForwardExit = 0x57430003;
static constexpr int k_ForwardAttempts = 20;
static constexpr DWORD k_ForwardWaitMs = 100;
static constexpr UINT k_ForwardTimeoutMs = 5000;

//
// How long --exit waits for the running WinCodeUI to go, and the message it
// posts itself to quit once the WM_COPYDATA asking it to has been answered.
//
static constexpr DWORD k_ExitWaitMs = 5000;
static constexpr UINT k_ExitMessage = WM_APP + 2;

//
// Where the window's state is kept in settings.json.
//
static constexpr WCHAR k_WindowX[] = L"window.x";
static constexpr WCHAR k_WindowY[] = L"window.y";
static constexpr WCHAR k_WindowWidth[] = L"window.width";
static constexpr WCHAR k_WindowHeight[] = L"window.height";
static constexpr WCHAR k_WindowMaximised[] = L"window.maximised";
static constexpr WCHAR k_WindowOnTop[] = L"window.alwaysOnTop";
static constexpr WCHAR k_TrayHintShown[] = L"trayHintShown";

//
// Segoe Fluent Icons code points. Segoe MDL2 Assets, the Windows 10 icon font,
// has the same ones.
//
static constexpr WCHAR k_GlyphBack[] = L"\xE72B";
static constexpr WCHAR k_GlyphForward[] = L"\xE72A";
static constexpr WCHAR k_GlyphPin[] = L"\xE718";
static constexpr WCHAR k_GlyphSettings[] = L"\xE713";

static constexpr WCHAR k_EmptyHint[] =
    L"Type a code or a name, or paste text from a log.\r\n"
    L"\r\n"
    L"For example: 0xC0000022, -2147024891, 80070005, ERROR_ACCESS_DENIED or ACCESS_DENIED.";


/*!

    @brief A place in the history: what was searched for and which row was
           selected.

*/
struct HistoryEntry
{
    std::wstring Query;

    //
    // The selected row, if there was one.
    //
    bool HasSelection;
    CodeKind Kind;
    uint32_t Value;

    //
    // What the Back and Forward tooltips call it: the row's name, or the query.
    //
    std::wstring Label;
};


static HWND g_MainWindow;
static HWND g_BackButton;
static HWND g_ForwardButton;
static HWND g_SearchEdit;
static HWND g_PinButton;
static HWND g_SettingsButton;
static HWND g_CountLabel;
static HWND g_ResultsList;
static HWND g_DetailsPane;
static HWND g_Tooltip;

static UiFonts g_Fonts;
static int g_KindHeight;
static int g_NameHeight;
static int g_RowHeight;
static UINT g_Dpi = USER_DEFAULT_SCREEN_DPI;

static std::vector<ResultRow> g_Rows;

static std::vector<HistoryEntry> g_Back;
static std::vector<HistoryEntry> g_Forward;

//
// Set while the search box's text is set in code, which runs the query itself
// rather than through EN_CHANGE.
//
static bool g_SettingQuery;

static Hotkey g_Hotkey;
static bool g_HotkeyRegistered;

//
// For a window that starts hidden, whether its first showing should be
// maximised, as it was when WinCode last saved its state.
//
static bool g_ShowMaximised;

static IAccPropServices* g_AccProps;


/*!

    @brief Scales a length from 96 DPI units to the window's DPI.

    @param[in] Value - The length at 96 DPI.

    @return The length in pixels.

*/
static
int
Scale (
    _In_ int Value
    )
{
    return MulDiv(Value, SCAST(int)(g_Dpi), USER_DEFAULT_SCREEN_DPI);
}


/*!

    @brief Creates the fonts for the window's current DPI, replacing any made
           for an earlier one, and works out the height of a result row.

*/
static
void
CreateFonts (
    void
    )
{
    UiFonts fonts;

    //
    // The old fonts stay if the new ones can't be made, rather than leave the
    // window with none.
    //
    if (!FontsCreate(g_Dpi, &fonts))
    {
        return;
    }

    FontsDelete(&g_Fonts);
    g_Fonts = fonts;

    g_KindHeight = FontLineHeight(nullptr, g_Fonts.Label);
    g_NameHeight = FontLineHeight(nullptr, g_Fonts.Name);
    g_RowHeight = (2 * Scale(k_RowPadding)) + g_KindHeight + g_NameHeight;
}


/*!

    @brief Gives every control its font.

*/
static
void
ApplyFonts (
    void
    )
{
    HWND textControls[] = { g_SearchEdit, g_CountLabel, g_Tooltip };
    HWND iconControls[] = { g_BackButton, g_ForwardButton, g_PinButton, g_SettingsButton };

    for (HWND control : textControls)
    {
        SendMessageW(control, WM_SETFONT, RCAST(WPARAM)(g_Fonts.Message), TRUE);
    }

    for (HWND control : iconControls)
    {
        SendMessageW(control, WM_SETFONT, RCAST(WPARAM)(g_Fonts.Icon), TRUE);
    }

    //
    // An owner drawn list measures its rows again when its font changes, which
    // is how a DPI change reaches the row height.
    //
    SendMessageW(g_ResultsList, WM_SETFONT, RCAST(WPARAM)(g_Fonts.Message), TRUE);

    DetailsPaneSetFonts(g_Fonts, g_Dpi);
}


/*!

    @brief Shows a row's details, or the hint when there's no row.

    @param[in] Index - The row, or -1 for none.

*/
static
void
ShowDetails (
    _In_ int Index
    )
{
    CodeMatch match;
    const ResultRow* row;

    if ((Index < 0) || (SCAST(size_t)(Index) >= g_Rows.size()))
    {
        DetailsPaneShow(nullptr, (GetWindowTextLengthW(g_SearchEdit) == 0) ? k_EmptyHint : nullptr);
        return;
    }

    row = &g_Rows[SCAST(size_t)(Index)];

    if (FindMatch(row->Kind, row->Value, &match))
    {
        DetailsPaneShow(&match, nullptr);
        return;
    }

    DetailsPaneShow(nullptr, nullptr);
}


/*!

    @brief Sizes the results list's one column to the list, so rows never
           scroll sideways.

*/
static
void
FitListColumn (
    void
    )
{
    RECT client;

    GetClientRect(g_ResultsList, &client);
    ListView_SetColumnWidth(g_ResultsList, 0, std::max(SCAST(int)(client.right), Scale(60)));
}


/*!

    @brief The search box's text.

    @return The text.

*/
static
std::wstring
QueryText (
    void
    )
{
    std::wstring query;
    int length;

    length = GetWindowTextLengthW(g_SearchEdit);
    query.resize(SCAST(size_t)(length) + 1);
    GetWindowTextW(g_SearchEdit, query.data(), length + 1);
    query.resize(SCAST(size_t)(length));

    return query;
}


/*!

    @brief Selects a row, scrolls it into view, and shows its details.

    @param[in] Index - The row.

*/
static
void
SelectRow (
    _In_ int Index
    )
{
    //
    // Clearing the selection first means selecting the row always counts as a
    // change, and so always shows its details.
    //
    ListView_SetItemState(g_ResultsList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(g_ResultsList, Index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(g_ResultsList, Index, FALSE);
}


/*!

    @brief Looks up whatever is in the search box, and shows the results.

*/
static
void
RunQuery (
    void
    )
{
    ResultSet results;
    std::wstring count;

    results = BuildResults(QueryText().c_str());
    count = ResultCountText(results);
    g_Rows = std::move(results.Rows);

    SetWindowTextW(g_CountLabel, count.c_str());

    ListView_SetItemState(g_ResultsList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemCountEx(g_ResultsList, SCAST(int)(g_Rows.size()), 0);

    //
    // The count decides whether there's a scroll bar, and so the column's width.
    //
    FitListColumn();

    if (g_Rows.empty())
    {
        ShowDetails(-1);
        return;
    }

    SelectRow(0);
}


/*!

    @brief Moves the selection in the results list, as Up and Down do in the
           search box.

    @param[in] Delta - How far to move: 1 for down, -1 for up.

*/
static
void
MoveSelection (
    _In_ int Delta
    )
{
    int count;
    int current;

    count = SCAST(int)(g_Rows.size());
    if (count == 0)
    {
        return;
    }

    current = ListView_GetNextItem(g_ResultsList, -1, LVNI_SELECTED);
    SelectRow(std::clamp((current < 0) ? 0 : (current + Delta), 0, count - 1));
}


/*!

    @brief Where the window is now, as a history entry.

    @return The entry.

*/
static
HistoryEntry
CurrentPlace (
    void
    )
{
    HistoryEntry entry = {};
    int selected;

    entry.Query = QueryText();
    entry.Label = entry.Query;

    selected = ListView_GetNextItem(g_ResultsList, -1, LVNI_SELECTED);
    if ((selected >= 0) && (SCAST(size_t)(selected) < g_Rows.size()))
    {
        entry.HasSelection = true;
        entry.Kind = g_Rows[SCAST(size_t)(selected)].Kind;
        entry.Value = g_Rows[SCAST(size_t)(selected)].Value;

        if (!g_Rows[SCAST(size_t)(selected)].Name.empty())
        {
            entry.Label = g_Rows[SCAST(size_t)(selected)].Name;
        }
    }

    return entry;
}


/*!

    @brief Sets a history button's tooltip.

    @param[in] Button - The button.

    @param[in] Text - The tip.

*/
static
void
SetTooltip (
    _In_ HWND Button,
    _In_ const std::wstring& Text
    )
{
    TOOLINFOW tool = {};

    tool.cbSize = sizeof(tool);
    tool.hwnd = g_MainWindow;
    tool.uId = RCAST(UINT_PTR)(Button);
    tool.lpszText = CCAST(PWSTR)(Text.c_str());

    //
    // The tooltip keeps its own copy of the text.
    //
    SendMessageW(g_Tooltip, TTM_UPDATETIPTEXTW, 0, RCAST(LPARAM)(&tool));
}


/*!

    @brief Enables Back and Forward to match the history, and names where each
           goes in its tooltip.

*/
static
void
UpdateHistoryButtons (
    void
    )
{
    HWND focus;

    //
    // A button that's disabled while it has the focus leaves the keyboard
    // nowhere, so the focus goes back to the search box first.
    //
    focus = GetFocus();
    if (((focus == g_BackButton) && g_Back.empty()) || ((focus == g_ForwardButton) && g_Forward.empty()))
    {
        SetFocus(g_SearchEdit);
    }

    EnableWindow(g_BackButton, !g_Back.empty());
    EnableWindow(g_ForwardButton, !g_Forward.empty());

    SetTooltip(g_BackButton, g_Back.empty() ? std::wstring(L"Back") : (L"Back to " + g_Back.back().Label));
    SetTooltip(g_ForwardButton, g_Forward.empty() ? std::wstring(L"Forward") : (L"Forward to " + g_Forward.back().Label));
}


/*!

    @brief Remembers where the window is, for Back, before going somewhere
           new.

*/
static
void
PushHistory (
    void
    )
{
    g_Back.push_back(CurrentPlace());
    if (g_Back.size() > k_HistoryLimit)
    {
        g_Back.erase(g_Back.begin());
    }

    g_Forward.clear();
}


/*!

    @brief Goes to a place: searches for its query, and selects its row.

    @param[in] Place - Where to go.

*/
static
void
GoTo (
    _In_ const HistoryEntry& Place
    )
{
    g_SettingQuery = true;
    SetWindowTextW(g_SearchEdit, Place.Query.c_str());
    g_SettingQuery = false;

    SendMessageW(g_SearchEdit, EM_SETSEL, Place.Query.size(), Place.Query.size());
    RunQuery();

    if (!Place.HasSelection)
    {
        return;
    }

    for (size_t i = 0; i < g_Rows.size(); i++)
    {
        if ((g_Rows[i].Kind == Place.Kind) && (g_Rows[i].Value == Place.Value))
        {
            if (i != 0)
            {
                SelectRow(SCAST(int)(i));
            }
            return;
        }
    }
}


/*!

    @brief Follows a link to a code: looks its value up, selects it among the
           results, and remembers where it came from.

    @param[in] Kind - The code's kind.

    @param[in] Value - Its value.

    @param[in] Name - Its name, or an empty string.

*/
static
void
FollowCode (
    _In_ CodeKind Kind,
    _In_ uint32_t Value,
    _In_z_ PCWSTR Name
    )
{
    HistoryEntry place = {};

    //
    // Looking the value up, rather than the name, lists every kind of code it
    // could be, as typing it would.
    //
    place.Query = FormatCodeValue(Kind, Value);
    place.HasSelection = true;
    place.Kind = Kind;
    place.Value = Value;
    place.Label = (Name[0] != L'\0') ? std::wstring(Name) : place.Query;

    PushHistory();
    GoTo(place);
    UpdateHistoryButtons();
}


/*!

    @brief Goes back, or forward, through the links followed.

    @param[in] Forward - True for forward, false for back.

*/
static
void
MoveThroughHistory (
    _In_ bool Forward
    )
{
    std::vector<HistoryEntry>& from = Forward ? g_Forward : g_Back;
    std::vector<HistoryEntry>& to = Forward ? g_Back : g_Forward;
    HistoryEntry place;

    if (from.empty())
    {
        return;
    }

    place = from.back();
    from.pop_back();
    to.push_back(CurrentPlace());

    GoTo(place);
    UpdateHistoryButtons();
}


/*!

    @brief Registers a hotkey as the popup's.

    @param[in] Key - The hotkey.

    @return True if it registered.

*/
static
bool
RegisterPopupHotkey (
    _In_ const Hotkey& Key
    )
{
    return RegisterHotKey(g_MainWindow, k_HotkeyId, Key.Modifiers | MOD_NOREPEAT, Key.Key) != FALSE;
}


/*!

    @brief The tray icon's tooltip, which names the hotkey when there is one.

    @return The tooltip.

*/
static
std::wstring
TrayTip (
    void
    )
{
    if (!g_HotkeyRegistered)
    {
        return L"WinCode";
    }

    return std::format(L"WinCode ({})", HotkeyFormat(g_Hotkey));
}


/*!

    @brief Saves the window's size, place and pin to settings.json.

*/
static
void
SaveWindowState (
    void
    )
{
    WINDOWPLACEMENT placement = {};

    //
    // The normal position, not the current one, so a window closed while
    // maximised still comes back to the size it had before.
    //
    placement.length = sizeof(placement);
    if (GetWindowPlacement(g_MainWindow, &placement))
    {
        SettingsSetInt(k_WindowX, placement.rcNormalPosition.left);
        SettingsSetInt(k_WindowY, placement.rcNormalPosition.top);
        SettingsSetInt(k_WindowWidth, placement.rcNormalPosition.right - placement.rcNormalPosition.left);
        SettingsSetInt(k_WindowHeight, placement.rcNormalPosition.bottom - placement.rcNormalPosition.top);
        SettingsSetBool(k_WindowMaximised,
                        (IsZoomed(g_MainWindow) != FALSE) || ((placement.flags & WPF_RESTORETOMAXIMIZED) != 0));
    }

    SettingsSetBool(k_WindowOnTop, SendMessageW(g_PinButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SettingsSave();
}


/*!

    @brief Puts the window where settings.json says it was.

    @param[in] Window - The window, still hidden.

    @param[out] Maximised - Receives whether it was maximised.

    @return True if a saved place was used, false to keep the default one.

*/
static
bool
RestoreWindowState (
    _In_ HWND Window,
    _Out_ bool* Maximised
    )
{
    WINDOWPLACEMENT placement = {};
    RECT rect;
    UINT dpi;
    int width;
    int height;

    *Maximised = false;

    width = SettingsGetInt(k_WindowWidth, 0);
    height = SettingsGetInt(k_WindowHeight, 0);
    if ((width <= 0) || (height <= 0))
    {
        return false;
    }

    dpi = GetDpiForWindow(Window);
    width = std::max(width, MulDiv(k_MinWidth, SCAST(int)(dpi), USER_DEFAULT_SCREEN_DPI));
    height = std::max(height, MulDiv(k_MinHeight, SCAST(int)(dpi), USER_DEFAULT_SCREEN_DPI));

    rect.left = SettingsGetInt(k_WindowX, 0);
    rect.top = SettingsGetInt(k_WindowY, 0);
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;

    //
    // A monitor that's gone since would leave the window where nobody can see
    // it, so it goes to the default place instead.
    //
    if (MonitorFromRect(&rect, MONITOR_DEFAULTTONULL) == nullptr)
    {
        return false;
    }

    placement.length = sizeof(placement);
    if (!GetWindowPlacement(Window, &placement))
    {
        return false;
    }

    placement.flags = 0;
    placement.showCmd = SW_HIDE;
    placement.rcNormalPosition = rect;

    if (!SetWindowPlacement(Window, &placement))
    {
        return false;
    }

    *Maximised = SettingsGetBool(k_WindowMaximised, false);
    return true;
}


/*!

    @brief Shows the window if it's hidden or minimised, and brings it to the
           front.

*/
static
void
BringToFront (
    void
    )
{
    if (IsIconic(g_MainWindow))
    {
        ShowWindow(g_MainWindow, SW_RESTORE);
    }
    else if (!IsWindowVisible(g_MainWindow))
    {
        ShowWindow(g_MainWindow, g_ShowMaximised ? SW_SHOWMAXIMIZED : SW_SHOW);
    }

    g_ShowMaximised = false;
    SetForegroundWindow(g_MainWindow);
}


/*!

    @brief What closing the window does: hides it, leaving WinCode in the
           tray with the hotkey working.

*/
static
void
HideToTray (
    void
    )
{
    SaveWindowState();
    ShowWindow(g_MainWindow, SW_HIDE);

    //
    // Said once, the first time, or the window would seem to have gone and
    // WinCode to have quit.
    //
    if (!SettingsGetBool(k_TrayHintShown, false))
    {
        TrayNotify(L"WinCode is still running",
                   g_HotkeyRegistered
                       ? std::format(L"Press {} to look up a code, or use this icon to open WinCode or exit.",
                                     HotkeyFormat(g_Hotkey)).c_str()
                       : L"Use this icon to open WinCode or exit.");

        SettingsSetBool(k_TrayHintShown, true);
        SettingsSave();
    }
}


/*!

    @brief Quits WinCode, as Exit on the tray icon's menu does.

*/
static
void
ExitWinCode (
    void
    )
{
    SaveWindowState();
    DestroyWindow(g_MainWindow);
}


/*!

    @brief Acts on the tray icon: a click opens the window, and a right click
           shows the menu.

    @param[in] WParam - Where, as the shell passes it.

    @param[in] LParam - What happened, as the shell passes it.

*/
static
void
OnTrayMessage (
    _In_ WPARAM WParam,
    _In_ LPARAM LParam
    )
{
    POINT point;
    UINT command;

    switch (LOWORD(LParam))
    {
    case NIN_SELECT:
    case NIN_KEYSELECT:
    case NIN_BALLOONUSERCLICK:
        BringToFront();
        return;

    case WM_CONTEXTMENU:
        point = { GET_X_LPARAM(WParam), GET_Y_LPARAM(WParam) };
        command = TrayShowMenu(g_MainWindow,
                               point,
                               StartupIsEnabled(),
                               g_HotkeyRegistered ? HotkeyFormat(g_Hotkey).c_str() : L"");
        break;

    default:
        return;
    }

    switch (command)
    {
    case IDM_TRAY_OPEN:
        BringToFront();
        return;

    case IDM_TRAY_POPUP:
        PopupShow(nullptr);
        return;

    case IDM_TRAY_SETTINGS:
        SettingsDialogShow(g_MainWindow);
        return;

    case IDM_TRAY_STARTUP:
        if (!StartupSetEnabled(!StartupIsEnabled()))
        {
            MessageBoxW(nullptr,
                        L"Windows wouldn't change whether WinCode starts when you sign in.",
                        L"WinCode",
                        MB_OK | MB_ICONWARNING);
        }
        return;

    case IDM_TRAY_EXIT:
        ExitWinCode();
        return;
    }
}


/*!

    @brief Takes a command line handed over by a second WinCodeUI.

    @param[in] Copy - The WM_COPYDATA it sent.

    @return True if it was one.

*/
static
bool
OnCopyData (
    _In_ const COPYDATASTRUCT* Copy
    )
{
    std::wstring query;

    //
    // WM_COPYDATA can come from any program on the desktop, so anything that
    // isn't a whole string from a WinCodeUI is ignored.
    //
    if (((Copy->dwData != k_ForwardWindow) && (Copy->dwData != k_ForwardPopup) && (Copy->dwData != k_ForwardExit)) ||
        ((Copy->cbData % sizeof(WCHAR)) != 0) ||
        ((Copy->cbData != 0) && (Copy->lpData == nullptr)))
    {
        return false;
    }

    //
    // Posted rather than done here, so the WinCodeUI asking gets its answer
    // before this one goes.
    //
    if (Copy->dwData == k_ForwardExit)
    {
        PostMessageW(g_MainWindow, k_ExitMessage, 0, 0);
        return true;
    }

    if (Copy->cbData != 0)
    {
        query.assign(SCAST(PCWSTR)(Copy->lpData), Copy->cbData / sizeof(WCHAR));
    }

    if (Copy->dwData == k_ForwardPopup)
    {
        PopupShow(query.empty() ? nullptr : query.c_str());
        return true;
    }

    MainWindowOpen(query.c_str(), false, CodeKind::WinError, 0);
    return true;
}


/*!

    @brief Subclass of the search box, so Up and Down move through the results
           without leaving it.

*/
static
LRESULT
CALLBACK
SearchEditProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam,
    _In_ UINT_PTR Id,
    _In_ DWORD_PTR Data
    )
{
    UNREFERENCED_PARAMETER(Data);

    if (Message == WM_KEYDOWN)
    {
        if (WParam == VK_DOWN)
        {
            MoveSelection(1);
            return 0;
        }

        if (WParam == VK_UP)
        {
            MoveSelection(-1);
            return 0;
        }
    }

    if (Message == WM_NCDESTROY)
    {
        RemoveWindowSubclass(Window, SearchEditProc, Id);
    }

    return DefSubclassProc(Window, Message, WParam, LParam);
}


/*!

    @brief Supplies a row's text to the virtual results list.

    @details The rows are owner drawn, so nothing here is painted. It's what a
             screen reader announces for the row.

    @param[in,out] Info - The request, whose buffer receives the text.

*/
static
void
OnGetDispInfo (
    _Inout_ NMLVDISPINFOW* Info
    )
{
    const ResultRow* row;
    std::wstring text;

    if (((Info->item.mask & LVIF_TEXT) == 0) ||
        (Info->item.iItem < 0) ||
        (SCAST(size_t)(Info->item.iItem) >= g_Rows.size()))
    {
        return;
    }

    row = &g_Rows[SCAST(size_t)(Info->item.iItem)];
    text = std::format(L"{}, {}, {}",
                       row->Name.empty() ? L"(no name)" : row->Name,
                       CodeKindName(row->Kind),
                       FormatCodeValue(row->Kind, row->Value));

    StringCchCopyW(Info->item.pszText, SCAST(size_t)(Info->item.cchTextMax), text.c_str());
}


/*!

    @brief Draws a row's name, highlighting the part a name search matched.

    @details The name is drawn in three runs, before, within and after the
             match, each placed where the last ended, so the highlight sits
             exactly behind the matched text. The match is only highlighted
             when the whole name fits. In a name cut short by an ellipsis, the
             highlight could land on text that isn't shown.

    @param[in] Dc - The device context, with the name font selected.

    @param[in] Row - The row.

    @param[in] Bounds - Where the name goes.

*/
static
void
DrawRowName (
    _In_ HDC Dc,
    _In_ const ResultRow& Row,
    _In_ const RECT& Bounds
    )
{
    size_t starts[3];
    size_t lengths[3];
    RECT measure;
    RECT rect;
    SIZE full;
    UINT flags;
    int x;
    int width;

    rect = Bounds;
    flags = DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER;

    if (Row.Name.empty())
    {
        SetTextColor(Dc, ThemeFaintColour());
        DrawTextW(Dc, L"(no name)", -1, &rect, flags);
        return;
    }

    SetTextColor(Dc, ThemeTextColour());
    GetTextExtentPoint32W(Dc, Row.Name.c_str(), SCAST(int)(Row.Name.size()), &full);

    if ((Row.MatchLength == 0) ||
        ((Row.MatchStart + Row.MatchLength) > Row.Name.size()) ||
        (full.cx > (Bounds.right - Bounds.left)))
    {
        DrawTextW(Dc, Row.Name.c_str(), SCAST(int)(Row.Name.size()), &rect, flags | DT_END_ELLIPSIS);
        return;
    }

    starts[0] = 0;
    lengths[0] = Row.MatchStart;
    starts[1] = Row.MatchStart;
    lengths[1] = Row.MatchLength;
    starts[2] = Row.MatchStart + Row.MatchLength;
    lengths[2] = Row.Name.size() - starts[2];

    x = Bounds.left;

    for (size_t i = 0; i < ARRAYSIZE(starts); i++)
    {
        if (lengths[i] == 0)
        {
            continue;
        }

        measure = {};
        DrawTextW(Dc, Row.Name.c_str() + starts[i], SCAST(int)(lengths[i]), &measure, flags | DT_CALCRECT);
        width = measure.right - measure.left;

        if (i == 1)
        {
            rect.left = x - Scale(2);
            rect.right = x + width + Scale(2);
            rect.top = Bounds.top + ((Bounds.bottom - Bounds.top - full.cy) / 2) - Scale(1);
            rect.bottom = rect.top + full.cy + Scale(2);
            FillRounded(Dc, rect, ThemeMatchColour(), Scale(4));
        }

        rect = { x, Bounds.top, Bounds.right, Bounds.bottom };
        DrawTextW(Dc, Row.Name.c_str() + starts[i], SCAST(int)(lengths[i]), &rect, flags);
        x += width;
    }
}


/*!

    @brief Draws one result row: the kind with its dot above the name, and the
           value on the right.

    @param[in] Item - The owner draw request.

*/
static
void
DrawResultRow (
    _In_ const DRAWITEMSTRUCT* Item
    )
{
    const ResultRow* row;
    std::wstring value;
    RECT bounds;
    RECT fill;
    RECT marker;
    RECT measure;
    RECT text;
    RECT dot;
    int saved;
    int valueLeft;
    int dotSize;
    int top;
    int x;

    if (Item->itemID >= g_Rows.size())
    {
        return;
    }

    row = &g_Rows[Item->itemID];
    bounds = Item->rcItem;
    saved = SaveDC(Item->hDC);

    FillRect(Item->hDC, &bounds, ThemeControlBrush());

    //
    // Windows 11's selection: a rounded fill inset from the edges, with a short
    // accent bar at its left.
    //
    if ((Item->itemState & ODS_SELECTED) != 0)
    {
        fill = bounds;
        InflateRect(&fill, -Scale(4), -Scale(1));
        FillRounded(Item->hDC, fill, ThemeSelectionColour(), Scale(k_RowRadius));

        marker = { fill.left, fill.top + Scale(12), fill.left + Scale(3), fill.bottom - Scale(12) };
        FillRounded(Item->hDC, marker, ThemeAccentColour(), Scale(3));
    }

    SetBkMode(Item->hDC, TRANSPARENT);

    value = FormatCodeValue(row->Kind, row->Value);
    SelectObject(Item->hDC, g_Fonts.Message);
    SetTextColor(Item->hDC, ThemeMutedColour());

    measure = {};
    DrawTextW(Item->hDC, value.c_str(), -1, &measure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

    text = bounds;
    text.right -= Scale(14);
    valueLeft = text.right - measure.right;
    DrawTextW(Item->hDC, value.c_str(), -1, &text, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    x = bounds.left + Scale(k_RowIndent);
    top = bounds.top + Scale(k_RowPadding);
    dotSize = Scale(k_KindDot);

    dot = { x, top + ((g_KindHeight - dotSize) / 2), x + dotSize, top + ((g_KindHeight - dotSize) / 2) + dotSize };
    FillDot(Item->hDC, dot, ThemeKindColour(row->Kind));

    text = { x + dotSize + Scale(6), top, valueLeft - Scale(12), top + g_KindHeight };
    SelectObject(Item->hDC, g_Fonts.Label);
    SetTextColor(Item->hDC, ThemeKindColour(row->Kind));
    DrawTextW(Item->hDC, CodeKindName(row->Kind), -1, &text, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    text = { x, top + g_KindHeight, valueLeft - Scale(12), bounds.bottom - Scale(k_RowPadding) };
    SelectObject(Item->hDC, g_Fonts.Name);
    DrawRowName(Item->hDC, *row, text);

    RestoreDC(Item->hDC, saved);
}


//
// The split between the results and the details, which the gap between them
// drags. The list's width is kept in 96 DPI units, so it keeps its proportion
// on a monitor at another scale, and is -1 until it's read from settings.json.
// Neither side can be dragged narrower than is still useful.
//
static constexpr int k_MinListWidth = 220;
static constexpr int k_MinDetailsWidth = 320;
static constexpr WCHAR k_ListWidthSetting[] = L"window.listWidth";

static int g_ListWidth = -1;
static RECT g_Splitter;
static bool g_Dragging;
static int g_DragOffset;

static void Layout(void);


/*!

    @brief The results list's width, in pixels, for a window this wide.

    @param[in] ClientWidth - The window's client width.

    @return The width, kept to what leaves both sides wide enough.

*/
static
int
ListWidthPixels (
    _In_ int ClientWidth
    )
{
    int widest;

    if (g_ListWidth < 0)
    {
        g_ListWidth = SettingsGetInt(k_ListWidthSetting, k_ListWidth);
    }

    widest = ClientWidth - (2 * Scale(k_Margin)) - Scale(k_Gap) - Scale(k_MinDetailsWidth);
    return std::max(std::min(Scale(g_ListWidth), widest), Scale(k_MinListWidth));
}


/*!

    @brief Handles the mouse over the gap between the results and the details,
           which drags to move the split.

    @param[in] Message - WM_SETCURSOR, WM_LBUTTONDOWN, WM_MOUSEMOVE,
                         WM_LBUTTONUP or WM_CAPTURECHANGED.

    @param[in] LParam - The message's LPARAM.

    @return True if the message was the splitter's.

*/
static
bool
OnSplitterMouse (
    _In_ UINT Message,
    _In_ LPARAM LParam
    )
{
    POINT point;
    RECT client;
    int listWidth;

    switch (Message)
    {
    case WM_SETCURSOR:
        if (LOWORD(LParam) != HTCLIENT)
        {
            return false;
        }

        GetCursorPos(&point);
        ScreenToClient(g_MainWindow, &point);

        if (!g_Dragging && !PtInRect(&g_Splitter, point))
        {
            return false;
        }

        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        return true;

    case WM_LBUTTONDOWN:
        point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
        if (!PtInRect(&g_Splitter, point))
        {
            return false;
        }

        g_Dragging = true;
        g_DragOffset = point.x - g_Splitter.left;
        SetCapture(g_MainWindow);
        return true;

    case WM_MOUSEMOVE:
        if (!g_Dragging)
        {
            return false;
        }

        GetClientRect(g_MainWindow, &client);
        g_ListWidth = MulDiv(GET_X_LPARAM(LParam) - g_DragOffset - Scale(k_Margin),
                             USER_DEFAULT_SCREEN_DPI,
                             SCAST(int)(g_Dpi));

        //
        // Kept to what the window can show, so dragging past the edge and back
        // doesn't leave the split somewhere it can't go.
        //
        listWidth = ListWidthPixels(client.right);
        g_ListWidth = MulDiv(listWidth, USER_DEFAULT_SCREEN_DPI, SCAST(int)(g_Dpi));

        Layout();
        return true;

    case WM_LBUTTONUP:
        if (!g_Dragging)
        {
            return false;
        }

        //
        // Releasing the capture sends WM_CAPTURECHANGED, which ends the drag.
        //
        ReleaseCapture();
        return true;

    case WM_CAPTURECHANGED:
        if (!g_Dragging)
        {
            return false;
        }

        //
        // Also where a drag ends if something else takes the mouse part way
        // through, so the split is saved wherever it was left.
        //
        g_Dragging = false;
        SettingsSetInt(k_ListWidthSetting, g_ListWidth);
        SettingsSave();
        return true;
    }

    return false;
}


/*!

    @brief Lays the controls out for the window's current size and DPI.

*/
static
void
Layout (
    void
    )
{
    RECT client;
    int width;
    int height;
    int toolbarY;
    int contentY;
    int searchX;
    int pinX;
    int settingsX;
    int listWidth;
    int listY;
    int detailsX;

    GetClientRect(g_MainWindow, &client);
    width = client.right;
    height = client.bottom;

    toolbarY = Scale(k_ToolbarTop);
    contentY = toolbarY + Scale(k_ButtonHeight) + Scale(k_Gap);

    MoveWindow(g_BackButton, Scale(k_Margin), toolbarY, Scale(k_ButtonWidth), Scale(k_ButtonHeight), TRUE);
    MoveWindow(g_ForwardButton,
               Scale(k_Margin) + Scale(k_ButtonWidth) + Scale(2),
               toolbarY,
               Scale(k_ButtonWidth),
               Scale(k_ButtonHeight),
               TRUE);

    settingsX = width - Scale(k_Margin) - Scale(k_ButtonWidth);
    MoveWindow(g_SettingsButton, settingsX, toolbarY, Scale(k_ButtonWidth), Scale(k_ButtonHeight), TRUE);

    pinX = settingsX - Scale(2) - Scale(k_ButtonWidth);
    MoveWindow(g_PinButton, pinX, toolbarY, Scale(k_ButtonWidth), Scale(k_ButtonHeight), TRUE);

    searchX = Scale(k_Margin) + (2 * Scale(k_ButtonWidth)) + Scale(2) + Scale(k_Gap);
    MoveWindow(g_SearchEdit,
               searchX,
               toolbarY + ((Scale(k_ButtonHeight) - Scale(k_EditHeight)) / 2),
               std::max(pinX - Scale(k_Gap) - searchX, Scale(100)),
               Scale(k_EditHeight),
               TRUE);

    listWidth = ListWidthPixels(width);
    listY = contentY + Scale(k_CountHeight);

    MoveWindow(g_CountLabel, Scale(k_Margin), contentY, listWidth, Scale(k_CountHeight), TRUE);
    MoveWindow(g_ResultsList, Scale(k_Margin), listY, listWidth, std::max(height - listY - Scale(k_Margin), 0), TRUE);
    FitListColumn();

    detailsX = Scale(k_Margin) + listWidth + Scale(k_Gap);
    g_Splitter = { Scale(k_Margin) + listWidth, contentY, detailsX, height - Scale(k_Margin) };
    MoveWindow(g_DetailsPane,
               detailsX,
               contentY,
               std::max(width - detailsX - Scale(k_Margin), 0),
               std::max(height - contentY - Scale(k_Margin), 0),
               TRUE);
}


/*!

    @brief Adds a tooltip to a control.

    @param[in] Control - The control.

    @param[in] Text - The tip.

*/
static
void
AddTooltip (
    _In_ HWND Control,
    _In_z_ PCWSTR Text
    )
{
    TOOLINFOW tool = {};

    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool.hwnd = g_MainWindow;
    tool.uId = RCAST(UINT_PTR)(Control);
    tool.lpszText = CCAST(PWSTR)(Text);

    SendMessageW(g_Tooltip, TTM_ADDTOOLW, 0, RCAST(LPARAM)(&tool));
}


/*!

    @brief Gives an icon button the name a screen reader announces. Its text
           is an icon font glyph, which would otherwise be read as nothing
           useful.

    @param[in] Control - The button.

    @param[in] Name - Its name.

*/
static
void
SetAccessibleName (
    _In_ HWND Control,
    _In_z_ PCWSTR Name
    )
{
    if (g_AccProps != nullptr)
    {
        g_AccProps->SetHwndPropStr(Control, SCAST(DWORD)(OBJID_CLIENT), CHILDID_SELF, PROPID_ACC_NAME, Name);
    }
}


/*!

    @brief Removes the accessible names SetAccessibleName gave, as the window
           closes.

*/
static
void
ClearAccessibleNames (
    void
    )
{
    HWND controls[] = { g_BackButton, g_ForwardButton, g_PinButton, g_SettingsButton };
    MSAAPROPID property;

    if (g_AccProps == nullptr)
    {
        return;
    }

    property = PROPID_ACC_NAME;

    for (HWND control : controls)
    {
        g_AccProps->ClearHwndProps(control, SCAST(DWORD)(OBJID_CLIENT), CHILDID_SELF, &property, 1);
    }

    g_AccProps->Release();
    g_AccProps = nullptr;
}


/*!

    @brief A control ID in the form CreateWindowEx takes it, in place of a menu.

    @param[in] Id - The control ID.

    @return The ID as an HMENU.

*/
static
HMENU
ControlId (
    _In_ int Id
    )
{
    return RCAST(HMENU)(SCAST(UINT_PTR)(Id));
}


/*!

    @brief Creates the child controls. Layout places them afterwards.

    @param[in] Instance - The module instance.

    @return True if every control was created.

*/
static
bool
CreateControls (
    _In_ HINSTANCE Instance
    )
{
    LVCOLUMNW column = {};

    g_BackButton = CreateWindowExW(0, WC_BUTTONW, k_GlyphBack,
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | BS_PUSHBUTTON,
                                   0, 0, 0, 0, g_MainWindow, ControlId(IDC_BACK), Instance, nullptr);

    g_ForwardButton = CreateWindowExW(0, WC_BUTTONW, k_GlyphForward,
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | BS_PUSHBUTTON,
                                      0, 0, 0, 0, g_MainWindow, ControlId(IDC_FORWARD), Instance, nullptr);

    g_SearchEdit = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                   0, 0, 0, 0, g_MainWindow, ControlId(IDC_SEARCH), Instance, nullptr);

    g_PinButton = CreateWindowExW(0, WC_BUTTONW, k_GlyphPin,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                                  0, 0, 0, 0, g_MainWindow, ControlId(IDC_PINBUTTON), Instance, nullptr);

    g_SettingsButton = CreateWindowExW(0, WC_BUTTONW, k_GlyphSettings,
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                       0, 0, 0, 0, g_MainWindow, ControlId(IDC_SETTINGSBUTTON), Instance, nullptr);

    g_CountLabel = CreateWindowExW(0, WC_STATICW, L"",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX,
                                   0, 0, 0, 0, g_MainWindow, ControlId(IDC_COUNT), Instance, nullptr);

    g_ResultsList = CreateWindowExW(0, WC_LISTVIEWW, L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA |
                                        LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER | LVS_SINGLESEL |
                                        LVS_SHOWSELALWAYS,
                                    0, 0, 0, 0, g_MainWindow, ControlId(IDC_RESULTS), Instance, nullptr);

    g_DetailsPane = DetailsPaneCreate(g_MainWindow, IDC_DETAILS, Instance);

    g_Tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                g_MainWindow, nullptr, Instance, nullptr);

    if ((g_BackButton == nullptr) || (g_ForwardButton == nullptr) || (g_SearchEdit == nullptr) ||
        (g_PinButton == nullptr) || (g_SettingsButton == nullptr) || (g_CountLabel == nullptr) ||
        (g_ResultsList == nullptr) || (g_DetailsPane == nullptr) || (g_Tooltip == nullptr))
    {
        return false;
    }

    SendMessageW(g_SearchEdit, EM_SETCUEBANNER, TRUE, RCAST(LPARAM)(L"Look up a code or a name, or paste log text"));
    SetWindowSubclass(g_SearchEdit, SearchEditProc, 0, 0);

    AddTooltip(g_BackButton, L"Back");
    AddTooltip(g_ForwardButton, L"Forward");
    AddTooltip(g_PinButton, L"Keep on top");
    AddTooltip(g_SettingsButton, L"Settings");

    //
    // Without the service the buttons still work; only their names are lost.
    //
    if (SUCCEEDED(CoCreateInstance(__uuidof(CAccPropServices),
                                   nullptr,
                                   CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&g_AccProps))))
    {
        SetAccessibleName(g_BackButton, L"Back");
        SetAccessibleName(g_ForwardButton, L"Forward");
        SetAccessibleName(g_PinButton, L"Keep on top");
        SetAccessibleName(g_SettingsButton, L"Settings");
    }

    ListView_SetExtendedListViewStyle(g_ResultsList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    column.mask = LVCF_WIDTH;
    column.cx = 100;
    ListView_InsertColumn(g_ResultsList, 0, &column);

    return true;
}


/*!

    @brief The main window's procedure.

*/
static
LRESULT
CALLBACK
MainWindowProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam
    )
{
    CREATESTRUCTW* create;
    NMHDR* header;
    NMLISTVIEW* change;
    const DetailsLinkNotify* link;
    MINMAXINFO* limits;
    MEASUREITEMSTRUCT* measure;
    const DRAWITEMSTRUCT* draw;
    RECT* suggested;
    RECT client;
    INT_PTR brush;
    UINT dpi;
    bool pinned;

    //
    // Explorer has restarted and forgotten every tray icon. A registered
    // message, so it can't be a case below.
    //
    if ((Message == TrayCreatedMessage()) && (Message != 0))
    {
        TrayAdd(Window, TrayTip().c_str());
        return 0;
    }

    switch (Message)
    {
    case WM_CREATE:
        create = RCAST(CREATESTRUCTW*)(LParam);
        g_MainWindow = Window;
        g_Dpi = GetDpiForWindow(Window);

        CreateFonts();
        if (!CreateControls(create->hInstance))
        {
            return -1;
        }

        ApplyFonts();
        ThemeApplyToWindow(Window);
        ThemeApplyToPopup(g_Tooltip);
        return 0;

    case WM_SIZE:
        if (g_DetailsPane != nullptr)
        {
            Layout();
        }
        return 0;

    case WM_DPICHANGED:
        g_Dpi = HIWORD(WParam);
        CreateFonts();
        ApplyFonts();

        suggested = RCAST(RECT*)(LParam);
        SetWindowPos(Window,
                     nullptr,
                     suggested->left,
                     suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;

    case WM_GETMINMAXINFO:
        limits = RCAST(MINMAXINFO*)(LParam);
        dpi = GetDpiForWindow(Window);
        limits->ptMinTrackSize.x = MulDiv(k_MinWidth, SCAST(int)(dpi), USER_DEFAULT_SCREEN_DPI);
        limits->ptMinTrackSize.y = MulDiv(k_MinHeight, SCAST(int)(dpi), USER_DEFAULT_SCREEN_DPI);
        return 0;

    case WM_MEASUREITEM:
        //
        // Sent while the list is being created, before g_ResultsList is set,
        // so it's recognised by type rather than by handle.
        //
        measure = RCAST(MEASUREITEMSTRUCT*)(LParam);
        if (measure->CtlType == ODT_LISTVIEW)
        {
            measure->itemHeight = SCAST(UINT)(g_RowHeight);
            return TRUE;
        }
        break;

    case WM_DRAWITEM:
        draw = RCAST(const DRAWITEMSTRUCT*)(LParam);
        if (draw->hwndItem == g_ResultsList)
        {
            DrawResultRow(draw);
            return TRUE;
        }
        break;

    case WM_HOTKEY:
        if (WParam == k_HotkeyId)
        {
            PopupToggle();
            return 0;
        }
        break;

    case k_TrayMessage:
        OnTrayMessage(WParam, LParam);
        return 0;

    case k_ExitMessage:
        ExitWinCode();
        return 0;

    case WM_COPYDATA:
        return OnCopyData(RCAST(const COPYDATASTRUCT*)(LParam)) ? TRUE : FALSE;

    case WM_COMMAND:
        switch (LOWORD(WParam))
        {
        case IDC_SEARCH:
            if ((HIWORD(WParam) == EN_CHANGE) && !g_SettingQuery)
            {
                RunQuery();
            }
            return 0;

        case IDC_BACK:
        case IDC_FORWARD:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                MoveThroughHistory(LOWORD(WParam) == IDC_FORWARD);
            }
            return 0;

        case IDC_PINBUTTON:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                pinned = (SendMessageW(g_PinButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SetWindowPos(Window,
                             pinned ? HWND_TOPMOST : HWND_NOTOPMOST,
                             0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

                SettingsSetBool(k_WindowOnTop, pinned);
                SettingsSave();
            }
            return 0;

        case IDC_SETTINGSBUTTON:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                SettingsDialogShow(Window);
            }
            return 0;

        case IDOK:
            SetFocus(g_ResultsList);
            return 0;

        case IDCANCEL:
            SetWindowTextW(g_SearchEdit, L"");
            SetFocus(g_SearchEdit);
            return 0;
        }
        break;

    case WM_APPCOMMAND:
        //
        // The mouse's back and forward buttons, and the keyboard's browser
        // keys, wherever the pointer or the focus is: DefWindowProc passes the
        // message up from the child that got it.
        //
        switch (GET_APPCOMMAND_LPARAM(LParam))
        {
        case APPCOMMAND_BROWSER_BACKWARD:
            MoveThroughHistory(false);
            return TRUE;

        case APPCOMMAND_BROWSER_FORWARD:
            MoveThroughHistory(true);
            return TRUE;
        }
        break;

    case WM_NOTIFY:
        header = RCAST(NMHDR*)(LParam);

        if ((header->hwndFrom == g_DetailsPane) && (header->code == k_DetailsFollowLink))
        {
            link = RCAST(const DetailsLinkNotify*)(LParam);
            FollowCode(link->Kind, link->Value, link->Name);
            return 0;
        }

        if (header->hwndFrom != g_ResultsList)
        {
            break;
        }

        switch (header->code)
        {
        case LVN_GETDISPINFOW:
            OnGetDispInfo(RCAST(NMLVDISPINFOW*)(LParam));
            return 0;

        case LVN_ITEMCHANGED:
            change = RCAST(NMLISTVIEW*)(LParam);
            if (((change->uNewState & LVIS_SELECTED) != 0) && ((change->uOldState & LVIS_SELECTED) == 0))
            {
                ShowDetails(change->iItem);
            }
            return 0;
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
        //
        // The search box is content; everything else sits on the window's
        // ground.
        //
        brush = ThemeOnCtlColor(WParam, RCAST(HWND)(LParam) == g_SearchEdit);
        if (brush != 0)
        {
            return brush;
        }
        break;

    case WM_ERASEBKGND:
        GetClientRect(Window, &client);
        FillRect(RCAST(HDC)(WParam), &client, ThemeBackgroundBrush());
        return 1;

    case WM_SETTINGCHANGE:
        //
        // Windows broadcasts ImmersiveColorSet when the light/dark setting
        // changes, and WinCode follows it while running.
        //
        if ((LParam != 0) && (wcscmp(RCAST(PCWSTR)(LParam), L"ImmersiveColorSet") == 0))
        {
            ThemeRefresh();
            ThemeApplyToWindow(Window);
            ThemeApplyToPopup(g_Tooltip);
            RedrawWindow(Window, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        }
        break;

    case WM_SETCURSOR:
        //
        // Only over the window itself, which between the controls means the
        // gap the splitter lives in.
        //
        if ((RCAST(HWND)(WParam) == Window) && OnSplitterMouse(Message, LParam))
        {
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        if (OnSplitterMouse(Message, LParam))
        {
            return 0;
        }
        break;

    case WM_CLOSE:
        //
        // Closing hides, so the hotkey keeps working. Exit on the tray icon's
        // menu is what quits.
        //
        HideToTray();
        return 0;

    case WM_ENDSESSION:
        //
        // Windows ends the process without a WM_DESTROY when the user signs
        // out, so this is the last chance to save. Restart Manager, closing
        // WinCode for an installer, asks the same way with ENDSESSION_CLOSEAPP,
        // and expects it to go rather than wait on in the tray.
        //
        if (WParam != FALSE)
        {
            if ((LParam & ENDSESSION_CLOSEAPP) != 0)
            {
                ExitWinCode();
            }
            else
            {
                SaveWindowState();
            }
        }
        return 0;

    case WM_DESTROY:
        if (g_HotkeyRegistered)
        {
            UnregisterHotKey(Window, k_HotkeyId);
            g_HotkeyRegistered = false;
        }

        TrayRemove();
        ClearAccessibleNames();
        FontsDelete(&g_Fonts);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(Window, Message, WParam, LParam);
}


bool
MainWindowRegister (
    _In_ HINSTANCE Instance
    )
{
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = MainWindowProc;
    windowClass.hInstance = Instance;
    windowClass.hIcon = LoadIconW(Instance, MAKEINTRESOURCEW(IDI_WINCODE));
    windowClass.hIconSm = RCAST(HICON)(LoadImageW(Instance,
                                                  MAKEINTRESOURCEW(IDI_WINCODE),
                                                  IMAGE_ICON,
                                                  GetSystemMetrics(SM_CXSMICON),
                                                  GetSystemMetrics(SM_CYSMICON),
                                                  LR_DEFAULTCOLOR));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;    // WM_ERASEBKGND paints the theme's ground
    windowClass.lpszClassName = k_MainWindowClass;

    return RegisterClassExW(&windowClass) != 0;
}


HWND
MainWindowCreate (
    _In_ HINSTANCE Instance,
    _In_z_ PCWSTR InitialQuery,
    _In_ int ShowCommand
    )
{
    MONITORINFO monitor = {};
    std::wstring message;
    HWND window;
    DWORD hotkeyError;
    UINT dpi;
    int width;
    int height;
    bool maximised;

    window = CreateWindowExW(0,
                             k_MainWindowClass,
                             L"WinCode",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             nullptr,
                             nullptr,
                             Instance,
                             nullptr);
    if (window == nullptr)
    {
        return nullptr;
    }

    //
    // Only now does the window know which monitor it's on, and so its scale.
    // Size it for that and centre it in the monitor's work area, unless
    // settings.json knows where it was.
    //
    dpi = GetDpiForWindow(window);
    width = MulDiv(k_DefaultWidth, SCAST(int)(dpi), USER_DEFAULT_SCREEN_DPI);
    height = MulDiv(k_DefaultHeight, SCAST(int)(dpi), USER_DEFAULT_SCREEN_DPI);

    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);

    SetWindowPos(window,
                 nullptr,
                 monitor.rcWork.left + ((monitor.rcWork.right - monitor.rcWork.left - width) / 2),
                 monitor.rcWork.top + ((monitor.rcWork.bottom - monitor.rcWork.top - height) / 2),
                 width,
                 height,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    RestoreWindowState(window, &maximised);

    if (SettingsGetBool(k_WindowOnTop, false))
    {
        SendMessageW(g_PinButton, BM_SETCHECK, BST_CHECKED, 0);
        SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    g_SettingQuery = true;
    SetWindowTextW(g_SearchEdit, InitialQuery);
    g_SettingQuery = false;
    RunQuery();

    g_Hotkey = HotkeyLoad();
    g_HotkeyRegistered = RegisterPopupHotkey(g_Hotkey);
    hotkeyError = g_HotkeyRegistered ? ERROR_SUCCESS : GetLastError();

    TrayAdd(window, TrayTip().c_str());

    if (ShowCommand == SW_HIDE)
    {
        g_ShowMaximised = maximised;
    }
    else
    {
        ShowWindow(window, maximised ? SW_SHOWMAXIMIZED : ShowCommand);
        UpdateWindow(window);

        SetFocus(g_SearchEdit);
        SendMessageW(g_SearchEdit, EM_SETSEL, 0, -1);
    }

    //
    // Said rather than left to fail silently, or the popup would just never
    // appear, and with a way to fix it there and then.
    //
    if (!g_HotkeyRegistered)
    {
        message = std::format(L"{} {}, so the WinCode popup has no hotkey.\n\nChoose a different hotkey now?",
                              (hotkeyError == ERROR_HOTKEY_ALREADY_REGISTERED) ? L"Another program is already using"
                                                                               : L"Windows wouldn't register",
                              HotkeyFormat(g_Hotkey));

        if (MessageBoxW((ShowCommand == SW_HIDE) ? nullptr : window,
                        message.c_str(),
                        L"WinCode",
                        MB_YESNO | MB_ICONINFORMATION) == IDYES)
        {
            SettingsDialogShow(window);
        }
    }

    return window;
}


bool
MainWindowPreTranslate (
    _In_ const MSG* Msg
    )
{
    //
    // Alt+Left and Alt+Right, as in a browser. With Alt held, the keys arrive
    // as WM_SYSKEYDOWN at whichever control has the focus, so they're caught
    // here, before any control sees them.
    //
    if ((Msg->message != WM_SYSKEYDOWN) || ((Msg->wParam != VK_LEFT) && (Msg->wParam != VK_RIGHT)))
    {
        return false;
    }

    if ((Msg->hwnd != g_MainWindow) && !IsChild(g_MainWindow, Msg->hwnd))
    {
        return false;
    }

    MoveThroughHistory(Msg->wParam == VK_RIGHT);
    return true;
}


void
MainWindowOpen (
    _In_z_ PCWSTR Query,
    _In_ bool HasSelection,
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    )
{
    HistoryEntry place = {};
    std::wstring current;

    if (Query[0] != L'\0')
    {
        place.Query = Query;
        place.HasSelection = HasSelection;
        place.Kind = Kind;
        place.Value = Value;
        place.Label = place.Query;

        //
        // Whatever the window was showing goes onto Back, unless it was
        // nothing, or this same query.
        //
        current = QueryText();
        if (!current.empty() && (current != place.Query))
        {
            PushHistory();
        }

        GoTo(place);
        UpdateHistoryButtons();
    }

    BringToFront();
    SetFocus(g_Rows.empty() ? g_SearchEdit : g_ResultsList);
}


bool
MainWindowForward (
    _In_z_ PCWSTR Query,
    _In_ bool Popup
    )
{
    COPYDATASTRUCT copy = {};
    DWORD_PTR result;
    DWORD processId;
    HWND window;

    //
    // The one already running may still be starting, with no window yet.
    //
    window = nullptr;
    for (int attempt = 0; (attempt < k_ForwardAttempts) && (window == nullptr); attempt++)
    {
        window = FindWindowW(k_MainWindowClass, nullptr);
        if (window == nullptr)
        {
            Sleep(k_ForwardWaitMs);
        }
    }

    if (window == nullptr)
    {
        return false;
    }

    //
    // Windows lets the program just launched take the foreground, not the one
    // already running. Passing the right on lets that one come to the front.
    //
    if (GetWindowThreadProcessId(window, &processId) != 0)
    {
        AllowSetForegroundWindow(processId);
    }

    copy.dwData = Popup ? k_ForwardPopup : k_ForwardWindow;
    copy.cbData = SCAST(DWORD)(wcslen(Query) * sizeof(WCHAR));
    copy.lpData = CCAST(PWSTR)(Query);

    return SendMessageTimeoutW(window,
                               WM_COPYDATA,
                               0,
                               RCAST(LPARAM)(&copy),
                               SMTO_ABORTIFHUNG,
                               k_ForwardTimeoutMs,
                               &result) != 0;
}


bool
MainWindowExitRunning (
    void
    )
{
    COPYDATASTRUCT copy = {};
    DWORD_PTR result;
    DWORD processId;
    HANDLE process;
    HWND window;
    bool exited;

    window = FindWindowW(k_MainWindowClass, nullptr);
    if (window == nullptr)
    {
        return true;
    }

    //
    // Opened before asking, so the process can't be gone, and its ID taken by
    // another, before it's waited for.
    //
    process = nullptr;
    if (GetWindowThreadProcessId(window, &processId) != 0)
    {
        process = OpenProcess(SYNCHRONIZE, FALSE, processId);
    }

    copy.dwData = k_ForwardExit;

    if (SendMessageTimeoutW(window,
                            WM_COPYDATA,
                            0,
                            RCAST(LPARAM)(&copy),
                            SMTO_ABORTIFHUNG,
                            k_ForwardTimeoutMs,
                            &result) == 0)
    {
        if (process != nullptr)
        {
            CloseHandle(process);
        }

        return false;
    }

    //
    // Without a handle there's nothing to wait on; it has been asked, and
    // Restart Manager is the installer's fallback.
    //
    exited = (process == nullptr) || (WaitForSingleObject(process, k_ExitWaitMs) == WAIT_OBJECT_0);

    if (process != nullptr)
    {
        CloseHandle(process);
    }

    return exited;
}


Hotkey
MainWindowHotkey (
    void
    )
{
    return g_Hotkey;
}


bool
MainWindowHotkeyRegistered (
    void
    )
{
    return g_HotkeyRegistered;
}


bool
MainWindowSetHotkey (
    _In_ const Hotkey& Key
    )
{
    bool wasRegistered;

    wasRegistered = g_HotkeyRegistered;

    if (wasRegistered && (Key.Modifiers == g_Hotkey.Modifiers) && (Key.Key == g_Hotkey.Key))
    {
        return true;
    }

    //
    // Registering again under the same ID keeps the old hotkey as well as
    // adding the new one, so the old one goes first.
    //
    if (wasRegistered)
    {
        UnregisterHotKey(g_MainWindow, k_HotkeyId);
        g_HotkeyRegistered = false;
    }

    if (!RegisterPopupHotkey(Key))
    {
        if (wasRegistered)
        {
            g_HotkeyRegistered = RegisterPopupHotkey(g_Hotkey);
        }

        return false;
    }

    g_Hotkey = Key;
    g_HotkeyRegistered = true;

    HotkeySave(Key);
    TraySetTip(TrayTip().c_str());
    return true;
}
