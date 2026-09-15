/*!

    @file Gui/DetailsPane.cpp

    @brief The details pane: everything known about the selected match, with
           every related code a link.

    @details Drawn by hand rather than built from standard controls. The links
             sit in labelled rows and lists that no standard control lays out.

             The pane is two windows. The outer one draws the header (the
             match's kind, name and value) and holds the copy buttons, which
             are real buttons, so they're reachable from the keyboard and
             announced like any other. Beneath it the body scrolls on its own,
             and is a tab stop in its own right: a window that holds tab stops,
             as the outer one does, is never one itself.

             Links are hit-tested against rectangles recorded when the body is
             laid out. From the keyboard, Tab moves into the body, Up and Down
             move between its links, and Enter follows one, so nothing needs the
             mouse. Following a link is the parent's business: the pane only
             tells it which code was asked for.

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

#include <windowsx.h>

#include <algorithm>
#include <cwctype>
#include <format>


constexpr WCHAR k_DetailsPaneClass[] = L"WinCodeDetailsPane";
constexpr WCHAR k_DetailsBodyClass[] = L"WinCodeDetailsBody";

//
// Layout, in 96 DPI units: the space around the content, between sections,
// and between the rows of a list; the column the relation labels and the
// reverse list's values sit in, and the one the kinds sit in.
//
static constexpr int k_Padding = 20;
static constexpr int k_SectionGap = 18;
static constexpr int k_RowGap = 4;
static constexpr int k_LabelWidth = 96;
static constexpr int k_KindWidth = 118;
static constexpr int k_KindDot = 7;
static constexpr int k_ButtonHeight = 28;
static constexpr int k_ButtonPadding = 12;
static constexpr int k_ButtonGap = 6;
static constexpr int k_LineScroll = 24;

//
// How many of the NTSTATUS codes behind a Win32 error are listed before
// "Show all". ERROR_ACCESS_DENIED has 31.
//
static constexpr size_t k_MappedFromShown = 10;

//
// The pane's own children.
//
static constexpr int k_CopyNameId = 1;
static constexpr int k_CopyValueId = 2;
static constexpr int k_CopyAllId = 3;
static constexpr int k_BodyId = 4;


/*!

    @brief Which theme colour a piece of text is drawn in.

*/
enum class Ink
{
    Text,
    Muted,
    Faint,
    Accent,
    Kind
};


/*!

    @brief Where a link goes: another code, or the pane's own "Show all".

*/
struct LinkTarget
{
    bool ToggleMappedFrom;
    CodeKind Kind;
    uint32_t Value;
    std::wstring Name;
};


/*!

    @brief One piece of the laid out body: a run of text, or a kind's dot.

*/
struct Piece
{
    //
    // In content coordinates: from the top of the body, before scrolling.
    //
    RECT Rect;
    std::wstring Text;
    HFONT Font;
    Ink Colour;
    CodeKind Kind;
    UINT Format;

    //
    // The link this piece belongs to, or -1.
    //
    int Link;
    bool IsDot;
};


static HWND g_Pane;
static HWND g_Body;
static HWND g_CopyName;
static HWND g_CopyValue;
static HWND g_CopyAll;

static HFONT g_TextFont;
static HFONT g_LabelFont;
static HFONT g_SmallFont;
static HFONT g_HeadingFont;
static HFONT g_MonoFont;
static UINT g_PaneDpi = USER_DEFAULT_SCREEN_DPI;

static CodeMatch g_Match;
static bool g_HasMatch;
static std::wstring g_Hint;
static bool g_ShowAllMappedFrom;

static std::vector<Piece> g_Pieces;
static std::vector<LinkTarget> g_Links;
static std::vector<RECT> g_LinkRects;

static int g_HeaderHeight;
static int g_ContentHeight;
static int g_ScrollY;
static int g_HoverLink = -1;
static int g_PressedLink = -1;
static int g_FocusLink = -1;
static bool g_TrackingMouse;


/*!

    @brief Scales a length from 96 DPI units to the pane's DPI.

    @param[in] Value - The length at 96 DPI.

    @return The length in pixels.

*/
static
int
Scale (
    _In_ int Value
    )
{
    return MulDiv(Value, SCAST(int)(g_PaneDpi), USER_DEFAULT_SCREEN_DPI);
}


/*!

    @brief The colour to draw a piece in.

    @param[in] Colour - The piece's ink.

    @param[in] Kind - Its kind, for Ink::Kind.

    @return The colour, for the current theme.

*/
static
COLORREF
InkColour (
    _In_ Ink Colour,
    _In_ CodeKind Kind
    )
{
    switch (Colour)
    {
    case Ink::Muted:
        return ThemeMutedColour();

    case Ink::Faint:
        return ThemeFaintColour();

    case Ink::Accent:
        return ThemeAccentColour();

    case Ink::Kind:
        return ThemeKindColour(Kind);

    default:
        return ThemeTextColour();
    }
}


/*!

    @brief The height of a font's lines.

    @param[in] Dc - A device context to measure with. The font is left
                    selected in it.

    @param[in] Font - The font.

    @return The height in pixels.

*/
static
int
LineHeight (
    _In_ HDC Dc,
    _In_ HFONT Font
    )
{
    TEXTMETRICW metrics = {};

    SelectObject(Dc, Font);
    GetTextMetricsW(Dc, &metrics);
    return metrics.tmHeight;
}


