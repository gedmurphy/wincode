/*!

    @file Gui/Theme.cpp

    @brief Dark mode, following the Windows light/dark setting.

    @details Adapted from FileMon++'s Theme.cpp. Win32 controls don't follow
             the app theme by themselves, and there's no documented API to make
             them. There's a handful of unnamed uxtheme exports, which every
             Win32 app with a dark mode uses. We get them by ordinal and fall
             back to light if any's missing, so a Windows that drops them only
             costs the look. The dark title bar is the one documented part,
             through DwmSetWindowAttribute.

             Unlike FileMon++ there's no theme setting. We follow Windows,
             including when it changes while we're running.

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

#include <dwmapi.h>
#include <uxtheme.h>


//
// The undocumented uxtheme exports. 135 was AllowDarkModeForApp(BOOL) before
// 1903, and AllowDark is 1, so it's TRUE to the old one too.
//
enum class PreferredAppMode
{
    Default = 0,
    AllowDark = 1,
    ForceDark = 2,
    ForceLight = 3
};

typedef BOOL (WINAPI *ShouldAppsUseDarkModeRoutine)(void);
typedef BOOL (WINAPI *AllowDarkModeForWindowRoutine)(HWND Window, BOOL Allow);
typedef PreferredAppMode (WINAPI *SetPreferredAppModeRoutine)(PreferredAppMode Mode);
typedef void (WINAPI *RefreshImmersiveColorPolicyStateRoutine)(void);
typedef void (WINAPI *FlushMenuThemesRoutine)(void);

static ShouldAppsUseDarkModeRoutine g_ShouldAppsUseDarkMode;
static AllowDarkModeForWindowRoutine g_AllowDarkModeForWindow;
static SetPreferredAppModeRoutine g_SetPreferredAppMode;
static RefreshImmersiveColorPolicyStateRoutine g_RefreshImmersiveColorPolicyState;
static FlushMenuThemesRoutine g_FlushMenuThemes;

//
// DWMWA_USE_IMMERSIVE_DARK_MODE moved between 1809 and 2004. We try both,
// oldest last, since setting the wrong one is harmless.
//
static constexpr DWORD k_ImmersiveDarkMode = 20;
static constexpr DWORD k_ImmersiveDarkModeLegacy = 19;

//
// Dark's FileMon++'s, close to the shell's, with text short of white so it
// doesn't glare. Light is system colours. Both add Windows 11's text shades.
//
static constexpr COLORREF k_DarkBackground = RGB(32, 32, 32);
static constexpr COLORREF k_DarkControl = RGB(43, 43, 43);
static constexpr COLORREF k_DarkText = RGB(240, 240, 240);
static constexpr COLORREF k_DarkMuted = RGB(184, 184, 184);
static constexpr COLORREF k_DarkFaint = RGB(140, 140, 140);
static constexpr COLORREF k_DarkAccent = RGB(0x60, 0xCD, 0xFF);
static constexpr COLORREF k_DarkSelection = RGB(0x3A, 0x3A, 0x3A);

static constexpr COLORREF k_LightMuted = RGB(0x5F, 0x5F, 0x5F);
static constexpr COLORREF k_LightFaint = RGB(0x8C, 0x8C, 0x8C);
static constexpr COLORREF k_LightAccent = RGB(0x00, 0x5F, 0xB8);
static constexpr COLORREF k_LightSelection = RGB(0xEB, 0xEB, 0xEB);

//
// Behind the part of a name a search matched, the accent at about a seventh
// of its strength over the content ground.
//
static constexpr COLORREF k_DarkMatch = RGB(0x32, 0x42, 0x49);
static constexpr COLORREF k_LightMatch = RGB(0xE3, 0xEC, 0xF6);

static HBRUSH g_BackgroundBrush;
static HBRUSH g_ControlBrush;
static bool g_Dark;
static bool g_Initialised;


/*!

    @brief Resolves one uxtheme export by ordinal.

    @param[in] Module - The uxtheme module.

    @param[in] Ordinal - The ordinal to resolve.

    @return The entry point, or nullptr.

*/
static
void*
ResolveOrdinal (
    _In_ HMODULE Module,
    _In_ WORD Ordinal
    )
{
    return RCAST(void*)(GetProcAddress(Module, MAKEINTRESOURCEA(Ordinal)));
}


