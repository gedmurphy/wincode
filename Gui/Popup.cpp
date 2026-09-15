/*!

    @file Gui/Popup.cpp

    @brief The launcher popup: a search box the hotkey brings up over whatever
           is in front, filled from the clipboard, with the results below it.

    @details A borderless, topmost tool window holding an edit control for the
             query; everything else in it is drawn here. Only the selected
             result is expanded to its message and links, so the popup stays
             short, and it grows and shrinks to fit what it shows, up to most of
             the monitor's height.

             The popup is for a quick look. Anything more, whether Enter, a
             double-click or a link, opens the main window with what was asked
             for.

             It hides when it loses the foreground, as launchers do, so there's
             never a stale popup left behind the debugger.

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
#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <format>


constexpr WCHAR k_PopupClass[] = L"WinCodePopup";

//
// Layout, in 96 DPI units, from the mockups.
//
static constexpr int k_Width = 620;
static constexpr int k_SearchHeight = 54;
static constexpr int k_SidePadding = 18;
static constexpr int k_IconWidth = 20;
static constexpr int k_IconGap = 12;
static constexpr int k_MetaGap = 8;
static constexpr int k_ListPadding = 6;
static constexpr int k_RowGap = 2;
static constexpr int k_RowPaddingTop = 9;
static constexpr int k_RowPaddingBottom = 10;
static constexpr int k_RowPaddingLeft = 16;
static constexpr int k_RowPaddingRight = 12;
static constexpr int k_RowRadius = 10;
static constexpr int k_KindColumn = 104;
static constexpr int k_ColumnGap = 12;
static constexpr int k_KindDot = 7;
static constexpr int k_BlockGap = 4;
static constexpr int k_LinkGapX = 18;
static constexpr int k_LinkGapY = 4;
static constexpr int k_ViaGap = 6;
static constexpr int k_RecentHeadingTop = 10;
static constexpr int k_RecentHeadingBottom = 4;
static constexpr int k_RecentRowPadding = 6;
static constexpr int k_RecentCodeColumn = 110;
static constexpr int k_RecentKindColumn = 110;
static constexpr int k_HintTop = 12;
static constexpr int k_HintBottom = 14;
static constexpr int k_FootPadding = 9;
static constexpr int k_KeyPadding = 5;
static constexpr int k_KeyGap = 4;
static constexpr int k_HintGap = 18;

//
// Where the popup opens, and the tallest it grows, as percentages of the
// monitor's work area.
//
static constexpr int k_TopPercent = 20;
static constexpr int k_MaxHeightPercent = 70;

//
// How many recent lookups are kept, and the longest clipboard text that's
// tried as a query.
//
static constexpr size_t k_RecentLimit = 5;
static constexpr size_t k_ClipboardLimit = 200;

//
// Where the recent lookups are kept in settings.json.
//
static constexpr WCHAR k_RecentSetting[] = L"recent";

//
// How many times, and how far apart, opening the clipboard is tried. Whatever
// just copied to it can still have it open for a moment, and the hotkey often
// follows the copy straight away.
//
static constexpr int k_ClipboardAttempts = 5;
static constexpr DWORD k_ClipboardWaitMs = 20;

static constexpr int k_EditId = 1;

//
// Segoe Fluent Icons' search glyph.
//
static constexpr WCHAR k_GlyphSearch[] = L"\xE721";

static constexpr WCHAR k_EmptyHint[] =
    L"Type a code or a name, or paste one from a log or the debugger. What you look up is listed "
    L"here next time.";

static constexpr WCHAR k_NoMatchHint[] = L"Nothing matches. Try the value in decimal or hex, or part of its name.";


/*!

    @brief What the popup is showing below the search box.

*/
enum class PopupView
{
    Recent,     // The query is empty: recent lookups, or a hint
    NoMatch,    // The query found nothing
    Results     // The query found something
};


/*!

    @brief A lookup made in the popup, kept to offer again when it opens empty.

*/
struct RecentLookup
{
    std::wstring Query;
    CodeKind Kind;
    uint32_t Value;
    std::wstring Name;
};


/*!

    @brief A link in the selected result.

*/
struct PopupLink
{
    //
    // Where the link and its note are, across the popup and down from the
    // selected row's top.
    //
    RECT Rect;
    RECT ViaRect;

    std::wstring Text;
    std::wstring Via;

    //
    // True for the link to the selected code itself, whose details list the
    // codes that map to it. It opens with the query as typed.
    //
    bool ThisCode;

    CodeKind Kind;
    uint32_t Value;
};


/*!

    @brief A hint in the footer: one or two keys, and what they do.

*/
struct KeyHint
{
    PCWSTR Keys[2];
    PCWSTR Label;
};


static HWND g_Popup;
static HWND g_Edit;

static UiFonts g_Fonts;
static UINT g_Dpi;
static int g_LabelLine;
static int g_NameLine;
static int g_MessageLine;
static int g_SmallLine;
static int g_MonoLine;
static int g_QueryLine;

//
// The height of a row showing only its kind and name, and what the selected
// row adds to that for its message and links.
//
static int g_Collapsed;
static int g_Extra;

static std::wstring g_Query;
static ResultSet g_Results;
static std::wstring g_Count;
static int g_Selected = -1;

//
// The selected result in full, and where its message goes, down from its
// row's top.
//
static CodeMatch g_Match;
static bool g_HasMatch;
static RECT g_TextRect;
static std::vector<PopupLink> g_Links;

static std::vector<RecentLookup> g_Recent;
static int g_RecentSelected = -1;

static RECT g_Work;
static int g_Left;
static int g_Top;
static int g_ScrollY;
static int g_HoverLink = -1;
static bool g_TrackingMouse;
static bool g_Copied;

//
// Set while the query is set in code, which runs it itself rather than through
// EN_CHANGE.
//
static bool g_SettingQuery;


/*!

    @brief Scales a length from 96 DPI units to the popup's DPI.

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

    @brief Text with the white space at either end removed.

    @param[in] Text - The text.

    @return The trimmed text.

*/
static
std::wstring
Trimmed (
    _In_ const std::wstring& Text
    )
{
    size_t first;
    size_t last;

    first = Text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos)
    {
        return std::wstring();
    }

    last = Text.find_last_not_of(L" \t\r\n");
    return Text.substr(first, last - first + 1);
}


/*!

    @brief Makes the fonts for a DPI, and measures them.

    @param[in] Dpi - The DPI.

*/
static
void
SetDpi (
    _In_ UINT Dpi
    )
{
    UiFonts fonts;

    if (Dpi == g_Dpi)
    {
        return;
    }

    //
    // The old fonts stay if the new ones can't be made, rather than leave the
    // popup with none.
    //
    if (!FontsCreate(Dpi, &fonts))
    {
        return;
    }

    FontsDelete(&g_Fonts);
    g_Fonts = fonts;
    g_Dpi = Dpi;

    g_LabelLine = FontLineHeight(nullptr, g_Fonts.Label);
    g_NameLine = FontLineHeight(nullptr, g_Fonts.Name);
    g_MessageLine = FontLineHeight(nullptr, g_Fonts.Message);
    g_SmallLine = FontLineHeight(nullptr, g_Fonts.Small);
    g_MonoLine = FontLineHeight(nullptr, g_Fonts.Mono);
    g_QueryLine = FontLineHeight(nullptr, g_Fonts.Query);
    g_Collapsed = Scale(k_RowPaddingTop) + g_NameLine + Scale(k_RowPaddingBottom);

    SendMessageW(g_Edit, WM_SETFONT, RCAST(WPARAM)(g_Fonts.Query), FALSE);
}