/*!

    @brief Converts text with lines separated by \n to \r\n, for the clipboard.

    @param[in] Text - The text.

    @return The converted text.

*/
static
std::wstring
ToCrLf (
    _In_ const std::wstring& Text
    )
{
    std::wstring result;

    for (WCHAR c : Text)
    {
        if (c == L'\n')
        {
            result += L'\r';
        }
        result += c;
    }

    return result;
}


/*!

    @brief Capitalises the first letter of a label.

    @param[in] Text - The label.

    @return The label, capitalised.

*/
static
std::wstring
Capitalise (
    _In_z_ PCWSTR Text
    )
{
    std::wstring result;

    result = Text;
    if (!result.empty())
    {
        result[0] = SCAST(WCHAR)(towupper(result[0]));
    }

    return result;
}


/*!

    @brief The match's name as headed: its primary name, a description for an
           unnamed window message, or "(no name)".

    @return The name.

*/
static
std::wstring
HeadingName (
    void
    )
{
    if (!g_Match.Names.empty())
    {
        return g_Match.Names[0];
    }

    if (!g_Match.Description.empty())
    {
        return g_Match.Description;
    }

    return L"(no name)";
}


/*!

    @brief Where the match's name comes from, for the line at the foot of the
           pane.

    @return The line.

*/
static
std::wstring
SourceLine (
    void
    )
{
    if (g_Match.Undocumented)
    {
        return L"Not in the SDK headers: from WinCode's list of undocumented window messages";
    }

    if (g_Match.Header != nullptr)
    {
        return std::format(L"From {} · Windows SDK {}", g_Match.Header, TablesSdkVersion());
    }

    return L"Decoded from the value itself: it isn't in any of the SDK's tables";
}


/*!

    @brief Everything in the pane as plain text, for Copy all.

    @return The text, with \r\n line ends.

*/
static
std::wstring
PlainText (
    void
    )
{
    std::wstring text;

    text = HeadingName() + L"\r\n";
    text += std::format(L"{} · {}\r\n", CodeKindName(g_Match.Kind), DescribeValue(g_Match.Kind, g_Match.Value));

    if (g_Match.Names.size() > 1)
    {
        text += L"Also defined as:";
        for (size_t i = 1; i < g_Match.Names.size(); i++)
        {
            text += (i == 1) ? L" " : L", ";
            text += g_Match.Names[i];
        }
        text += L"\r\n";
    }

    if (!g_Match.Text.empty())
    {
        text += L"\r\n" + ToCrLf(g_Match.Text) + L"\r\n";
    }

    if (g_Match.Fields.has_value() || !g_Match.Related.empty())
    {
        text += L"\r\n";
    }

    if (g_Match.Fields.has_value())
    {
        text += L"Fields: " + DescribeFields(*g_Match.Fields) + L"\r\n";
    }

    for (const RelatedCode& related : g_Match.Related)
    {
        text += Capitalise(RelationLabel(related.Relation)) + L": " + DescribeRelated(related) + L"\r\n";
    }

    if (!g_Match.MappedFrom.empty())
    {
        text += std::format(L"\r\nNTSTATUS codes that map here ({}):\r\n", g_Match.MappedFrom.size());
        for (const RelatedCode& from : g_Match.MappedFrom)
        {
            text += std::format(L"  {}  {}\r\n", FormatCodeValue(from.Kind, from.Value), from.Name);
        }
    }

    text += L"\r\n" + SourceLine() + L"\r\n";
    return text;
}


/*!

    @brief Adds a link, and returns its index for the pieces that make it up.

    @param[in] Target - Where it goes.

    @return The link's index.

*/
static
int
AddLink (
    _In_ const LinkTarget& Target
    )
{
    g_Links.push_back(Target);
    g_LinkRects.push_back({ 0, 0, 0, 0 });
    return SCAST(int)(g_Links.size()) - 1;
}


/*!

    @brief Lays out a run of text and adds it to the body.

    @param[in] Dc - A device context to measure with.

    @param[in] X - Its left edge, in content coordinates.

    @param[in] Y - Its top.

    @param[in] Width - The widest it may be.

    @param[in] Text - The text.

    @param[in] Font - Its font.

    @param[in] Colour - Its ink.

    @param[in] Kind - Its kind, for Ink::Kind.

    @param[in] Wrap - True to wrap onto as many lines as it needs; otherwise
                      one line, cut short with an ellipsis if it must be.

    @param[in] Link - The link it belongs to, or -1.

    @return Where it went.

*/
static
RECT
AddText (
    _In_ HDC Dc,
    _In_ int X,
    _In_ int Y,
    _In_ int Width,
    _In_ const std::wstring& Text,
    _In_ HFONT Font,
    _In_ Ink Colour,
    _In_ CodeKind Kind,
    _In_ bool Wrap,
    _In_ int Link
    )
{
    RECT rect;
    RECT* linkRect;
    UINT format;

    Width = std::max(Width, 1);
    rect = { X, Y, X + Width, Y };

    if (Text.empty())
    {
        return rect;
    }

    format = DT_NOPREFIX | (Wrap ? (DT_WORDBREAK | DT_EDITCONTROL) : (DT_SINGLELINE | DT_END_ELLIPSIS));

    SelectObject(Dc, Font);
    DrawTextW(Dc, Text.c_str(), SCAST(int)(Text.size()), &rect, format | DT_CALCRECT);

    if (rect.right > (X + Width))
    {
        rect.right = X + Width;
    }

    g_Pieces.push_back({ rect, Text, Font, Colour, Kind, format, Link, false });

    if (Link >= 0)
    {
        linkRect = &g_LinkRects[SCAST(size_t)(Link)];

        if (IsRectEmpty(linkRect))
        {
            *linkRect = rect;
        }
        else
        {
            UnionRect(linkRect, linkRect, &rect);
        }
    }

    return rect;
}


