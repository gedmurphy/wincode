/*!

    @file Gui/UserCodesDialog.cpp

    @brief The Your codes dialog, and the watch on the user's headers that
           reads them again when they change.

    @details Changes take effect at once, not on OK. Adding a header or changing
             what it's treated as reads it there and then, so the dialog can
             show what came of it.

             The watch is a thread waiting on change notifications for the
             codes folder and each added header's folder. It just tells the
             main window, which reads a moment after the last change. So a
             build that writes a header several times reads it once, and
             nothing reads the tables while they're being replaced.

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
#include <shellapi.h>
#include <shobjidl.h>


//
// WaitForMultipleObjects takes 64 handles, one of which is the stop event.
//
static constexpr size_t k_MaxWatched = MAXIMUM_WAIT_OBJECTS - 1;

//
// The dialog while it's open.
//
static HWND g_CodesDialog;
static HFONT g_CodesHeadingFont;

//
// The headers the dialog lists, in its order.
//
static std::vector<UserHeader> g_Headers;

//
// The watch, the window it tells, its thread and the event that stops it.
//
static HWND g_WatchOwner;
static HANDLE g_WatchThread;
static HANDLE g_WatchStop;


/*!

    @brief What the watch thread is given.

*/
struct WatchContext
{
    HWND Owner;
    HANDLE Stop;
    std::vector<std::wstring> Folders;
};


/*!

    @brief Waits for a change in any of the watched folders, and tells the
           main window of each.

    @param[in] Parameter - The WatchContext, which the thread owns.

    @return Zero.

*/
static
DWORD
WINAPI
WatchThread (
    _In_ LPVOID Parameter
    )
{
    WatchContext* context;
    std::vector<HANDLE> handles;
    HANDLE change;
    DWORD waited;

    context = SCAST(WatchContext*)(Parameter);
    handles.push_back(context->Stop);

    for (const std::wstring& folder : context->Folders)
    {
        change = FindFirstChangeNotificationW(folder.c_str(),
                                              FALSE,
                                              FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
        if ((change != INVALID_HANDLE_VALUE) && (handles.size() <= k_MaxWatched))
        {
            handles.push_back(change);
        }
        else if (change != INVALID_HANDLE_VALUE)
        {
            FindCloseChangeNotification(change);
        }
    }

    for (;;)
    {
        waited = WaitForMultipleObjects(SCAST(DWORD)(handles.size()), handles.data(), FALSE, INFINITE);

        if ((waited == WAIT_OBJECT_0) || (waited < WAIT_OBJECT_0) || (waited >= (WAIT_OBJECT_0 + handles.size())))
        {
            break;
        }

        PostMessageW(context->Owner, k_UserCodesMessage, 0, 0);
        FindNextChangeNotification(handles[waited - WAIT_OBJECT_0]);
    }

    for (size_t i = 1; i < handles.size(); i++)
    {
        FindCloseChangeNotification(handles[i]);
    }

    delete context;
    return 0;
}


/*!

    @brief Stops watching the user's headers.

*/
void
UserCodesWatchStop (
    void
    )
{
    if (g_WatchThread == nullptr)
    {
        return;
    }

    SetEvent(g_WatchStop);
    WaitForSingleObject(g_WatchThread, INFINITE);

    CloseHandle(g_WatchThread);
    CloseHandle(g_WatchStop);
    g_WatchThread = nullptr;
    g_WatchStop = nullptr;
}


/*!

    @brief Starts watching the user's headers for changes, or starts again
           after the folders to watch change.

    @param[in] Owner - The window to tell of a change, or null for the one
                       given before.

*/
void
UserCodesWatchStart (
    _In_opt_ HWND Owner
    )
{
    WatchContext* context;
    std::wstring folder;

    UserCodesWatchStop();

    if (Owner != nullptr)
    {
        g_WatchOwner = Owner;
    }

    if (g_WatchOwner == nullptr)
    {
        return;
    }

    //
    // Make the codes folder up front, so a header dropped in gets noticed and
    // Open folder has somewhere to open.
    //
    folder = UserCodesFolder();
    if (!folder.empty())
    {
        CreateDirectoryW(folder.c_str(), nullptr);
    }

    g_WatchStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_WatchStop == nullptr)
    {
        return;
    }

    context = new WatchContext();
    context->Owner = g_WatchOwner;
    context->Stop = g_WatchStop;
    context->Folders = UserCodesWatchFolders();

    g_WatchThread = CreateThread(nullptr, 0, WatchThread, context, 0, nullptr);
    if (g_WatchThread == nullptr)
    {
        delete context;
        CloseHandle(g_WatchStop);
        g_WatchStop = nullptr;
    }
}


