/*!

    @file Core/UserCodes.cpp

    @brief Your own codes, which headers they come from and the tables built
           from them.

    @details Your headers live in the codes folder beside settings.json, and
             wherever "codes.files" points. ReadHeaders reads them together so
             one can use another's names, and each becomes up to five tables,
             one per kind. They sit after the built-in tables in AllKindTables,
             so lookups and searches treat them like any other.

             Each header's a source in Codes to show, keyed by "file:" and its
             path in lower case, since paths ignore case.

             One store owns everything a table points at, and a reload replaces
             it whole. Nothing outside the core keeps a pointer into it, since
             CodeMatch and CodeSource hold copies.

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

#include <algorithm>
#include <cwctype>
#include <deque>
#include <memory>
#include <unordered_set>


static constexpr WCHAR k_CodesSetting[] = L"codes";
static constexpr WCHAR k_FilesSetting[] = L"files";
static constexpr WCHAR k_TreatAsSetting[] = L"treatAs";
static constexpr WCHAR k_FolderName[] = L"codes";
static constexpr WCHAR k_KeyPrefix[] = L"file:";

//
// The kinds a header's codes can be, each of which becomes its own table.
//
static constexpr CodeKind k_UserKinds[] =
{
    CodeKind::WinError,
    CodeKind::HResult,
    CodeKind::NtStatus,
    CodeKind::PoolTag,
    CodeKind::Ioctl
};


/*!

    @brief One table built from a header.

*/
struct UserTable
{
    std::vector<TableEntry> Entries;
    TableSpan Span;
};


/*!

    @brief Everything built from the user's headers. A deque, so what a table
           points at stays put as more is added.

*/
struct UserStore
{
    std::deque<std::string> Strings;
    std::deque<std::wstring> WideStrings;
    std::deque<UserTable> Tables;
    std::vector<KindTable> AllTables;
    std::vector<UserHeader> Headers;
};

static std::unique_ptr<UserStore> g_Store;


/*!

    @brief Text in lower case, as paths are compared.

*/
static
std::wstring
Lower (
    _In_ const std::wstring& Text
    )
{
    std::wstring lower;

    lower = Text;
    for (WCHAR& c : lower)
    {
        c = SCAST(WCHAR)(towlower(c));
    }

    return lower;
}


/*!

    @brief Converts text to UTF-8, for a table entry.

*/
static
std::string
WideToUtf8 (
    _In_ const std::wstring& Text
    )
{
    std::string utf8;
    int bytes;

    if (Text.empty())
    {
        return utf8;
    }

    bytes = WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), SCAST(int)(Text.size()), nullptr, 0, nullptr, nullptr);
    if (bytes <= 0)
    {
        return utf8;
    }

    utf8.resize(SCAST(size_t)(bytes));
    WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), SCAST(int)(Text.size()), utf8.data(), bytes, nullptr, nullptr);
    return utf8;
}


/*!

    @brief A store holding only the built-in tables.

*/
static
std::unique_ptr<UserStore>
EmptyStore (
    void
    )
{
    std::unique_ptr<UserStore> store;

    store = std::make_unique<UserStore>();
    store->AllTables.assign(g_KindTables, g_KindTables + k_KindTableCount);
    return store;
}


/*!

    @brief The file name at the end of a path.

*/
static
std::wstring
FileName (
    _In_ const std::wstring& Path
    )
{
    size_t separator;

    separator = Path.find_last_of(L"\\/");
    return (separator == std::wstring::npos) ? Path : Path.substr(separator + 1);
}


/*!

    @brief How a Treat as setting is written in settings.json.

*/
static
PCWSTR
TreatAsName (
    _In_ UserTreatAs TreatAs
    )
{
    switch (TreatAs)
    {
    case UserTreatAs::NtStatus:
        return L"ntstatus";

    case UserTreatAs::HResult:
        return L"hresult";

    case UserTreatAs::WinError:
        return L"win32";

    default:
        return L"automatic";
    }
}


/*!

    @brief One header to build tables from, its text and what's known of it
           before it's read.

*/
struct PendingHeader
{
    HeaderSource Source;
    bool Added;
    bool Readable;
};


