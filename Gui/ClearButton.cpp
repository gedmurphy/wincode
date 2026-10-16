/*!

    @file Gui/ClearButton.cpp

    @brief The × at the end of a search box that clears it, for the main
           window and the popup.

    @details A small window of our own inside the edit control, drawn here, and
             shown only while the box has text. The edit keeps a right margin
             so text never runs under it, and clips it so typing never draws
             over it.

             It's not a tab stop and never takes the focus, so clicking it
             leaves the caret in the box. Esc does the same from the keyboard.
             A click comes to the box's parent as WM_COMMAND and BN_CLICKED,
             with the ID it was attached with, like any button.

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

#include <strsafe.h>
#include <windowsx.h>

//
// initguid.h first, so oleacc.h defines the PROPID_ACC_* GUIDs here too.
//
#include <initguid.h>
#include <oleacc.h>

#include <algorithm>
#include <new>


constexpr WCHAR k_ClearButtonClass[] = L"WinCodeClearButton";

//
// Segoe Fluent Icons' Clear, the × WinUI's own text boxes use.
//
static constexpr WCHAR k_GlyphClear[] = L"\xE894";

//
// Sizes, in 96 DPI units. The button's at least this wide, and square in a
// box taller than that.
//
static constexpr int k_MinWidth = 28;
static constexpr int k_GlyphSize = 10;
static constexpr int k_HoverInset = 3;
static constexpr int k_HoverRadius = 4;

//
// The edit's subclass ID.
//
static constexpr UINT_PTR k_EditSubclass = 0x434C52;


/*!

    @brief What one clear button keeps.

*/
struct ClearButtonState
{
    HWND Tooltip;
    IAccPropServices* AccProps;

    //
    // The theme the tooltip was last given.
    //
    bool TooltipDark;

    //
    // The glyph's font, and the DPI it was made for.
    //
    HFONT Font;
    UINT FontDpi;

    bool Hot;
    bool Pressed;
    bool Tracking;
};


/*!

    @brief Scales a length from 96 DPI units to a window's DPI.

    @param[in] Window - The window.

    @param[in] Value - The length at 96 DPI.

    @return The length in pixels.

*/
static
int
ScaleFor (
    _In_ HWND Window,
    _In_ int Value
    )
{
    return MulDiv(Value, SCAST(int)(GetDpiForWindow(Window)), USER_DEFAULT_SCREEN_DPI);
}


/*!

    @brief The face a font actually got, which is a stand-in if the one asked
           for isn't installed.

    @param[in] Font - The font.

    @return The face's name.

*/
static
std::wstring
FontFace (
    _In_ HFONT Font
    )
{
    WCHAR face[LF_FACESIZE] = {};
    HGDIOBJ old;
    HDC dc;

    dc = GetDC(nullptr);
    old = SelectObject(dc, Font);
    GetTextFaceW(dc, ARRAYSIZE(face), face);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);

    return face;
}


/*!

    @brief The glyph's font for the button's DPI, made again when the DPI
           changes.

    @param[in] Window - The button.

    @param[in,out] State - The button's state, which owns the font.

    @return The font, or nullptr if it couldn't be made.

*/
static
HFONT
GlyphFont (
    _In_ HWND Window,
    _Inout_ ClearButtonState* State
    )
{
    LOGFONTW font = {};
    UINT dpi;

    dpi = GetDpiForWindow(Window);
    if ((State->Font != nullptr) && (State->FontDpi == dpi))
    {
        return State->Font;
    }

    if (State->Font != nullptr)
    {
        DeleteObject(State->Font);
        State->Font = nullptr;
    }

    font.lfHeight = -MulDiv(k_GlyphSize, SCAST(int)(dpi), USER_DEFAULT_SCREEN_DPI);
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;

    StringCchCopyW(font.lfFaceName, ARRAYSIZE(font.lfFaceName), L"Segoe Fluent Icons");
    State->Font = CreateFontIndirectW(&font);
    State->FontDpi = dpi;

    //
    // Windows 10 hasn't got it and would quietly give us some other face.
    // Its own icon font, Segoe MDL2 Assets, has the same code point.
    //
    if ((State->Font != nullptr) && (_wcsicmp(FontFace(State->Font).c_str(), font.lfFaceName) != 0))
    {
        DeleteObject(State->Font);
        StringCchCopyW(font.lfFaceName, ARRAYSIZE(font.lfFaceName), L"Segoe MDL2 Assets");
        State->Font = CreateFontIndirectW(&font);
    }

    return State->Font;
}