/*!

    @brief Shows the selected header's path, its setting and what was skipped,
           or what the dialog is for if none is selected.

    @param[in] Dialog - The dialog.

*/
static
void
ShowHeaderDetails (
    _In_ HWND Dialog
    )
{
    std::wstring details;
    const UserHeader* header;
    int selected;

    selected = ListView_GetNextItem(GetDlgItem(Dialog, IDC_UC_LIST), -1, LVNI_SELECTED);
    header = ((selected >= 0) && (SCAST(size_t)(selected) < g_Headers.size())) ? &g_Headers[SCAST(size_t)(selected)] : nullptr;

    EnableWindow(GetDlgItem(Dialog, IDC_UC_TREATAS), (header != nullptr) ? TRUE : FALSE);
    EnableWindow(GetDlgItem(Dialog, IDC_UC_REMOVE), ((header != nullptr) && header->Added) ? TRUE : FALSE);
    SendDlgItemMessageW(Dialog,
                        IDC_UC_TREATAS,
                        CB_SETCURSEL,
                        (header != nullptr) ? SCAST(WPARAM)(header->TreatAs) : SCAST(WPARAM)(-1),
                        0);

    if (header == nullptr)
    {
        details = std::format(L"Put headers in {}, or use Add file to read one where it is, "
                              L"such as in a source tree.",
                              UserCodesFolder());
    }
    else if (!header->Readable)
    {
        details = std::format(L"{}\r\n\r\nThis file can't be read. It may have been moved or deleted.", header->Path);
    }
    else if (header->Skipped.empty())
    {
        details = std::format(L"{}\r\nRead as a {}.\r\n\r\nEvery definition that looks like a code was read.",
                              header->Path,
                              header->Format);
    }
    else
    {
        details = std::format(L"{}\r\nRead as a {}.\r\n\r\nThese look like codes, but WinCode couldn't work out their values:",
                              header->Path,
                              header->Format);
        for (const HeaderSkip& skip : header->Skipped)
        {
            details += std::format(L"\r\nLine {}, {}: {}", skip.Line, skip.Name, skip.Reason);
        }
    }

    SetDlgItemTextW(Dialog, IDC_UC_DETAILS, details.c_str());
}


