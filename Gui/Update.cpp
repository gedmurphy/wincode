/*!

    @file Gui/Update.cpp

    @brief Checking GitHub, once a day, for a newer release.

    @details The only network access we make, and you can turn it off in
             settings. A thread asks GitHub's API for the latest release, so a
             slow or missing network never holds up the window, and posts what
             it found back. A newer version gets offered once, in a tray
             notification that opens its download page.

             Nothing's sent but the request itself, with our version in the
             user agent as GitHub's API asks.

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

#include <shellapi.h>
#include <winhttp.h>

#include <ctime>
#include <format>
#include <memory>


static constexpr WCHAR k_Host[] = L"api.github.com";
static constexpr WCHAR k_LatestRelease[] = L"/repos/gedmurphy/wincode/releases/latest";
static constexpr WCHAR k_ReleasesPage[] = L"https://github.com/gedmurphy/wincode/releases/latest";

//
// Only a page on GitHub is ever opened, whatever the answer says.
//
static constexpr WCHAR k_GitHub[] = L"https://github.com/";

static constexpr WCHAR k_CheckSetting[] = L"updates.check";
static constexpr WCHAR k_LastCheckSetting[] = L"updates.lastCheck";
static constexpr WCHAR k_NotifiedSetting[] = L"updates.notified";

static constexpr double k_IntervalSeconds = 24.0 * 60.0 * 60.0;
static constexpr DWORD k_TimeoutMs = 15000;
static constexpr size_t k_MaxResponse = 1024 * 1024;


/*!

    @brief What the check found, the latest release's tag and page.

*/
struct UpdateResult
{
    std::wstring Tag;
    std::wstring Page;
};


static bool g_Checking;

//
// The page a notification about a new version opens, until it's clicked.
//
static std::wstring g_PendingPage;


/*!

    @brief Asks GitHub for the latest release.

    @return The answer's body, or an empty string if there's no answer, as when
            offline or when there's no release yet.

*/
static
std::string
FetchLatestRelease (
    void
    )
{
    std::wstring agent;
    std::string body;
    HINTERNET session;
    HINTERNET connection;
    HINTERNET request;
    DWORD status;
    DWORD size;
    DWORD available;
    DWORD read;
    size_t used;

    agent = std::format(L"WinCode/{}", WinCodeVersion());

    session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr)
    {
        return body;
    }

    WinHttpSetTimeouts(session, SCAST(int)(k_TimeoutMs), SCAST(int)(k_TimeoutMs), SCAST(int)(k_TimeoutMs), SCAST(int)(k_TimeoutMs));

    connection = WinHttpConnect(session, k_Host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    request = (connection != nullptr)
                  ? WinHttpOpenRequest(connection, L"GET", k_LatestRelease, nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                  : nullptr;

    status = 0;
    size = sizeof(status);

    if ((request != nullptr) &&
        WinHttpAddRequestHeaders(request, L"Accept: application/vnd.github+json", SCAST(DWORD)(-1), WINHTTP_ADDREQ_FLAG_ADD) &&
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr) &&
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &size, WINHTTP_NO_HEADER_INDEX) &&
        (status == 200))
    {
        for (;;)
        {
            available = 0;
            if (!WinHttpQueryDataAvailable(request, &available) || (available == 0) ||
                ((body.size() + available) > k_MaxResponse))
            {
                break;
            }

            used = body.size();
            body.resize(used + available);

            read = 0;
            if (!WinHttpReadData(request, body.data() + used, available, &read))
            {
                body.clear();
                break;
            }

            body.resize(used + read);
        }
    }

    if (request != nullptr)
    {
        WinHttpCloseHandle(request);
    }

    if (connection != nullptr)
    {
        WinHttpCloseHandle(connection);
    }

    WinHttpCloseHandle(session);
    return body;
}