/*!

    @brief Paints the button: the box's own ground, the × in the muted text
           colour, and behind it the selection grey while the pointer's over
           it.

    @param[in] Window - The button.

    @param[in,out] State - Its state.

*/
static
void
Paint (
    _In_ HWND Window,
    _Inout_ ClearButtonState* State
    )
{
    PAINTSTRUCT paint;
    RECT client;
    RECT hover;
    HFONT font;
    HGDIOBJ oldFont;
    COLORREF glyph;
    HDC dc;
    int inset;

    dc = BeginPaint(Window, &paint);
    GetClientRect(Window, &client);

    FillRect(dc, &client, ThemeControlBrush());

    glyph = ThemeMutedColour();

    if (State->Hot || State->Pressed)
    {
        inset = ScaleFor(Window, k_HoverInset);
        hover = client;
        InflateRect(&hover, -inset, -inset);
        FillRounded(dc, hover, ThemeSelectionColour(), ScaleFor(Window, k_HoverRadius));

        glyph = (State->Hot && State->Pressed) ? ThemeFaintColour() : ThemeTextColour();
    }

    font = GlyphFont(Window, State);
    oldFont = (font != nullptr) ? SelectObject(dc, font) : nullptr;

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, glyph);
    DrawTextW(dc, k_GlyphClear, -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (oldFont != nullptr)
    {
        SelectObject(dc, oldFont);
    }

    EndPaint(Window, &paint);
}


/*!

    @brief Gives the button its tooltip and its accessible role and name.

    @param[in] Window - The button.

    @param[in,out] State - Its state, which keeps both.

*/
static
void
Describe (
    _In_ HWND Window,
    _Inout_ ClearButtonState* State
    )
{
    TOOLINFOW tool = {};
    VARIANT role = {};

    State->Tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                                     WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                     GetAncestor(Window, GA_ROOT), nullptr,
                                     RCAST(HINSTANCE)(GetWindowLongPtrW(Window, GWLP_HINSTANCE)), nullptr);

    if (State->Tooltip != nullptr)
    {
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tool.hwnd = Window;
        tool.uId = RCAST(UINT_PTR)(Window);
        tool.lpszText = CCAST(PWSTR)(L"Clear");

        SendMessageW(State->Tooltip, TTM_ADDTOOLW, 0, RCAST(LPARAM)(&tool));

        ThemeApplyToPopup(State->Tooltip);
        State->TooltipDark = ThemeIsDark();
    }

    //
    // Without the service it still works, a screen reader just can't tell
    // it's a button.
    //
    if (FAILED(CoCreateInstance(__uuidof(CAccPropServices),
                                nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&State->AccProps))))
    {
        State->AccProps = nullptr;
        return;
    }

    role.vt = VT_I4;
    role.lVal = ROLE_SYSTEM_PUSHBUTTON;

    State->AccProps->SetHwndPropStr(Window, SCAST(DWORD)(OBJID_CLIENT), CHILDID_SELF, PROPID_ACC_NAME, L"Clear");
    State->AccProps->SetHwndProp(Window, SCAST(DWORD)(OBJID_CLIENT), CHILDID_SELF, PROPID_ACC_ROLE, role);
}