/*!

    @brief The search box's text.

    @return The text.

*/
static
std::wstring
EditText (
    void
    )
{
    std::wstring text;
    int length;

    length = GetWindowTextLengthW(g_Edit);
    text.resize(SCAST(size_t)(length) + 1);
    GetWindowTextW(g_Edit, text.data(), length + 1);
    text.resize(SCAST(size_t)(length));

    return text;
}


/*!

    @brief What the popup is showing.

*/
static
PopupView
CurrentView (
    void
    )
{
    if (Trimmed(g_Query).empty())
    {
        return PopupView::Recent;
    }

    if (g_Results.Rows.empty())
    {
        return PopupView::NoMatch;
    }

    return PopupView::Results;
}


/*!

    @brief The distance from one collapsed result row to the next.

*/
static
int
Stride (
    void
    )
{
    return g_Collapsed + Scale(k_RowGap);
}


/*!

    @brief Where a result row starts, down from the top of the list.

    @param[in] Index - The row.

    @return The offset in pixels.

*/
static
int
RowTop (
    _In_ int Index
    )
{
    int top;

    top = Scale(k_ListPadding) + (Index * Stride());

    if ((g_Selected >= 0) && (Index > g_Selected))
    {
        top += g_Extra;
    }

    return top;
}


/*!

    @brief How tall a result row is: taller when it's the selected one.

    @param[in] Index - The row.

    @return The height in pixels.

*/
static
int
RowHeight (
    _In_ int Index
    )
{
    return g_Collapsed + ((Index == g_Selected) ? g_Extra : 0);
}


/*!

    @brief How tall a recent lookup's row is.

*/
static
int
RecentRowHeight (
    void
    )
{
    return (2 * Scale(k_RecentRowPadding)) + g_MessageLine;
}


/*!

    @brief Where a recent lookup's row starts, down from the top of the list.

    @param[in] Index - The row.

    @return The offset in pixels.

*/
static
int
RecentTop (
    _In_ int Index
    )
{
    return Scale(k_RecentHeadingTop) + g_LabelLine + Scale(k_RecentHeadingBottom) + (Index * RecentRowHeight());
}


/*!

    @brief The height of the line under the search box giving the selected
           value in its every form, which is only there with a selection.

*/
static
int
MetaHeight (
    void
    )
{
    return g_HasMatch ? (Scale(k_MetaGap) + g_MonoLine) : 0;
}


/*!

    @brief The height of a key drawn in the footer.

*/
static
int
KeyHeight (
    void
    )
{
    return g_SmallLine + Scale(4);
}


/*!

    @brief The height of the footer, its rule included.

*/
static
int
FootHeight (
    void
    )
{
    return 1 + (2 * Scale(k_FootPadding)) + KeyHeight();
}


/*!

    @brief Where the list starts, down from the top of the popup.

*/
static
int
ListTop (
    void
    )
{
    return Scale(k_SearchHeight) + 1 + MetaHeight() + (g_HasMatch ? Scale(2) : 0);
}


/*!

    @brief The hint shown in place of a list, if there is one.

    @return The hint, or nullptr when there's a list.

*/
static
PCWSTR
HintText (
    void
    )
{
    switch (CurrentView())
    {
    case PopupView::Recent:
        return g_Recent.empty() ? k_EmptyHint : nullptr;

    case PopupView::NoMatch:
        return k_NoMatchHint;

    default:
        return nullptr;
    }
}