/*!

    @brief The check, on its own thread. Fetches the latest release and posts
           what it found to the main window.

    @param[in] Owner - The main window.

    @return Zero.

*/
static
DWORD
WINAPI
UpdateThread (
    _In_ LPVOID Owner
    )
{
    std::unique_ptr<UpdateResult> result;
    std::wstring text;
    std::string body;
    JsonValue answer;
    const JsonValue* tag;
    const JsonValue* page;
    int length;

    body = FetchLatestRelease();

    if (!body.empty())
    {
        length = MultiByteToWideChar(CP_UTF8, 0, body.data(), SCAST(int)(body.size()), nullptr, 0);
        if (length > 0)
        {
            text.resize(SCAST(size_t)(length));
            MultiByteToWideChar(CP_UTF8, 0, body.data(), SCAST(int)(body.size()), text.data(), length);
        }

        if (JsonValue::Parse(text, answer))
        {
            tag = answer.Find(L"tag_name");
            page = answer.Find(L"html_url");

            if ((tag != nullptr) && !tag->AsString(L"").empty())
            {
                result = std::make_unique<UpdateResult>();
                result->Tag = tag->AsString(L"");
                result->Page = (page != nullptr) ? page->AsString(L"") : std::wstring();
            }
        }
    }

    //
    // Posted even with nothing found, so the window knows the check is over.
    // If the window has gone, the result goes with it.
    //
    if (PostMessageW(SCAST(HWND)(Owner), k_UpdateMessage, 0, RCAST(LPARAM)(result.get())))
    {
        SCAST(void)(result.release());
    }

    return 0;
}


/*!

    @brief Starts a check for a newer release, if checks are on and a day has
           passed since the last.

    @param[in] Owner - The window the result is posted to.

*/
void
UpdateCheckIfDue (
    _In_ HWND Owner
    )
{
    const JsonValue* last;
    HANDLE thread;
    double now;

    if (g_Checking || !SettingsGetBool(k_CheckSetting, true))
    {
        return;
    }

    now = SCAST(double)(time(nullptr));
    last = SettingsRoot().Resolve(k_LastCheckSetting);

    if ((last != nullptr) && ((now - last->AsNumber(0.0)) < k_IntervalSeconds))
    {
        return;
    }

    //
    // Recorded as the check starts, so a failing one waits a day like any
    // other rather than trying again every hour.
    //
    SettingsRoot().ResolveOrCreate(k_LastCheckSetting).SetNumber(now);
    SettingsSave();

    g_Checking = true;
    thread = CreateThread(nullptr, 0, UpdateThread, Owner, 0, nullptr);
    if (thread == nullptr)
    {
        g_Checking = false;
        return;
    }

    CloseHandle(thread);
}


/*!

    @brief Handles k_UpdateMessage, offering a newer version once from the
           tray icon.

    @param[in] Result - The message's LPARAM, which this takes ownership of.

*/
void
UpdateOnResult (
    _In_ LPARAM Result
    )
{
    std::unique_ptr<UpdateResult> result(RCAST(UpdateResult*)(Result));
    std::wstring version;

    g_Checking = false;

    if (result == nullptr)
    {
        return;
    }

    version = result->Tag;
    if (!version.empty() && ((version[0] == L'v') || (version[0] == L'V')))
    {
        version.erase(0, 1);
    }

    //
    // Offered once a version, not every day until it's installed.
    //
    if ((CompareVersions(version.c_str(), WinCodeVersion()) <= 0) ||
        (SettingsGetString(k_NotifiedSetting, L"") == version))
    {
        return;
    }

    SettingsSetString(k_NotifiedSetting, version.c_str());
    SettingsSave();

    g_PendingPage = (result->Page.rfind(k_GitHub, 0) == 0) ? result->Page : std::wstring(k_ReleasesPage);

    TrayNotify(std::format(L"WinCode {} is available", version).c_str(), L"Click here to open its download page.");
}


/*!

    @brief Opens the download page a notification offered, if there is one.
           Call when a tray notification is clicked.

    @return True if it opened one, false if the notification was about
            something else.

*/
bool
UpdateOpenPending (
    void
    )
{
    if (g_PendingPage.empty())
    {
        return false;
    }

    ShellExecuteW(nullptr, L"open", g_PendingPage.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    g_PendingPage.clear();
    return true;
}


/*!

    @brief Whether the daily check for updates is on.

*/
bool
UpdateCheckEnabled (
    void
    )
{
    return SettingsGetBool(k_CheckSetting, true);
}


/*!

    @brief Turns the daily check for updates on or off, and saves it.

    @param[in] Enabled - True to check.

*/
void
UpdateSetCheckEnabled (
    _In_ bool Enabled
    )
{
    SettingsSetBool(k_CheckSetting, Enabled);
    SettingsSave();
}
