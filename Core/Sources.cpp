/*!

    @file Core/Sources.cpp

    @brief Which sources of codes get shown, always, only when nothing else
           matches, or never.

    @details Each table's a source, with a default in its KindTable. One of
             your headers is one source, though it can make up to five tables,
             and the SDK's and WDK's IOCTLs share one too.
             Your choices go in settings.json under "sources", by key, and only
             where they differ from the default.

             By key rather than by table, since your headers come and go. A
             choice for one that's briefly missing mid rebuild is still there
             when it comes back.

             Lookups and searches read from here so every front end agrees.
             Until a front end calls LoadSourceShows everything has its
             default, which keeps the tests independent of your settings.

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

#include "Tables.hpp"
#include "Settings.hpp"

#include <cwctype>
#include <map>


static constexpr WCHAR k_SourcesSetting[] = L"sources";

//
// The sources set to something, by key in lower case.
//
static std::map<std::wstring, SourceShow> g_Shows;
static bool g_ShowEvery;


/*!

    @brief A key in lower case, as g_Shows keeps them.

*/
static
std::wstring
LowerKey (
    _In_z_ PCWSTR Key
    )
{
    std::wstring lower;

    lower = Key;
    for (WCHAR& c : lower)
    {
        c = SCAST(WCHAR)(towlower(c));
    }

    return lower;
}


/*!

    @brief How a source is shown until the user chooses otherwise.

    @param[in] Key - The source's key.

    @return Its default, or Always for a key that isn't a source.

*/
static
SourceShow
DefaultShow (
    _In_z_ PCWSTR Key
    )
{
    for (const KindTable& table : AllKindTables())
    {
        if (_wcsicmp(table.Key, Key) == 0)
        {
            return table.Default;
        }
    }

    return SourceShow::Always;
}


/*!

    @brief How a setting is written in settings.json.

    @param[in] Show - The setting.

    @return A static, never null, name.

*/
static
PCWSTR
ShowName (
    _In_ SourceShow Show
    )
{
    switch (Show)
    {
    case SourceShow::Quiet:
        return L"quiet";

    case SourceShow::Never:
        return L"never";

    default:
        return L"always";
    }
}


/*!

    @brief How a source is shown, all things considered. Its setting, or
           Always when every source is being shown.

    @param[in] Key - The source's key.

    @return The setting, or Always for a key that isn't a source.

*/
SourceShow
SourceShowFor (
    _In_z_ PCWSTR Key
    )
{
    return g_ShowEvery ? SourceShow::Always : GetSourceShow(Key);
}


/*!

    @brief Every source of codes, in the order the settings list them.

    @return The sources.

*/
std::vector<CodeSource>
CodeSources (
    void
    )
{
    std::vector<CodeSource> sources;
    bool listed;

    for (const KindTable& table : AllKindTables())
    {
        listed = false;
        for (const CodeSource& source : sources)
        {
            listed = listed || (_wcsicmp(source.Key.c_str(), table.Key) == 0);
        }

        if (!listed)
        {
            sources.push_back({ table.Key, table.Name, table.Kind, table.Default, table.User });
        }
    }

    return sources;
}


/*!

    @brief How a source is set to be shown.

    @param[in] Key - The source's key.

    @return The setting, or Always for a key that isn't a source.

*/
SourceShow
GetSourceShow (
    _In_z_ PCWSTR Key
    )
{
    std::map<std::wstring, SourceShow>::const_iterator found;

    found = g_Shows.find(LowerKey(Key));
    return (found != g_Shows.end()) ? found->second : DefaultShow(Key);
}


/*!

    @brief Sets how a source is shown, for lookups and searches from now on.
           SaveSourceShows keeps it.

    @param[in] Key - The source's key.

    @param[in] Show - The setting.

*/
void
SetSourceShow (
    _In_z_ PCWSTR Key,
    _In_ SourceShow Show
    )
{
    g_Shows[LowerKey(Key)] = Show;
}


/*!

    @brief Puts every source back to its default.

*/
void
ResetSourceShows (
    void
    )
{
    g_Shows.clear();
}


/*!

    @brief Shows every source whatever it's set to, as wincode --all does, or
           stops doing so.

    @param[in] All - True to show every source.

*/
void
ShowEverySource (
    _In_ bool All
    )
{
    g_ShowEvery = All;
}


/*!

    @brief Reads how each source is shown from settings.json. Front ends call
           it once, as they start; until then every source has its default.

*/
void
LoadSourceShows (
    void
    )
{
    const JsonValue* sources;
    std::wstring name;

    ResetSourceShows();

    sources = SettingsRoot().Find(k_SourcesSetting);
    if (sources == nullptr)
    {
        return;
    }

    for (const std::pair<std::wstring, JsonValue>& member : sources->Members())
    {
        //
        // A setting a hand edit has garbled leaves the source at its default.
        //
        name = member.second.AsString(L"");
        if (name == L"always")
        {
            g_Shows[LowerKey(member.first.c_str())] = SourceShow::Always;
        }
        else if (name == L"quiet")
        {
            g_Shows[LowerKey(member.first.c_str())] = SourceShow::Quiet;
        }
        else if (name == L"never")
        {
            g_Shows[LowerKey(member.first.c_str())] = SourceShow::Never;
        }
    }
}


/*!

    @brief Saves how each source is shown to settings.json. Sources left at
           their default aren't written, so a changed default reaches them.

*/
void
SaveSourceShows (
    void
    )
{
    JsonValue* sources;

    sources = &SettingsRoot().Member(k_SourcesSetting);

    for (const std::pair<const std::wstring, SourceShow>& show : g_Shows)
    {
        if (show.second == DefaultShow(show.first.c_str()))
        {
            sources->RemoveMember(show.first.c_str());
        }
        else
        {
            sources->Member(show.first.c_str()).SetString(ShowName(show.second));
        }
    }

    SettingsSave();
}