/*!

    @brief Releases everything a button keeps, as it's destroyed.

    @param[in] Window - The button.

    @param[in] State - Its state, which is deleted.

*/
static
void
Release (
    _In_ HWND Window,
    _In_ ClearButtonState* State
    )
{
    MSAAPROPID properties[] = { PROPID_ACC_NAME, PROPID_ACC_ROLE };

    if (State->AccProps != nullptr)
    {
        State->AccProps->ClearHwndProps(Window,
                                        SCAST(DWORD)(OBJID_CLIENT),
                                        CHILDID_SELF,
                                        properties,
                                        ARRAYSIZE(properties));
        State->AccProps->Release();
    }

    if (State->Tooltip != nullptr)
    {
        DestroyWindow(State->Tooltip);
    }

    if (State->Font != nullptr)
    {
        DeleteObject(State->Font);
    }

    delete State;
}


/*!

    @brief Tells the search box's parent the button was clicked.

    @param[in] Window - The button.

*/
static
void
Clicked (
    _In_ HWND Window
    )
{
    HWND edit;

    edit = GetParent(Window);

    SendMessageW(GetParent(edit),
                 WM_COMMAND,
                 MAKEWPARAM(GetDlgCtrlID(Window), BN_CLICKED),
                 RCAST(LPARAM)(Window));
}


