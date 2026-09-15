/*!

    @file Core/Tables/Flags.g.cpp

    @brief Flag families, for decoding a value into its named bits.

    @details DO NOT EDIT. Regenerate with tools/generate_tables.py.

             Generated from Windows SDK 10.0.28000.0. Each family is sorted by
             value, with the preferred name for each value first.

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


static const TableEntry k_Access[] =
{
    { 0x00010000u, 0, "DELETE", nullptr },
    { 0x00020000u, 0, "READ_CONTROL", nullptr },
    { 0x00040000u, 0, "WRITE_DAC", nullptr },
    { 0x00080000u, 0, "WRITE_OWNER", nullptr },
    { 0x000F0000u, 0, "STANDARD_RIGHTS_REQUIRED", nullptr },
    { 0x00100000u, 0, "SYNCHRONIZE", nullptr },
    { 0x001F0000u, 0, "STANDARD_RIGHTS_ALL", nullptr },
    { 0x01000000u, 0, "ACCESS_SYSTEM_SECURITY", nullptr },
    { 0x02000000u, 0, "MAXIMUM_ALLOWED", nullptr },
    { 0x10000000u, 0, "GENERIC_ALL", nullptr },
    { 0x20000000u, 0, "GENERIC_EXECUTE", nullptr },
    { 0x40000000u, 0, "GENERIC_WRITE", nullptr },
    { 0x80000000u, 0, "GENERIC_READ", nullptr },
};

static const TableEntry k_File[] =
{
    { 0x00000001u, 0, "FILE_READ_DATA", nullptr },
    { 0x00000001u, k_TableEntryAlias, "FILE_LIST_DIRECTORY", nullptr },
    { 0x00000002u, 0, "FILE_WRITE_DATA", nullptr },
    { 0x00000002u, k_TableEntryAlias, "FILE_ADD_FILE", nullptr },
    { 0x00000004u, 0, "FILE_APPEND_DATA", nullptr },
    { 0x00000004u, k_TableEntryAlias, "FILE_ADD_SUBDIRECTORY", nullptr },
    { 0x00000004u, k_TableEntryAlias, "FILE_CREATE_PIPE_INSTANCE", nullptr },
    { 0x00000008u, 0, "FILE_READ_EA", nullptr },
    { 0x00000010u, 0, "FILE_WRITE_EA", nullptr },
    { 0x00000020u, 0, "FILE_EXECUTE", nullptr },
    { 0x00000020u, k_TableEntryAlias, "FILE_TRAVERSE", nullptr },
    { 0x00000040u, 0, "FILE_DELETE_CHILD", nullptr },
    { 0x00000080u, 0, "FILE_READ_ATTRIBUTES", nullptr },
    { 0x00000100u, 0, "FILE_WRITE_ATTRIBUTES", nullptr },
    { 0x00010000u, 0, "DELETE", nullptr },
    { 0x00020000u, 0, "READ_CONTROL", nullptr },
    { 0x00040000u, 0, "WRITE_DAC", nullptr },
    { 0x00080000u, 0, "WRITE_OWNER", nullptr },
    { 0x000F0000u, 0, "STANDARD_RIGHTS_REQUIRED", nullptr },
    { 0x00100000u, 0, "SYNCHRONIZE", nullptr },
    { 0x00120089u, 0, "FILE_GENERIC_READ", nullptr },
    { 0x001200A0u, 0, "FILE_GENERIC_EXECUTE", nullptr },
    { 0x00120116u, 0, "FILE_GENERIC_WRITE", nullptr },
    { 0x001F0000u, 0, "STANDARD_RIGHTS_ALL", nullptr },
    { 0x001F01FFu, 0, "FILE_ALL_ACCESS", nullptr },
    { 0x01000000u, 0, "ACCESS_SYSTEM_SECURITY", nullptr },
    { 0x02000000u, 0, "MAXIMUM_ALLOWED", nullptr },
    { 0x10000000u, 0, "GENERIC_ALL", nullptr },
    { 0x20000000u, 0, "GENERIC_EXECUTE", nullptr },
    { 0x40000000u, 0, "GENERIC_WRITE", nullptr },
    { 0x80000000u, 0, "GENERIC_READ", nullptr },
};

static const TableEntry k_Key[] =
{
    { 0x00000001u, 0, "KEY_QUERY_VALUE", nullptr },
    { 0x00000002u, 0, "KEY_SET_VALUE", nullptr },
    { 0x00000004u, 0, "KEY_CREATE_SUB_KEY", nullptr },
    { 0x00000008u, 0, "KEY_ENUMERATE_SUB_KEYS", nullptr },
    { 0x00000010u, 0, "KEY_NOTIFY", nullptr },
    { 0x00000020u, 0, "KEY_CREATE_LINK", nullptr },
    { 0x00000100u, 0, "KEY_WOW64_64KEY", nullptr },
    { 0x00000200u, 0, "KEY_WOW64_32KEY", nullptr },
    { 0x00010000u, 0, "DELETE", nullptr },
    { 0x00020000u, 0, "READ_CONTROL", nullptr },
    { 0x00020006u, 0, "KEY_WRITE", nullptr },
    { 0x00020019u, 0, "KEY_READ", nullptr },
    { 0x00020019u, k_TableEntryAlias, "KEY_EXECUTE", nullptr },
    { 0x00040000u, 0, "WRITE_DAC", nullptr },
    { 0x00080000u, 0, "WRITE_OWNER", nullptr },
    { 0x000F0000u, 0, "STANDARD_RIGHTS_REQUIRED", nullptr },
    { 0x000F003Fu, 0, "KEY_ALL_ACCESS", nullptr },
    { 0x00100000u, 0, "SYNCHRONIZE", nullptr },
    { 0x001F0000u, 0, "STANDARD_RIGHTS_ALL", nullptr },
    { 0x01000000u, 0, "ACCESS_SYSTEM_SECURITY", nullptr },
    { 0x02000000u, 0, "MAXIMUM_ALLOWED", nullptr },
    { 0x10000000u, 0, "GENERIC_ALL", nullptr },
    { 0x20000000u, 0, "GENERIC_EXECUTE", nullptr },
    { 0x40000000u, 0, "GENERIC_WRITE", nullptr },
    { 0x80000000u, 0, "GENERIC_READ", nullptr },
};

static const TableEntry k_Process[] =
{
    { 0x00000001u, 0, "PROCESS_TERMINATE", nullptr },
    { 0x00000002u, 0, "PROCESS_CREATE_THREAD", nullptr },
    { 0x00000004u, 0, "PROCESS_SET_SESSIONID", nullptr },
    { 0x00000008u, 0, "PROCESS_VM_OPERATION", nullptr },
    { 0x00000010u, 0, "PROCESS_VM_READ", nullptr },
    { 0x00000020u, 0, "PROCESS_VM_WRITE", nullptr },
    { 0x00000040u, 0, "PROCESS_DUP_HANDLE", nullptr },
    { 0x00000080u, 0, "PROCESS_CREATE_PROCESS", nullptr },
    { 0x00000100u, 0, "PROCESS_SET_QUOTA", nullptr },
    { 0x00000200u, 0, "PROCESS_SET_INFORMATION", nullptr },
    { 0x00000400u, 0, "PROCESS_QUERY_INFORMATION", nullptr },
    { 0x00000800u, 0, "PROCESS_SUSPEND_RESUME", nullptr },
    { 0x00001000u, 0, "PROCESS_QUERY_LIMITED_INFORMATION", nullptr },
    { 0x00002000u, 0, "PROCESS_SET_LIMITED_INFORMATION", nullptr },
    { 0x00010000u, 0, "DELETE", nullptr },
    { 0x00020000u, 0, "READ_CONTROL", nullptr },
    { 0x00040000u, 0, "WRITE_DAC", nullptr },
    { 0x00080000u, 0, "WRITE_OWNER", nullptr },
    { 0x000F0000u, 0, "STANDARD_RIGHTS_REQUIRED", nullptr },
    { 0x00100000u, 0, "SYNCHRONIZE", nullptr },
    { 0x001F0000u, 0, "STANDARD_RIGHTS_ALL", nullptr },
    { 0x001FFFFFu, 0, "PROCESS_ALL_ACCESS", nullptr },
    { 0x01000000u, 0, "ACCESS_SYSTEM_SECURITY", nullptr },
    { 0x02000000u, 0, "MAXIMUM_ALLOWED", nullptr },
    { 0x10000000u, 0, "GENERIC_ALL", nullptr },
    { 0x20000000u, 0, "GENERIC_EXECUTE", nullptr },
    { 0x40000000u, 0, "GENERIC_WRITE", nullptr },
    { 0x80000000u, 0, "GENERIC_READ", nullptr },
};

static const TableEntry k_Thread[] =
{
    { 0x00000001u, 0, "THREAD_TERMINATE", nullptr },
    { 0x00000002u, 0, "THREAD_SUSPEND_RESUME", nullptr },
    { 0x00000008u, 0, "THREAD_GET_CONTEXT", nullptr },
    { 0x00000010u, 0, "THREAD_SET_CONTEXT", nullptr },
    { 0x00000020u, 0, "THREAD_SET_INFORMATION", nullptr },
    { 0x00000040u, 0, "THREAD_QUERY_INFORMATION", nullptr },
    { 0x00000080u, 0, "THREAD_SET_THREAD_TOKEN", nullptr },
    { 0x00000100u, 0, "THREAD_IMPERSONATE", nullptr },
    { 0x00000200u, 0, "THREAD_DIRECT_IMPERSONATION", nullptr },
    { 0x00000400u, 0, "THREAD_SET_LIMITED_INFORMATION", nullptr },
    { 0x00000800u, 0, "THREAD_QUERY_LIMITED_INFORMATION", nullptr },
    { 0x00001000u, 0, "THREAD_RESUME", nullptr },
    { 0x00010000u, 0, "DELETE", nullptr },
    { 0x00020000u, 0, "READ_CONTROL", nullptr },
    { 0x00040000u, 0, "WRITE_DAC", nullptr },
    { 0x00080000u, 0, "WRITE_OWNER", nullptr },
    { 0x000F0000u, 0, "STANDARD_RIGHTS_REQUIRED", nullptr },
    { 0x00100000u, 0, "SYNCHRONIZE", nullptr },
    { 0x001F0000u, 0, "STANDARD_RIGHTS_ALL", nullptr },
    { 0x001FFFFFu, 0, "THREAD_ALL_ACCESS", nullptr },
    { 0x01000000u, 0, "ACCESS_SYSTEM_SECURITY", nullptr },
    { 0x02000000u, 0, "MAXIMUM_ALLOWED", nullptr },
    { 0x10000000u, 0, "GENERIC_ALL", nullptr },
    { 0x20000000u, 0, "GENERIC_EXECUTE", nullptr },
    { 0x40000000u, 0, "GENERIC_WRITE", nullptr },
    { 0x80000000u, 0, "GENERIC_READ", nullptr },
};

static const TableEntry k_Page[] =
{
    { 0x00000001u, 0, "PAGE_NOACCESS", nullptr },
    { 0x00000002u, 0, "PAGE_READONLY", nullptr },
    { 0x00000004u, 0, "PAGE_READWRITE", nullptr },
    { 0x00000008u, 0, "PAGE_WRITECOPY", nullptr },
    { 0x00000010u, 0, "PAGE_EXECUTE", nullptr },
    { 0x00000020u, 0, "PAGE_EXECUTE_READ", nullptr },
    { 0x00000040u, 0, "PAGE_EXECUTE_READWRITE", nullptr },
    { 0x00000080u, 0, "PAGE_EXECUTE_WRITECOPY", nullptr },
    { 0x00000100u, 0, "PAGE_GUARD", nullptr },
    { 0x00000200u, 0, "PAGE_NOCACHE", nullptr },
    { 0x00000400u, 0, "PAGE_WRITECOMBINE", nullptr },
    { 0x20000000u, 0, "PAGE_ENCLAVE_UNVALIDATED", nullptr },
    { 0x40000000u, 0, "PAGE_TARGETS_INVALID", nullptr },
    { 0x40000000u, k_TableEntryAlias, "PAGE_TARGETS_NO_UPDATE", nullptr },
    { 0x80000000u, 0, "PAGE_REVERT_TO_FILE_MAP", nullptr },
    { 0x80000000u, k_TableEntryAlias, "PAGE_ENCLAVE_THREAD_CONTROL", nullptr },
};

static const TableEntry k_Mem[] =
{
    { 0x00000001u, 0, "MEM_UNMAP_WITH_TRANSIENT_BOOST", nullptr },
    { 0x00000001u, k_TableEntryAlias, "MEM_COALESCE_PLACEHOLDERS", nullptr },
    { 0x00000002u, 0, "MEM_PRESERVE_PLACEHOLDER", nullptr },
    { 0x00001000u, 0, "MEM_COMMIT", nullptr },
    { 0x00002000u, 0, "MEM_RESERVE", nullptr },
    { 0x00004000u, 0, "MEM_REPLACE_PLACEHOLDER", nullptr },
    { 0x00004000u, k_TableEntryAlias, "MEM_DECOMMIT", nullptr },
    { 0x00008000u, 0, "MEM_RELEASE", nullptr },
    { 0x00010000u, 0, "MEM_FREE", nullptr },
    { 0x00020000u, 0, "MEM_PRIVATE", nullptr },
    { 0x00040000u, 0, "MEM_RESERVE_PLACEHOLDER", nullptr },
    { 0x00040000u, k_TableEntryAlias, "MEM_MAPPED", nullptr },
    { 0x00080000u, 0, "MEM_RESET", nullptr },
    { 0x00100000u, 0, "MEM_TOP_DOWN", nullptr },
    { 0x00200000u, 0, "MEM_WRITE_WATCH", nullptr },
    { 0x00400000u, 0, "MEM_PHYSICAL", nullptr },
    { 0x00800000u, 0, "MEM_ROTATE", nullptr },
    { 0x00800000u, k_TableEntryAlias, "MEM_DIFFERENT_IMAGE_BASE_OK", nullptr },
    { 0x01000000u, 0, "MEM_RESET_UNDO", nullptr },
    { 0x01000000u, k_TableEntryAlias, "MEM_IMAGE", nullptr },
    { 0x20000000u, 0, "MEM_LARGE_PAGES", nullptr },
    { 0x20400000u, 0, "MEM_64K_PAGES", nullptr },
    { 0x80000000u, 0, "MEM_4MB_PAGES", nullptr },
};

static const TableEntry k_Share[] =
{
    { 0x00000001u, 0, "FILE_SHARE_READ", nullptr },
    { 0x00000002u, 0, "FILE_SHARE_WRITE", nullptr },
    { 0x00000004u, 0, "FILE_SHARE_DELETE", nullptr },
};

static const TableEntry k_Disposition[] =
{
    { 0x00000001u, 0, "CREATE_NEW", nullptr },
    { 0x00000002u, 0, "CREATE_ALWAYS", nullptr },
    { 0x00000003u, 0, "OPEN_EXISTING", nullptr },
    { 0x00000004u, 0, "OPEN_ALWAYS", nullptr },
    { 0x00000005u, 0, "TRUNCATE_EXISTING", nullptr },
};

static const TableEntry k_Fileflags[] =
{
    { 0x00000001u, 0, "FILE_ATTRIBUTE_READONLY", nullptr },
    { 0x00000002u, 0, "FILE_ATTRIBUTE_HIDDEN", nullptr },
    { 0x00000004u, 0, "FILE_ATTRIBUTE_SYSTEM", nullptr },
    { 0x00000010u, 0, "FILE_ATTRIBUTE_DIRECTORY", nullptr },
    { 0x00000020u, 0, "FILE_ATTRIBUTE_ARCHIVE", nullptr },
    { 0x00000040u, 0, "FILE_ATTRIBUTE_DEVICE", nullptr },
    { 0x00000080u, 0, "FILE_ATTRIBUTE_NORMAL", nullptr },
    { 0x00000100u, 0, "FILE_ATTRIBUTE_TEMPORARY", nullptr },
    { 0x00000200u, 0, "FILE_ATTRIBUTE_SPARSE_FILE", nullptr },
    { 0x00000400u, 0, "FILE_ATTRIBUTE_REPARSE_POINT", nullptr },
    { 0x00000800u, 0, "FILE_ATTRIBUTE_COMPRESSED", nullptr },
    { 0x00001000u, 0, "FILE_ATTRIBUTE_OFFLINE", nullptr },
    { 0x00002000u, 0, "FILE_ATTRIBUTE_NOT_CONTENT_INDEXED", nullptr },
    { 0x00004000u, 0, "FILE_ATTRIBUTE_ENCRYPTED", nullptr },
    { 0x00008000u, 0, "FILE_ATTRIBUTE_INTEGRITY_STREAM", nullptr },
    { 0x00010000u, 0, "FILE_ATTRIBUTE_VIRTUAL", nullptr },
    { 0x00010000u, k_TableEntryAlias, "FILE_FLAG_DISALLOW_PATH_REDIRECTS", nullptr },
    { 0x00020000u, 0, "FILE_ATTRIBUTE_NO_SCRUB_DATA", nullptr },
    { 0x00020000u, k_TableEntryAlias, "FILE_FLAG_IGNORE_IMPERSONATED_DEVICEMAP", nullptr },
    { 0x00040000u, 0, "FILE_ATTRIBUTE_EA", nullptr },
    { 0x00040000u, k_TableEntryAlias, "FILE_ATTRIBUTE_RECALL_ON_OPEN", nullptr },
    { 0x00040000u, k_TableEntryAlias, "FILE_FLAG_OPEN_REQUIRING_OPLOCK", nullptr },
    { 0x00080000u, 0, "FILE_ATTRIBUTE_PINNED", nullptr },
    { 0x00080000u, k_TableEntryAlias, "FILE_FLAG_FIRST_PIPE_INSTANCE", nullptr },
    { 0x00100000u, 0, "FILE_ATTRIBUTE_UNPINNED", nullptr },
    { 0x00100000u, k_TableEntryAlias, "FILE_FLAG_OPEN_NO_RECALL", nullptr },
    { 0x00200000u, 0, "FILE_FLAG_OPEN_REPARSE_POINT", nullptr },
    { 0x00400000u, 0, "FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS", nullptr },
    { 0x00800000u, 0, "FILE_FLAG_SESSION_AWARE", nullptr },
    { 0x01000000u, 0, "FILE_FLAG_POSIX_SEMANTICS", nullptr },
    { 0x02000000u, 0, "FILE_FLAG_BACKUP_SEMANTICS", nullptr },
    { 0x04000000u, 0, "FILE_FLAG_DELETE_ON_CLOSE", nullptr },
    { 0x08000000u, 0, "FILE_FLAG_SEQUENTIAL_SCAN", nullptr },
    { 0x10000000u, 0, "FILE_FLAG_RANDOM_ACCESS", nullptr },
    { 0x20000000u, 0, "FILE_ATTRIBUTE_STRICTLY_SEQUENTIAL", nullptr },
    { 0x20000000u, k_TableEntryAlias, "FILE_FLAG_NO_BUFFERING", nullptr },
    { 0x40000000u, 0, "FILE_FLAG_OVERLAPPED", nullptr },
    { 0x80000000u, 0, "FILE_FLAG_WRITE_THROUGH", nullptr },
};

static const TableEntry k_Style[] =
{
    { 0x00000000u, 0, "WS_OVERLAPPED", nullptr },
    { 0x00000000u, k_TableEntryAlias, "WS_TILED", nullptr },
    { 0x00010000u, 0, "WS_MAXIMIZEBOX", nullptr },
    { 0x00010000u, k_TableEntryAlias, "WS_TABSTOP", nullptr },
    { 0x00020000u, 0, "WS_MINIMIZEBOX", nullptr },
    { 0x00020000u, k_TableEntryAlias, "WS_GROUP", nullptr },
    { 0x00040000u, 0, "WS_THICKFRAME", nullptr },
    { 0x00040000u, k_TableEntryAlias, "WS_SIZEBOX", nullptr },
    { 0x00080000u, 0, "WS_SYSMENU", nullptr },
    { 0x00100000u, 0, "WS_HSCROLL", nullptr },
    { 0x00200000u, 0, "WS_VSCROLL", nullptr },
    { 0x00400000u, 0, "WS_DLGFRAME", nullptr },
    { 0x00800000u, 0, "WS_BORDER", nullptr },
    { 0x00C00000u, 0, "WS_CAPTION", nullptr },
    { 0x00CF0000u, 0, "WS_OVERLAPPEDWINDOW", nullptr },
    { 0x00CF0000u, k_TableEntryAlias, "WS_TILEDWINDOW", nullptr },
    { 0x01000000u, 0, "WS_MAXIMIZE", nullptr },
    { 0x02000000u, 0, "WS_CLIPCHILDREN", nullptr },
    { 0x04000000u, 0, "WS_CLIPSIBLINGS", nullptr },
    { 0x08000000u, 0, "WS_DISABLED", nullptr },
    { 0x10000000u, 0, "WS_VISIBLE", nullptr },
    { 0x20000000u, 0, "WS_MINIMIZE", nullptr },
    { 0x20000000u, k_TableEntryAlias, "WS_ICONIC", nullptr },
    { 0x40000000u, 0, "WS_CHILD", nullptr },
    { 0x40000000u, k_TableEntryAlias, "WS_CHILDWINDOW", nullptr },
    { 0x80000000u, 0, "WS_POPUP", nullptr },
    { 0x80880000u, 0, "WS_POPUPWINDOW", nullptr },
};

static const TableEntry k_Exstyle[] =
{
    { 0x00000000u, 0, "WS_EX_LEFT", nullptr },
    { 0x00000000u, k_TableEntryAlias, "WS_EX_LTRREADING", nullptr },
    { 0x00000000u, k_TableEntryAlias, "WS_EX_RIGHTSCROLLBAR", nullptr },
    { 0x00000001u, 0, "WS_EX_DLGMODALFRAME", nullptr },
    { 0x00000004u, 0, "WS_EX_NOPARENTNOTIFY", nullptr },
    { 0x00000008u, 0, "WS_EX_TOPMOST", nullptr },
    { 0x00000010u, 0, "WS_EX_ACCEPTFILES", nullptr },
    { 0x00000020u, 0, "WS_EX_TRANSPARENT", nullptr },
    { 0x00000040u, 0, "WS_EX_MDICHILD", nullptr },
    { 0x00000080u, 0, "WS_EX_TOOLWINDOW", nullptr },
    { 0x00000100u, 0, "WS_EX_WINDOWEDGE", nullptr },
    { 0x00000188u, 0, "WS_EX_PALETTEWINDOW", nullptr },
    { 0x00000200u, 0, "WS_EX_CLIENTEDGE", nullptr },
    { 0x00000300u, 0, "WS_EX_OVERLAPPEDWINDOW", nullptr },
    { 0x00000400u, 0, "WS_EX_CONTEXTHELP", nullptr },
    { 0x00001000u, 0, "WS_EX_RIGHT", nullptr },
    { 0x00002000u, 0, "WS_EX_RTLREADING", nullptr },
    { 0x00004000u, 0, "WS_EX_LEFTSCROLLBAR", nullptr },
    { 0x00010000u, 0, "WS_EX_CONTROLPARENT", nullptr },
    { 0x00020000u, 0, "WS_EX_STATICEDGE", nullptr },
    { 0x00040000u, 0, "WS_EX_APPWINDOW", nullptr },
    { 0x00080000u, 0, "WS_EX_LAYERED", nullptr },
    { 0x00100000u, 0, "WS_EX_NOINHERITLAYOUT", nullptr },
    { 0x00200000u, 0, "WS_EX_NOREDIRECTIONBITMAP", nullptr },
    { 0x00400000u, 0, "WS_EX_LAYOUTRTL", nullptr },
    { 0x02000000u, 0, "WS_EX_COMPOSITED", nullptr },
    { 0x08000000u, 0, "WS_EX_NOACTIVATE", nullptr },
};

const FlagFamily g_FlagFamilies[] =
{
    { L"access", L"Generic and standard access rights", L"winnt.h", false, k_Access, ARRAYSIZE(k_Access) },
    { L"file", L"File access rights", L"winnt.h", false, k_File, ARRAYSIZE(k_File) },
    { L"key", L"Registry key access rights", L"winnt.h", false, k_Key, ARRAYSIZE(k_Key) },
    { L"process", L"Process access rights", L"winnt.h", false, k_Process, ARRAYSIZE(k_Process) },
    { L"thread", L"Thread access rights", L"winnt.h", false, k_Thread, ARRAYSIZE(k_Thread) },
    { L"page", L"Page protections", L"winnt.h", false, k_Page, ARRAYSIZE(k_Page) },
    { L"mem", L"Memory allocation types and states", L"winnt.h", false, k_Mem, ARRAYSIZE(k_Mem) },
    { L"share", L"File sharing modes", L"winnt.h", false, k_Share, ARRAYSIZE(k_Share) },
    { L"disposition", L"CreateFile dispositions", L"fileapi.h", true, k_Disposition, ARRAYSIZE(k_Disposition) },
    { L"fileflags", L"File attributes and flags", L"winnt.h", false, k_Fileflags, ARRAYSIZE(k_Fileflags) },
    { L"style", L"Window styles", L"winuser.h", false, k_Style, ARRAYSIZE(k_Style) },
    { L"exstyle", L"Extended window styles", L"winuser.h", false, k_Exstyle, ARRAYSIZE(k_Exstyle) },
};

const size_t g_FlagFamilyCount = ARRAYSIZE(g_FlagFamilies);