/*!

    @brief Adds a kind's label, with its coloured dot, to the body.

    @param[in] Dc - A device context to measure with.

    @param[in] X - Its left edge.

    @param[in] Y - The top of the line it sits in.

    @param[in] Width - The widest it may be.

    @param[in] Kind - The kind.

    @param[in] Height - The height of the line it sits in, which it's centred
                        in.

*/
static
void
AddKind (
    _In_ HDC Dc,
    _In_ int X,
    _In_ int Y,
    _In_ int Width,
    _In_ CodeKind Kind,
    _In_ int Height
    )
{
    RECT dot;
    int size;
    int labelHeight;
    int top;

    size = Scale(k_KindDot);
    labelHeight = LineHeight(Dc, g_LabelFont);
    top = Y + ((Height - labelHeight) / 2);

    AddText(Dc, X + size + Scale(6), top, Width - size - Scale(6), CodeKindName(Kind), g_LabelFont, Ink::Kind, Kind, false, -1);

    dot = { X, top + ((labelHeight - size) / 2), X + size, top + ((labelHeight - size) / 2) + size };
    g_Pieces.push_back({ dot, std::wstring(), nullptr, Ink::Kind, Kind, 0, -1, true });
}


/*!

    @brief Adds a section heading, as in "Fields", to the body.

    @param[in] Dc - A device context to measure with.

    @param[in] X - Its left edge.

    @param[in,out] Y - Its top, moved on past it.

    @param[in] Width - The widest it may be.

    @param[in] Text - The heading.

    @param[in] Note - Fainter text after it, or an empty string.

*/
static
void
AddHeading (
    _In_ HDC Dc,
    _In_ int X,
    _Inout_ int* Y,
    _In_ int Width,
    _In_ const std::wstring& Text,
    _In_ const std::wstring& Note
    )
{
    RECT heading;

    heading = AddText(Dc, X, *Y, Width, Text, g_LabelFont, Ink::Muted, CodeKind::WinError, false, -1);

    if (!Note.empty())
    {
        AddText(Dc,
                heading.right + Scale(8),
                *Y + ((heading.bottom - heading.top) - LineHeight(Dc, g_SmallFont)) / 2,
                X + Width - heading.right - Scale(8),
                Note,
                g_SmallFont,
                Ink::Faint,
                CodeKind::WinError,
                false,
                -1);
    }

    *Y = heading.bottom + Scale(6);
}


/*!

    @brief The link text for a related code: its value, and its name if it has
           one.

    @param[in] Related - The related code.

    @return The text.

*/
static
std::wstring
RelatedLinkText (
    _In_ const RelatedCode& Related
    )
{
    if (Related.Name.empty())
    {
        return FormatCodeValue(Related.Kind, Related.Value);
    }

    return FormatCodeValue(Related.Kind, Related.Value) + L" " + Related.Name;
}


