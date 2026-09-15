/*!

    @file Cli/WinCodeCli.cpp

    @brief The command-line front end, wincode.exe.

    @details Every argument is joined into one query, so a value can be given
             bare, or pasted with the log text around it, and names can be
             given whole or in part.

             Output goes to the console as UTF-16, and as UTF-8 when it is
             redirected, so message text in any language survives either way.
             On a console, long lines wrap at the window's width and keep their
             indent, so text stays in its column. Redirected output isn't
             wrapped, since whatever reads it knows its own width.

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

#include "WinCode.hpp"

#include <algorithm>
#include <format>


//
// Where names and text line up: a two space indent, the longest kind name
// ("Window message"), and two spaces after it.
//
static constexpr size_t k_KindWidth = 18;

//
// Width of the labels (also, fields, maps to...) in front of a match's details.
// "as HRESULT" is the longest.
//
static constexpr size_t k_LabelWidth = 12;

//
// With fewer columns than this left for text, wrapping does more harm than good.
//
static constexpr size_t k_MinWrapWidth = 30;

//
// A name search for something short can match hundreds of names. Past this
// many, the rest are counted rather than listed.
//
static constexpr size_t k_MaxNameResults = 20;
static constexpr size_t k_MaxNameWidth = 48;

//
// Whether anything has been written yet, so blocks get a blank line between
// them, and whether anything matched, for the exit code.
//
static bool g_BlockWritten;
static bool g_AnyMatch;

//
// The console window's width in columns, or zero when output is redirected,
// which turns wrapping off.
//
static size_t g_ConsoleWidth;


/*!

    @brief Writes text to a standard handle.

    @param[in] Handle - STD_OUTPUT_HANDLE or STD_ERROR_HANDLE.

    @param[in] Text - The text.

*/
static
void
Write (
    _In_ DWORD Handle,
    _In_ const std::wstring& Text
    )
{
    std::string utf8;
    HANDLE handle;
    DWORD mode;
    DWORD written;
    int length;

    handle = GetStdHandle(Handle);

    if (GetConsoleMode(handle, &mode))
    {
        WriteConsoleW(handle, Text.c_str(), SCAST(DWORD)(Text.size()), &written, nullptr);
        return;
    }

    length = WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), SCAST(int)(Text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0)
    {
        return;
    }

    utf8.resize(SCAST(size_t)(length));
    WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), SCAST(int)(Text.size()), utf8.data(), length, nullptr, nullptr);
    WriteFile(handle, utf8.data(), SCAST(DWORD)(utf8.size()), &written, nullptr);
}


/*!

    @brief Writes to standard output.

    @param[in] Text - The text.

*/
static
void
Out (
    _In_ const std::wstring& Text
    )
{
    Write(STD_OUTPUT_HANDLE, Text);
}


/*!

    @brief Starts a block of output, with a blank line before it unless it is
           the first.

*/
static
void
StartBlock (
    void
    )
{
    if (g_BlockWritten)
    {
        Out(L"\n");
    }

    g_BlockWritten = true;
}


/*!

    @brief The width of the console window standard output is going to.

    @return The width in columns, or zero if standard output isn't a console.

*/
static
size_t
ConsoleWidth (
    void
    )
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
    {
        return 0;
    }

    return SCAST(size_t)(info.srWindow.Right - info.srWindow.Left + 1);
}