/*!

    @brief Where the hint goes, wrapped to the popup's width.

    @param[in] Dc - A device context to measure with.

    @param[in] Top - Its top, in the popup's client coordinates.

    @return The rectangle.

*/
static
RECT
HintRect (
    _In_ HDC Dc,
    _In_ int Top
    )
{
    RECT rect;
    HGDIOBJ previous;
    PCWSTR hint;

    rect = { Scale(k_SidePadding), Top, Scale(k_Width) - Scale(k_SidePadding), Top };

    hint = HintText();
    if (hint == nullptr)
    {
        return rect;
    }

    previous = SelectObject(Dc, g_Fonts.Message);
    DrawTextW(Dc, hint, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(Dc, previous);

    rect.right = Scale(k_Width) - Scale(k_SidePadding);
    return rect;
}


/*!

    @brief The height of everything in the list, scrolled out of sight or not.

    @return The height in pixels.

*/
static
int
ContentHeight (
    void
    )
{
    RECT hint;
    HDC dc;
    int rows;

    switch (CurrentView())
    {
    case PopupView::Results:
        rows = SCAST(int)(g_Results.Rows.size());
        return (2 * Scale(k_ListPadding)) + (rows * Stride()) - Scale(k_RowGap) + g_Extra;

    case PopupView::Recent:
        if (!g_Recent.empty())
        {
            return RecentTop(SCAST(int)(g_Recent.size())) + Scale(k_ListPadding) + Scale(2);
        }
        break;

    default:
        break;
    }

    dc = GetDC(g_Popup);
    hint = HintRect(dc, 0);
    ReleaseDC(g_Popup, dc);

    return Scale(k_HintTop) + (hint.bottom - hint.top) + Scale(k_HintBottom);
}


/*!

    @brief The height of the part of the list that shows.

*/
static
int
ViewportHeight (
    void
    )
{
    RECT client;

    GetClientRect(g_Popup, &client);
    return std::max(SCAST(int)(client.bottom) - ListTop() - FootHeight(), 0);
}


/*!

    @brief The link text for a related code, as in "Win32 error 5
           ERROR_ACCESS_DENIED".

    @param[in] Related - The related code.

    @return The text.

*/
static
std::wstring
RelatedLinkText (
    _In_ const RelatedCode& Related
    )
{
    std::wstring text;

    text = std::format(L"{} {}", CodeKindName(Related.Kind), FormatCodeValue(Related.Kind, Related.Value));

    if (!Related.Name.empty())
    {
        text += L" " + Related.Name;
    }

    return text;
}


/*!

    @brief The note after a related code's link: how it relates.

    @param[in] Related - The related code.

    @return The note, as in "maps to" or "as HRESULT_FROM_NT".

*/
static
std::wstring
RelatedVia (
    _In_ const RelatedCode& Related
    )
{
    //
    // An HRESULT form's link already says HRESULT, so the macro that makes it
    // says more than the label would.
    //
    if ((Related.Relation == RelationKind::AsHResult) && (Related.Via != nullptr) && (Related.Via[0] != L'\0'))
    {
        return std::wstring(L"as ") + Related.Via;
    }

    return RelationLabel(Related.Relation);
}


/*!

    @brief Adds a link to the selected row, flowing it onto a new line when
           the current one is full.

    @param[in] Dc - A device context to measure with.

    @param[in,out] Link - The link, whose rectangles are filled in.

    @param[in] Left - Where lines of links start.

    @param[in] Right - Where they end.

    @param[in,out] X - Where the link goes on the current line, moved on past
                       it.

    @param[in,out] Y - The current line's top, moved down when the link starts
                       a new one.

*/
static
void
AddLink (
    _In_ HDC Dc,
    _Inout_ PopupLink& Link,
    _In_ int Left,
    _In_ int Right,
    _Inout_ int* X,
    _Inout_ int* Y
    )
{
    int textWidth;
    int viaWidth;
    int total;

    textWidth = TextWidth(Dc, g_Fonts.Small, Link.Text);
    viaWidth = TextWidth(Dc, g_Fonts.Small, Link.Via);
    total = textWidth + ((viaWidth > 0) ? (Scale(k_ViaGap) + viaWidth) : 0);

    if ((*X > Left) && ((*X + total) > Right))
    {
        *X = Left;
        *Y += g_SmallLine + Scale(k_LinkGapY);
    }

    Link.Rect = { *X, *Y, std::min(*X + textWidth, Right), *Y + g_SmallLine };
    Link.ViaRect = { Link.Rect.right + Scale(k_ViaGap), *Y, std::min(*X + total, Right), *Y + g_SmallLine };

    g_Links.push_back(Link);
    *X += total + Scale(k_LinkGapX);
}


/*!

    @brief Looks up the selected result in full, and lays out its message and
           links.

*/
static
void
LayoutSelected (
    void
    )
{
    PopupLink link = {};
    const ResultRow* row;
    HGDIOBJ previous;
    HDC dc;
    int left;
    int right;
    int x;
    int y;

    g_Links.clear();
    g_Extra = 0;
    g_HasMatch = false;
    g_TextRect = {};

    if ((CurrentView() != PopupView::Results) || (g_Selected < 0))
    {
        return;
    }

    row = &g_Results.Rows[SCAST(size_t)(g_Selected)];
    g_HasMatch = FindMatch(row->Kind, row->Value, &g_Match);
    if (!g_HasMatch)
    {
        return;
    }

    left = Scale(k_ListPadding) + Scale(k_RowPaddingLeft) + Scale(k_KindColumn) + Scale(k_ColumnGap);
    right = Scale(k_Width) - Scale(k_ListPadding) - Scale(k_RowPaddingRight);
    y = Scale(k_RowPaddingTop) + g_NameLine;

    dc = GetDC(g_Popup);

    if (!g_Match.Text.empty())
    {
        y += Scale(k_BlockGap);
        g_TextRect = { left, y, right, y };

        previous = SelectObject(dc, g_Fonts.Message);
        DrawTextW(dc, g_Match.Text.c_str(), -1, &g_TextRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
        SelectObject(dc, previous);

        g_TextRect.right = right;
        y = g_TextRect.bottom;
    }

    if (!g_Match.Related.empty() || !g_Match.MappedFrom.empty())
    {
        y += Scale(k_BlockGap) + Scale(2);
        x = left;

        for (const RelatedCode& related : g_Match.Related)
        {
            link.Text = RelatedLinkText(related);
            link.Via = RelatedVia(related);
            link.ThisCode = false;
            link.Kind = related.Kind;
            link.Value = related.Value;
            AddLink(dc, link, left, right, &x, &y);
        }

        //
        // Dozens of codes can map to one Win32 error, far too many for the
        // popup, so they're a link to the window, which lists them all.
        //
        if (!g_Match.MappedFrom.empty())
        {
            link.Text = std::format(L"{} NTSTATUS {} here",
                                    g_Match.MappedFrom.size(),
                                    (g_Match.MappedFrom.size() == 1) ? L"code maps" : L"codes map");
            link.Via.clear();
            link.ThisCode = true;
            link.Kind = g_Match.Kind;
            link.Value = g_Match.Value;
            AddLink(dc, link, left, right, &x, &y);
        }

        y += g_SmallLine;
    }

    ReleaseDC(g_Popup, dc);

    g_Extra = std::max(y + Scale(k_RowPaddingBottom) - g_Collapsed, 0);
}


/*!

    @brief Places the search box between the icon and the count.

*/
static
void
LayoutEdit (
    void
    )
{
    HDC dc;
    int countWidth;
    int left;
    int right;
    int height;

    dc = GetDC(g_Popup);
    countWidth = TextWidth(dc, g_Fonts.Small, g_Count);
    ReleaseDC(g_Popup, dc);

    left = Scale(k_SidePadding) + Scale(k_IconWidth) + Scale(k_IconGap);
    right = Scale(k_Width) - Scale(k_SidePadding) - ((countWidth > 0) ? (countWidth + Scale(k_IconGap)) : 0);
    height = g_QueryLine + Scale(2);

    MoveWindow(g_Edit, left, (Scale(k_SearchHeight) - height) / 2, std::max(right - left, Scale(40)), height, TRUE);
}


/*!

    @brief Sizes the popup to what it's showing, up to most of the monitor's
           height, keeping the scroll position in range.

*/
static
void
Resize (
    void
    )
{
    int maxHeight;
    int height;
    int content;

    content = ContentHeight();
    maxHeight = MulDiv(g_Work.bottom - g_Work.top, k_MaxHeightPercent, 100);
    height = std::min(ListTop() + content + FootHeight(), maxHeight);

    g_ScrollY = std::clamp(g_ScrollY, 0, std::max(content - (height - ListTop() - FootHeight()), 0));

    SetWindowPos(g_Popup, nullptr, g_Left, g_Top, Scale(k_Width), height, SWP_NOZORDER | SWP_NOACTIVATE);
}


/*!

    @brief Scrolls the list so the selected row, or recent lookup, is in view.

*/
static
void
EnsureSelectedVisible (
    void
    )
{
    int top;
    int bottom;
    int viewport;

    switch (CurrentView())
    {
    case PopupView::Results:
        if (g_Selected < 0)
        {
            return;
        }

        top = RowTop(g_Selected) - Scale(k_ListPadding);
        bottom = RowTop(g_Selected) + RowHeight(g_Selected) + Scale(k_ListPadding);
        break;

    case PopupView::Recent:
        if (g_RecentSelected < 0)
        {
            g_ScrollY = 0;
            return;
        }

        top = RecentTop(g_RecentSelected);
        bottom = top + RecentRowHeight() + Scale(k_ListPadding);
        break;

    default:
        return;
    }

    viewport = ViewportHeight();

    if (top < g_ScrollY)
    {
        g_ScrollY = top;
    }
    else if (bottom > (g_ScrollY + viewport))
    {
        g_ScrollY = bottom - viewport;
    }
}


/*!

    @brief Lays everything out again for a new query or selection, and
           resizes to fit.

*/
static
void
Relayout (
    void
    )
{
    LayoutSelected();
    LayoutEdit();
    Resize();
    EnsureSelectedVisible();
    InvalidateRect(g_Popup, nullptr, FALSE);
}


/*!

    @brief Looks up whatever is in the search box.

*/
static
void
RunQuery (
    void
    )
{
    g_Query = EditText();
    g_Results = BuildResults(g_Query.c_str());

    if (Trimmed(g_Query).empty())
    {
        g_Count.clear();
    }
    else if (!g_Results.HasQuery)
    {
        g_Count = L"No match";
    }
    else
    {
        g_Count = ResultCountText(g_Results);
    }

    g_Selected = g_Results.Rows.empty() ? -1 : 0;
    g_RecentSelected = -1;
    g_ScrollY = 0;
    g_HoverLink = -1;
    g_Copied = false;

    Relayout();
}


/*!

    @brief Sets the query and looks it up.

    @param[in] Text - The query.

*/
static
void
SetQuery (
    _In_ const std::wstring& Text
    )
{
    g_SettingQuery = true;
    SetWindowTextW(g_Edit, Text.c_str());
    g_SettingQuery = false;

    RunQuery();
}


/*!

    @brief The name a kind of code is saved under in settings.json.

    @param[in] Kind - The kind.

    @return A static, never null, name.

*/
static
PCWSTR
KindKey (
    _In_ CodeKind Kind
    )
{
    switch (Kind)
    {
    case CodeKind::WinError:
        return L"win32";

    case CodeKind::HResult:
        return L"hresult";

    case CodeKind::NtStatus:
        return L"ntstatus";

    case CodeKind::BugCheck:
        return L"bugcheck";

    default:
        return L"message";
    }
}


/*!

    @brief The kind of code a name from settings.json means.

    @param[in] Key - The name, as KindKey gives it.

    @param[out] Kind - Receives the kind.

    @return True if the name is one KindKey gives.

*/
static
bool
KindFromKey (
    _In_ const std::wstring& Key,
    _Out_ CodeKind* Kind
    )
{
    static const CodeKind kinds[] =
    {
        CodeKind::WinError,
        CodeKind::HResult,
        CodeKind::NtStatus,
        CodeKind::BugCheck,
        CodeKind::WindowMessage
    };

    for (CodeKind kind : kinds)
    {
        if (Key == KindKey(kind))
        {
            *Kind = kind;
            return true;
        }
    }

    *Kind = CodeKind::WinError;
    return false;
}


/*!

    @brief Reads the recent lookups from settings.json.

*/
static
void
LoadRecent (
    void
    )
{
    const JsonValue* list;
    const JsonValue* query;
    const JsonValue* kind;
    const JsonValue* value;
    const JsonValue* name;
    RecentLookup recent;
    double number;

    g_Recent.clear();

    list = SettingsRoot().Find(k_RecentSetting);
    if ((list == nullptr) || (list->GetType() != JsonValue::Type::Array))
    {
        return;
    }

    for (const JsonValue& entry : list->Elements())
    {
        query = entry.Find(L"query");
        kind = entry.Find(L"kind");
        value = entry.Find(L"value");
        name = entry.Find(L"name");

        //
        // An entry a hand edit has broken costs that entry, not the list.
        //
        if ((query == nullptr) || (kind == nullptr) || (value == nullptr) ||
            !KindFromKey(kind->AsString(L""), &recent.Kind))
        {
            continue;
        }

        number = value->AsNumber(-1.0);
        if ((number < 0.0) || (number > 4294967295.0))
        {
            continue;
        }

        recent.Query = query->AsString(L"");
        recent.Value = SCAST(uint32_t)(number);
        recent.Name = (name != nullptr) ? name->AsString(L"") : std::wstring();

        if (recent.Query.empty())
        {
            continue;
        }

        g_Recent.push_back(recent);
        if (g_Recent.size() == k_RecentLimit)
        {
            break;
        }
    }
}


/*!

    @brief Writes the recent lookups to settings.json.

*/
static
void
SaveRecent (
    void
    )
{
    JsonValue* list;
    JsonValue* entry;

    list = &SettingsRoot().Member(k_RecentSetting);
    list->SetArray();

    for (const RecentLookup& recent : g_Recent)
    {
        entry = &list->Append();
        entry->Member(L"query").SetString(recent.Query.c_str());
        entry->Member(L"kind").SetString(KindKey(recent.Kind));
        entry->Member(L"value").SetNumber(SCAST(double)(recent.Value));
        entry->Member(L"name").SetString(recent.Name.c_str());
    }

    SettingsSave();
}


/*!

    @brief Adds the selected result to the recent lookups, if there is one,
           and saves them.

*/
static
void
RememberSelected (
    void
    )
{
    RecentLookup recent;
    const ResultRow* row;

    if ((CurrentView() != PopupView::Results) || (g_Selected < 0))
    {
        return;
    }

    row = &g_Results.Rows[SCAST(size_t)(g_Selected)];
    recent = { Trimmed(g_Query), row->Kind, row->Value, row->Name };

    std::erase_if(g_Recent,
                  [&recent] (const RecentLookup& Other)
                  {
                      return (Other.Kind == recent.Kind) && (Other.Value == recent.Value);
                  });

    g_Recent.insert(g_Recent.begin(), recent);
    if (g_Recent.size() > k_RecentLimit)
    {
        g_Recent.resize(k_RecentLimit);
    }

    SaveRecent();
}


/*!

    @brief Hides the popup, remembering what it was showing.

*/
static
void
PopupHide (
    void
    )
{
    if (!IsWindowVisible(g_Popup))
    {
        return;
    }

    RememberSelected();
    ShowWindow(g_Popup, SW_HIDE);
}


/*!

    @brief Opens the main window with a lookup, and hides the popup.

    @param[in] Query - What to look up.

    @param[in] Kind - The result to select's kind.

    @param[in] Value - Its value.

*/
static
void
OpenInWindow (
    _In_ const std::wstring& Query,
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    )
{
    std::wstring query;

    //
    // A copy, as hiding the popup can change what the reference refers to.
    //
    query = Trimmed(Query);

    //
    // The window comes forward first, while WinCode still has the foreground
    // to give it. Hiding the popup first would hand the foreground back to
    // whatever was in front before.
    //
    MainWindowOpen(query.c_str(), true, Kind, Value);
    PopupHide();
}


/*!

    @brief Looks a recent lookup up again, and selects what it found before.

    @param[in] Index - The recent lookup.

*/
static
void
UseRecent (
    _In_ int Index
    )
{
    RecentLookup recent;

    if ((Index < 0) || (SCAST(size_t)(Index) >= g_Recent.size()))
    {
        return;
    }

    recent = g_Recent[SCAST(size_t)(Index)];
    SetQuery(recent.Query);
    SendMessageW(g_Edit, EM_SETSEL, recent.Query.size(), recent.Query.size());

    for (size_t i = 0; i < g_Results.Rows.size(); i++)
    {
        if ((g_Results.Rows[i].Kind == recent.Kind) && (g_Results.Rows[i].Value == recent.Value))
        {
            g_Selected = SCAST(int)(i);
            Relayout();
            return;
        }
    }
}


/*!

    @brief Moves the selection through the results, or the recent lookups.

    @param[in] Delta - How far: 1 for down, -1 for up.

*/
static
void
MoveSelection (
    _In_ int Delta
    )
{
    int next;

    switch (CurrentView())
    {
    case PopupView::Results:
        next = std::clamp(g_Selected + Delta, 0, SCAST(int)(g_Results.Rows.size()) - 1);
        if (next == g_Selected)
        {
            return;
        }

        g_Selected = next;
        g_HoverLink = -1;
        g_Copied = false;
        Relayout();
        return;

    case PopupView::Recent:
        if (g_Recent.empty())
        {
            return;
        }

        //
        // Up from the first recent lookup goes back to having none selected,
        // where Enter does nothing.
        //
        g_RecentSelected = std::clamp(g_RecentSelected + Delta, -1, SCAST(int)(g_Recent.size()) - 1);
        EnsureSelectedVisible();
        InvalidateRect(g_Popup, nullptr, FALSE);
        return;

    default:
        return;
    }
}


/*!

    @brief What Enter does: opens the selected result in the window, or looks
           the selected recent lookup up again.

*/
static
void
Activate (
    void
    )
{
    const ResultRow* row;

    switch (CurrentView())
    {
    case PopupView::Results:
        row = &g_Results.Rows[SCAST(size_t)(g_Selected)];
        OpenInWindow(g_Query, row->Kind, row->Value);
        return;

    case PopupView::Recent:
        UseRecent(g_RecentSelected);
        return;

    default:
        return;
    }
}


/*!

    @brief What Esc does: clears the query, or hides the popup when it's
           already clear.

*/
static
void
Escape (
    void
    )
{
    if (GetWindowTextLengthW(g_Edit) > 0)
    {
        //
        // Remembered before it's cleared, or dismissing the popup with Esc,
        // Esc would never leave anything in the recent lookups.
        //
        RememberSelected();
        SetQuery(std::wstring());
        return;
    }

    PopupHide();
}


/*!

    @brief The selected result in one line, for Ctrl+C.

    @return For example "0xC0000022 STATUS_ACCESS_DENIED (NTSTATUS): {Access
            Denied} A process has requested access to an object, ...".

*/
static
std::wstring
Summary (
    void
    )
{
    std::wstring name;
    std::wstring text;

    if (!g_Match.Names.empty())
    {
        name = g_Match.Names[0];
    }
    else if (!g_Match.Description.empty())
    {
        name = g_Match.Description;
    }
    else
    {
        name = L"(no name)";
    }

    for (WCHAR c : g_Match.Text)
    {
        if (c != L'\r')
        {
            text += (c == L'\n') ? L' ' : c;
        }
    }

    text = Trimmed(text);

    if (text.empty())
    {
        return std::format(L"{} {} ({})", FormatCodeValue(g_Match.Kind, g_Match.Value), name, CodeKindName(g_Match.Kind));
    }

    return std::format(L"{} {} ({}): {}",
                       FormatCodeValue(g_Match.Kind, g_Match.Value),
                       name,
                       CodeKindName(g_Match.Kind),
                       text);
}


/*!

    @brief The clipboard's text, if it's worth looking up.

    @details Only a single short line that finds something is used: a code or
             a name copied from a debugger or a log. Anything longer is log
             text, which is for log scanning, and anything that finds nothing
             would only open the popup on "No match".

    @return The text, or an empty string.

*/
static
std::wstring
ClipboardQuery (
    void
    )
{
    std::wstring text;
    HANDLE data;
    PCWSTR chars;
    bool opened;

    if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        return text;
    }

    opened = false;
    for (int attempt = 0; (attempt < k_ClipboardAttempts) && !opened; attempt++)
    {
        opened = (OpenClipboard(g_Popup) != FALSE);
        if (!opened)
        {
            Sleep(k_ClipboardWaitMs);
        }
    }

    if (!opened)
    {
        return text;
    }

    data = GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr)
    {
        chars = SCAST(PCWSTR)(GlobalLock(data));
        if (chars != nullptr)
        {
            text.assign(chars, wcsnlen(chars, k_ClipboardLimit + 1));
            GlobalUnlock(data);
        }
    }

    CloseClipboard();

    if (text.size() > k_ClipboardLimit)
    {
        return std::wstring();
    }

    text = Trimmed(text);
    if (text.empty() || (text.find_first_of(L"\r\n") != std::wstring::npos))
    {
        return std::wstring();
    }

    if (BuildResults(text.c_str()).Rows.empty())
    {
        return std::wstring();
    }

    return text;
}