/*!

    @brief Lays out the body for its current width: the pieces, the links and
           the content's height.

*/
static
void
BuildBody (
    void
    )
{
    RECT client;
    RECT row;
    RECT link;
    std::wstring aliases;
    size_t shown;
    HDC dc;
    int x;
    int y;
    int width;
    int index;
    int textHeight;

    g_Pieces.clear();
    g_Links.clear();
    g_LinkRects.clear();

    GetClientRect(g_Body, &client);
    x = Scale(k_Padding);
    width = std::max(SCAST(int)(client.right) - (2 * Scale(k_Padding)), Scale(40));
    y = Scale(16);

    dc = GetDC(g_Body);
    textHeight = LineHeight(dc, g_TextFont);

    if (!g_HasMatch)
    {
        y = Scale(k_Padding);
        row = AddText(dc, x, y, width, g_Hint, g_TextFont, Ink::Muted, CodeKind::WinError, true, -1);
        g_ContentHeight = row.bottom + Scale(k_Padding);
        ReleaseDC(g_Body, dc);
        return;
    }

    if (!g_Match.Text.empty())
    {
        row = AddText(dc, x, y, width, g_Match.Text, g_TextFont, Ink::Text, CodeKind::WinError, true, -1);
        y = row.bottom + Scale(6);

        //
        // Captured text is English whatever language this Windows runs in,
        // and describes the code as it was where the tables were made.
        //
        if (g_Match.Source == TextSource::Captured)
        {
            row = AddText(dc,
                          x,
                          y,
                          width,
                          L"This Windows has no text for this code, so this is the English captured when the tables were generated.",
                          g_SmallFont,
                          Ink::Faint,
                          CodeKind::WinError,
                          true,
                          -1);
            y = row.bottom;
        }

        y += Scale(k_SectionGap);
    }

    if (g_Match.Names.size() > 1)
    {
        for (size_t i = 1; i < g_Match.Names.size(); i++)
        {
            if (i > 1)
            {
                aliases += L", ";
            }
            aliases += g_Match.Names[i];
        }

        AddHeading(dc, x, &y, width, L"Also defined as", std::wstring());
        row = AddText(dc, x, y, width, aliases, g_TextFont, Ink::Text, CodeKind::WinError, true, -1);
        y = row.bottom + Scale(k_SectionGap);
    }

    if (g_Match.Fields.has_value())
    {
        AddHeading(dc, x, &y, width, L"Fields", std::wstring());
        row = AddText(dc, x, y, width, DescribeFields(*g_Match.Fields), g_TextFont, Ink::Text, CodeKind::WinError, true, -1);
        y = row.bottom + Scale(k_SectionGap);
    }

    if (!g_Match.Related.empty())
    {
        AddHeading(dc, x, &y, width, L"Converts to", std::wstring());

        for (const RelatedCode& related : g_Match.Related)
        {
            index = AddLink({ false, related.Kind, related.Value, related.Name });

            row = AddText(dc,
                          x,
                          y,
                          Scale(k_LabelWidth),
                          Capitalise(RelationLabel(related.Relation)),
                          g_TextFont,
                          Ink::Muted,
                          CodeKind::WinError,
                          false,
                          -1);

            AddKind(dc, x + Scale(k_LabelWidth), y, Scale(k_KindWidth), related.Kind, textHeight);

            link = AddText(dc,
                           x + Scale(k_LabelWidth) + Scale(k_KindWidth),
                           y,
                           width - Scale(k_LabelWidth) - Scale(k_KindWidth),
                           RelatedLinkText(related),
                           g_TextFont,
                           Ink::Accent,
                           related.Kind,
                           false,
                           index);

            if ((related.Via != nullptr) && (related.Via[0] != L'\0'))
            {
                AddText(dc,
                        link.right + Scale(10),
                        y + ((textHeight - LineHeight(dc, g_SmallFont)) / 2),
                        x + width - link.right - Scale(10),
                        related.Via,
                        g_SmallFont,
                        Ink::Faint,
                        CodeKind::WinError,
                        false,
                        -1);
            }

            y = std::max(row.bottom, link.bottom) + Scale(k_RowGap);
        }

        y += Scale(k_SectionGap) - Scale(k_RowGap);
    }

    if (!g_Match.MappedFrom.empty())
    {
        AddHeading(dc,
                   x,
                   &y,
                   width,
                   std::format(L"NTSTATUS codes that map here ({})", g_Match.MappedFrom.size()),
                   L"by RtlNtStatusToDosError");

        shown = g_ShowAllMappedFrom ? g_Match.MappedFrom.size() : std::min(g_Match.MappedFrom.size(), k_MappedFromShown);

        for (size_t i = 0; i < shown; i++)
        {
            const RelatedCode& from = g_Match.MappedFrom[i];

            index = AddLink({ false, from.Kind, from.Value, from.Name });

            AddText(dc,
                    x,
                    y + ((textHeight - LineHeight(dc, g_MonoFont)) / 2),
                    Scale(k_LabelWidth),
                    FormatCodeValue(from.Kind, from.Value),
                    g_MonoFont,
                    Ink::Faint,
                    CodeKind::WinError,
                    false,
                    -1);

            row = AddText(dc, x + Scale(k_LabelWidth), y, width - Scale(k_LabelWidth), from.Name, g_TextFont, Ink::Accent, from.Kind, false, index);

            y = row.bottom + Scale(k_RowGap);
        }

        if (g_Match.MappedFrom.size() > k_MappedFromShown)
        {
            index = AddLink({ true, CodeKind::WinError, 0, std::wstring() });
            row = AddText(dc,
                          x + Scale(k_LabelWidth),
                          y + Scale(2),
                          width - Scale(k_LabelWidth),
                          g_ShowAllMappedFrom ? std::wstring(L"Show fewer") : std::format(L"Show all {}", g_Match.MappedFrom.size()),
                          g_TextFont,
                          Ink::Accent,
                          CodeKind::WinError,
                          false,
                          index);
            y = row.bottom;
        }

        y += Scale(k_SectionGap);
    }

    row = AddText(dc, x, y, width, SourceLine(), g_SmallFont, Ink::Faint, CodeKind::WinError, true, -1);
    g_ContentHeight = row.bottom + Scale(k_Padding);

    ReleaseDC(g_Body, dc);
}


/*!

    @brief The height of the part of the body that shows.

    @return The height in pixels.

*/
static
int
VisibleHeight (
    void
    )
{
    RECT client;

    GetClientRect(g_Body, &client);
    return SCAST(int)(client.bottom);
}


/*!

    @brief Brings the scroll bar into line with the content and the body's
           height, keeping the scroll position within range.

*/
static
void
UpdateScrollBar (
    void
    )
{
    SCROLLINFO info = {};
    int visible;

    visible = VisibleHeight();
    g_ScrollY = std::clamp(g_ScrollY, 0, std::max(g_ContentHeight - visible, 0));

    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = std::max(g_ContentHeight - 1, 0);
    info.nPage = SCAST(UINT)(visible);
    info.nPos = g_ScrollY;
    SetScrollInfo(g_Body, SB_VERT, &info, TRUE);
}