/*!

    @brief Lays text out after a prefix, wrapping it to the console.

    @details The first line follows the prefix. Every other line, whether it is
             a line of its own in the text or wrapped from a long one, is
             indented to line up under it. Words are never split: one too long
             for the space left gets a line to itself and runs over. Runs of
             spaces collapse to one, so nothing should rely on them for layout.

    @param[in] Prefix - What goes before the first line. Its length is the
                        indent for the rest.

    @param[in] Text - The text, with lines separated by \n.

    @return The lines, each ending in a newline.

*/
static
std::wstring
FormatLines (
    _In_ const std::wstring& Prefix,
    _In_ const std::wstring& Text
    )
{
    std::wstring result;
    std::wstring padding;
    std::wstring line;
    std::wstring word;
    size_t available;
    size_t start;
    size_t end;
    bool first;

    padding.assign(Prefix.size(), L' ');

    available = SIZE_MAX;
    if (g_ConsoleWidth > (Prefix.size() + k_MinWrapWidth))
    {
        //
        // One column short of the edge: a line that fills the console exactly
        // moves the cursor down by itself, and the newline after it would then
        // leave a blank line.
        //
        available = g_ConsoleWidth - Prefix.size() - 1;
    }

    first = true;
    start = 0;

    for (;;)
    {
        end = Text.find(L'\n', start);
        if (end == std::wstring::npos)
        {
            end = Text.size();
        }

        line.clear();

        for (size_t i = start; i <= end; i++)
        {
            if ((i < end) && (Text[i] != L' '))
            {
                word += Text[i];
                continue;
            }

            if (word.empty())
            {
                continue;
            }

            if (!line.empty() && ((line.size() + 1 + word.size()) > available))
            {
                result += (first ? Prefix : padding) + line + L"\n";
                first = false;
                line.clear();
            }

            if (!line.empty())
            {
                line += L' ';
            }

            line += word;
            word.clear();
        }

        result += (first ? Prefix : padding) + line + L"\n";
        first = false;

        if (end >= Text.size())
        {
            break;
        }

        start = end + 1;
    }

    return result;
}


/*!

    @brief The kind column: indent, kind name, and padding up to the names.

    @param[in] Kind - The kind.

    @return The column text.

*/
static
std::wstring
KindColumn (
    _In_ CodeKind Kind
    )
{
    std::wstring kind;

    kind = CodeKindName(Kind);
    kind.resize(k_KindWidth - 2, L' ');
    return L"  " + kind;
}


/*!

    @brief A detail label, padded to line its text up with the others.

    @param[in] Name - The label.

    @return The padded label.

*/
static
std::wstring
Label (
    _In_z_ PCWSTR Name
    )
{
    std::wstring label;

    label = Name;
    label.resize(k_LabelWidth, L' ');
    return label;
}


/*!

    @brief Describes a number: its value in hex and decimal, and how it was
           read when that isn't obvious.

    @param[in] Number - The number.

    @return The description, ending in a newline.

*/
static
std::wstring
DescribeNumber (
    _In_ const ParsedNumber& Number
    )
{
    std::wstring text;

    text = std::format(L"0x{:08X}  {}", Number.Value, Number.Value);

    if ((Number.Value & 0x80000000) != 0)
    {
        text += std::format(L"  (signed {})", SCAST(int32_t)(Number.Value));
    }

    if (Number.Form == NumberForm::HexGuess)
    {
        text += std::format(L"  [{} read as hex]", Number.Token);
    }
    else if (Number.Form == NumberForm::SignExtended)
    {
        text += std::format(L"  [{} is sign-extended]", Number.Token);
    }

    return text + L"\n";
}