/*!

    @brief Finds the result row at a point in the list.

    @param[in] Y - The point, down from the top of the list, scrolling
                   included.

    @return The row, or -1 for none.

*/
static
int
IndexAt (
    _In_ int Y
    )
{
    int stride;
    int y;
    int index;

    stride = Stride();
    y = Y - Scale(k_ListPadding);

    if ((y < 0) || g_Results.Rows.empty())
    {
        return -1;
    }

    if ((g_Selected >= 0) && (y >= (g_Selected * stride)))
    {
        if (y < ((g_Selected * stride) + g_Collapsed + g_Extra))
        {
            return g_Selected;
        }

        index = (y - g_Extra) / stride;
    }
    else
    {
        index = y / stride;
    }

    if (index >= SCAST(int)(g_Results.Rows.size()))
    {
        return -1;
    }

    //
    // In the gap between two rows.
    //
    if ((Y - RowTop(index)) >= RowHeight(index))
    {
        return -1;
    }

    return index;
}


/*!

    @brief Finds the recent lookup at a point in the list.

    @param[in] Y - The point, down from the top of the list, scrolling
                   included.

    @return The recent lookup, or -1 for none.

*/
static
int
RecentAt (
    _In_ int Y
    )
{
    int index;

    if (Y < RecentTop(0))
    {
        return -1;
    }

    index = (Y - RecentTop(0)) / RecentRowHeight();
    if (index >= SCAST(int)(g_Recent.size()))
    {
        return -1;
    }

    return index;
}


