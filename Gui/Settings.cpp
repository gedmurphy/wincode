/*!

    @file Gui/Settings.cpp

    @brief The settings file.

    @details Copied from FileMon++, as the plan decided. Settings live in a
             JSON file rather than the registry: a copy that carries its own
             settings.json beside the executable is portable, arriving
             configured on a test machine or in a VM, and the file can be read
             and edited by hand.

             The file is looked for beside the executable first, and in the
             user's application data otherwise.

             N.B. starting at sign-in is deliberately not here. Windows keeps
             that in the Run key, and Task Manager can switch it off there too,
             so the Run key is the only record that can't go stale
             (Startup.cpp).

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


static constexpr WCHAR k_FileName[] = L"settings.json";
static constexpr WCHAR k_FolderName[] = L"WinCode";

//
// The largest settings file read. Anything bigger isn't one WinCode wrote.
//
static constexpr LONGLONG k_MaxFileSize = 4 * 1024 * 1024;

static JsonValue g_Settings;
static std::wstring g_Path;
static bool g_Loaded;

//
// Set when the file is there but won't parse, which stops every save until
// it's fixed.
//
static bool g_Unreadable;


/*!

    @brief Works out where the settings file lives.

    @details Beside the executable if one is already there, which is what makes
             a copied build portable, otherwise under the user's application
             data. The application data directory is created if it is needed.

    @return The full path, or an empty string if neither location can be used.

*/
static
std::wstring
ResolvePath (
    void
    )
{
    WCHAR module[MAX_PATH];
    WCHAR appData[MAX_PATH];
    std::wstring candidate;
    std::wstring folder;
    PWSTR separator;

    //
    // Beside the executable first.
    //
    if (GetModuleFileNameW(nullptr, module, ARRAYSIZE(module)) != 0)
    {
        separator = wcsrchr(module, L'\\');
        if (separator != nullptr)
        {
            separator[1] = L'\0';

            candidate = module;
            candidate += k_FileName;

            if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                return candidate;
            }
        }
    }

    //
    // Otherwise the roaming profile.
    //
    if (GetEnvironmentVariableW(L"APPDATA", appData, ARRAYSIZE(appData)) == 0)
    {
        return std::wstring();
    }

    folder = appData;
    folder += L"\\";
    folder += k_FolderName;

    if (!CreateDirectoryW(folder.c_str(), nullptr) &&
        (GetLastError() != ERROR_ALREADY_EXISTS))
    {
        return std::wstring();
    }

    return folder + L"\\" + k_FileName;
}

