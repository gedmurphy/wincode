/*!

    @file Gui/SettingsDialog.cpp

    @brief The settings dialog, for the popup's hotkey, starting at sign-in
           and the recent lookups.

    @details A dialog template in WinCodeUI.rc, with OK and Cancel. FileMon++'s
             settings apply as you change them, but you can't try a hotkey half
             chosen, since each modifier tick would register a different one.
             So nothing applies until OK, and OK only closes once the new
             hotkey's registered. If another program has it, we say so and
             stay open.

             Clearing recent lookups is the exception, a button that acts at
             once and says it has.

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
#include <map>


//
// The dialog while it's open, so a second request (from the tray, say) brings
// it forward rather than opening another.
//
static HWND g_Dialog;

//
// The heading's semibold font, made from the dialog's own so it matches it
// at any DPI.
//
static HFONT g_HeadingFont;

//
// The sources of codes, and how each will be shown if OK is pressed.
//
static std::vector<CodeSource> g_Sources;
static std::vector<SourceShow> g_PendingShows;


/*!

    @brief Shows a message under the hotkey, or clears it.

    @param[in] Dialog - The dialog.

    @param[in] Text - The message, or an empty string.

*/
static
void
SetStatus (
    _In_ HWND Dialog,
    _In_ const std::wstring& Text
    )
{
    SetDlgItemTextW(Dialog, IDC_HOTKEY_STATUS, Text.c_str());
}


/*!

    @brief Gives the heading a semibold version of the dialog's font.

    @param[in] Dialog - The dialog.

*/
static
void
SetHeadingFont (
    _In_ HWND Dialog
    )
{
    LOGFONTW font;
    HFONT dialogFont;

    dialogFont = RCAST(HFONT)(SendMessageW(Dialog, WM_GETFONT, 0, 0));
    if ((dialogFont == nullptr) || (GetObjectW(dialogFont, sizeof(font), &font) == 0))
    {
        return;
    }

    font.lfWeight = FW_SEMIBOLD;

    g_HeadingFont = CreateFontIndirectW(&font);
    if (g_HeadingFont != nullptr)
    {
        SendDlgItemMessageW(Dialog, IDC_SETTINGS_HEADING, WM_SETFONT, RCAST(WPARAM)(g_HeadingFont), FALSE);
        SendDlgItemMessageW(Dialog, IDC_SOURCES_HEADING, WM_SETFONT, RCAST(WPARAM)(g_HeadingFont), FALSE);
    }
}