/*!

    @brief Finds the link under a point.

    @param[in] Point - The point, in the popup's client coordinates.

    @return The link, or -1 for none.

*/
static
int
LinkAt (
    _In_ POINT Point
    )
{
    RECT client;
    RECT rect;
    int offset;

    if ((CurrentView() != PopupView::Results) || g_Links.empty())
    {
        return -1;
    }

    GetClientRect(g_Popup, &client);
    if ((Point.y < ListTop()) || (Point.y >= (client.bottom - FootHeight())))
    {
        return -1;
    }

    offset = ListTop() - g_ScrollY + RowTop(g_Selected);

    for (size_t i = 0; i < g_Links.size(); i++)
    {
        rect = g_Links[i].Rect;
        OffsetRect(&rect, 0, offset);

        if (PtInRect(&rect, Point))
        {
            return SCAST(int)(i);
        }
    }

    return -1;
}


/*!

    @brief Follows a link, opening the window at the code it names.

    @param[in] Index - The link.

*/
static
void
FollowLink (
    _In_ int Index
    )
{
    PopupLink link;

    link = g_Links[SCAST(size_t)(Index)];

    if (link.ThisCode)
    {
        OpenInWindow(g_Query, link.Kind, link.Value);
        return;
    }

    OpenInWindow(FormatCodeValue(link.Kind, link.Value), link.Kind, link.Value);
}


/*!

    @brief Handles a click, or a double-click, in the popup.

    @param[in] Point - Where, in client coordinates.

    @param[in] Double - True for a double-click.

*/
static
void
OnClick (
    _In_ POINT Point,
    _In_ bool Double
    )
{
    RECT client;
    int y;
    int index;

    GetClientRect(g_Popup, &client);
    if ((Point.y < ListTop()) || (Point.y >= (client.bottom - FootHeight())))
    {
        return;
    }

    y = Point.y - ListTop() + g_ScrollY;

    switch (CurrentView())
    {
    case PopupView::Results:
        index = LinkAt(Point);
        if (index >= 0)
        {
            FollowLink(index);
            return;
        }

        index = IndexAt(y);
        if (index < 0)
        {
            return;
        }

        //
        // The first click of a double-click has already selected the row.
        //
        if (Double && (index == g_Selected))
        {
            Activate();
            return;
        }

        if (index != g_Selected)
        {
            g_Selected = index;
            g_HoverLink = -1;
            g_Copied = false;
            Relayout();
        }
        return;

    case PopupView::Recent:
        UseRecent(RecentAt(y));
        return;

    default:
        return;
    }
}