/*!

    @brief Reads a UTF-8 file as text.

    @param[in] Path - The file to read.

    @param[out] Text - Receives the contents.

    @return True if the file was read.

*/
static
bool
ReadTextFile (
    _In_ const std::wstring& Path,
    _Out_ std::wstring& Text
    )
{
    std::vector<char> bytes;
    LARGE_INTEGER size;
    HANDLE file;
    DWORD read;
    size_t offset;
    int characters;

    Text.clear();

    file = CreateFileW(Path.c_str(),
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       nullptr,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    if (!GetFileSizeEx(file, &size) || (size.QuadPart <= 0) || (size.QuadPart > k_MaxFileSize))
    {
        CloseHandle(file);
        return false;
    }

    bytes.resize(SCAST(size_t)(size.QuadPart));

    if (!ReadFile(file, bytes.data(), SCAST(DWORD)(bytes.size()), &read, nullptr) ||
        (read != bytes.size()))
    {
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);

    //
    // Skip a byte order mark if some editor has added one.
    //
    offset = 0;

    if ((bytes.size() >= 3) &&
        (SCAST(unsigned char)(bytes[0]) == 0xEF) &&
        (SCAST(unsigned char)(bytes[1]) == 0xBB) &&
        (SCAST(unsigned char)(bytes[2]) == 0xBF))
    {
        offset = 3;
    }

    characters = MultiByteToWideChar(CP_UTF8,
                                     0,
                                     bytes.data() + offset,
                                     SCAST(int)(bytes.size() - offset),
                                     nullptr,
                                     0);
    if (characters <= 0)
    {
        return false;
    }

    Text.resize(SCAST(size_t)(characters));

    MultiByteToWideChar(CP_UTF8,
                        0,
                        bytes.data() + offset,
                        SCAST(int)(bytes.size() - offset),
                        Text.data(),
                        characters);

    return true;
}

/*!

    @brief Writes text to a file as UTF-8, atomically.

    @details Written to a temporary beside the target and moved over it, so that
             losing power part way through a save costs the new settings rather
             than the old ones as well.

    @param[in] Path - The file to write.

    @param[in] Text - The contents.

    @return True if the file was written.

*/
static
bool
WriteTextFile (
    _In_ const std::wstring& Path,
    _In_ const std::wstring& Text
    )
{
    std::wstring temporary;
    std::vector<char> bytes;
    HANDLE file;
    DWORD written;
    int length;

    length = WideCharToMultiByte(CP_UTF8,
                                 0,
                                 Text.c_str(),
                                 SCAST(int)(Text.size()),
                                 nullptr,
                                 0,
                                 nullptr,
                                 nullptr);
    if (length <= 0)
    {
        return false;
    }

    bytes.resize(SCAST(size_t)(length));

    WideCharToMultiByte(CP_UTF8,
                        0,
                        Text.c_str(),
                        SCAST(int)(Text.size()),
                        bytes.data(),
                        length,
                        nullptr,
                        nullptr);

    temporary = Path + L".tmp";

    file = CreateFileW(temporary.c_str(),
                       GENERIC_WRITE,
                       0,
                       nullptr,
                       CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,
                       nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    if (!WriteFile(file, bytes.data(), SCAST(DWORD)(bytes.size()), &written, nullptr) ||
        (written != bytes.size()))
    {
        CloseHandle(file);
        DeleteFileW(temporary.c_str());
        return false;
    }

    FlushFileBuffers(file);
    CloseHandle(file);

    if (!MoveFileExW(temporary.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        DeleteFileW(temporary.c_str());
        return false;
    }

    return true;
}


void
SettingsLoad (
    void
    )
{
    std::wstring text;

    if (g_Loaded)
    {
        return;
    }

    g_Loaded = true;
    g_Settings.SetObject();

    g_Path = ResolvePath();
    if (g_Path.empty())
    {
        return;
    }

    if (!ReadTextFile(g_Path, text))
    {
        return;
    }

    //
    // A file that will not parse is left alone rather than overwritten, so that
    // a typo in a hand edit can be found and fixed rather than silently
    // discarded. Defaults are used until it is, and nothing is saved over it.
    //
    if (!JsonValue::Parse(text, g_Settings))
    {
        g_Settings.SetObject();
        g_Unreadable = true;
    }
}


bool
SettingsSave (
    void
    )
{
    SettingsLoad();

    if (g_Path.empty() || g_Unreadable)
    {
        return false;
    }

    return WriteTextFile(g_Path, g_Settings.Format());
}


std::wstring
SettingsPath (
    void
    )
{
    SettingsLoad();

    return g_Path;
}


bool
SettingsIsUnreadable (
    void
    )
{
    SettingsLoad();

    return g_Unreadable;
}


JsonValue&
SettingsRoot (
    void
    )
{
    SettingsLoad();

    return g_Settings;
}


int
SettingsGetInt (
    _In_ PCWSTR Path,
    _In_ int Default
    )
{
    const JsonValue* value;

    SettingsLoad();

    value = g_Settings.Resolve(Path);

    return (value != nullptr) ? value->AsInt(Default) : Default;
}


void
SettingsSetInt (
    _In_ PCWSTR Path,
    _In_ int Value
    )
{
    SettingsLoad();

    g_Settings.ResolveOrCreate(Path).SetInt(Value);
}


bool
SettingsGetBool (
    _In_ PCWSTR Path,
    _In_ bool Default
    )
{
    const JsonValue* value;

    SettingsLoad();

    value = g_Settings.Resolve(Path);

    return (value != nullptr) ? value->AsBool(Default) : Default;
}


void
SettingsSetBool (
    _In_ PCWSTR Path,
    _In_ bool Value
    )
{
    SettingsLoad();

    g_Settings.ResolveOrCreate(Path).SetBool(Value);
}


std::wstring
SettingsGetString (
    _In_ PCWSTR Path,
    _In_ PCWSTR Default
    )
{
    const JsonValue* value;

    SettingsLoad();

    value = g_Settings.Resolve(Path);

    return (value != nullptr) ? value->AsString(Default) : Default;
}


void
SettingsSetString (
    _In_ PCWSTR Path,
    _In_ PCWSTR Value
    )
{
    SettingsLoad();

    g_Settings.ResolveOrCreate(Path).SetString(Value);
}
