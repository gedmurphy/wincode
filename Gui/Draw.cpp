/*!

    @file Gui/Draw.cpp

    @brief Fonts and drawing shared by WinCodeUI's windows.

    @details Each window makes its own set of fonts, because the popup and the
             main window can be on monitors at different scales.

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

#include <format>


//
// The icons' size, in 96 DPI units.
//
static constexpr int k_IconSize = 16;


/*!

    @brief Callback for FontExists: any call at all means the family exists.

*/
static
int
CALLBACK
FontExistsCallback (
    _In_ const LOGFONTW* Font,
    _In_ const TEXTMETRICW* Metrics,
    _In_ DWORD FontType,
    _In_ LPARAM Found
    )
{
    UNREFERENCED_PARAMETER(Font);
    UNREFERENCED_PARAMETER(Metrics);
    UNREFERENCED_PARAMETER(FontType);

    *RCAST(bool*)(Found) = true;
    return 0;
}


/*!

    @brief Whether a font family is installed.

    @param[in] Face - The family name.

    @return True if it is.

*/
static
bool
FontExists (
    _In_z_ PCWSTR Face
    )
{
    LOGFONTW font = {};
    HDC dc;
    bool found;

    found = false;
    font.lfCharSet = DEFAULT_CHARSET;
    StringCchCopyW(font.lfFaceName, ARRAYSIZE(font.lfFaceName), Face);

    dc = GetDC(nullptr);
    EnumFontFamiliesExW(dc, &font, FontExistsCallback, RCAST(LPARAM)(&found), 0);
    ReleaseDC(nullptr, dc);

    return found;
}


/*!

    @brief Creates a variant of a font at a different size or weight.

    @param[in] Base - The font to start from.

    @param[in] Percent - The size, as a percentage of Base's.

    @param[in] Weight - The weight, such as FW_SEMIBOLD.

    @return The font, or nullptr.

*/
static
HFONT
DerivedFont (
    _In_ const LOGFONTW& Base,
    _In_ int Percent,
    _In_ LONG Weight
    )
{
    LOGFONTW font;

    font = Base;
    font.lfHeight = MulDiv(Base.lfHeight, Percent, 100);
    font.lfWeight = Weight;

    return CreateFontIndirectW(&font);
}


