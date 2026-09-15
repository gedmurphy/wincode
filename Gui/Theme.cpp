/*!

    @file Gui/Theme.cpp

    @brief Dark mode, following the Windows light/dark setting.

    @details Adapted from FileMon++'s Theme.cpp. Win32 controls don't follow
             the application theme on their own, and Windows has no documented
             API to make them. What it has are a handful of uxtheme exports
             with no names, which is the route every Win32 application with a
             dark mode takes. They are resolved by ordinal, and everything
             falls back to the normal light appearance if any is missing, so a
             future Windows that drops them costs the look and nothing else.
             The dark title bar is the one documented part, through
             DwmSetWindowAttribute.

             Unlike FileMon++, WinCode has no theme setting of its own. It
             follows Windows, including when the setting changes while it runs.

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
// The undocumented uxtheme exports, by ordinal.
//
//     104  RefreshImmersiveColorPolicyState()
//     132  ShouldAppsUseDarkMode()
//     133  AllowDarkModeForWindow(HWND, BOOL)
//     135  SetPreferredAppMode(PreferredAppMode)
//     136  FlushMenuThemes()
//
// N.B. ordinal 135 was AllowDarkModeForApp(BOOL) before Windows 10 1903. The
// signatures are compatible for AllowDark, which is 1 and so also TRUE.
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
// DWMWA_USE_IMMERSIVE_DARK_MODE moved between Windows 10 1809 and 2004. Both
// are tried, oldest last, because setting the wrong one is harmless.
//
static constexpr DWORD k_ImmersiveDarkMode = 20;
static constexpr DWORD k_ImmersiveDarkModeLegacy = 19;

//
// Dark: FileMon++'s palette, close to the shell's own dark surfaces. A near
// black ground, a slightly lighter one for content, and text a little short of
// white so it doesn't glare. Light uses the system colours for the grounds and
// text, as light mode always has. Both add Windows 11's secondary text shades
// and its default accent, which links are drawn in.
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
// The ground behind the part of a name a search matched: the accent at about a
// seventh of its strength, over the content ground.
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

    @details Debug builds only. WINCODE_THEME=dark or light overrides the
             Windows setting, so both themes can be checked without changing
             the machine's.

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
    // AllowDark rather than ForceDark: the point is to follow Windows, not to
    // override somebody who has chosen light. Only the debug override forces.
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


COLORREF
ThemeKindColour (
    _In_ CodeKind Kind
    )
{
    //
    // One hue per kind, from the mockups: the same lightness and saturation in
    // each theme, dark enough to read on white or light enough to read on the
    // dark ground.
    //
    switch (Kind)
    {
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


INT_PTR
ThemeOnCtlColor (
    _In_ WPARAM WParam,
    _In_ bool Content
    )
{
    HDC dc;

    //
    // In light mode the system's own colours are right for everything except
    // content, which a read-only edit would otherwise draw in the window's
    // grey.
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
        // The header is a separate control and inherits none of that. The
        // DarkMode_ItemsView class is honoured on some builds and quietly
        // ignored on others, so in dark its styles come off instead.
        //
        header = ListView_GetHeader(Window);
        if (header != nullptr)
        {
            SetWindowTheme(header, g_Dark ? L"" : nullptr, g_Dark ? L"" : nullptr);
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
    // mistake, whatever the client area does.
    //
    dark = g_Dark ? TRUE : FALSE;
    if (FAILED(DwmSetWindowAttribute(Window, k_ImmersiveDarkMode, &dark, sizeof(dark))))
    {
        DwmSetWindowAttribute(Window, k_ImmersiveDarkModeLegacy, &dark, sizeof(dark));
    }

    EnumChildWindows(Window, ThemeChildProc, 0);
}


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