/*!

    @brief Releases the cached brushes so they are rebuilt in the new colours.

*/
static
void
ReleaseBrushes (
    void
    )
{
    if (g_BackgroundBrush != nullptr)
    {
        DeleteObject(g_BackgroundBrush);
        g_BackgroundBrush = nullptr;
    }

    if (g_ControlBrush != nullptr)
    {
        DeleteObject(g_ControlBrush);
        g_ControlBrush = nullptr;
    }
}


/*!

    @brief Reads the debug build's theme override, if there is one.

    @details Debug builds only. WINCODE_THEME=dark or light overrides Windows,
             so you can check both themes without changing the machine.

    @param[out] Dark - Receives whether the override asks for dark.

    @return True if an override applies.

*/
static
bool
DebugThemeOverride (
    _Out_ bool* Dark
    )
{
    *Dark = false;

#if defined(_DEBUG)
    WCHAR mode[16];

    if (GetEnvironmentVariableW(L"WINCODE_THEME", mode, ARRAYSIZE(mode)) != 0)
    {
        *Dark = (_wcsicmp(mode, L"dark") == 0);
        return true;
    }
#endif

    return false;
}


/*!

    @brief Sets up dark mode support for the process and reads the Windows
           setting.

    @details Call once, before any window is created.

*/
void
ThemeInitialise (
    void
    )
{
    HMODULE uxtheme;
    bool dark;

    if (g_Initialised)
    {
        return;
    }

    g_Initialised = true;

    uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (uxtheme == nullptr)
    {
        return;
    }

    g_RefreshImmersiveColorPolicyState =
        RCAST(RefreshImmersiveColorPolicyStateRoutine)(ResolveOrdinal(uxtheme, 104));
    g_ShouldAppsUseDarkMode =
        RCAST(ShouldAppsUseDarkModeRoutine)(ResolveOrdinal(uxtheme, 132));
    g_AllowDarkModeForWindow =
        RCAST(AllowDarkModeForWindowRoutine)(ResolveOrdinal(uxtheme, 133));
    g_SetPreferredAppMode =
        RCAST(SetPreferredAppModeRoutine)(ResolveOrdinal(uxtheme, 135));
    g_FlushMenuThemes =
        RCAST(FlushMenuThemesRoutine)(ResolveOrdinal(uxtheme, 136));

    //
    // AllowDark, not ForceDark, since we follow Windows rather than override
    // someone who chose light. Only the debug override forces it.
    //
    if (g_SetPreferredAppMode != nullptr)
    {
        if (DebugThemeOverride(&dark))
        {
            g_SetPreferredAppMode(dark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
        }
        else
        {
            g_SetPreferredAppMode(PreferredAppMode::AllowDark);
        }
    }

    ThemeRefresh();
}


/*!

    @brief Reads the Windows light/dark setting again. Call when Windows
           broadcasts WM_SETTINGCHANGE with "ImmersiveColorSet".

*/
void
ThemeRefresh (
    void
    )
{
    bool dark;

    if (g_RefreshImmersiveColorPolicyState != nullptr)
    {
        g_RefreshImmersiveColorPolicyState();
    }

    if (DebugThemeOverride(&dark))
    {
        g_Dark = dark;
    }
    else
    {
        g_Dark = (g_ShouldAppsUseDarkMode != nullptr) && (g_ShouldAppsUseDarkMode() != FALSE);
    }

    ReleaseBrushes();

    if (g_FlushMenuThemes != nullptr)
    {
        g_FlushMenuThemes();
    }
}


/*!

    @brief Whether WinCode is currently dark.

*/
bool
ThemeIsDark (
    void
    )
{
    return g_Dark;
}


COLORREF
ThemeTextColour (
    void
    )
{
    return g_Dark ? k_DarkText : GetSysColor(COLOR_WINDOWTEXT);
}


COLORREF
ThemeMutedColour (
    void
    )
{
    return g_Dark ? k_DarkMuted : k_LightMuted;
}


COLORREF
ThemeFaintColour (
    void
    )
{
    return g_Dark ? k_DarkFaint : k_LightFaint;
}


COLORREF
ThemeAccentColour (
    void
    )
{
    return g_Dark ? k_DarkAccent : k_LightAccent;
}


COLORREF
ThemeSelectionColour (
    void
    )
{
    return g_Dark ? k_DarkSelection : k_LightSelection;
}


COLORREF
ThemeMatchColour (
    void
    )
{
    return g_Dark ? k_DarkMatch : k_LightMatch;
}


COLORREF
ThemeBackgroundColour (
    void
    )
{
    return g_Dark ? k_DarkBackground : GetSysColor(COLOR_BTNFACE);
}


COLORREF
ThemeControlColour (
    void
    )
{
    return g_Dark ? k_DarkControl : GetSysColor(COLOR_WINDOW);
}


/*!

    @brief The colour a kind of code is labelled in.

    @param[in] Kind - The kind.

    @return The colour, for the current theme.

*/
COLORREF
ThemeKindColour (
    _In_ CodeKind Kind
    )
{
    //
    // One hue per kind, from the mockups, with the same lightness and
    // saturation in each theme so it reads on white and on the dark ground.
    //
    switch (Kind)
    {
    case CodeKind::KnownValue:
        return g_Dark ? RGB(0x6F, 0xD0, 0xE0) : RGB(0x0E, 0x6E, 0x80);

    case CodeKind::WinError:
        return g_Dark ? RGB(0x8F, 0xB8, 0xF0) : RGB(0x2E, 0x62, 0xA8);

    case CodeKind::HResult:
        return g_Dark ? RGB(0xC6, 0xA6, 0xF2) : RGB(0x7A, 0x4A, 0xA8);

    case CodeKind::NtStatus:
        return g_Dark ? RGB(0xE0, 0xB8, 0x76) : RGB(0x93, 0x61, 0x1E);

    case CodeKind::BugCheck:
        return g_Dark ? RGB(0xF2, 0x9A, 0x8F) : RGB(0xB3, 0x39, 0x2F);

    case CodeKind::WindowMessage:
        return g_Dark ? RGB(0x86, 0xD4, 0xA5) : RGB(0x2F, 0x7D, 0x4F);

    case CodeKind::DeviceProblem:
        return g_Dark ? RGB(0xF0, 0xA8, 0xD0) : RGB(0xA8, 0x2F, 0x6F);

    case CodeKind::UsbStatus:
        return g_Dark ? RGB(0xC4, 0xCC, 0x7A) : RGB(0x5E, 0x6B, 0x1A);

    case CodeKind::LdapError:
        return g_Dark ? RGB(0xA8, 0xB8, 0xD0) : RGB(0x4A, 0x5A, 0x78);

    case CodeKind::PoolTag:
        return g_Dark ? RGB(0xD8, 0xC0, 0x9A) : RGB(0x7A, 0x5C, 0x2E);

    case CodeKind::Ioctl:
        return g_Dark ? RGB(0x8C, 0xD0, 0xC8) : RGB(0x1F, 0x6B, 0x63);

    case CodeKind::Flags:
        return g_Dark ? RGB(0xB0, 0xB0, 0xB0) : RGB(0x55, 0x55, 0x55);
    }

    return ThemeTextColour();
}


HBRUSH
ThemeBackgroundBrush (
    void
    )
{
    if (g_BackgroundBrush == nullptr)
    {
        g_BackgroundBrush = CreateSolidBrush(ThemeBackgroundColour());
    }

    return g_BackgroundBrush;
}


HBRUSH
ThemeControlBrush (
    void
    )
{
    if (g_ControlBrush == nullptr)
    {
        g_ControlBrush = CreateSolidBrush(ThemeControlColour());
    }

    return g_ControlBrush;
}


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
    )
{
    HDC dc;

    //
    // In light the system colours are right except for content, which a read
    // only edit would draw in the window's grey.
    //
    if (!g_Dark && !Content)
    {
        return 0;
    }

    dc = RCAST(HDC)(WParam);
    SetTextColor(dc, ThemeTextColour());

    if (Content)
    {
        SetBkColor(dc, ThemeControlColour());
        return RCAST(INT_PTR)(ThemeControlBrush());
    }

    SetBkColor(dc, ThemeBackgroundColour());
    return RCAST(INT_PTR)(ThemeBackgroundBrush());
}