bool
FontsCreate (
    _In_ UINT Dpi,
    _Out_ UiFonts* Fonts
    )
{
    NONCLIENTMETRICSW metrics = {};
    LOGFONTW mono;
    LOGFONTW icon = {};

    *Fonts = {};

    //
    // The system message font at this DPI rather than a fixed face, which is
    // what keeps text crisp when a window moves to a monitor at another scale.
    //
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, Dpi))
    {
        return false;
    }

    Fonts->Message = CreateFontIndirectW(&metrics.lfMessageFont);
    Fonts->Label = DerivedFont(metrics.lfMessageFont, 85, FW_SEMIBOLD);
    Fonts->Name = DerivedFont(metrics.lfMessageFont, 100, FW_SEMIBOLD);
    Fonts->Small = DerivedFont(metrics.lfMessageFont, 85, FW_NORMAL);
    Fonts->Heading = DerivedFont(metrics.lfMessageFont, 150, FW_SEMIBOLD);
    Fonts->Query = DerivedFont(metrics.lfMessageFont, 150, FW_NORMAL);

    //
    // Cascadia Mono comes with Windows 11; Consolas is there on Windows 10.
    //
    mono = metrics.lfMessageFont;
    mono.lfWeight = FW_NORMAL;
    mono.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    StringCchCopyW(mono.lfFaceName,
                   ARRAYSIZE(mono.lfFaceName),
                   FontExists(L"Cascadia Mono") ? L"Cascadia Mono" : L"Consolas");
    Fonts->Mono = CreateFontIndirectW(&mono);

    //
    // The icons come from Windows' own icon font, so nothing is bundled.
    // Segoe MDL2 Assets, the Windows 10 one, has the same code points.
    //
    icon.lfHeight = -MulDiv(k_IconSize, SCAST(int)(Dpi), USER_DEFAULT_SCREEN_DPI);
    icon.lfCharSet = DEFAULT_CHARSET;
    icon.lfQuality = CLEARTYPE_QUALITY;
    StringCchCopyW(icon.lfFaceName,
                   ARRAYSIZE(icon.lfFaceName),
                   FontExists(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets");
    Fonts->Icon = CreateFontIndirectW(&icon);

    if ((Fonts->Message == nullptr) || (Fonts->Label == nullptr) || (Fonts->Name == nullptr) ||
        (Fonts->Small == nullptr) || (Fonts->Heading == nullptr) || (Fonts->Query == nullptr) ||
        (Fonts->Mono == nullptr) || (Fonts->Icon == nullptr))
    {
        FontsDelete(Fonts);
        return false;
    }

    return true;
}


void
FontsDelete (
    _Inout_ UiFonts* Fonts
    )
{
    HFONT* fonts[] = { &Fonts->Message,
                       &Fonts->Label,
                       &Fonts->Name,
                       &Fonts->Small,
                       &Fonts->Heading,
                       &Fonts->Query,
                       &Fonts->Mono,
                       &Fonts->Icon };

    for (HFONT* font : fonts)
    {
        if (*font != nullptr)
        {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}


int
FontLineHeight (
    _In_opt_ HDC Dc,
    _In_ HFONT Font
    )
{
    TEXTMETRICW metrics = {};
    HGDIOBJ previous;
    HDC dc;

    dc = (Dc != nullptr) ? Dc : GetDC(nullptr);

    previous = SelectObject(dc, Font);
    GetTextMetricsW(dc, &metrics);
    SelectObject(dc, previous);

    if (Dc == nullptr)
    {
        ReleaseDC(nullptr, dc);
    }

    return metrics.tmHeight;
}


int
TextWidth (
    _In_ HDC Dc,
    _In_ HFONT Font,
    _In_ const std::wstring& Text
    )
{
    RECT rect = {};
    HGDIOBJ previous;

    if (Text.empty())
    {
        return 0;
    }

    //
    // Measured with DrawText rather than GetTextExtentPoint32, so the width
    // agrees with what DrawText later draws.
    //
    previous = SelectObject(Dc, Font);
    DrawTextW(Dc, Text.c_str(), SCAST(int)(Text.size()), &rect, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(Dc, previous);

    return rect.right - rect.left;
}


void
FillRounded (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour,
    _In_ int Radius
    )
{
    HGDIOBJ previousBrush;
    HGDIOBJ previousPen;
    HBRUSH brush;

    brush = CreateSolidBrush(Colour);
    previousBrush = SelectObject(Dc, brush);
    previousPen = SelectObject(Dc, GetStockObject(NULL_PEN));

    //
    // With no pen, RoundRect fills one pixel short on the right and bottom.
    //
    RoundRect(Dc, Rect.left, Rect.top, Rect.right + 1, Rect.bottom + 1, Radius, Radius);

    SelectObject(Dc, previousPen);
    SelectObject(Dc, previousBrush);
    DeleteObject(brush);
}


void
FrameRounded (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour,
    _In_ int Radius
    )
{
    HGDIOBJ previousBrush;
    HGDIOBJ previousPen;
    HPEN pen;

    pen = CreatePen(PS_SOLID, 1, Colour);
    previousPen = SelectObject(Dc, pen);
    previousBrush = SelectObject(Dc, GetStockObject(NULL_BRUSH));

    RoundRect(Dc, Rect.left, Rect.top, Rect.right, Rect.bottom, Radius, Radius);

    SelectObject(Dc, previousBrush);
    SelectObject(Dc, previousPen);
    DeleteObject(pen);
}


void
FillDot (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour
    )
{
    HGDIOBJ previousBrush;
    HGDIOBJ previousPen;
    HBRUSH brush;

    brush = CreateSolidBrush(Colour);
    previousBrush = SelectObject(Dc, brush);
    previousPen = SelectObject(Dc, GetStockObject(NULL_PEN));

    //
    // With no pen, Ellipse fills one pixel short on the right and bottom.
    //
    Ellipse(Dc, Rect.left, Rect.top, Rect.right + 1, Rect.bottom + 1);

    SelectObject(Dc, previousPen);
    SelectObject(Dc, previousBrush);
    DeleteObject(brush);
}


void
FillSolid (
    _In_ HDC Dc,
    _In_ const RECT& Rect,
    _In_ COLORREF Colour
    )
{
    HBRUSH brush;

    brush = CreateSolidBrush(Colour);
    FillRect(Dc, &Rect, brush);
    DeleteObject(brush);
}


bool
CopyTextToClipboard (
    _In_ HWND Owner,
    _In_ const std::wstring& Text
    )
{
    HGLOBAL memory;
    void* data;
    size_t bytes;
    bool copied;

    if (!OpenClipboard(Owner))
    {
        return false;
    }

    copied = false;
    EmptyClipboard();

    bytes = (Text.size() + 1) * sizeof(WCHAR);
    memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory != nullptr)
    {
        data = GlobalLock(memory);
        if (data != nullptr)
        {
            memcpy(data, Text.c_str(), bytes);
            GlobalUnlock(memory);

            //
            // The clipboard owns the memory once SetClipboardData succeeds.
            //
            copied = (SetClipboardData(CF_UNICODETEXT, memory) != nullptr);
        }

        if (!copied)
        {
            GlobalFree(memory);
        }
    }

    CloseClipboard();
    return copied;
}


std::wstring
DescribeValue (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    )
{
    std::wstring text;

    if (Kind == CodeKind::WinError)
    {
        return std::format(L"{} · 0x{:08X}", Value, Value);
    }

    text = std::format(L"{} · {}", FormatCodeValue(Kind, Value), Value);

    if ((Value & 0x80000000) != 0)
    {
        text += std::format(L" · signed {}", SCAST(int32_t)(Value));
    }

    return text;
}