/*!

    @brief Draws the search row's icon, the count, the rule beneath, and the
           selected value in its every form.

    @param[in] Dc - The device context.

*/
static
void
PaintSearch (
    _In_ HDC Dc
    )
{
    std::wstring value;
    RECT rect;
    int width;

    width = Scale(k_Width);

    SelectObject(Dc, g_Fonts.Icon);
    SetTextColor(Dc, ThemeMutedColour());
    rect = { Scale(k_SidePadding), 0, Scale(k_SidePadding) + Scale(k_IconWidth), Scale(k_SearchHeight) };
    DrawTextW(Dc, k_GlyphSearch, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);

    if (!g_Count.empty())
    {
        SelectObject(Dc, g_Fonts.Small);
        SetTextColor(Dc, ThemeFaintColour());
        rect = { 0, 0, width - Scale(k_SidePadding), Scale(k_SearchHeight) };
        DrawTextW(Dc, g_Count.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
    }

    rect = { 0, Scale(k_SearchHeight), width, Scale(k_SearchHeight) + 1 };
    FillSolid(Dc, rect, ThemeSelectionColour());

    if (!g_HasMatch)
    {
        return;
    }

    value = DescribeValue(g_Match.Kind, g_Match.Value);
    rect.left = Scale(k_SidePadding);
    rect.top = Scale(k_SearchHeight) + 1 + Scale(k_MetaGap);
    rect.right = width - Scale(k_SidePadding);
    rect.bottom = rect.top + g_MonoLine;

    SelectObject(Dc, g_Fonts.Mono);
    SetTextColor(Dc, ThemeFaintColour());
    DrawTextW(Dc, value.c_str(), -1, &rect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}


/*!

    @brief Draws a kind's dot and name.

    @param[in] Dc - The device context.

    @param[in] Kind - The kind.

    @param[in] X - Where it starts.

    @param[in] Top - The top of the line it's centred in.

    @param[in] Height - The line's height.

    @param[in] Right - Where it must end.

*/
static
void
PaintKind (
    _In_ HDC Dc,
    _In_ CodeKind Kind,
    _In_ int X,
    _In_ int Top,
    _In_ int Height,
    _In_ int Right
    )
{
    RECT rect;
    int size;
    int top;

    size = Scale(k_KindDot);
    top = Top + ((Height - g_LabelLine) / 2);

    rect = { X, top + ((g_LabelLine - size) / 2), X + size, top + ((g_LabelLine - size) / 2) + size };
    FillDot(Dc, rect, ThemeKindColour(Kind));

    rect = { X + size + Scale(6), top, Right, top + g_LabelLine };
    SelectObject(Dc, g_Fonts.Label);
    SetTextColor(Dc, ThemeKindColour(Kind));
    DrawTextW(Dc, CodeKindName(Kind), -1, &rect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}


/*!

    @brief Draws a selected row's rounded fill and accent marker.

    @param[in] Dc - The device context.

    @param[in] Bounds - The row.

    @param[in] MarkerBottom - Where the marker ends, which keeps it beside the
                              row's first line in an expanded row.

*/
static
void
PaintSelection (
    _In_ HDC Dc,
    _In_ const RECT& Bounds,
    _In_ int MarkerBottom
    )
{
    RECT marker;

    FillRounded(Dc, Bounds, ThemeSelectionColour(), Scale(k_RowRadius));

    marker = { Bounds.left, Bounds.top + Scale(11), Bounds.left + Scale(3), MarkerBottom };
    if (marker.bottom > marker.top)
    {
        FillRounded(Dc, marker, ThemeAccentColour(), Scale(3));
    }
}


/*!

    @brief Draws one result row: kind, name and value, and for the selected
           row its message and links.

    @param[in] Dc - The device context.

    @param[in] Index - The row.

    @param[in] Top - Its top, in client coordinates.

*/
static
void
PaintRow (
    _In_ HDC Dc,
    _In_ int Index,
    _In_ int Top
    )
{
    const ResultRow* row;
    std::wstring value;
    HGDIOBJ previousPen;
    HPEN pen;
    RECT bounds;
    RECT rect;
    int x;
    int headTop;
    int valueWidth;

    row = &g_Results.Rows[SCAST(size_t)(Index)];
    bounds = { Scale(k_ListPadding), Top, Scale(k_Width) - Scale(k_ListPadding), Top + RowHeight(Index) };

    if (Index == g_Selected)
    {
        PaintSelection(Dc, bounds, Top + g_Collapsed - Scale(11));
    }

    x = bounds.left + Scale(k_RowPaddingLeft);
    headTop = Top + Scale(k_RowPaddingTop);

    PaintKind(Dc, row->Kind, x, headTop, g_NameLine, x + Scale(k_KindColumn));

    value = FormatCodeValue(row->Kind, row->Value);
    valueWidth = TextWidth(Dc, g_Fonts.Message, value);
    rect = { bounds.right - Scale(k_RowPaddingRight) - valueWidth, headTop, bounds.right - Scale(k_RowPaddingRight), headTop + g_NameLine };
    SelectObject(Dc, g_Fonts.Message);
    SetTextColor(Dc, ThemeFaintColour());
    DrawTextW(Dc, value.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    rect = { x + Scale(k_KindColumn) + Scale(k_ColumnGap), headTop, rect.left - Scale(k_ColumnGap), headTop + g_NameLine };
    SelectObject(Dc, g_Fonts.Name);
    SetTextColor(Dc, row->Name.empty() ? ThemeFaintColour() : ThemeTextColour());
    DrawTextW(Dc,
              row->Name.empty() ? L"(no name)" : row->Name.c_str(),
              -1,
              &rect,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);

    if ((Index != g_Selected) || !g_HasMatch)
    {
        return;
    }

    if (!g_Match.Text.empty())
    {
        rect = g_TextRect;
        OffsetRect(&rect, 0, Top);
        SelectObject(Dc, g_Fonts.Message);
        SetTextColor(Dc, ThemeMutedColour());
        DrawTextW(Dc, g_Match.Text.c_str(), -1, &rect, DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
    }

    SelectObject(Dc, g_Fonts.Small);

    for (size_t i = 0; i < g_Links.size(); i++)
    {
        rect = g_Links[i].Rect;
        OffsetRect(&rect, 0, Top);
        SetTextColor(Dc, ThemeAccentColour());
        DrawTextW(Dc, g_Links[i].Text.c_str(), -1, &rect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        //
        // Links are only underlined under the mouse, as Windows 11 does.
        //
        if (SCAST(int)(i) == g_HoverLink)
        {
            pen = CreatePen(PS_SOLID, 1, ThemeAccentColour());
            previousPen = SelectObject(Dc, pen);
            MoveToEx(Dc, rect.left, rect.bottom - 1, nullptr);
            LineTo(Dc, rect.right, rect.bottom - 1);
            SelectObject(Dc, previousPen);
            DeleteObject(pen);
        }

        if (!g_Links[i].Via.empty())
        {
            rect = g_Links[i].ViaRect;
            OffsetRect(&rect, 0, Top);
            SetTextColor(Dc, ThemeFaintColour());
            DrawTextW(Dc, g_Links[i].Via.c_str(), -1, &rect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        }
    }
}


/*!

    @brief Draws the result rows in view.

    @param[in] Dc - The device context.

    @param[in] Client - The popup's client area.

*/
static
void
PaintRows (
    _In_ HDC Dc,
    _In_ const RECT& Client
    )
{
    int origin;
    int bottom;
    int first;
    int top;

    origin = ListTop() - g_ScrollY;
    bottom = Client.bottom - FootHeight();

    //
    // Starts early enough to be sure of the first row in view, however far
    // the expanded row has pushed the rest down.
    //
    first = std::max(((g_ScrollY - Scale(k_ListPadding) - g_Extra) / Stride()) - 1, 0);

    for (int i = first; i < SCAST(int)(g_Results.Rows.size()); i++)
    {
        top = origin + RowTop(i);
        if (top > bottom)
        {
            break;
        }

        PaintRow(Dc, i, top);
    }
}


/*!

    @brief Draws the recent lookups.

    @param[in] Dc - The device context.

*/
static
void
PaintRecent (
    _In_ HDC Dc
    )
{
    RECT bounds;
    RECT rect;
    int origin;
    int top;
    int textTop;
    int x;

    origin = ListTop() - g_ScrollY;

    rect = { Scale(k_SidePadding),
             origin + Scale(k_RecentHeadingTop),
             Scale(k_Width) - Scale(k_SidePadding),
             origin + Scale(k_RecentHeadingTop) + g_LabelLine };
    SelectObject(Dc, g_Fonts.Label);
    SetTextColor(Dc, ThemeMutedColour());
    DrawTextW(Dc, L"Recent", -1, &rect, DT_SINGLELINE | DT_NOPREFIX);

    for (int i = 0; i < SCAST(int)(g_Recent.size()); i++)
    {
        const RecentLookup& recent = g_Recent[SCAST(size_t)(i)];

        top = origin + RecentTop(i);
        bounds = { Scale(k_ListPadding), top, Scale(k_Width) - Scale(k_ListPadding), top + RecentRowHeight() };

        if (i == g_RecentSelected)
        {
            PaintSelection(Dc, bounds, bounds.bottom - Scale(8));
        }

        x = bounds.left + Scale(k_RowPaddingLeft);
        textTop = top + Scale(k_RecentRowPadding);

        rect = { x, textTop, x + Scale(k_RecentCodeColumn), textTop + g_MessageLine };
        SelectObject(Dc, g_Fonts.Mono);
        SetTextColor(Dc, ThemeMutedColour());
        DrawTextW(Dc, recent.Query.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        x += Scale(k_RecentCodeColumn) + Scale(k_ColumnGap);

        PaintKind(Dc, recent.Kind, x, textTop, g_MessageLine, x + Scale(k_RecentKindColumn));
        x += Scale(k_RecentKindColumn) + Scale(k_ColumnGap);

        rect = { x, textTop, bounds.right - Scale(k_RowPaddingRight), textTop + g_MessageLine };
        SelectObject(Dc, g_Fonts.Message);
        SetTextColor(Dc, ThemeTextColour());
        DrawTextW(Dc,
                  recent.Name.empty() ? L"(no name)" : recent.Name.c_str(),
                  -1,
                  &rect,
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
}


/*!

    @brief Draws the hint shown when there's no list.

    @param[in] Dc - The device context.

*/
static
void
PaintHint (
    _In_ HDC Dc
    )
{
    RECT rect;
    PCWSTR hint;

    hint = HintText();
    if (hint == nullptr)
    {
        return;
    }

    rect = HintRect(Dc, ListTop() + Scale(k_HintTop));
    SelectObject(Dc, g_Fonts.Message);
    SetTextColor(Dc, ThemeFaintColour());
    DrawTextW(Dc, hint, -1, &rect, DT_WORDBREAK | DT_NOPREFIX);
}


/*!

    @brief Draws the footer: the keys that work in what's showing, and what
           they do.

    @param[in] Dc - The device context.

    @param[in] Client - The popup's client area.

*/
static
void
PaintFoot (
    _In_ HDC Dc,
    _In_ const RECT& Client
    )
{
    std::vector<KeyHint> hints;
    bool hasQuery;
    RECT rect;
    RECT key;
    int x;
    int top;
    int width;

    hasQuery = (GetWindowTextLengthW(g_Edit) > 0);

    switch (CurrentView())
    {
    case PopupView::Results:
        hints.push_back({ { L"Enter", nullptr }, L"Open in window" });
        hints.push_back({ { L"\x2191", L"\x2193" }, L"Move" });
        hints.push_back({ { L"Ctrl", L"C" }, g_Copied ? L"Copied" : L"Copy summary" });
        break;

    case PopupView::Recent:
        if (!g_Recent.empty())
        {
            hints.push_back({ { L"Enter", nullptr }, L"Look up" });
            hints.push_back({ { L"\x2193", nullptr }, L"Recent" });
        }
        break;

    default:
        break;
    }

    //
    // Esc clears a query before it closes, and says which it will do.
    //
    hints.push_back({ { L"Esc", nullptr }, hasQuery ? L"Clear" : L"Close" });

    rect = { 0, Client.bottom - FootHeight(), Client.right, Client.bottom };
    FillRect(Dc, &rect, ThemeBackgroundBrush());

    rect.bottom = rect.top + 1;
    FillSolid(Dc, rect, ThemeSelectionColour());

    x = Scale(k_SidePadding);
    top = Client.bottom - FootHeight() + 1 + Scale(k_FootPadding);
    SelectObject(Dc, g_Fonts.Small);

    for (const KeyHint& hint : hints)
    {
        for (PCWSTR name : hint.Keys)
        {
            if (name == nullptr)
            {
                continue;
            }

            width = TextWidth(Dc, g_Fonts.Small, name) + (2 * Scale(k_KeyPadding));
            key = { x, top, x + width, top + KeyHeight() };
            FillRounded(Dc, key, ThemeControlColour(), Scale(6));
            FrameRounded(Dc, key, ThemeFaintColour(), Scale(6));

            SetTextColor(Dc, ThemeMutedColour());
            DrawTextW(Dc, name, -1, &key, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);

            x += width + Scale(k_KeyGap);
        }

        x += Scale(2);
        width = TextWidth(Dc, g_Fonts.Small, hint.Label);
        rect = { x, top, x + width, top + KeyHeight() };
        SetTextColor(Dc, ThemeMutedColour());
        DrawTextW(Dc, hint.Label, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        x += width + Scale(k_HintGap);
    }
}


/*!

    @brief Paints the popup through an off-screen bitmap, so it never
           flickers as it resizes.

*/
static
void
Paint (
    void
    )
{
    PAINTSTRUCT paint;
    HBITMAP bitmap;
    HGDIOBJ previousBitmap;
    PopupView view;
    RECT client;
    HDC dc;
    HDC memory;
    int saved;

    dc = BeginPaint(g_Popup, &paint);
    GetClientRect(g_Popup, &client);

    memory = CreateCompatibleDC(dc);
    bitmap = CreateCompatibleBitmap(dc, std::max(SCAST(int)(client.right), 1), std::max(SCAST(int)(client.bottom), 1));
    previousBitmap = SelectObject(memory, bitmap);

    FillRect(memory, &client, ThemeControlBrush());
    SetBkMode(memory, TRANSPARENT);

    saved = SaveDC(memory);
    PaintSearch(memory);

    //
    // The list scrolls under the search row and the footer, so it's clipped
    // to the space between them.
    //
    IntersectClipRect(memory, 0, ListTop(), client.right, client.bottom - FootHeight());

    view = CurrentView();
    if (view == PopupView::Results)
    {
        PaintRows(memory, client);
    }
    else if ((view == PopupView::Recent) && !g_Recent.empty())
    {
        PaintRecent(memory);
    }
    else
    {
        PaintHint(memory);
    }

    RestoreDC(memory, saved);

    saved = SaveDC(memory);
    PaintFoot(memory, client);
    RestoreDC(memory, saved);

    BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);

    SelectObject(memory, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);

    EndPaint(g_Popup, &paint);
}


/*!

    @brief Subclass of the search box: the popup's keys.

*/
static
LRESULT
CALLBACK
PopupEditProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam,
    _In_ UINT_PTR Id,
    _In_ DWORD_PTR Data
    )
{
    int page;

    UNREFERENCED_PARAMETER(Data);

    switch (Message)
    {
    case WM_KEYDOWN:
        page = std::max(ViewportHeight() / std::max(Stride(), 1), 1);

        switch (WParam)
        {
        case VK_DOWN:
            MoveSelection(1);
            return 0;

        case VK_UP:
            MoveSelection(-1);
            return 0;

        case VK_NEXT:
            MoveSelection(page);
            return 0;

        case VK_PRIOR:
            MoveSelection(-page);
            return 0;

        case VK_RETURN:
            Activate();
            return 0;

        case VK_ESCAPE:
            Escape();
            return 0;

        case 'C':
            //
            // Ctrl+C copies the selected result rather than the query, which
            // is already on the clipboard or was just typed.
            //
            if (((GetKeyState(VK_CONTROL) & 0x8000) != 0) && g_HasMatch)
            {
                g_Copied = CopyTextToClipboard(g_Popup, Summary());
                InvalidateRect(g_Popup, nullptr, FALSE);
                return 0;
            }
            break;
        }
        break;

    case WM_CHAR:
        //
        // The characters Enter, Esc and Ctrl+C make, which a single-line edit
        // would otherwise beep at or act on a second time.
        //
        if ((WParam == L'\r') || (WParam == 0x1B) || ((WParam == 0x03) && g_HasMatch))
        {
            return 0;
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(Window, PopupEditProc, Id);
        break;
    }

    return DefSubclassProc(Window, Message, WParam, LParam);
}


/*!

    @brief The popup's window procedure.

*/
static
LRESULT
CALLBACK
PopupProc (
    _In_ HWND Window,
    _In_ UINT Message,
    _In_ WPARAM WParam,
    _In_ LPARAM LParam
    )
{
    TRACKMOUSEEVENT track = {};
    POINT point;
    INT_PTR brush;
    int hit;

    switch (Message)
    {
    case WM_PAINT:
        Paint();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_COMMAND:
        if ((LOWORD(WParam) == k_EditId) && (HIWORD(WParam) == EN_CHANGE) && !g_SettingQuery)
        {
            RunQuery();
        }
        return 0;

    case WM_CTLCOLOREDIT:
        brush = ThemeOnCtlColor(WParam, true);
        if (brush != 0)
        {
            return brush;
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(WParam) == WA_INACTIVE)
        {
            PopupHide();
            return 0;
        }

        SetFocus(g_Edit);
        return 0;

    case WM_MOUSEMOVE:
        point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
        hit = LinkAt(point);

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
    case WM_LBUTTONDBLCLK:
        point = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
        OnClick(point, Message == WM_LBUTTONDBLCLK);
        return 0;

    case WM_MOUSEWHEEL:
        g_ScrollY -= (GET_WHEEL_DELTA_WPARAM(WParam) * 3 * Stride()) / WHEEL_DELTA;
        g_ScrollY = std::clamp(g_ScrollY, 0, std::max(ContentHeight() - ViewportHeight(), 0));
        InvalidateRect(Window, nullptr, FALSE);
        return 0;

    case WM_DPICHANGED:
        //
        // The popup places itself on each show, so the suggested rectangle
        // isn't wanted; only the fonts need making again.
        //
        if (HIWORD(WParam) != g_Dpi)
        {
            SetDpi(HIWORD(WParam));
            Relayout();
        }
        return 0;

    case WM_SETTINGCHANGE:
        if ((LParam != 0) && (wcscmp(RCAST(PCWSTR)(LParam), L"ImmersiveColorSet") == 0))
        {
            ThemeRefresh();
            ThemeApplyToWindow(Window);
            RedrawWindow(Window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        break;

    case WM_CLOSE:
        PopupHide();
        return 0;
    }

    return DefWindowProcW(Window, Message, WParam, LParam);
}


bool
PopupRegister (
    _In_ HINSTANCE Instance
    )
{
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_DROPSHADOW | CS_DBLCLKS;
    windowClass.lpfnWndProc = PopupProc;
    windowClass.hInstance = Instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = k_PopupClass;

    return RegisterClassExW(&windowClass) != 0;
}


bool
PopupCreate (
    _In_ HINSTANCE Instance
    )
{
    DWM_WINDOW_CORNER_PREFERENCE corner;

    //
    // A tool window, so it has no taskbar button and isn't in Alt+Tab.
    //
    g_Popup = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                              k_PopupClass,
                              L"WinCode",
                              WS_POPUP | WS_CLIPCHILDREN,
                              0, 0, 0, 0,
                              nullptr,
                              nullptr,
                              Instance,
                              nullptr);
    if (g_Popup == nullptr)
    {
        return false;
    }

    g_Edit = CreateWindowExW(0,
                             WC_EDITW,
                             L"",
                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                             0, 0, 0, 0,
                             g_Popup,
                             RCAST(HMENU)(SCAST(UINT_PTR)(k_EditId)),
                             Instance,
                             nullptr);
    if (g_Edit == nullptr)
    {
        DestroyWindow(g_Popup);
        g_Popup = nullptr;
        return false;
    }

    SendMessageW(g_Edit, EM_SETCUEBANNER, TRUE, RCAST(LPARAM)(L"Look up a code or a name, or paste log text"));
    SetWindowSubclass(g_Edit, PopupEditProc, 0, 0);

    //
    // Windows 11 rounds it like its own flyouts. Windows 10 doesn't know the
    // attribute, and leaves it square.
    //
    corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(g_Popup, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    ThemeApplyToWindow(g_Popup);
    SetDpi(GetDpiForWindow(g_Popup));
    LoadRecent();

    return true;
}


bool
PopupHasRecent (
    void
    )
{
    return !g_Recent.empty();
}


void
PopupClearRecent (
    void
    )
{
    g_Recent.clear();
    g_RecentSelected = -1;
    SaveRecent();

    if (IsWindowVisible(g_Popup))
    {
        Relayout();
    }
}


void
PopupShow (
    _In_opt_z_ PCWSTR Query
    )
{
    MONITORINFO monitor = {};
    std::wstring text;
    HMONITOR handle;
    UINT dpiX;
    UINT dpiY;

    //
    // The monitor with the focused window, since that's where the debugger or
    // the log usually is.
    //
    handle = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(handle, &monitor);
    g_Work = monitor.rcWork;

    if (FAILED(GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
    {
        dpiX = USER_DEFAULT_SCREEN_DPI;
    }

    SetDpi(dpiX);

    g_Left = g_Work.left + ((g_Work.right - g_Work.left - Scale(k_Width)) / 2);
    g_Top = g_Work.top + MulDiv(g_Work.bottom - g_Work.top, k_TopPercent, 100);

    //
    // The query is set, and the popup laid out and sized, while it's still
    // hidden, so an old query never flashes up.
    //
    text = (Query != nullptr) ? std::wstring(Query) : ClipboardQuery();
    SetQuery(text);

    SetWindowPos(g_Popup, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(g_Popup);
    SetFocus(g_Edit);

    //
    // Selected, so typing replaces it.
    //
    SendMessageW(g_Edit, EM_SETSEL, 0, -1);
}


void
PopupToggle (
    void
    )
{
    if (IsWindowVisible(g_Popup) && (GetForegroundWindow() == g_Popup))
    {
        PopupHide();
        return;
    }

    PopupShow(nullptr);
}