/*!

    @brief Reads the headers, builds their tables, and makes them the ones in
           use.

    @param[in] Headers - The headers.

*/
static
void
BuildStore (
    _In_ const std::vector<PendingHeader>& Headers
    )
{
    std::unique_ptr<UserStore> store;
    std::vector<HeaderSource> sources;
    std::vector<HeaderRead> reads;
    std::unordered_set<uint32_t> tags;
    UserHeader status;
    TableEntry entry;
    PCWSTR key;
    PCWSTR name;
    PCWSTR file;

    for (const PendingHeader& header : Headers)
    {
        sources.push_back(header.Source);
    }

    reads = ReadHeaders(sources);
    store = EmptyStore();

    for (size_t i = 0; i < Headers.size(); i++)
    {
        status = UserHeader();
        status.Path = Headers[i].Source.Path;
        status.Key = k_KeyPrefix + Lower(status.Path);
        status.Added = Headers[i].Added;
        status.Readable = Headers[i].Readable;
        status.TreatAs = Headers[i].Source.TreatAs;
        status.CodeCount = reads[i].Codes.size();
        status.Skipped = reads[i].Skipped;
        store->Headers.push_back(status);

        key = store->WideStrings.emplace_back(status.Key).c_str();
        name = store->WideStrings.emplace_back(L"Your codes: " + FileName(status.Path)).c_str();
        file = store->WideStrings.emplace_back(FileName(status.Path)).c_str();

        for (CodeKind kind : k_UserKinds)
        {
            UserTable& table = store->Tables.emplace_back();

            tags.clear();

            for (const HeaderCode& code : reads[i].Codes)
            {
                if (code.Kind != kind)
                {
                    continue;
                }

                entry = TableEntry();
                entry.Value = code.Value;
                entry.Text = code.Text.empty() ? nullptr : store->Strings.emplace_back(WideToUtf8(code.Text)).c_str();

                //
                // A pool tag is known by its tag first, as the SDK's are, with
                // the header's names for it after.
                //
                if ((kind == CodeKind::PoolTag) && tags.insert(code.Value).second)
                {
                    entry.Name = store->Strings.emplace_back(WideToUtf8(code.Tag)).c_str();
                    table.Entries.push_back(entry);
                }

                entry.Name = store->Strings.emplace_back(WideToUtf8(code.Name)).c_str();
                table.Entries.push_back(entry);
            }

            if (table.Entries.empty())
            {
                store->Tables.pop_back();
                continue;
            }

            //
            // In value order, keeping each value's first name first, as the
            // lookup expects. Every later name for a value is an alias.
            //
            std::stable_sort(table.Entries.begin(),
                             table.Entries.end(),
                             [](const TableEntry& A, const TableEntry& B)
                             {
                                 return A.Value < B.Value;
                             });

            for (size_t j = 1; j < table.Entries.size(); j++)
            {
                if (table.Entries[j].Value == table.Entries[j - 1].Value)
                {
                    table.Entries[j].Flags |= k_TableEntryAlias;
                }
            }

            table.Span = { table.Entries.data(), table.Entries.size(), file };
            store->AllTables.push_back({ kind, &table.Span, key, name, SourceShow::Always, true });
        }
    }

    g_Store = std::move(store);
}


/*!

    @brief The "codes" object in the settings, if there is one.

*/
static
const JsonValue*
CodesSettings (
    void
    )
{
    return SettingsRoot().Find(k_CodesSetting);
}


/*!

    @brief The paths of the headers added in settings, in the order they were
           added.

*/
static
std::vector<std::wstring>
AddedFiles (
    void
    )
{
    std::vector<std::wstring> files;
    const JsonValue* codes;
    const JsonValue* list;
    std::wstring path;

    codes = CodesSettings();
    list = (codes != nullptr) ? codes->Find(k_FilesSetting) : nullptr;
    if (list == nullptr)
    {
        return files;
    }

    for (const JsonValue& element : list->Elements())
    {
        path = element.AsString(L"");
        if (!path.empty())
        {
            files.push_back(path);
        }
    }

    return files;
}