/*!

    @brief Lists the user's headers as last read, keeping the selection.

    @param[in] Dialog - The dialog.

*/
static
void
FillHeaders (
    _In_ HWND Dialog
    )
{
    std::wstring selectedPath;
    std::wstring text;
    LVITEMW item = {};
    size_t separator;
    HWND list;
    int selected;
    int select;

    list = GetDlgItem(Dialog, IDC_UC_LIST);

    selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if ((selected >= 0) && (SCAST(size_t)(selected) < g_Headers.size()))
    {
        selectedPath = g_Headers[SCAST(size_t)(selected)].Path;
    }

    ListView_DeleteAllItems(list);
    g_Headers = UserHeaders();
    select = g_Headers.empty() ? -1 : 0;

    for (size_t i = 0; i < g_Headers.size(); i++)
    {
        const UserHeader& header = g_Headers[i];

        separator = header.Path.find_last_of(L"\\/");
        text = (separator == std::wstring::npos) ? header.Path : header.Path.substr(separator + 1);

        item.mask = LVIF_TEXT;
        item.iItem = SCAST(int)(i);
        item.iSubItem = 0;
        item.pszText = text.data();
        ListView_InsertItem(list, &item);

        text = header.Readable ? std::to_wstring(header.CodeCount) : L"-";
        ListView_SetItemText(list, SCAST(int)(i), 1, text.data());

        text = header.Readable ? std::to_wstring(header.Skipped.size()) : L"-";
        ListView_SetItemText(list, SCAST(int)(i), 2, text.data());

        text = !header.Readable ? L"Can't be read" : (header.Added ? L"Added" : L"Codes folder");
        ListView_SetItemText(list, SCAST(int)(i), 3, text.data());

        if (_wcsicmp(header.Path.c_str(), selectedPath.c_str()) == 0)
        {
            select = SCAST(int)(i);
        }
    }

    if (select >= 0)
    {
        ListView_SetItemState(list, select, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(list, select, FALSE);
    }

    ShowHeaderDetails(Dialog);
}


/*!

    @brief Reads the user's headers again, and refreshes whatever shows them.

*/
void
UserCodesReload (
    void
    )
{
    LoadUserCodes();
    MainWindowRefresh();
    SettingsDialogOnCodesReloaded();

    if (g_CodesDialog != nullptr)
    {
        FillHeaders(g_CodesDialog);
    }
}


/*!

    @brief Asks for a header to add, with the common file dialog.

    @param[in] Dialog - The dialog, which owns the file dialog.

    @param[out] Path - Receives the header chosen.

    @return True if one was chosen.

*/
static
bool
ChooseHeader (
    _In_ HWND Dialog,
    _Out_ std::wstring* Path
    )
{
    std::wstring patterns;
    IFileOpenDialog* picker;
    IShellItem* item;
    PWSTR chosen;
    bool ok;

    Path->clear();
    picker = nullptr;
    item = nullptr;
    chosen = nullptr;
    ok = false;

    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker))))
    {
        return false;
    }

    //
    // Whatever the formats read, so a new one shows up here too.
    //
    patterns = UserCodeFilePatterns();
    COMDLG_FILTERSPEC filters[] =
    {
        { L"Headers and tag lists", patterns.c_str() },
        { L"All files", L"*.*" }
    };

    picker->SetFileTypes(ARRAYSIZE(filters), filters);
    picker->SetTitle(L"Add a file of your codes");

    if (SUCCEEDED(picker->Show(Dialog)) &&
        SUCCEEDED(picker->GetResult(&item)) &&
        SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &chosen)))
    {
        *Path = chosen;
        ok = true;
    }

    if (chosen != nullptr)
    {
        CoTaskMemFree(chosen);
    }

    if (item != nullptr)
    {
        item->Release();
    }

    picker->Release();
    return ok;
}


/*!

    @brief Sets up the list's columns, the Treat as choices and the heading.

    @param[in] Dialog - The dialog.

*/
static
void
SetUpDialog (
    _In_ HWND Dialog
    )
{
    static constexpr PCWSTR k_Columns[] = { L"Header", L"Codes", L"Skipped", L"Where" };
    static constexpr int k_Widths[] = { 45, 15, 15, 25 };
    static constexpr PCWSTR k_TreatAs[] = { L"Decide from each code", L"NTSTATUS codes", L"HRESULTs", L"Win32 errors", L"Pool tags", L"IOCTLs" };

    LVCOLUMNW column = {};
    LOGFONTW font;
    HFONT dialogFont;
    RECT client;
    HWND list;
    int width;

    list = GetDlgItem(Dialog, IDC_UC_LIST);
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    GetClientRect(list, &client);
    width = client.right - GetSystemMetricsForDpi(SM_CXVSCROLL, GetDpiForWindow(Dialog));

    for (size_t i = 0; i < ARRAYSIZE(k_Columns); i++)
    {
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.pszText = CCAST(PWSTR)(k_Columns[i]);
        column.cx = (width * k_Widths[i]) / 100;
        ListView_InsertColumn(list, SCAST(int)(i), &column);
    }

    for (PCWSTR choice : k_TreatAs)
    {
        SendDlgItemMessageW(Dialog, IDC_UC_TREATAS, CB_ADDSTRING, 0, RCAST(LPARAM)(choice));
    }

    dialogFont = RCAST(HFONT)(SendMessageW(Dialog, WM_GETFONT, 0, 0));
    if ((dialogFont != nullptr) && (GetObjectW(dialogFont, sizeof(font), &font) != 0))
    {
        font.lfWeight = FW_SEMIBOLD;
        g_CodesHeadingFont = CreateFontIndirectW(&font);
        if (g_CodesHeadingFont != nullptr)
        {
            SendDlgItemMessageW(Dialog, IDC_UC_HEADING, WM_SETFONT, RCAST(WPARAM)(g_CodesHeadingFont), FALSE);
        }
    }
}