/*!

    @brief Themes the list a drop-down list box opens, which is a window of its
           own that EnumChildWindows doesn't reach.

    @param[in] ComboBox - The drop-down list box.

*/
static
void
ThemeComboBoxList (
    _In_ HWND ComboBox
    )
{
    COMBOBOXINFO info = {};

    info.cbSize = sizeof(info);
    if (!GetComboBoxInfo(ComboBox, &info) || (info.hwndList == nullptr))
    {
        return;
    }

    if (g_AllowDarkModeForWindow != nullptr)
    {
        g_AllowDarkModeForWindow(info.hwndList, g_Dark ? TRUE : FALSE);
    }

    SetWindowTheme(info.hwndList, g_Dark ? L"DarkMode_Explorer" : nullptr, nullptr);
}


//
// The subclass that draws a list view's header in dark.
//
static constexpr UINT_PTR k_DarkHeaderSubclass = 1;


/*!

    @brief Draws a list view's header in the theme's colours, which a header
           with its styles taken off won't do itself.

    @details The header tells its parent, the list view, about each stage of
             painting, so we subclass that. In light we leave the header be.

*/
static
LRESULT
CALLBACK
DarkHeaderProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam,
    _In_ UINT_PTR Id,
    _In_ DWORD_PTR Data
    )
{
    NMCUSTOMDRAW* draw;
    WCHAR text[128];
    HDITEMW item = {};
    HPEN pen;
    HGDIOBJ oldPen;
    RECT bounds;
    RECT last;
    int count;

    UNREFERENCED_PARAMETER(Data);

    if (Message == WM_NCDESTROY)
    {
        RemoveWindowSubclass(Window, DarkHeaderProc, Id);
    }

    if ((Message != WM_NOTIFY) || !g_Dark)
    {
        return DefSubclassProc(Window, Message, WParam, LParam);
    }

    draw = RCAST(NMCUSTOMDRAW*)(LParam);
    if ((draw->hdr.hwndFrom != ListView_GetHeader(Window)) || (draw->hdr.code != NM_CUSTOMDRAW))
    {
        return DefSubclassProc(Window, Message, WParam, LParam);
    }

    switch (draw->dwDrawStage)
    {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;

    case CDDS_POSTPAINT:
        //
        // The space past the last column, which the header paints itself
        // after the columns.
        //
        GetClientRect(draw->hdr.hwndFrom, &bounds);
        count = Header_GetItemCount(draw->hdr.hwndFrom);
        if ((count > 0) && Header_GetItemRect(draw->hdr.hwndFrom, count - 1, &last))
        {
            bounds.left = last.right;
        }

        FillRect(draw->hdc, &bounds, ThemeControlBrush());
        return CDRF_DODEFAULT;

    case CDDS_ITEMPREPAINT:
        item.mask = HDI_TEXT;
        item.pszText = text;
        item.cchTextMax = ARRAYSIZE(text);
        text[0] = L'\0';
        Header_GetItem(draw->hdr.hwndFrom, SCAST(int)(draw->dwItemSpec), &item);

        bounds = draw->rc;
        FillRect(draw->hdc, &bounds, ThemeControlBrush());

        pen = CreatePen(PS_SOLID, 1, ThemeFaintColour());
        oldPen = SelectObject(draw->hdc, pen);
        MoveToEx(draw->hdc, bounds.right - 1, bounds.top, nullptr);
        LineTo(draw->hdc, bounds.right - 1, bounds.bottom);
        MoveToEx(draw->hdc, bounds.left, bounds.bottom - 1, nullptr);
        LineTo(draw->hdc, bounds.right, bounds.bottom - 1);
        SelectObject(draw->hdc, oldPen);
        DeleteObject(pen);

        bounds.left += 6;
        SetBkMode(draw->hdc, TRANSPARENT);
        SetTextColor(draw->hdc, ThemeMutedColour());
        DrawTextW(draw->hdc, text, -1, &bounds, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        return CDRF_SKIPDEFAULT;
    }

    return DefSubclassProc(Window, Message, WParam, LParam);
}


/*!

    @brief Applies the right theme class to one control.

    @param[in] Window - The control.

*/
static
void
ThemeChild (
    _In_ HWND Window
    )
{
    WCHAR className[64];
    HWND header;

    if (g_AllowDarkModeForWindow != nullptr)
    {
        g_AllowDarkModeForWindow(Window, g_Dark ? TRUE : FALSE);
    }

    className[0] = L'\0';
    GetClassNameW(Window, className, ARRAYSIZE(className));

    if (_wcsicmp(className, WC_LISTVIEWW) == 0)
    {
        SetWindowTheme(Window, g_Dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);

        ListView_SetBkColor(Window, ThemeControlColour());
        ListView_SetTextBkColor(Window, ThemeControlColour());
        ListView_SetTextColor(Window, ThemeTextColour());

        //
        // The header's its own control. DarkMode_ItemsView works on some builds
        // and not others, so in dark we take its styles off instead.
        //
        header = ListView_GetHeader(Window);
        if (header != nullptr)
        {
            SetWindowTheme(header, g_Dark ? L"" : nullptr, g_Dark ? L"" : nullptr);
            SetWindowSubclass(Window, DarkHeaderProc, k_DarkHeaderSubclass, 0);
            InvalidateRect(header, nullptr, TRUE);
        }

        return;
    }

    if (_wcsicmp(className, WC_EDITW) == 0)
    {
        SetWindowTheme(Window, g_Dark ? L"DarkMode_CFD" : nullptr, nullptr);
        return;
    }

    //
    // A drop-down list draws dark with the class Explorer's file dialogs use,
    // not DarkMode_Explorer. Its list is a window of its own, themed with it.
    //
    if (_wcsicmp(className, WC_COMBOBOXW) == 0)
    {
        SetWindowTheme(Window, g_Dark ? L"DarkMode_CFD" : nullptr, nullptr);
        ThemeComboBoxList(Window);
        return;
    }

    SetWindowTheme(Window, g_Dark ? L"DarkMode_Explorer" : nullptr, nullptr);
}


/*!

    @brief EnumChildWindows callback that themes each child.

*/
static
BOOL
CALLBACK
ThemeChildProc (
    _In_ HWND Window,
    _In_ LPARAM Parameter
    )
{
    UNREFERENCED_PARAMETER(Parameter);

    ThemeChild(Window);
    return TRUE;
}


/*!

    @brief Applies the theme to a top-level window, its title bar and every
           control in it.

    @param[in] Window - The window.

*/
void
ThemeApplyToWindow (
    _In_ HWND Window
    )
{
    BOOL dark;

    if (g_AllowDarkModeForWindow != nullptr)
    {
        g_AllowDarkModeForWindow(Window, g_Dark ? TRUE : FALSE);
    }

    //
    // Without this the caption stays light and the whole window looks like a
    // mistake.
    //
    dark = g_Dark ? TRUE : FALSE;
    if (FAILED(DwmSetWindowAttribute(Window, k_ImmersiveDarkMode, &dark, sizeof(dark))))
    {
        DwmSetWindowAttribute(Window, k_ImmersiveDarkModeLegacy, &dark, sizeof(dark));
    }

    EnumChildWindows(Window, ThemeChildProc, 0);
}


/*!

    @brief Applies the theme to a popup, such as a tooltip, which isn't a child
           and so isn't reached by ThemeApplyToWindow.

    @param[in] Popup - The popup.

*/
void
ThemeApplyToPopup (
    _In_ HWND Popup
    )
{
    if (g_AllowDarkModeForWindow != nullptr)
    {
        g_AllowDarkModeForWindow(Popup, g_Dark ? TRUE : FALSE);
    }

    SetWindowTheme(Popup, g_Dark ? L"DarkMode_Explorer" : nullptr, nullptr);
}