/*!

    @brief Saves the headers added in settings.

*/
static
void
SaveAddedFiles (
    _In_ const std::vector<std::wstring>& Files
    )
{
    JsonValue* list;

    list = &SettingsRoot().Member(k_CodesSetting).Member(k_FilesSetting);
    list->SetArray();

    for (const std::wstring& file : Files)
    {
        list->Append().SetString(file.c_str());
    }

    SettingsSave();
}


/*!

    @brief A path in full, as it's kept in settings.

*/
static
std::wstring
FullPath (
    _In_z_ PCWSTR Path
    )
{
    WCHAR full[MAX_PATH * 4];
    DWORD length;

    length = GetFullPathNameW(Path, ARRAYSIZE(full), full, nullptr);
    if ((length == 0) || (length >= ARRAYSIZE(full)))
    {
        return Path;
    }

    return full;
}


/*!

    @brief Whether a file name is a header's.

*/
static
bool
IsHeaderName (
    _In_ const std::wstring& Name
    )
{
    std::wstring lower;

    lower = Lower(Name);

    return ((lower.size() > 2) && (lower.compare(lower.size() - 2, 2, L".h") == 0)) ||
           ((lower.size() > 4) && (lower.compare(lower.size() - 4, 4, L".hpp") == 0));
}


/*!

    @brief Every table, the built-in ones in g_KindTables and then any built
           from your headers.

    @details A reload of the user's headers replaces the list, so it isn't
             kept across one.

    @return The tables.

*/
const std::vector<KindTable>&
AllKindTables (
    void
    )
{
    if (g_Store == nullptr)
    {
        g_Store = EmptyStore();
    }

    return g_Store->AllTables;
}


/*!

    @brief The folder your headers are read from, "codes" beside
           settings.json.

    @return The folder, which may not exist, or an empty string.

*/
std::wstring
UserCodesFolder (
    void
    )
{
    std::wstring settings;
    size_t separator;

    settings = SettingsPath();
    separator = settings.find_last_of(L'\\');
    if (separator == std::wstring::npos)
    {
        return std::wstring();
    }

    return settings.substr(0, separator + 1) + k_FolderName;
}


/*!

    @brief Reads the user's headers, from the codes folder and those added in
           settings, replacing any read before. Front ends call it as they
           start, and again when a header changes.

*/
void
LoadUserCodes (
    void
    )
{
    std::vector<PendingHeader> headers;
    std::unordered_set<std::wstring> seen;
    std::vector<std::wstring> files;
    WIN32_FIND_DATAW found;
    const JsonValue* codes;
    const JsonValue* treatAs;
    const JsonValue* setting;
    PendingHeader header;
    std::wstring folder;
    std::wstring name;
    HANDLE find;

    folder = UserCodesFolder();

    //
    // The folder's headers in name order, so the first definition of a name
    // is the same one every time.
    //
    if (!folder.empty())
    {
        find = FindFirstFileW((folder + L"\\*").c_str(), &found);
        if (find != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) && IsHeaderName(found.cFileName))
                {
                    files.push_back(folder + L"\\" + found.cFileName);
                }
            } while (FindNextFileW(find, &found));

            FindClose(find);
        }

        std::sort(files.begin(),
                  files.end(),
                  [](const std::wstring& A, const std::wstring& B)
                  {
                      return _wcsicmp(A.c_str(), B.c_str()) < 0;
                  });
    }

    for (const std::wstring& file : files)
    {
        header = PendingHeader();
        header.Source.Path = file;
        header.Added = false;
        headers.push_back(header);
        seen.insert(Lower(file));
    }

    //
    // A header added in settings that's also in the folder is read once.
    //
    for (const std::wstring& file : AddedFiles())
    {
        if (!seen.insert(Lower(file)).second)
        {
            continue;
        }

        header = PendingHeader();
        header.Source.Path = file;
        header.Added = true;
        headers.push_back(header);
    }

    codes = CodesSettings();
    treatAs = (codes != nullptr) ? codes->Find(k_TreatAsSetting) : nullptr;

    for (PendingHeader& pending : headers)
    {
        pending.Readable = ReadHeaderFile(pending.Source.Path, &pending.Source.Text);
        pending.Source.TreatAs = UserTreatAs::Automatic;

        setting = (treatAs != nullptr) ? treatAs->Find(Lower(pending.Source.Path).c_str()) : nullptr;
        name = (setting != nullptr) ? setting->AsString(L"") : std::wstring();

        if (name == L"ntstatus")
        {
            pending.Source.TreatAs = UserTreatAs::NtStatus;
        }
        else if (name == L"hresult")
        {
            pending.Source.TreatAs = UserTreatAs::HResult;
        }
        else if (name == L"win32")
        {
            pending.Source.TreatAs = UserTreatAs::WinError;
        }
    }

    BuildStore(headers);
}