/*!

    @brief Describes one match: its kind and name, then its details, each
           labelled.

    @param[in] Match - The match.

    @return The description, ending in a newline.

*/
static
std::wstring
DescribeMatch (
    _In_ const CodeMatch& Match
    )
{
    std::wstring text;
    std::wstring heading;
    std::wstring aliases;
    std::wstring mappedFrom;
    std::wstring indent;

    indent.assign(k_KindWidth, L' ');

    if (!Match.Names.empty())
    {
        heading = Match.Names[0];
    }
    else if (!Match.Description.empty())
    {
        heading = Match.Description;
    }
    else
    {
        heading = L"(no name)";
    }

    if (Match.Undocumented)
    {
        heading += L" [undocumented]";
    }

    text = FormatLines(KindColumn(Match.Kind), heading);

    for (size_t i = 1; i < Match.Names.size(); i++)
    {
        if (i > 1)
        {
            aliases += L", ";
        }
        aliases += Match.Names[i];
    }

    if (!aliases.empty())
    {
        text += FormatLines(indent + Label(L"also"), aliases);
    }

    if (!Match.Text.empty())
    {
        text += FormatLines(indent, Match.Text);
    }

    //
    // Worth saying, because captured text is English whatever language this
    // Windows runs in, and describes the code as it was on the machine that
    // generated the tables.
    //
    if (Match.Source == TextSource::Captured)
    {
        text += FormatLines(indent + Label(L"source"),
                            L"captured when the tables were generated; this Windows has no text for it");
    }

    if (Match.Fields.has_value())
    {
        text += FormatLines(indent + Label(L"fields"), DescribeFields(*Match.Fields));
    }

    for (const RelatedCode& related : Match.Related)
    {
        text += FormatLines(indent + Label(RelationLabel(related.Relation)), DescribeRelated(related));
    }

    //
    // All of them, by name only: there can be dozens, and a name is enough to
    // look any one of them up.
    //
    if (!Match.MappedFrom.empty())
    {
        mappedFrom = std::format(L"{} NTSTATUS {}:",
                                 Match.MappedFrom.size(),
                                 (Match.MappedFrom.size() == 1) ? L"code" : L"codes");

        for (size_t i = 0; i < Match.MappedFrom.size(); i++)
        {
            mappedFrom += (i == 0) ? L" " : L", ";
            mappedFrom += Match.MappedFrom[i].Name;
        }

        text += FormatLines(indent + Label(RelationLabel(RelationKind::MappedFrom)), mappedFrom);
    }

    return text;
}


/*!

    @brief Prints every match for the numbers in a query.

    @details A token can have more than one reading. The ones that mean
             something are shown, and nothing is admitted to only when none of
             them do.

    @param[in] Numbers - The numbers, as ParseQuery found them.

*/
static
void
PrintNumbers (
    _In_ const std::vector<ParsedNumber>& Numbers
    )
{
    std::vector<CodeMatch> matches;
    size_t first;
    size_t last;
    bool anyForToken;

    for (first = 0; first < Numbers.size(); first = last)
    {
        for (last = first + 1; (last < Numbers.size()) && (Numbers[last].Token == Numbers[first].Token); last++)
        {
        }

        anyForToken = false;
        for (size_t i = first; i < last; i++)
        {
            matches = LookupValue(Numbers[i].Value);
            if (matches.empty())
            {
                continue;
            }

            StartBlock();
            Out(DescribeNumber(Numbers[i]));
            for (const CodeMatch& match : matches)
            {
                Out(DescribeMatch(match));
            }

            anyForToken = true;
            g_AnyMatch = true;
        }

        if (!anyForToken)
        {
            StartBlock();
            Out(std::format(L"{}  no match\n", Numbers[first].Token));
        }
    }
}


/*!

    @brief Prints the results of searching for a name.

    @details An exact hit gets the full story, as though its value had been
             asked for, but only for the kind of code the name belongs to: asking
             for ERROR_ACCESS_DENIED is asking about a Win32 error, not about
             every other code that happens to be 5. Anything else that matched
             is listed briefly.

    @param[in] Name - The name, or part of one, to search for.

*/
static
void
PrintNameSearch (
    _In_ const std::wstring& Name
    )
{
    std::vector<NameMatch> results;
    std::vector<const NameMatch*> others;
    std::vector<CodeMatch> matches;
    std::wstring line;
    size_t shown;
    size_t width;
    bool anyExact;

    anyExact = false;

    results = SearchNames(Name.c_str());
    if (results.empty())
    {
        StartBlock();
        Out(std::format(L"{}  no match\n", Name));
        return;
    }

    g_AnyMatch = true;

    for (const NameMatch& result : results)
    {
        if (result.Quality != NameQuality::Exact)
        {
            others.push_back(&result);
            continue;
        }

        anyExact = true;

        StartBlock();
        Out(std::format(L"{} = {}\n", result.Name, FormatCodeValue(result.Kind, result.Value)));

        matches = LookupValue(result.Value);
        for (const CodeMatch& match : matches)
        {
            if (match.Kind == result.Kind)
            {
                Out(DescribeMatch(match));
            }
        }
    }

    if (others.empty())
    {
        return;
    }

    shown = std::min(others.size(), k_MaxNameResults);

    width = 0;
    for (size_t i = 0; i < shown; i++)
    {
        width = std::max(width, others[i]->Name.size());
    }
    width = std::min(width, k_MaxNameWidth);

    StartBlock();
    Out(std::format(L"{} {}{} containing \"{}\":\n",
                    others.size(),
                    anyExact ? L"other " : L"",
                    (others.size() == 1) ? L"name" : L"names",
                    Name));

    for (size_t i = 0; i < shown; i++)
    {
        line = others[i]->Name;
        if (line.size() < width)
        {
            line.resize(width, L' ');
        }

        line = KindColumn(others[i]->Kind) + line + L"  " + FormatCodeValue(others[i]->Kind, others[i]->Value);

        if (others[i]->Undocumented)
        {
            line += L" [undocumented]";
        }

        Out(line + L"\n");
    }

    if (others.size() > shown)
    {
        Out(std::format(L"  ... and {} more\n", others.size() - shown));
    }
}