/*!

    @brief The clear button's window procedure.

*/
static
LRESULT
CALLBACK
ClearButtonProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam
    )
{
    TRACKMOUSEEVENT track = {};
    ClearButtonState* state;
    const NMHDR* header;
    RECT client;
    POINT point;
    bool hot;

    state = RCAST(ClearButtonState*)(GetWindowLongPtrW(Window, GWLP_USERDATA));

    switch (Message)
    {
    case WM_NCCREATE:
        state = new (std::nothrow) ClearButtonState();
        if (state == nullptr)
        {
            return FALSE;
        }

        SetWindowLongPtrW(Window, GWLP_USERDATA, RCAST(LONG_PTR)(state));
        break;

    case WM_CREATE:
        Describe(Window, state);
        return 0;

    case WM_NCDESTROY:
        if (state != nullptr)
        {
            SetWindowLongPtrW(Window, GWLP_USERDATA, 0);
            Release(Window, state);
        }
        break;
    }

    if (state == nullptr)
    {
        return DefWindowProcW(Window, Message, WParam, LParam);
    }

    switch (Message)
    {
    case WM_PAINT:
        Paint(Window, state);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SETCURSOR:
        //
        // The arrow, not the box's I-beam, since this is a button.
        //
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;

    case WM_MOUSEMOVE:
        if (!state->Tracking)
        {
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = Window;
            state->Tracking = TrackMouseEvent(&track) != FALSE;
        }

        GetClientRect(Window, &client);
        point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
        hot = PtInRect(&client, point) != FALSE;

        if (hot != state->Hot)
        {
            state->Hot = hot;
            InvalidateRect(Window, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSELEAVE:
        state->Tracking = false;
        if (state->Hot)
        {
            state->Hot = false;
            InvalidateRect(Window, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(Window);
        state->Pressed = true;
        state->Hot = true;
        InvalidateRect(Window, nullptr, FALSE);
        return 0;

    case WM_LBUTTONUP:
        if (!state->Pressed)
        {
            return 0;
        }

        GetClientRect(Window, &client);
        point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };

        //
        // Releasing capture clears Pressed, through WM_CAPTURECHANGED.
        //
        ReleaseCapture();

        if (PtInRect(&client, point))
        {
            Clicked(Window);
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (state->Pressed)
        {
            state->Pressed = false;
            InvalidateRect(Window, nullptr, FALSE);
        }
        return 0;

    case WM_SHOWWINDOW:
        //
        // Hidden under the pointer, by the click that cleared the box, it
        // never hears the pointer leave.
        //
        if (WParam == FALSE)
        {
            state->Hot = false;
        }
        break;

    case WM_NOTIFY:
        //
        // The theme can change while we're running, so the tip follows it as
        // it shows. Only on a change, since theming it shows it again.
        //
        header = RCAST(const NMHDR*)(LParam);
        if ((header->hwndFrom == state->Tooltip) && (header->code == TTN_SHOW) && (state->TooltipDark != ThemeIsDark()))
        {
            state->TooltipDark = ThemeIsDark();
            ThemeApplyToPopup(state->Tooltip);
        }
        return 0;
    }

    return DefWindowProcW(Window, Message, WParam, LParam);
}


/*!

    @brief Places the button at the box's right end, and keeps the box's text
           clear of it.

    @param[in] Edit - The search box.

    @param[in] Button - Its clear button.

*/
static
void
Place (
    _In_ HWND Edit,
    _In_ HWND Button
    )
{
    RECT client;
    int width;

    GetClientRect(Edit, &client);
    width = std::max(SCAST(int)(client.bottom), ScaleFor(Edit, k_MinWidth));

    MoveWindow(Button, client.right - width, 0, width, client.bottom, TRUE);

    //
    // Kept whether the button shows or not, so text doesn't jump sideways
    // when the first character is typed.
    //
    SendMessageW(Edit, EM_SETMARGINS, EC_RIGHTMARGIN, MAKELPARAM(0, width));
}


/*!

    @brief Subclass of the search box, which keeps its clear button in place
           as it's resized or given a new font.

*/
static
LRESULT
CALLBACK
ClearEditProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam,
    _In_ UINT_PTR Id,
    _In_ DWORD_PTR Data
    )
{
    LRESULT result;

    if (Message == WM_NCDESTROY)
    {
        RemoveWindowSubclass(Window, ClearEditProc, Id);
        return DefSubclassProc(Window, Message, WParam, LParam);
    }

    result = DefSubclassProc(Window, Message, WParam, LParam);

    //
    // A new font resets the margins, and a new size moves the end.
    //
    if ((Message == WM_SIZE) || (Message == WM_SETFONT))
    {
        Place(Window, RCAST(HWND)(Data));
    }

    return result;
}


/*!

    @brief Registers the clear button's window class.

    @param[in] Instance - The module instance.

    @return True on success.

*/
bool
ClearButtonRegister (
    _In_ HINSTANCE Instance
    )
{
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ClearButtonProc;
    windowClass.hInstance = Instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = k_ClearButtonClass;

    return RegisterClassExW(&windowClass) != 0;
}


/*!

    @brief Gives a search box its clear button.

    @param[in] Edit - The search box.

    @param[in] Id - The ID the button's clicks come to the box's parent with.

    @return The button, or nullptr if it couldn't be made. The box works the
            same without it.

*/
HWND
ClearButtonAttach (
    _In_ HWND Edit,
    _In_ int Id
    )
{
    HWND button;

    button = CreateWindowExW(0, k_ClearButtonClass, L"Clear", WS_CHILD,
                             0, 0, 0, 0,
                             Edit,
                             RCAST(HMENU)(SCAST(UINT_PTR)(Id)),
                             RCAST(HINSTANCE)(GetWindowLongPtrW(Edit, GWLP_HINSTANCE)),
                             nullptr);
    if (button == nullptr)
    {
        return nullptr;
    }

    //
    // An edit draws its text straight to the screen as you type, which
    // would paint over the button without this.
    //
    SetWindowLongPtrW(Edit, GWL_STYLE, GetWindowLongPtrW(Edit, GWL_STYLE) | WS_CLIPCHILDREN);

    SetWindowSubclass(Edit, ClearEditProc, k_EditSubclass, RCAST(DWORD_PTR)(button));
    Place(Edit, button);
    ClearButtonSync(Edit);

    return button;
}


/*!

    @brief Shows a search box's clear button if the box has text, and hides it
           if not. Call on every EN_CHANGE.

    @param[in] Edit - The search box.

*/
void
ClearButtonSync (
    _In_ HWND Edit
    )
{
    HWND button;
    bool show;

    button = FindWindowExW(Edit, nullptr, k_ClearButtonClass, nullptr);
    if (button == nullptr)
    {
        return;
    }

    //
    // Its own style rather than IsWindowVisible, which is false for every
    // child while the popup's hidden.
    //
    show = GetWindowTextLengthW(Edit) > 0;
    if (((GetWindowLongPtrW(button, GWL_STYLE) & WS_VISIBLE) != 0) == show)
    {
        return;
    }

    ShowWindow(button, show ? SW_SHOWNA : SW_HIDE);
}