/*!

    @brief Uses headers given as text as the user's codes, replacing any read
           before, for the tests. An empty list removes them all.

    @param[in] Headers - The headers.

*/
void
LoadUserCodeText (
    _In_ const std::vector<HeaderSource>& Headers
    )
{
    std::vector<PendingHeader> headers;

    for (const HeaderSource& source : Headers)
    {
        headers.push_back({ source, false, true });
    }

    BuildStore(headers);
}


/*!

    @brief The user's headers, as last read.

    @return The headers.

*/
std::vector<UserHeader>
UserHeaders (
    void
    )
{
    AllKindTables();

    return g_Store->Headers;
}


/*!

    @brief Adds a header in settings, to be read where it is. LoadUserCodes
           reads it.

    @param[in] Path - The header.

*/
void
UserCodesAddFile (
    _In_z_ PCWSTR Path
    )
{
    std::vector<std::wstring> files;
    std::wstring path;

    path = FullPath(Path);
    files = AddedFiles();

    for (const std::wstring& file : files)
    {
        if (_wcsicmp(file.c_str(), path.c_str()) == 0)
        {
            return;
        }
    }

    files.push_back(path);
    SaveAddedFiles(files);
}


/*!

    @brief Removes a header added in settings.

    @param[in] Path - The header, as UserHeaders gives it.

*/
void
UserCodesRemoveFile (
    _In_z_ PCWSTR Path
    )
{
    std::vector<std::wstring> files;

    files = AddedFiles();
    files.erase(std::remove_if(files.begin(),
                               files.end(),
                               [Path](const std::wstring& File)
                               {
                                   return _wcsicmp(File.c_str(), Path) == 0;
                               }),
                files.end());

    SettingsRoot().Member(k_CodesSetting).Member(k_TreatAsSetting).RemoveMember(Lower(Path).c_str());
    SaveAddedFiles(files);
}


/*!

    @brief Sets what a header's plain numbers are. LoadUserCodes reads it
           again with the setting.

    @param[in] Path - The header, as UserHeaders gives it.

    @param[in] TreatAs - The setting.

*/
void
UserCodesSetTreatAs (
    _In_z_ PCWSTR Path,
    _In_ UserTreatAs TreatAs
    )
{
    JsonValue* treatAs;

    treatAs = &SettingsRoot().Member(k_CodesSetting).Member(k_TreatAsSetting);

    if (TreatAs == UserTreatAs::Automatic)
    {
        treatAs->RemoveMember(Lower(Path).c_str());
    }
    else
    {
        treatAs->Member(Lower(Path).c_str()).SetString(TreatAsName(TreatAs));
    }

    SettingsSave();
}


/*!

    @brief The folders to watch for changes to the user's headers.

    @return The codes folder, and the folder of each header added in settings.

*/
std::vector<std::wstring>
UserCodesWatchFolders (
    void
    )
{
    std::vector<std::wstring> folders;
    std::unordered_set<std::wstring> seen;
    std::wstring folder;
    size_t separator;

    folder = UserCodesFolder();
    if (!folder.empty() && seen.insert(Lower(folder)).second)
    {
        folders.push_back(folder);
    }

    for (const std::wstring& file : AddedFiles())
    {
        separator = file.find_last_of(L"\\/");
        if (separator == std::wstring::npos)
        {
            continue;
        }

        folder = file.substr(0, separator);
        if (seen.insert(Lower(folder)).second)
        {
            folders.push_back(folder);
        }
    }

    return folders;
}