/*!

    @brief Prints how to use the tool.

    @param[in] Requested - True if the user asked for it, which sends it to
                           standard output and isn't an error.

    @return The exit code: 0 if it was asked for, 2 for a usage error.

*/
static
int
PrintUsage (
    _In_ bool Requested
    )
{
    Write(Requested ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE,
          L"usage: wincode <values, names, or text containing them>\n"
          L"       wincode --version\n"
          L"       wincode --help\n"
          L"\n"
          L"Numbers can be decimal, hex with or without 0x, negative (a signed\n"
          L"32-bit value) or sign-extended, and can be pasted with the text around\n"
          L"them. Names are searched for in every table, whole or in part, ignoring\n"
          L"case.\n"
          L"\n"
          L"examples: wincode 0xC0000005\n"
          L"          wincode -2147024891\n"
          L"          wincode \"CreateFile failed (HRESULT: 0x80070005)\"\n"
          L"          wincode ERROR_ACCESS_DENIED\n"
          L"          wincode ACCESS_DENIED\n"
          L"\n"
          L"Exit code: 0 if anything matched, 1 if nothing did, 2 for a usage error.\n");

    return Requested ? 0 : 2;
}


/*!

    @brief Entry point.

    @param[in] ArgCount - Number of arguments, including the program name.

    @param[in] Args - The arguments.

    @return 0 if anything matched, 1 if nothing did, 2 for a usage error.

*/
int
wmain (
    _In_ int ArgCount,
    _In_reads_(ArgCount) PWSTR* Args
    )
{
    ParsedQuery parsed;
    std::wstring query;

    g_ConsoleWidth = ConsoleWidth();

    if (ArgCount < 2)
    {
        return PrintUsage(false);
    }

    if (ArgCount == 2)
    {
        if (_wcsicmp(Args[1], L"--version") == 0)
        {
            Out(std::format(L"wincode {}\n", WinCodeVersion()));
            return 0;
        }

        if ((_wcsicmp(Args[1], L"--help") == 0) || (_wcsicmp(Args[1], L"-h") == 0) || (wcscmp(Args[1], L"/?") == 0))
        {
            return PrintUsage(true);
        }
    }

    for (int i = 1; i < ArgCount; i++)
    {
        if (i > 1)
        {
            query += L' ';
        }
        query += Args[i];
    }

    parsed = ParseQuery(query.c_str());
    if (parsed.Numbers.empty() && parsed.Names.empty())
    {
        Write(STD_ERROR_HANDLE, std::format(L"No numbers or names found in \"{}\".\n", query));
        return 1;
    }

    PrintNumbers(parsed.Numbers);

    for (const std::wstring& name : parsed.Names)
    {
        PrintNameSearch(name);
    }

    return g_AnyMatch ? 0 : 1;
}