/*!

    @brief Sets the hotkey's check boxes and key list to a hotkey.

    @param[in] Dialog - The dialog.

    @param[in] Key - The hotkey.

*/
static
void
ShowHotkey (
    _In_ HWND Dialog,
    _In_ const Hotkey& Key
    )
{
    std::wstring name;
    LRESULT index;
    HWND list;

    CheckDlgButton(Dialog, IDC_HOTKEY_WIN, ((Key.Modifiers & MOD_WIN) != 0) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(Dialog, IDC_HOTKEY_CTRL, ((Key.Modifiers & MOD_CONTROL) != 0) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(Dialog, IDC_HOTKEY_ALT, ((Key.Modifiers & MOD_ALT) != 0) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(Dialog, IDC_HOTKEY_SHIFT, ((Key.Modifiers & MOD_SHIFT) != 0) ? BST_CHECKED : BST_UNCHECKED);

    list = GetDlgItem(Dialog, IDC_HOTKEY_KEY);

    for (UINT key : HotkeyKeys())
    {
        name = HotkeyKeyName(key);

        index = SendMessageW(list, CB_ADDSTRING, 0, RCAST(LPARAM)(name.c_str()));
        if (index < 0)
        {
            continue;
        }

        SendMessageW(list, CB_SETITEMDATA, SCAST(WPARAM)(index), SCAST(LPARAM)(key));

        if (key == Key.Key)
        {
            SendMessageW(list, CB_SETCURSEL, SCAST(WPARAM)(index), 0);
        }
    }
}


/*!

    @brief The hotkey the dialog's controls describe.

    @param[in] Dialog - The dialog.

    @return The hotkey. Its modifiers are zero if none are ticked.

*/
static
Hotkey
ReadHotkey (
    _In_ HWND Dialog
    )
{
    Hotkey key = {};
    LRESULT index;

    if (IsDlgButtonChecked(Dialog, IDC_HOTKEY_WIN) == BST_CHECKED)
    {
        key.Modifiers |= MOD_WIN;
    }

    if (IsDlgButtonChecked(Dialog, IDC_HOTKEY_CTRL) == BST_CHECKED)
    {
        key.Modifiers |= MOD_CONTROL;
    }

    if (IsDlgButtonChecked(Dialog, IDC_HOTKEY_ALT) == BST_CHECKED)
    {
        key.Modifiers |= MOD_ALT;
    }

    if (IsDlgButtonChecked(Dialog, IDC_HOTKEY_SHIFT) == BST_CHECKED)
    {
        key.Modifiers |= MOD_SHIFT;
    }

    index = SendDlgItemMessageW(Dialog, IDC_HOTKEY_KEY, CB_GETCURSEL, 0, 0);
    if (index != CB_ERR)
    {
        key.Key = SCAST(UINT)(SendDlgItemMessageW(Dialog, IDC_HOTKEY_KEY, CB_GETITEMDATA, SCAST(WPARAM)(index), 0));
    }

    return key;
}


/*!

    @brief Where settings are saved, in words, for the foot of the dialog.

    @return The text.

*/
static
std::wstring
SettingsFileText (
    void
    )
{
    std::wstring path;

    path = SettingsPath();

    if (path.empty())
    {
        return L"There's nowhere to save settings, so changes last only until WinCode exits.";
    }

    if (SettingsIsUnreadable())
    {
        return std::format(L"{} has a mistake in it, so nothing is saved until it's fixed or deleted.", path);
    }

    return L"Saved in " + path;
}


/*!

    @brief How a setting reads in the dialog.

    @param[in] Show - The setting.

    @return A static, never null, label.

*/
static
PCWSTR
ShowLabel (
    _In_ SourceShow Show
    )
{
    switch (Show)
    {
    case SourceShow::Quiet:
        return L"When nothing else matches";

    case SourceShow::Never:
        return L"Never";

    default:
        return L"Always";
    }
}


/*!

    @brief Shows a source's setting, as chosen so far, in its row.

    @param[in] Dialog - The dialog.

    @param[in] Row - The source's row.

*/
static
void
ShowSourceRow (
    _In_ HWND Dialog,
    _In_ int Row
    )
{
    ListView_SetItemText(GetDlgItem(Dialog, IDC_SOURCES_LIST),
                         Row,
                         1,
                         CCAST(PWSTR)(ShowLabel(g_PendingShows[SCAST(size_t)(Row)])));
}


/*!

    @brief Puts the selected source's setting in the drop-down list, which is
           only enabled while a source is selected.

    @param[in] Dialog - The dialog.

*/
static
void
ShowSourceChoice (
    _In_ HWND Dialog
    )
{
    int selected;

    selected = ListView_GetNextItem(GetDlgItem(Dialog, IDC_SOURCES_LIST), -1, LVNI_SELECTED);

    EnableWindow(GetDlgItem(Dialog, IDC_SOURCE_SHOW), (selected >= 0) ? TRUE : FALSE);
    SendDlgItemMessageW(Dialog,
                        IDC_SOURCE_SHOW,
                        CB_SETCURSEL,
                        (selected >= 0) ? SCAST(WPARAM)(g_PendingShows[SCAST(size_t)(selected)]) : SCAST(WPARAM)(-1),
                        0);
}


/*!

    @brief Fills the list of sources, keeping unapplied choices and the
           selection, as after your headers are read again.

    @param[in] Dialog - The dialog.

*/
static
void
FillSourceRows (
    _In_ HWND Dialog
    )
{
    std::map<std::wstring, SourceShow> pending;
    std::map<std::wstring, SourceShow>::const_iterator found;
    std::wstring selectedKey;
    LVITEMW item = {};
    HWND list;
    int selected;
    int select;

    list = GetDlgItem(Dialog, IDC_SOURCES_LIST);

    selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if ((selected >= 0) && (SCAST(size_t)(selected) < g_Sources.size()))
    {
        selectedKey = g_Sources[SCAST(size_t)(selected)].Key;
    }

    for (size_t i = 0; i < g_Sources.size(); i++)
    {
        pending[g_Sources[i].Key] = g_PendingShows[i];
    }

    ListView_DeleteAllItems(list);
    g_Sources = CodeSources();
    g_PendingShows.clear();
    select = 0;

    for (size_t i = 0; i < g_Sources.size(); i++)
    {
        found = pending.find(g_Sources[i].Key);
        g_PendingShows.push_back((found != pending.end()) ? found->second : GetSourceShow(g_Sources[i].Key.c_str()));

        item.mask = LVIF_TEXT;
        item.iItem = SCAST(int)(i);
        item.pszText = CCAST(PWSTR)(g_Sources[i].Name.c_str());
        ListView_InsertItem(list, &item);

        ShowSourceRow(Dialog, SCAST(int)(i));

        if (g_Sources[i].Key == selectedKey)
        {
            select = SCAST(int)(i);
        }
    }

    ListView_SetItemState(list, select, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(list, select, FALSE);
    ShowSourceChoice(Dialog);
}


/*!

    @brief Sets up the list of sources, and fills it with how each is shown
           now.

    @param[in] Dialog - The dialog.

*/
static
void
FillSources (
    _In_ HWND Dialog
    )
{
    LVCOLUMNW column = {};
    RECT client;
    HWND list;
    int width;

    list = GetDlgItem(Dialog, IDC_SOURCES_LIST);
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    GetClientRect(list, &client);
    width = client.right - GetSystemMetricsForDpi(SM_CXVSCROLL, GetDpiForWindow(Dialog));

    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = CCAST(PWSTR)(L"Source");
    column.cx = (width * 3) / 5;
    ListView_InsertColumn(list, 0, &column);

    column.pszText = CCAST(PWSTR)(L"Show");
    column.cx = width - ((width * 3) / 5);
    ListView_InsertColumn(list, 1, &column);

    for (PCWSTR label : { ShowLabel(SourceShow::Always), ShowLabel(SourceShow::Quiet), ShowLabel(SourceShow::Never) })
    {
        SendDlgItemMessageW(Dialog, IDC_SOURCE_SHOW, CB_ADDSTRING, 0, RCAST(LPARAM)(label));
    }

    //
    // What's left from the last time the dialog was open isn't pending now.
    //
    g_Sources.clear();
    g_PendingShows.clear();
    FillSourceRows(Dialog);
}


/*!

    @brief Applies what the dialog shows, as OK does.

    @param[in] Dialog - The dialog.

    @return True if everything applied; false, with the reason shown, if not.

*/
static
bool
Apply (
    _In_ HWND Dialog
    )
{
    Hotkey key;
    bool startAtSignIn;

    key = ReadHotkey(Dialog);

    if (key.Modifiers == 0)
    {
        SetStatus(Dialog, L"Tick at least one of Win, Ctrl, Alt and Shift, or the key would stop working in every other program.");
        return false;
    }

    if (!MainWindowSetHotkey(key))
    {
        SetStatus(Dialog, std::format(L"{} is already used by Windows or another program. Choose another.", HotkeyFormat(key)));
        return false;
    }

    startAtSignIn = (IsDlgButtonChecked(Dialog, IDC_STARTUP) == BST_CHECKED);

    if ((startAtSignIn != StartupIsEnabled()) && !StartupSetEnabled(startAtSignIn))
    {
        SetStatus(Dialog, L"Windows wouldn't change whether WinCode starts when you sign in.");
        return false;
    }

    UpdateSetCheckEnabled(IsDlgButtonChecked(Dialog, IDC_UPDATECHECK) == BST_CHECKED);

    for (size_t i = 0; i < g_Sources.size(); i++)
    {
        SetSourceShow(g_Sources[i].Key.c_str(), g_PendingShows[i]);
    }

    SaveSourceShows();
    MainWindowRefresh();
    return true;
}


/*!

    @brief The settings dialog's procedure.

*/
static
INT_PTR
CALLBACK
SettingsDialogProc (
    _In_ HWND Dialog,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam
    )
{
    const NMHDR* notify;
    INT_PTR brush;
    LRESULT choice;
    int selected;

    switch (Message)
    {
    case WM_INITDIALOG:
        g_Dialog = Dialog;

        SetHeadingFont(Dialog);

        ShowHotkey(Dialog, MainWindowHotkey());
        if (!MainWindowHotkeyRegistered())
        {
            SetStatus(Dialog,
                      std::format(L"{} is in use by another program, so the popup has no hotkey. Choose another.",
                                  HotkeyFormat(MainWindowHotkey())));
        }

        CheckDlgButton(Dialog, IDC_STARTUP, StartupIsEnabled() ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(Dialog, IDC_UPDATECHECK, UpdateCheckEnabled() ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(Dialog, IDC_CLEAR_RECENT), PopupHasRecent() ? TRUE : FALSE);
        SetDlgItemTextW(Dialog, IDC_SETTINGS_PATH, SettingsFileText().c_str());
        FillSources(Dialog);

        ThemeApplyToWindow(Dialog);

        //
        // Opened from the tray, with the main window hidden, it would
        // otherwise come up behind whatever was in front.
        //
        SetForegroundWindow(Dialog);
        return TRUE;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        brush = ThemeOnCtlColor(WParam, false);
        if (brush != 0)
        {
            return brush;
        }
        break;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        brush = ThemeOnCtlColor(WParam, true);
        if (brush != 0)
        {
            return brush;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(WParam))
        {
        case IDOK:
            if (Apply(Dialog))
            {
                EndDialog(Dialog, IDOK);
            }
            return TRUE;

        case IDCANCEL:
            EndDialog(Dialog, IDCANCEL);
            return TRUE;

        case IDC_CLEAR_RECENT:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                PopupClearRecent();

                //
                // The button is about to be disabled, which would leave the
                // keyboard focus nowhere.
                //
                SendMessageW(Dialog, WM_NEXTDLGCTL, RCAST(WPARAM)(GetDlgItem(Dialog, IDOK)), TRUE);
                SetDlgItemTextW(Dialog, IDC_CLEAR_RECENT, L"Cleared");
                EnableWindow(GetDlgItem(Dialog, IDC_CLEAR_RECENT), FALSE);
            }
            return TRUE;

        case IDC_SOURCE_SHOW:
            if (HIWORD(WParam) == CBN_SELCHANGE)
            {
                selected = ListView_GetNextItem(GetDlgItem(Dialog, IDC_SOURCES_LIST), -1, LVNI_SELECTED);
                choice = SendDlgItemMessageW(Dialog, IDC_SOURCE_SHOW, CB_GETCURSEL, 0, 0);

                if ((selected >= 0) && (choice >= 0))
                {
                    g_PendingShows[SCAST(size_t)(selected)] = SCAST(SourceShow)(choice);
                    ShowSourceRow(Dialog, selected);
                }
            }
            return TRUE;

        case IDC_USERCODES:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                UserCodesDialogShow(Dialog);
            }
            return TRUE;

        case IDC_SOURCES_RESET:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                for (size_t i = 0; i < g_Sources.size(); i++)
                {
                    g_PendingShows[i] = g_Sources[i].Default;
                    ShowSourceRow(Dialog, SCAST(int)(i));
                }

                ShowSourceChoice(Dialog);
            }
            return TRUE;

        case IDC_HOTKEY_WIN:
        case IDC_HOTKEY_CTRL:
        case IDC_HOTKEY_ALT:
        case IDC_HOTKEY_SHIFT:
        case IDC_HOTKEY_KEY:
            //
            // Whatever was wrong with the last choice doesn't apply to this
            // one.
            //
            SetStatus(Dialog, std::wstring());
            return TRUE;
        }
        break;

    case WM_NOTIFY:
        notify = RCAST(const NMHDR*)(LParam);
        if ((notify->idFrom == IDC_SOURCES_LIST) && (notify->code == LVN_ITEMCHANGED))
        {
            ShowSourceChoice(Dialog);
        }
        break;

    case WM_DESTROY:
        g_Dialog = nullptr;

        if (g_HeadingFont != nullptr)
        {
            DeleteObject(g_HeadingFont);
            g_HeadingFont = nullptr;
        }
        break;
    }

    return FALSE;
}


/*!

    @brief Shows the settings dialog, or brings it forward if it's open.

    @param[in] Owner - The window that owns it, or nullptr.

*/
void
SettingsDialogShow (
    _In_opt_ HWND Owner
    )
{
    if (g_Dialog != nullptr)
    {
        SetForegroundWindow(g_Dialog);
        return;
    }

    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SETTINGS), Owner, SettingsDialogProc, 0);
}


/*!

    @brief Fills the settings dialog's list of sources again, if it's open,
           after your headers are read again.

*/
void
SettingsDialogOnCodesReloaded (
    void
    )
{
    if (g_Dialog != nullptr)
    {
        FillSourceRows(g_Dialog);
    }
}