/*!

    @brief Scrolls the body to a position.

    @param[in] Y - The position, which is kept within range.

*/
static
void
ScrollTo (
    _In_ int Y
    )
{
    Y = std::clamp(Y, 0, std::max(g_ContentHeight - VisibleHeight(), 0));
    if (Y == g_ScrollY)
    {
        return;
    }

    g_ScrollY = Y;
    SetScrollPos(g_Body, SB_VERT, g_ScrollY, TRUE);
    InvalidateRect(g_Body, nullptr, FALSE);
}


/*!

    @brief Lays out the header, places the copy buttons in it and the body
           beneath it.

*/
static
void
LayoutPane (
    void
    )
{
    HWND buttons[] = { g_CopyName, g_CopyValue, g_CopyAll };
    WCHAR label[64];
    RECT client;
    SIZE size;
    HDC dc;
    int x;
    int y;
    int buttonWidth;

    GetClientRect(g_Pane, &client);
    dc = GetDC(g_Pane);

    y = Scale(k_Padding) - Scale(4);
    y += LineHeight(dc, g_LabelFont) + Scale(2);
    y += LineHeight(dc, g_HeadingFont);
    y += LineHeight(dc, g_MonoFont) + Scale(12);

    x = Scale(k_Padding);
    SelectObject(dc, g_TextFont);

    for (HWND button : buttons)
    {
        GetWindowTextW(button, label, ARRAYSIZE(label));
        GetTextExtentPoint32W(dc, label, SCAST(int)(wcslen(label)), &size);

        buttonWidth = size.cx + (2 * Scale(k_ButtonPadding));
        MoveWindow(button, x, y, buttonWidth, Scale(k_ButtonHeight), TRUE);
        ShowWindow(button, g_HasMatch ? SW_SHOWNA : SW_HIDE);

        x += buttonWidth + Scale(k_ButtonGap);
    }

    g_HeaderHeight = g_HasMatch ? (y + Scale(k_ButtonHeight) + Scale(14)) : 0;

    ReleaseDC(g_Pane, dc);

    MoveWindow(g_Body,
               0,
               g_HeaderHeight,
               client.right,
               std::max(SCAST(int)(client.bottom) - g_HeaderHeight, 0),
               TRUE);
}


/*!

    @brief Lays out the body again, as after a new match or a resize.

*/
static
void
RelayoutBody (
    void
    )
{
    BuildBody();
    UpdateScrollBar();

    if (g_FocusLink >= SCAST(int)(g_Links.size()))
    {
        g_FocusLink = g_Links.empty() ? -1 : 0;
    }

    InvalidateRect(g_Body, nullptr, FALSE);
}


/*!

    @brief Where a link is in the body's client area.

    @param[in] Index - The link.

    @return The rectangle.

*/
static
RECT
LinkClientRect (
    _In_ int Index
    )
{
    RECT rect;

    rect = g_LinkRects[SCAST(size_t)(Index)];
    OffsetRect(&rect, 0, -g_ScrollY);
    return rect;
}


/*!

    @brief Finds the link under a point.

    @param[in] Point - The point, in the body's client coordinates.

    @return The link, or -1 for none.

*/
static
int
HitTest (
    _In_ POINT Point
    )
{
    RECT rect;

    for (size_t i = 0; i < g_Links.size(); i++)
    {
        rect = LinkClientRect(SCAST(int)(i));
        if (PtInRect(&rect, Point))
        {
            return SCAST(int)(i);
        }
    }

    return -1;
}


/*!

    @brief Scrolls so a link is fully in view.

    @param[in] Index - The link.

*/
static
void
EnsureLinkVisible (
    _In_ int Index
    )
{
    RECT rect;
    int visible;

    if ((Index < 0) || (SCAST(size_t)(Index) >= g_Links.size()))
    {
        return;
    }

    visible = VisibleHeight();
    rect = g_LinkRects[SCAST(size_t)(Index)];

    if (rect.top < g_ScrollY)
    {
        ScrollTo(rect.top - Scale(k_Padding));
    }
    else if (rect.bottom > (g_ScrollY + visible))
    {
        ScrollTo(rect.bottom - visible + Scale(k_Padding));
    }
}


/*!

    @brief Follows a link: toggles "Show all", or tells the parent which code
           was asked for.

    @param[in] Index - The link.

*/
static
void
FollowLink (
    _In_ int Index
    )
{
    DetailsLinkNotify notify = {};
    LinkTarget target;

    if ((Index < 0) || (SCAST(size_t)(Index) >= g_Links.size()))
    {
        return;
    }

    //
    // A copy, because the parent will usually show a new match in answer,
    // which lays the pane out afresh and replaces g_Links.
    //
    target = g_Links[SCAST(size_t)(Index)];

    if (target.ToggleMappedFrom)
    {
        g_ShowAllMappedFrom = !g_ShowAllMappedFrom;
        RelayoutBody();

        //
        // The toggle moves as the list grows or shrinks, so it's found again
        // to keep the focus on it.
        //
        g_FocusLink = SCAST(int)(g_Links.size()) - 1;
        EnsureLinkVisible(g_FocusLink);
        return;
    }

    notify.Header.hwndFrom = g_Pane;
    notify.Header.idFrom = SCAST(UINT_PTR)(GetDlgCtrlID(g_Pane));
    notify.Header.code = k_DetailsFollowLink;
    notify.Kind = target.Kind;
    notify.Value = target.Value;
    notify.Name = target.Name.c_str();

    SendMessageW(GetParent(g_Pane), WM_NOTIFY, notify.Header.idFrom, RCAST(LPARAM)(&notify));
}