/*!

    @brief The selected header, if there is one.

    @param[in] Dialog - The dialog.

    @return The header, or null.

*/
static
const UserHeader*
SelectedHeader (
    _In_ HWND Dialog
    )
{
    int selected;

    selected = ListView_GetNextItem(GetDlgItem(Dialog, IDC_UC_LIST), -1, LVNI_SELECTED);
    if ((selected < 0) || (SCAST(size_t)(selected) >= g_Headers.size()))
    {
        return nullptr;
    }

    return &g_Headers[SCAST(size_t)(selected)];
}


/*!

    @brief The Your codes dialog's procedure.

*/
static
INT_PTR
CALLBACK
UserCodesDialogProc (
    _In_ HWND Dialog,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam
    )
{
    const UserHeader* header;
    const NMHDR* notify;
    std::wstring path;
    std::wstring folder;
    INT_PTR brush;
    LRESULT choice;

    switch (Message)
    {
    case WM_INITDIALOG:
        g_CodesDialog = Dialog;

        SetUpDialog(Dialog);
        g_Headers.clear();
        FillHeaders(Dialog);

        ThemeApplyToWindow(Dialog);
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
        case IDCANCEL:
            EndDialog(Dialog, IDCANCEL);
            return TRUE;

        case IDC_UC_ADD:
            if ((HIWORD(WParam) == BN_CLICKED) && ChooseHeader(Dialog, &path))
            {
                UserCodesAddFile(path.c_str());

                //
                // A header elsewhere means another folder to watch.
                //
                UserCodesWatchStart(nullptr);
                UserCodesReload();
            }
            return TRUE;

        case IDC_UC_REMOVE:
            header = SelectedHeader(Dialog);
            if ((HIWORD(WParam) == BN_CLICKED) && (header != nullptr) && header->Added)
            {
                path = header->Path;
                UserCodesRemoveFile(path.c_str());
                UserCodesWatchStart(nullptr);
                UserCodesReload();
            }
            return TRUE;

        case IDC_UC_TREATAS:
            header = SelectedHeader(Dialog);
            choice = SendDlgItemMessageW(Dialog, IDC_UC_TREATAS, CB_GETCURSEL, 0, 0);
            if ((HIWORD(WParam) == CBN_SELCHANGE) &&
                (header != nullptr) &&
                (choice >= 0) &&
                (SCAST(UserTreatAs)(choice) != header->TreatAs))
            {
                path = header->Path;
                UserCodesSetTreatAs(path.c_str(), SCAST(UserTreatAs)(choice));
                UserCodesReload();
            }
            return TRUE;

        case IDC_UC_FOLDER:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                folder = UserCodesFolder();
                if (!folder.empty())
                {
                    CreateDirectoryW(folder.c_str(), nullptr);
                    ShellExecuteW(Dialog, L"explore", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return TRUE;

        case IDC_UC_RELOAD:
            if (HIWORD(WParam) == BN_CLICKED)
            {
                UserCodesReload();
            }
            return TRUE;
        }
        break;

    case WM_NOTIFY:
        notify = RCAST(const NMHDR*)(LParam);
        if ((notify->idFrom == IDC_UC_LIST) && (notify->code == LVN_ITEMCHANGED))
        {
            ShowHeaderDetails(Dialog);
        }
        break;

    case WM_DESTROY:
        g_CodesDialog = nullptr;

        if (g_CodesHeadingFont != nullptr)
        {
            DeleteObject(g_CodesHeadingFont);
            g_CodesHeadingFont = nullptr;
        }
        break;
    }

    return FALSE;
}


/*!

    @brief Shows the Your codes dialog.

    @param[in] Owner - The window that owns it.

*/
void
UserCodesDialogShow (
    _In_opt_ HWND Owner
    )
{
    if (g_CodesDialog != nullptr)
    {
        SetForegroundWindow(g_CodesDialog);
        return;
    }

    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_USERCODES), Owner, UserCodesDialogProc, 0);
}