/*!

    @brief Paints the header: the kind with its dot, the name and the value,
           and a rule beneath.

*/
static
void
PaintPane (
    void
    )
{
    PAINTSTRUCT paint;
    std::wstring text;
    RECT client;
    RECT rect;
    RECT dot;
    HDC dc;
    int x;
    int y;
    int size;
    int height;

    dc = BeginPaint(g_Pane, &paint);
    GetClientRect(g_Pane, &client);

    rect = { 0, 0, client.right, g_HeaderHeight };
    FillRect(dc, &rect, ThemeControlBrush());

    if (!g_HasMatch)
    {
        EndPaint(g_Pane, &paint);
        return;
    }

    SetBkMode(dc, TRANSPARENT);
    x = Scale(k_Padding);
    y = Scale(k_Padding) - Scale(4);
    size = Scale(k_KindDot);

    height = LineHeight(dc, g_LabelFont);
    dot = { x, y + ((height - size) / 2), x + size, y + ((height - size) / 2) + size };
    FillDot(dc, dot, ThemeKindColour(g_Match.Kind));

    rect = { x + size + Scale(6), y, client.right - Scale(k_Padding), y + height };
    SetTextColor(dc, ThemeKindColour(g_Match.Kind));
    DrawTextW(dc, CodeKindName(g_Match.Kind), -1, &rect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    y += height + Scale(2);

    text = HeadingName();
    height = LineHeight(dc, g_HeadingFont);
    rect = { x, y, client.right - Scale(k_Padding), y + height };
    SetTextColor(dc, ThemeTextColour());
    DrawTextW(dc, text.c_str(), -1, &rect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    y += height;

    text = DescribeValue(g_Match.Kind, g_Match.Value);
    height = LineHeight(dc, g_MonoFont);
    rect = { x, y, client.right - Scale(k_Padding), y + height };
    SetTextColor(dc, ThemeMutedColour());
    DrawTextW(dc, text.c_str(), -1, &rect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    rect = { Scale(k_Padding), g_HeaderHeight - 1, client.right - Scale(k_Padding), g_HeaderHeight };
    FillSolid(dc, rect, ThemeSelectionColour());

    EndPaint(g_Pane, &paint);
}


/*!

    @brief Draws the body's pieces, scrolled.

    @param[in] Dc - The device context.

    @param[in] Client - The body's client area.

*/
static
void
PaintPieces (
    _In_ HDC Dc,
    _In_ const RECT& Client
    )
{
    HGDIOBJ previousPen;
    HPEN pen;
    RECT rect;

    for (const Piece& piece : g_Pieces)
    {
        rect = piece.Rect;
        OffsetRect(&rect, 0, -g_ScrollY);

        if ((rect.bottom < 0) || (rect.top > Client.bottom))
        {
            continue;
        }

        if (piece.IsDot)
        {
            FillDot(Dc, rect, ThemeKindColour(piece.Kind));
            continue;
        }

        SelectObject(Dc, piece.Font);
        SetTextColor(Dc, InkColour(piece.Colour, piece.Kind));
        DrawTextW(Dc, piece.Text.c_str(), SCAST(int)(piece.Text.size()), &rect, piece.Format);

        //
        // Links are only underlined under the mouse, as Windows 11 does.
        //
        if ((piece.Link >= 0) && (piece.Link == g_HoverLink))
        {
            pen = CreatePen(PS_SOLID, 1, ThemeAccentColour());
            previousPen = SelectObject(Dc, pen);
            MoveToEx(Dc, rect.left, rect.bottom - 1, nullptr);
            LineTo(Dc, rect.right, rect.bottom - 1);
            SelectObject(Dc, previousPen);
            DeleteObject(pen);
        }
    }

    if ((GetFocus() == g_Body) && (g_FocusLink >= 0) && (SCAST(size_t)(g_FocusLink) < g_Links.size()))
    {
        rect = LinkClientRect(g_FocusLink);
        InflateRect(&rect, Scale(3), Scale(1));
        SetTextColor(Dc, ThemeTextColour());
        SetBkColor(Dc, ThemeControlColour());
        DrawFocusRect(Dc, &rect);
    }
}


/*!

    @brief Paints the body through an off-screen bitmap, so scrolling and
           hovering don't flicker.

*/
static
void
PaintBody (
    void
    )
{
    PAINTSTRUCT paint;
    HBITMAP bitmap;
    HGDIOBJ previousBitmap;
    RECT client;
    HDC dc;
    HDC memory;
    int saved;

    dc = BeginPaint(g_Body, &paint);
    GetClientRect(g_Body, &client);

    memory = CreateCompatibleDC(dc);
    bitmap = CreateCompatibleBitmap(dc, std::max(SCAST(int)(client.right), 1), std::max(SCAST(int)(client.bottom), 1));
    previousBitmap = SelectObject(memory, bitmap);

    FillRect(memory, &client, ThemeControlBrush());
    SetBkMode(memory, TRANSPARENT);

    saved = SaveDC(memory);
    PaintPieces(memory, client);
    RestoreDC(memory, saved);

    BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);

    SelectObject(memory, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);

    EndPaint(g_Body, &paint);
}


/*!

    @brief Scrolls in answer to the scroll bar.

    @param[in] Request - The WM_VSCROLL request.

*/
static
void
OnVScroll (
    _In_ WORD Request
    )
{
    SCROLLINFO info = {};

    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    GetScrollInfo(g_Body, SB_VERT, &info);

    switch (Request)
    {
    case SB_LINEUP:
        ScrollTo(g_ScrollY - Scale(k_LineScroll));
        break;

    case SB_LINEDOWN:
        ScrollTo(g_ScrollY + Scale(k_LineScroll));
        break;

    case SB_PAGEUP:
        ScrollTo(g_ScrollY - SCAST(int)(info.nPage));
        break;

    case SB_PAGEDOWN:
        ScrollTo(g_ScrollY + SCAST(int)(info.nPage));
        break;

    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        ScrollTo(info.nTrackPos);
        break;

    case SB_TOP:
        ScrollTo(0);
        break;

    case SB_BOTTOM:
        ScrollTo(g_ContentHeight);
        break;
    }
}


/*!

    @brief Moves the keyboard focus to another link, or scrolls when there are
           none.

    @param[in] Delta - 1 for the next link, -1 for the previous.

*/
static
void
MoveFocusLink (
    _In_ int Delta
    )
{
    if (g_Links.empty())
    {
        OnVScroll((Delta > 0) ? SB_LINEDOWN : SB_LINEUP);
        return;
    }

    g_FocusLink = std::clamp((g_FocusLink < 0) ? 0 : (g_FocusLink + Delta), 0, SCAST(int)(g_Links.size()) - 1);
    EnsureLinkVisible(g_FocusLink);
    InvalidateRect(g_Body, nullptr, FALSE);
}


/*!

    @brief The body's window procedure: drawing, scrolling and the links.

*/
static
LRESULT
CALLBACK
DetailsBodyProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam
    )
{
    TRACKMOUSEEVENT track = {};
    const MSG* message;
    POINT point;
    int hit;

    switch (Message)
    {
    case WM_SIZE:
        if (g_Body != nullptr)
        {
            RelayoutBody();
        }
        return 0;

    case WM_PAINT:
        PaintBody();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_VSCROLL:
        OnVScroll(LOWORD(WParam));
        return 0;

    case WM_MOUSEWHEEL:
        ScrollTo(g_ScrollY - ((GET_WHEEL_DELTA_WPARAM(WParam) * 3 * Scale(k_LineScroll)) / WHEEL_DELTA));
        return 0;

    case WM_MOUSEMOVE:
        point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
        hit = HitTest(point);

        if (!g_TrackingMouse)
        {
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = Window;
            g_TrackingMouse = TrackMouseEvent(&track) != FALSE;
        }

        if (hit != g_HoverLink)
        {
            g_HoverLink = hit;
            InvalidateRect(Window, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSELEAVE:
        g_TrackingMouse = false;
        if (g_HoverLink >= 0)
        {
            g_HoverLink = -1;
            InvalidateRect(Window, nullptr, FALSE);
        }
        return 0;

    case WM_SETCURSOR:
        if ((LOWORD(LParam) == HTCLIENT) && (g_HoverLink >= 0))
        {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
        point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
        g_PressedLink = HitTest(point);
        if (g_PressedLink >= 0)
        {
            g_FocusLink = g_PressedLink;
            SetCapture(Window);
        }
        SetFocus(Window);
        return 0;

    case WM_LBUTTONUP:
        if (GetCapture() == Window)
        {
            ReleaseCapture();
        }

        point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
        hit = HitTest(point);

        //
        // Only a press and release on the same link follows it, so dragging
        // off a link cancels.
        //
        if ((hit >= 0) && (hit == g_PressedLink))
        {
            g_PressedLink = -1;
            FollowLink(hit);
        }

        g_PressedLink = -1;
        return 0;

    case WM_GETDLGCODE:
        //
        // Arrows move between links, and Enter follows one, rather than the
        // dialog manager taking Enter as the default button.
        //
        message = RCAST(const MSG*)(LParam);
        if ((message != nullptr) && (message->message == WM_KEYDOWN) && (message->wParam == VK_RETURN))
        {
            return DLGC_WANTARROWS | DLGC_WANTMESSAGE;
        }
        return DLGC_WANTARROWS;

    case WM_KEYDOWN:
        switch (WParam)
        {
        case VK_DOWN:
            MoveFocusLink(1);
            return 0;

        case VK_UP:
            MoveFocusLink(-1);
            return 0;

        case VK_RETURN:
        case VK_SPACE:
            FollowLink(g_FocusLink);
            return 0;

        case VK_PRIOR:
            OnVScroll(SB_PAGEUP);
            return 0;

        case VK_NEXT:
            OnVScroll(SB_PAGEDOWN);
            return 0;

        case VK_HOME:
            ScrollTo(0);
            return 0;

        case VK_END:
            ScrollTo(g_ContentHeight);
            return 0;
        }
        break;

    case WM_SETFOCUS:
        if ((g_FocusLink < 0) && !g_Links.empty())
        {
            g_FocusLink = 0;
        }
        InvalidateRect(Window, nullptr, FALSE);
        return 0;

    case WM_KILLFOCUS:
        InvalidateRect(Window, nullptr, FALSE);
        return 0;
    }

    return DefWindowProcW(Window, Message, WParam, LParam);
}


/*!

    @brief The pane's window procedure: the header and the copy buttons.

*/
static
LRESULT
CALLBACK
DetailsPaneProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam
    )
{
    INT_PTR brush;

    switch (Message)
    {
    case WM_SIZE:
        if (g_Body != nullptr)
        {
            LayoutPane();
            InvalidateRect(Window, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT:
        PaintPane();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_COMMAND:
        if (HIWORD(WParam) != BN_CLICKED)
        {
            break;
        }

        switch (LOWORD(WParam))
        {
        case k_CopyNameId:
            CopyTextToClipboard(g_Pane, HeadingName());
            return 0;

        case k_CopyValueId:
            CopyTextToClipboard(g_Pane, FormatCodeValue(g_Match.Kind, g_Match.Value));
            return 0;

        case k_CopyAllId:
            CopyTextToClipboard(g_Pane, PlainText());
            return 0;
        }
        break;

    case WM_CTLCOLORBTN:
        //
        // The buttons sit on the content ground, in either theme.
        //
        brush = ThemeOnCtlColor(WParam, true);
        if (brush != 0)
        {
            return brush;
        }
        break;
    }

    return DefWindowProcW(Window, Message, WParam, LParam);
}


bool
DetailsPaneRegister (
    _In_ HINSTANCE Instance
    )
{
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = DetailsPaneProc;
    windowClass.hInstance = Instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = k_DetailsPaneClass;

    if (RegisterClassExW(&windowClass) == 0)
    {
        return false;
    }

    windowClass.lpfnWndProc = DetailsBodyProc;
    windowClass.lpszClassName = k_DetailsBodyClass;

    return RegisterClassExW(&windowClass) != 0;
}


HWND
DetailsPaneCreate (
    _In_ HWND Parent,
    _In_ int Id,
    _In_ HINSTANCE Instance
    )
{
    //
    // WS_EX_CONTROLPARENT lets the dialog manager's Tab walk into the pane, to
    // the buttons and the body.
    //
    g_Pane = CreateWindowExW(WS_EX_CONTROLPARENT,
                             k_DetailsPaneClass,
                             L"Details",
                             WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                             0, 0, 0, 0,
                             Parent,
                             RCAST(HMENU)(SCAST(UINT_PTR)(Id)),
                             Instance,
                             nullptr);
    if (g_Pane == nullptr)
    {
        return nullptr;
    }

    g_CopyName = CreateWindowExW(0, WC_BUTTONW, L"Copy name", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                                 0, 0, 0, 0, g_Pane, RCAST(HMENU)(SCAST(UINT_PTR)(k_CopyNameId)), Instance, nullptr);
    g_CopyValue = CreateWindowExW(0, WC_BUTTONW, L"Copy value", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                                  0, 0, 0, 0, g_Pane, RCAST(HMENU)(SCAST(UINT_PTR)(k_CopyValueId)), Instance, nullptr);
    g_CopyAll = CreateWindowExW(0, WC_BUTTONW, L"Copy all", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                                0, 0, 0, 0, g_Pane, RCAST(HMENU)(SCAST(UINT_PTR)(k_CopyAllId)), Instance, nullptr);
    g_Body = CreateWindowExW(0, k_DetailsBodyClass, L"Details", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL,
                             0, 0, 0, 0, g_Pane, RCAST(HMENU)(SCAST(UINT_PTR)(k_BodyId)), Instance, nullptr);

    if ((g_CopyName == nullptr) || (g_CopyValue == nullptr) || (g_CopyAll == nullptr) || (g_Body == nullptr))
    {
        DestroyWindow(g_Pane);
        g_Pane = nullptr;
        g_Body = nullptr;
        return nullptr;
    }

    return g_Pane;
}


void
DetailsPaneSetFonts (
    _In_ const UiFonts& Fonts,
    _In_ UINT Dpi
    )
{
    HWND buttons[] = { g_CopyName, g_CopyValue, g_CopyAll };

    g_TextFont = Fonts.Message;
    g_LabelFont = Fonts.Label;
    g_SmallFont = Fonts.Small;
    g_HeadingFont = Fonts.Heading;
    g_MonoFont = Fonts.Mono;
    g_PaneDpi = Dpi;

    for (HWND button : buttons)
    {
        SendMessageW(button, WM_SETFONT, RCAST(WPARAM)(Fonts.Message), FALSE);
    }

    LayoutPane();
    RelayoutBody();
    InvalidateRect(g_Pane, nullptr, TRUE);
}


void
DetailsPaneShow (
    _In_opt_ const CodeMatch* Match,
    _In_opt_z_ PCWSTR Hint
    )
{
    bool hadMatch;

    hadMatch = g_HasMatch;
    g_HasMatch = (Match != nullptr);
    g_Match = g_HasMatch ? *Match : CodeMatch();
    g_Hint = (Hint != nullptr) ? Hint : L"";

    g_ShowAllMappedFrom = false;
    g_ScrollY = 0;
    g_HoverLink = -1;
    g_PressedLink = -1;
    g_FocusLink = -1;

    //
    // The header only changes height when it comes or goes.
    //
    if (hadMatch != g_HasMatch)
    {
        LayoutPane();
    }

    RelayoutBody();

    if ((GetFocus() == g_Body) && !g_Links.empty())
    {
        g_FocusLink = 0;
    }

    InvalidateRect(g_Pane, nullptr, FALSE);
}
