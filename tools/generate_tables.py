#!/usr/bin/env python3
"""Generate WinCode's lookup tables from the Windows SDK headers.

Windows has no API that turns an error code into its symbolic name: the names are
preprocessor macros and do not exist at run time. So they come from the
authoritative headers instead of being transcribed by hand.

Names are picked out of the headers here, but their values are not parsed here.
A small program that #includes the real headers is compiled and run, and prints
every value. The compiler therefore evaluates expressions such as
HRESULT_FROM_WIN32(ERROR_NOT_FOUND), and takes whichever half of an #if is
actually live (E_INVALIDARG is defined in both), which a script reading the text
cannot do reliably. The same program captures each code's English message text,
which the tool falls back to when the Windows it runs on has none.

Window messages also come from tools/wm_messages.txt, a list that includes
messages missing from the headers. Anything only found there is marked
undocumented.

Run from an x64 developer shell, so that cl and the Windows SDK are on the path:

    python tools/generate_tables.py

The generated files in Core/Tables/ are committed, so building needs nothing
beyond what Visual Studio already has.
"""

import collections
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT_DIR = os.path.join(ROOT, "Core", "Tables")
WM_LIST = os.path.join(ROOT, "tools", "wm_messages.txt")

# Where the probe asks FormatMessage for text
TEXT_NONE = 0
TEXT_SYSTEM = 1
TEXT_NTDLL = 2

# key, file, brief, source, text source
TABLES = [
    ("WinError", "WinErrors.g.cpp", "Win32 error codes", "winerror.h", TEXT_SYSTEM),
    ("HResult", "HResults.g.cpp", "HRESULT codes", "winerror.h", TEXT_SYSTEM),
    ("HResultFacility", "HResultFacilities.g.cpp", "HRESULT facility codes", "winerror.h", TEXT_NONE),
    ("NtStatus", "NtStatus.g.cpp", "NTSTATUS codes", "ntstatus.h", TEXT_NTDLL),
    ("NtFacility", "NtFacilities.g.cpp", "NTSTATUS facility codes", "ntstatus.h", TEXT_NONE),
    ("BugCheck", "BugChecks.g.cpp", "Bugcheck codes", "bugcodes.h", TEXT_NONE),
    ("WindowMessage", "WindowMessages.g.cpp", "Window messages",
     "winuser.h and tools/wm_messages.txt", TEXT_NONE),
    ("HResultWindowsUpdate", "HResultsWindowsUpdate.g.cpp", "Windows Update HRESULTs", "wuerror.h", TEXT_SYSTEM),
    ("HResultAudio", "HResultsAudio.g.cpp", "Audio client HRESULTs", "audioclient.h", TEXT_NONE),
    ("HResultShadowCopy", "HResultsShadowCopy.g.cpp", "Volume Shadow Copy HRESULTs", "vsserror.h", TEXT_SYSTEM),
    ("HResultClr", "HResultsClr.g.cpp", ".NET runtime (CLR) HRESULTs",
     "corerror.h from the .NET Framework SDK", TEXT_NONE),
    ("DeviceProblem", "DeviceProblems.g.cpp", "Device Manager problem codes", "cfg.h", TEXT_NONE),
    ("UsbStatus", "UsbStatus.g.cpp", "USB request status codes", "usb.h", TEXT_NONE),
    ("LdapError", "LdapErrors.g.cpp", "LDAP result codes", "winldap.h", TEXT_NONE),
    ("IoctlSdk", "IoctlsSdk.g.cpp", "Named IOCTLs and FSCTLs", "winioctl.h and the SDK's other IOCTL headers",
     TEXT_NONE),
    ("IoctlWdk", "IoctlsWdk.g.cpp", "Named kernel IOCTLs and FSCTLs", "ntifs.h and the WDK's other IOCTL headers",
     TEXT_NONE),
    ("IoctlDeviceType", "IoctlDeviceTypes.g.cpp", "IOCTL device types", "devioctl.h", TEXT_NONE),
    ("PoolTag", "PoolTags.g.cpp", "Kernel pool tags", "pooltag.txt from the Debugging Tools for Windows",
     TEXT_NONE),
]

# The extra headers of HRESULTs, each into its own table so a match can say
# where it came from: (header under the SDK's include folder, table, the
# header's own wrapper macros). corerror.h isn't in the Windows SDK, so it's
# found separately.
EXTRA_HRESULT_HEADERS = [
    ("um\\wuerror.h", "HResultWindowsUpdate", ()),
    ("um\\audioclient.h", "HResultAudio", ("AUDCLNT_ERR", "AUDCLNT_SUCCESS")),
    ("um\\vsserror.h", "HResultShadowCopy", ()),
]
CLR_HEADER = ("corerror.h", "HResultClr", ("EMAKEHR", "SMAKEHR"))

# Headers whose comments carry each code's message, as the .mc file had it,
# for codes Windows itself has no text for
COMMENT_TEXT_TABLES = {"HResultWindowsUpdate", "HResultShadowCopy"}

# Kinds of their own, each from one header: Device Manager's problem codes, USB
# request statuses, and LDAP results. LDAP's are an enum rather than #defines.
DEVICE_PROBLEM_HEADER = "shared\\cfg.h"
USB_STATUS_HEADER = "shared\\usb.h"
LDAP_HEADER = "um\\winldap.h"
LDAP_ENUM = "LDAP_RETCODE"

# Flag families: values decoded into their named bits rather than looked up
# whole. Each is (key, name, source, enumerated, names, scans), where names are
# listed in order of preference, the first of several with one value being the
# one shown, and scans are (header, prefix, prefixes to leave out) for families
# too large to list. An enumerated family's values are whole values, not bits.
ACCESS_NAMES = [
    "GENERIC_READ", "GENERIC_WRITE", "GENERIC_EXECUTE", "GENERIC_ALL",
    "DELETE", "READ_CONTROL", "WRITE_DAC", "WRITE_OWNER", "SYNCHRONIZE",
    "ACCESS_SYSTEM_SECURITY", "MAXIMUM_ALLOWED",
    "STANDARD_RIGHTS_REQUIRED", "STANDARD_RIGHTS_ALL",
]
FLAG_FAMILIES = [
    ("access", "Generic and standard access rights", "winnt.h", False, ACCESS_NAMES, []),
    ("file", "File access rights", "winnt.h", False, [
        "FILE_READ_DATA", "FILE_LIST_DIRECTORY", "FILE_WRITE_DATA", "FILE_ADD_FILE",
        "FILE_APPEND_DATA", "FILE_ADD_SUBDIRECTORY", "FILE_CREATE_PIPE_INSTANCE",
        "FILE_READ_EA", "FILE_WRITE_EA", "FILE_EXECUTE", "FILE_TRAVERSE", "FILE_DELETE_CHILD",
        "FILE_READ_ATTRIBUTES", "FILE_WRITE_ATTRIBUTES",
        "FILE_GENERIC_READ", "FILE_GENERIC_WRITE", "FILE_GENERIC_EXECUTE", "FILE_ALL_ACCESS",
    ] + ACCESS_NAMES, []),
    ("key", "Registry key access rights", "winnt.h", False, [
        "KEY_QUERY_VALUE", "KEY_SET_VALUE", "KEY_CREATE_SUB_KEY", "KEY_ENUMERATE_SUB_KEYS",
        "KEY_NOTIFY", "KEY_CREATE_LINK", "KEY_WOW64_32KEY", "KEY_WOW64_64KEY",
        "KEY_READ", "KEY_EXECUTE", "KEY_WRITE", "KEY_ALL_ACCESS",
    ] + ACCESS_NAMES, []),
    ("process", "Process access rights", "winnt.h", False, [
        "PROCESS_TERMINATE", "PROCESS_CREATE_THREAD", "PROCESS_SET_SESSIONID",
        "PROCESS_VM_OPERATION", "PROCESS_VM_READ", "PROCESS_VM_WRITE", "PROCESS_DUP_HANDLE",
        "PROCESS_CREATE_PROCESS", "PROCESS_SET_QUOTA", "PROCESS_SET_INFORMATION",
        "PROCESS_QUERY_INFORMATION", "PROCESS_SUSPEND_RESUME", "PROCESS_QUERY_LIMITED_INFORMATION",
        "PROCESS_SET_LIMITED_INFORMATION", "PROCESS_ALL_ACCESS",
    ] + ACCESS_NAMES, []),
    ("thread", "Thread access rights", "winnt.h", False, [
        "THREAD_TERMINATE", "THREAD_SUSPEND_RESUME", "THREAD_GET_CONTEXT", "THREAD_SET_CONTEXT",
        "THREAD_QUERY_INFORMATION", "THREAD_SET_INFORMATION", "THREAD_SET_THREAD_TOKEN",
        "THREAD_IMPERSONATE", "THREAD_DIRECT_IMPERSONATION", "THREAD_SET_LIMITED_INFORMATION",
        "THREAD_QUERY_LIMITED_INFORMATION", "THREAD_RESUME", "THREAD_ALL_ACCESS",
    ] + ACCESS_NAMES, []),
    ("page", "Page protections", "winnt.h", False, [
        "PAGE_NOACCESS", "PAGE_READONLY", "PAGE_READWRITE", "PAGE_WRITECOPY", "PAGE_EXECUTE",
        "PAGE_EXECUTE_READ", "PAGE_EXECUTE_READWRITE", "PAGE_EXECUTE_WRITECOPY", "PAGE_GUARD",
        "PAGE_NOCACHE", "PAGE_WRITECOMBINE", "PAGE_TARGETS_INVALID", "PAGE_TARGETS_NO_UPDATE",
        "PAGE_ENCLAVE_UNVALIDATED", "PAGE_REVERT_TO_FILE_MAP", "PAGE_ENCLAVE_THREAD_CONTROL",
    ], []),
    ("mem", "Memory allocation types and states", "winnt.h", False, [
        "MEM_COMMIT", "MEM_RESERVE", "MEM_REPLACE_PLACEHOLDER", "MEM_RESERVE_PLACEHOLDER",
        "MEM_RESET", "MEM_TOP_DOWN", "MEM_WRITE_WATCH", "MEM_PHYSICAL", "MEM_ROTATE",
        "MEM_DIFFERENT_IMAGE_BASE_OK", "MEM_RESET_UNDO", "MEM_LARGE_PAGES", "MEM_4MB_PAGES",
        "MEM_64K_PAGES", "MEM_UNMAP_WITH_TRANSIENT_BOOST", "MEM_COALESCE_PLACEHOLDERS",
        "MEM_PRESERVE_PLACEHOLDER", "MEM_DECOMMIT", "MEM_RELEASE", "MEM_FREE", "MEM_PRIVATE",
        "MEM_MAPPED", "MEM_IMAGE",
    ], []),
    ("share", "File sharing modes", "winnt.h", False, [
        "FILE_SHARE_READ", "FILE_SHARE_WRITE", "FILE_SHARE_DELETE",
    ], []),
    ("disposition", "CreateFile dispositions", "fileapi.h", True, [
        "CREATE_NEW", "CREATE_ALWAYS", "OPEN_EXISTING", "OPEN_ALWAYS", "TRUNCATE_EXISTING",
    ], []),
    ("fileflags", "File attributes and flags", "winnt.h", False, [], [
        ("um\\winnt.h", "FILE_ATTRIBUTE_", ()),
        ("um\\winbase.h", "FILE_FLAG_", ()),
    ]),
    # The names a top-level window's styles go by come first, so they're the
    # ones shown where two share a bit (WS_MAXIMIZEBOX over WS_TABSTOP)
    ("style", "Window styles", "winuser.h", False, [
        "WS_OVERLAPPED", "WS_POPUP", "WS_CHILD", "WS_MINIMIZE", "WS_VISIBLE", "WS_DISABLED",
        "WS_CLIPSIBLINGS", "WS_CLIPCHILDREN", "WS_MAXIMIZE", "WS_CAPTION", "WS_BORDER",
        "WS_DLGFRAME", "WS_VSCROLL", "WS_HSCROLL", "WS_SYSMENU", "WS_THICKFRAME",
        "WS_MINIMIZEBOX", "WS_MAXIMIZEBOX", "WS_OVERLAPPEDWINDOW", "WS_POPUPWINDOW",
    ], [
        ("um\\winuser.h", "WS_", ("WS_EX_", "WS_ACTIVECAPTION")),
    ]),
    ("exstyle", "Extended window styles", "winuser.h", False, [], [
        ("um\\winuser.h", "WS_EX_", ()),
    ]),
]
# IOCTLs come from every header that uses CTL_CODE: the SDK's shared and um
# headers, then the WDK's km ones. The order decides which header a name is
# credited to when several define it.
IOCTL_SDK_DIRS = ("shared", "um")
IOCTL_WDK_DIR = "km"
IOCTL_NAME = re.compile(r"^(?:IOCTL|FSCTL)_")
DEVICE_TYPE_HEADER = "shared\\devioctl.h"
FUNCTION_DEFINE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)\(([^)]*)\)\s*(.*)$")
CASTS = re.compile(r"\(\s*(?:ULONG|DWORD|LONG|UINT|USHORT|ULONG32|UINT32|ULONG_PTR|DEVICE_TYPE|"
                   r"unsigned\s+long|unsigned\s+int|unsigned|int|long)\s*\)")

FLAGS_FILE = "Flags.g.cpp"

# Kernel pool tags, from the list the debugger's !pool uses
POOL_TAGS = "Debuggers\\x64\\triage\\pooltag.txt"
TEXT_SOURCE = {key: text for key, _, _, _, text in TABLES}

# Where several names share a value, the one that is actually meant. Otherwise
# the first definition in the header wins.
PREFERRED = {
    ("WinError", 0x00000000): "ERROR_SUCCESS",
    ("HResult", 0x00000000): "S_OK",
    ("NtStatus", 0x00000000): "STATUS_SUCCESS",
}

HEX = r"0x[0-9A-Fa-f]+[uUlL]*"
DEC = r"\d+[uUlL]*"
NUMBER = r"(?:%s|%s)" % (HEX, DEC)
IDENTIFIER = r"[A-Za-z_]\w*"

# Range markers and masks: real macros, but not values anything returns
MARKER = re.compile(r"_(FIRST|LAST|MIN|MAX|MASK|MSGMAX|MSGMAX_OLD)$")

# Window message markers often have no underscore (WM_KEYFIRST, WM_AFXLAST).
# Only for messages: elsewhere the looser pattern catches real codes such as
# ERROR_LAST_ADMIN.
WM_MARKER = re.compile(r"(FIRST|LAST)$")

# bugcodes.h also carries the IDs of the text on the bugcheck screen
BUGCHECK_SCREEN_TEXT = re.compile(r"(_STRING|_STRING_PLURAL|_BANNER)$")

# HRESULT names that winerror.h writes as a bare number rather than a typed one
BARE_HRESULTS = {"NOERROR", "NTE_OP_OK"}

# Plain numbers in winerror.h, ntstatus.h and bugcodes.h without the L that codes
# carry are range bases, severities and modifiers, not codes
NOT_A_CODE = "base, severity or modifier (a plain number, not a typed code)"

DEFINE = re.compile(r"^\s*#\s*define\s+(%s)\b(?!\()\s+(.+)$" % IDENTIFIER)

ALIAS = "alias"

LICENCE = """    This file is part of WinCode.

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
"""


def read_defines(path):
    """Every object-like #define in a header, as (name, value text), in order."""
    defines = []
    pending = ""
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line.endswith("\\"):
                pending += line[:-1] + " "
                continue
            line = pending + line
            pending = ""
            m = DEFINE.match(line)
            if not m:
                continue
            value = re.sub(r"/\*.*?\*/", "", m.group(2))
            value = re.sub(r"//.*", "", value).strip()
            if value:
                defines.append((m.group(1), value))
    return defines


def classify_winerror(name, value):
    if MARKER.search(name):
        return None, "range marker or mask"
    if name.startswith("SEVERITY_") or name == "FACILITY_NT_BIT":
        return None, "bit field, not a code"
    if name.startswith("FACILITY_") and re.fullmatch(DEC, value):
        return "HResultFacility", None
    if name in BARE_HRESULTS and re.fullmatch(NUMBER, value):
        return "HResult", None
    if (re.fullmatch(r"_(?:HRESULT|NDIS_ERROR)_TYPEDEF_\(\s*%s\s*\)" % NUMBER, value) or
            re.fullmatch(r"\(\(HRESULT\)\s*%s\s*\)" % NUMBER, value) or
            re.fullmatch(r"HRESULT_FROM_WIN32\(\s*%s\s*\)" % r"\w+", value)):
        return "HResult", None
    if re.fullmatch(r"\d+L", value):
        return "WinError", None
    if re.fullmatch(IDENTIFIER, value):
        return ALIAS, None
    if re.fullmatch(NUMBER, value):
        return None, NOT_A_CODE
    return None, "unrecognised"


def classify_ntstatus(name, value):
    if MARKER.search(name):
        return None, "range marker or mask"
    if name.startswith("FACILITY_") and re.fullmatch(HEX, value):
        return "NtFacility", None
    if re.fullmatch(r"\(\(NTSTATUS\)\s*%s\s*\)" % NUMBER, value):
        return "NtStatus", None
    if re.fullmatch(IDENTIFIER, value):
        return ALIAS, None
    if re.fullmatch(NUMBER, value):
        return None, NOT_A_CODE
    return None, "unrecognised"


def classify_bugcodes(name, value):
    if BUGCHECK_SCREEN_TEXT.search(name):
        return None, "bugcheck screen text ID, not a code"
    if re.fullmatch(r"\(\(ULONG\)\s*%s\s*\)" % NUMBER, value):
        return "BugCheck", None
    if re.fullmatch(IDENTIFIER, value):
        return ALIAS, None
    if re.fullmatch(NUMBER, value):
        return None, NOT_A_CODE
    return None, "unrecognised"


def make_classify_winuser(listed):
    """winuser.h has thousands of defines; only messages are wanted. Every WM_
    name is one, and other prefixes (CB_, EM_, LB_...) count only when the
    message list agrees, since those prefixes also name return codes."""

    def classify(name, value):
        if not (name.startswith("WM_") or name in listed):
            return None, None
        if MARKER.search(name) or WM_MARKER.search(name):
            return None, "range marker or mask"
        if re.fullmatch(HEX, value):
            return "WindowMessage", None
        if re.fullmatch(IDENTIFIER, value):
            return ALIAS, None
        return None, "unrecognised"

    return classify


def make_classify_hresults(table, macros):
    """Classifies a header of HRESULTs that all go into one table. macros are
    the header's own wrappers, as in AUDCLNT_ERR(n). Its other macros aren't
    codes, and aren't worth reporting as skipped."""
    wrapped = re.compile(r"(?:%s)\(\s*%s\s*\)" % ("|".join(macros), NUMBER)) if macros else None

    def classify(name, value):
        if MARKER.search(name):
            return None, "range marker or mask"
        if (re.fullmatch(r"_HRESULT_TYPEDEF_\(\s*%s\s*\)" % NUMBER, value) or
                re.fullmatch(r"\(\(HRESULT\)\s*%s\s*\)" % NUMBER, value) or
                (wrapped is not None and wrapped.fullmatch(value))):
            return table, None
        return None, None

    return classify


def read_comment_texts(path):
    """The message text in a header's comments, as message compiler output
    leaves it: name -> text.

        // MessageId: VSS_E_BAD_STATE
        //
        // MessageText:
        //
        // A method call was made when the object was in an incorrect state
    """
    texts = {}
    name = None
    lines = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            m = re.match(r"//\s*MessageId:\s*(%s)" % IDENTIFIER, line)
            if m:
                name, lines = m.group(1), None
                continue
            if name and re.match(r"//\s*MessageText:", line):
                lines = []
                continue
            if lines is not None and line.startswith("//"):
                lines.append(line[2:].strip())
                continue
            if lines is not None:
                text = " ".join(part for part in lines if part)
                if text:
                    texts[name] = text
                name, lines = None, None
    return texts


def find_clr_header():
    """corerror.h, from the newest .NET Framework SDK, or None."""
    root = os.environ.get("NETFXSDKDir")
    candidates = []
    if root:
        candidates.append(os.path.join(root, "Include", "um", CLR_HEADER[0]))
    base = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                        "Windows Kits", "NETFXSDK")
    if os.path.isdir(base):
        for version in sorted(os.listdir(base), reverse=True):
            candidates.append(os.path.join(base, version, "Include", "um", CLR_HEADER[0]))
    for path in candidates:
        if os.path.isfile(path):
            return path
    return None


def classify_device_problem(name, value):
    if not name.startswith("CM_PROB_"):
        return None, None
    if re.fullmatch(r"\(?\s*%s\s*\)?" % NUMBER, value):
        return "DeviceProblem", None
    return None, "unrecognised"


def classify_usb_status(name, value):
    if not name.startswith("USBD_STATUS_"):
        return None, None
    if MARKER.search(name):
        return None, "range marker or mask"
    if re.fullmatch(r"\(\(USBD_STATUS\)\s*%s\s*\)" % NUMBER, value):
        return "UsbStatus", None
    return None, None


def read_enum_members(path, enum_name):
    """The members of one typedef'd enum, in order."""
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    m = re.search(r"typedef\s+enum\s*(?:\w+\s*)?\{([^{}]*)\}\s*%s\s*;" % enum_name, text)
    if not m:
        raise SystemExit("enum %s not found in %s" % (enum_name, path))
    members = []
    for line in m.group(1).splitlines():
        line = re.sub(r"//.*", "", line)
        line = re.sub(r"/\*.*?\*/", "", line)
        found = re.match(r"\s*(%s)\s*(?:=|,|$)" % IDENTIFIER, line)
        if found:
            members.append(found.group(1))
    return members


def read_trailing_comments(path, prefix):
    """The comment after each #define whose name starts with prefix, as a
    sentence: name -> text. cfg.h has no other text for its problem codes."""
    texts = {}
    pattern = re.compile(r"^\s*#\s*define\s+(%s\w*)\s+[^/]*//\s*(.+?)\s*$" % prefix)
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = pattern.match(line)
            if m:
                text = m.group(2)
                text = text[0].upper() + text[1:]
                if not text.endswith("."):
                    text += "."
                texts[m.group(1)] = text
    return texts


def read_pool_tags(path):
    """pooltag.txt: <tag> - <binary> - <description>, one tag per line, some
    tags listed more than once. Returns (name, value, text) per tag, the value
    being the tag's four bytes read as the little-endian DWORD a debugger shows
    in memory."""
    tags = collections.OrderedDict()
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if len(line) < 6 or line.startswith(("rem", "//", ";")) or line[4:6] != " -":
                continue
            tag = line[:4]
            # A quote or backslash would need escaping in the generated C++
            if not all(" " <= c <= "~" for c in tag) or not tag.strip() or '"' in tag or "\\" in tag:
                continue
            m = re.match(r"\s*-\s*(.*?)\s+-\s+(.*)$", line[4:])
            if m:
                binary, description = m.group(1).strip(), m.group(2).strip()
            else:
                binary, description = "", line[4:].lstrip(" -").strip()
            if binary and binary != "<unknown>":
                description = "%s: %s" % (binary, description)
            tags.setdefault(tag, [])
            if description and description not in tags[tag]:
                tags[tag].append(description)

    result = []
    for tag, descriptions in tags.items():
        value = int.from_bytes(tag.encode("latin-1"), "little")
        result.append((tag.rstrip(), value, "\n".join(descriptions)))
    return result


def read_all_defines(path):
    """A header's object-like and function-like #defines, as name -> value
    text and name -> (parameters, body). The first definition of a name wins."""
    objects, functions = {}, {}
    pending = ""
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line.endswith("\\"):
                pending += line[:-1] + " "
                continue
            line = pending + line
            pending = ""
            if "define" not in line:
                continue
            line = re.sub(r"/\*.*?\*/", "", line)
            line = re.sub(r"//.*", "", line)
            m = FUNCTION_DEFINE.match(line)
            if m:
                params = [p.strip() for p in m.group(2).split(",") if p.strip()]
                functions.setdefault(m.group(1), (params, m.group(3).strip()))
                continue
            m = DEFINE.match(line)
            if m and m.group(2).strip():
                objects.setdefault(m.group(1), m.group(2).strip())
    return objects, functions


def split_args(text, start):
    """The arguments of a macro call whose ( is at start, and the index after
    its closing bracket."""
    args, depth, current = [], 0, ""
    for i in range(start + 1, len(text)):
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            if depth == 0:
                if current.strip() or args:
                    args.append(current.strip())
                return args, i + 1
            depth -= 1
        elif c == "," and depth == 0:
            args.append(current.strip())
            current = ""
            continue
        current += c
    raise ValueError("a macro call with no closing bracket")


class IoctlEvaluator:
    """Works out IOCTL values from their #defines. The WDK's kernel headers
    can't go in the user-mode probe, so the generator does this itself, and
    checks it against the compiler on winioctl.h's names."""

    def __init__(self, objects, functions):
        self.objects = objects
        self.functions = functions

    def expand(self, text, depth):
        if depth > 32:
            raise ValueError("macros nested too deeply")
        out, used, i = [], False, 0
        while True:
            m = re.compile(IDENTIFIER).search(text, i)
            if not m:
                out.append(text[i:])
                break
            out.append(text[i:m.start()])
            name, i = m.group(0), m.end()

            # The tail of a number, as in 0x1F or 10UL
            if m.start() > 0 and text[m.start() - 1].isalnum():
                out.append(name)
                continue

            rest = text[i:]
            gap = len(rest) - len(rest.lstrip())
            if name in self.functions and rest[gap:gap + 1] == "(":
                args, i = split_args(text, i + gap)
                params, body = self.functions[name]
                if len(args) != len(params):
                    raise ValueError("%s given %d arguments" % (name, len(args)))
                if params:
                    body = re.sub(r"\b(%s)\b" % "|".join(map(re.escape, params)),
                                  lambda p: "(" + args[params.index(p.group(1))] + ")", body)
                expanded, inner = self.expand(body, depth + 1)
                used = used or inner or name == "CTL_CODE"
                out.append("(" + expanded + ")")
                continue

            if name in self.objects:
                expanded, inner = self.expand(self.objects[name], depth + 1)
                used = used or inner
                out.append("(" + expanded + ")")
                continue

            out.append(name)
        return "".join(out), used

    def evaluate(self, name):
        """(value, whether CTL_CODE made it), or ValueError saying why not."""
        text, used = self.expand(self.objects[name], 0)
        text = CASTS.sub("", text)
        # A character constant, as in dsm.h's 'dsm', with the first character on top
        text = re.sub(r"'([ -&(-~]{1,4})'", lambda c: str(int.from_bytes(c.group(1).encode("latin-1"), "big")), text)
        text = re.sub(r"\b(0[xX][0-9A-Fa-f]+|\d+)[uUlL]+\b", r"\1", text)
        text = re.sub(r"\b0([0-7]+)\b", r"0o\1", text)
        left = re.findall(IDENTIFIER, re.sub(r"\b0[xXoO][0-9A-Fa-f]+\b|\b\d+\b", "", text))
        if left:
            raise ValueError("uses %s" % left[0])
        if re.search(r"[^\s\w()|&^~<>+\-*%/]", text):
            raise ValueError("an expression the generator doesn't evaluate")
        try:
            value = eval(text.replace("/", "//"), {"__builtins__": {}}, {})
        except Exception:
            raise ValueError("an expression the generator doesn't evaluate")
        return value & 0xFFFFFFFF, used


def collect_ioctls(include, skipped):
    """Named IOCTLs from every header that uses CTL_CODE. Returns the SDK's
    and the WDK's as name -> (value, header), and whether the WDK is there."""
    objects, functions = {}, {}
    candidates = []
    have_wdk = os.path.isdir(os.path.join(include, IOCTL_WDK_DIR))

    for folder in IOCTL_SDK_DIRS + ((IOCTL_WDK_DIR,) if have_wdk else ()):
        root = os.path.join(include, folder)
        # Subfolders too, since NDIS keeps its IOCTL macro in shared\ndis
        paths = sorted(os.path.join(directory, file)
                       for directory, _, files in os.walk(root) for file in files)
        for path in paths:
            file = os.path.basename(path)
            if not file.lower().endswith(".h"):
                continue
            with open(path, encoding="utf-8", errors="replace") as f:
                if "CTL_CODE" not in f.read():
                    continue
            header_objects, header_functions = read_all_defines(path)
            for name, value in header_objects.items():
                objects.setdefault(name, value)
                if IOCTL_NAME.match(name) or "CTL_CODE" in value:
                    candidates.append((name, file, folder == IOCTL_WDK_DIR))
            for name, value in header_functions.items():
                functions.setdefault(name, value)

    evaluator = IoctlEvaluator(objects, functions)
    sdk, wdk = collections.OrderedDict(), collections.OrderedDict()
    for name, header, from_wdk in candidates:
        if name in sdk or name in wdk:
            continue
        try:
            value, used = evaluator.evaluate(name)
        except ValueError as e:
            skipped["IOCTL that doesn't evaluate (%s)" % re.sub(r" \w+$", "", str(e))].append(name)
            continue
        # A base or a device type, as in IOCTL_STORAGE_BASE, isn't an IOCTL
        if used:
            (wdk if from_wdk else sdk)[name] = (value, header)
    return sdk, wdk, have_wdk


def collect_device_types(include):
    """FILE_DEVICE_* from devioctl.h, as name -> value."""
    objects, _ = read_all_defines(os.path.join(include, DEVICE_TYPE_HEADER))
    types = collections.OrderedDict()
    for name, value in objects.items():
        if name.startswith("FILE_DEVICE_") and re.fullmatch(NUMBER, value):
            types[name] = int(re.sub(r"[uUlL]+$", "", value), 0)
    return types


def collect_flag_families(include):
    """Each flag family with its names, in order: the listed ones, then any a
    scan finds."""
    families = []
    for key, display, source, enumerated, names, scans in FLAG_FAMILIES:
        members = list(names)
        for header, prefix, leave_out in scans:
            for name, _ in read_defines(os.path.join(include, header)):
                if (name.startswith(prefix) and not name.startswith(tuple(leave_out)) and
                        not MARKER.search(name) and "VALID" not in name and name not in members):
                    members.append(name)
        families.append((key, display, source, enumerated, members))
    return families


def write_flags(families, values, sdk_version):
    """Core/Tables/Flags.g.cpp: one table per family, and the list of them."""
    path = os.path.join(OUT_DIR, FLAGS_FILE)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("/*!\n\n")
        f.write("    @file Core/Tables/%s\n\n" % FLAGS_FILE)
        f.write("    @brief Flag families, for decoding a value into its named bits.\n\n")
        f.write("    @details DO NOT EDIT. Regenerate with tools/generate_tables.py.\n\n")
        f.write("             Generated from Windows SDK %s. Each family is sorted by\n" % sdk_version)
        f.write("             value, with the preferred name for each value first.\n\n")
        f.write("    @copyright Copyright (c) 2026 Ged Murphy\n\n")
        f.write(LICENCE)
        f.write("\n*/\n\n")
        f.write('#include "Tables.hpp"\n\n')
        for key, _, _, _, members in families:
            rows = build_rows("Flags:" + key, [(name, values[name][0], index, "", False)
                                               for index, name in enumerate(members) if name in values])
            f.write("\nstatic const TableEntry k_%s[] =\n{\n" % key.capitalize())
            for value, flags, name, _ in rows:
                f.write('    { 0x%08Xu, %s, "%s", nullptr },\n' % (value, flags, name))
            f.write("};\n")
            print("%-22s %5d names in %s" % (FLAGS_FILE, len(rows), key))
        f.write("\nconst FlagFamily g_FlagFamilies[] =\n{\n")
        for key, display, source, enumerated, _ in families:
            f.write('    { L"%s", L"%s", L"%s", %s, k_%s, ARRAYSIZE(k_%s) },\n'
                    % (key, display, source, "true" if enumerated else "false",
                       key.capitalize(), key.capitalize()))
        f.write("};\n\nconst size_t g_FlagFamilyCount = ARRAYSIZE(g_FlagFamilies);\n")


def collect(path, classify, skipped):
    """Candidate names from one header: name -> (table, order)."""
    found = {}
    aliases = []
    for order, (name, value) in enumerate(read_defines(path)):
        if name in found:
            continue  # the other half of an #if; the compiler decides which is live
        table, reason = classify(name, value)
        if table == ALIAS:
            aliases.append((order, name, value))
        elif table is not None:
            found[name] = (table, order)
        elif reason:
            skipped[reason].append(name)

    # An alias belongs to whichever table the name it stands for is in
    for order, name, value in aliases:
        if name in found:
            continue
        if value in found:
            found[name] = (found[value][0], order)
        else:
            skipped["alias of something that is not a code"].append(name)
    return found


def read_wm_list(path):
    """name -> value from wm_messages.txt. 0x means hex, anything else decimal."""
    listed = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            parts = line.split()
            if len(parts) < 3 or parts[0] != "#define":
                continue
            text = parts[2]
            listed[parts[1]] = int(text, 16) if text.lower().startswith("0x") else int(text, 10)
    return listed


PROBE_HEAD = r"""
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <bugcodes.h>
#include <stdint.h>
#include <stdio.h>

struct Probe
{
    uint32_t Value;
    const char* Name;
    int Text;
};

static const Probe g_Probes[] =
{
"""

PROBE_TAIL = r"""
};

static WCHAR g_Buffer[65536];
static char g_Utf8[262144];

//
// English text if Windows has it, otherwise whatever it has.
//
static bool Format(DWORD Flags, HMODULE Module, DWORD Id)
{
    const DWORD languages[] = { MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), 0 };
    for (DWORD language : languages)
    {
        if (FormatMessageW(Flags | FORMAT_MESSAGE_IGNORE_INSERTS, Module, Id, language,
                           g_Buffer, ARRAYSIZE(g_Buffer), nullptr) != 0)
        {
            return true;
        }
    }
    return false;
}

int wmain(int ArgCount, wchar_t** Args)
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    FILE* out;

    if (ArgCount != 2 || _wfopen_s(&out, Args[1], L"wb") != 0)
    {
        return 1;
    }

    for (const Probe& probe : g_Probes)
    {
        bool found = false;
        if (probe.Text == 1) found = Format(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, probe.Value);
        if (probe.Text == 2) found = Format(FORMAT_MESSAGE_FROM_HMODULE, ntdll, probe.Value);

        fprintf(out, "%s\t%08X\t", probe.Name, probe.Value);
        if (found &&
            WideCharToMultiByte(CP_UTF8, 0, g_Buffer, -1, g_Utf8, sizeof(g_Utf8), nullptr, nullptr) != 0)
        {
            for (const char* c = g_Utf8; *c != '\0'; c++)
            {
                if (*c == '\r') continue;
                if (*c == '\n') { fputs("\\n", out); continue; }
                if (*c == '\t') { fputs("\\t", out); continue; }
                if (*c == '\\') { fputs("\\\\", out); continue; }
                fputc(*c, out);
            }
        }
        fputc('\n', out);
    }

    fclose(out);
    return 0;
}
"""


def run_probe(names, skipped, includes, include_dirs):
    """Compile and run the probe. names: name -> text source. includes are the
    extra headers it needs, and include_dirs where to find any outside the SDK.
    Returns name -> (value, text)."""
    head = PROBE_HEAD.replace("#include <bugcodes.h>\n",
                              "#include <bugcodes.h>\n" + "".join("#include <%s>\n" % h for h in includes))
    work = tempfile.mkdtemp(prefix="wincode-probe-")
    source = os.path.join(work, "probe.cpp")
    remaining = dict(names)

    for attempt in range(10):
        line_of = {}
        with open(source, "w", encoding="utf-8", newline="\n") as f:
            f.write(head)
            line = head.count("\n") + 1
            for name, text in remaining.items():
                f.write("    { (uint32_t)(%s), \"%s\", %d },\n" % (name, name, text))
                line_of[line] = name
                line += 1
            f.write(PROBE_TAIL)

        result = subprocess.run(
            ["cl", "/nologo", "/EHsc", "/std:c++20", "/utf-8", "/W0", "/DUNICODE", "/D_UNICODE"] +
            ["/I" + d for d in include_dirs] +
            ["probe.cpp", "/Fe:probe.exe", "/link", "user32.lib"],
            cwd=work, capture_output=True, text=True, errors="replace")
        if result.returncode == 0:
            break

        # Drop whatever wouldn't compile, typically a name only defined under an
        # #if that isn't live, and try again
        bad = {line_of[int(n)] for n in re.findall(r"probe\.cpp\((\d+)\)[^:]*: (?:fatal )?error", result.stdout)
               if int(n) in line_of}
        if not bad:
            print(result.stdout)
            raise SystemExit("probe failed to compile for a reason other than a bad name")
        for name in bad:
            del remaining[name]
            skipped["does not compile, e.g. defined under an inactive #if"].append(name)
    else:
        raise SystemExit("probe still failing after 10 attempts")

    output = os.path.join(work, "probe.txt")
    subprocess.run([os.path.join(work, "probe.exe"), output], check=True)

    values = {}
    with open(output, encoding="utf-8", errors="replace") as f:
        for line in f:
            name, value, text = line.rstrip("\n").split("\t", 2)
            text = re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t"}.get(m.group(1), m.group(1)), text).strip()

            # ntdll's message table often holds nothing but the symbolic name,
            # which tells the reader nothing the name column doesn't
            if re.fullmatch(IDENTIFIER, text):
                text = ""
            values[name] = (int(value, 16), text)

    shutil.rmtree(work, ignore_errors=True)
    return values


def c_string(text):
    out = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif ord(ch) < 0x20:
            out.append("\\%03o" % ord(ch))
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def build_rows(key, members):
    """members: list of (name, value, order, text, undocumented).
    Returns rows sorted by value, primary name first, as
    (value, flags, name, text or None)."""
    by_value = collections.defaultdict(list)
    for member in members:
        by_value[member[1]].append(member)

    rows = []
    for value in sorted(by_value):
        group = by_value[value]
        preferred = PREFERRED.get((key, value))
        group.sort(key=lambda m: (m[0] != preferred, m[2]))
        text = next((m[3] for m in group if m[3]), "")
        for index, (name, _, _, _, undocumented) in enumerate(group):
            flags = []
            if index > 0:
                flags.append("k_TableEntryAlias")
            if undocumented:
                flags.append("k_TableEntryUndocumented")
            rows.append((value, " | ".join(flags) or "0", name, text if index == 0 and text else None))
    return rows


def write_table(key, file, brief, source, sdk_version, rows, headers=None):
    path = os.path.join(OUT_DIR, file)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("/*!\n\n")
        f.write("    @file Core/Tables/%s\n\n" % file)
        f.write("    @brief %s, generated from %s.\n\n" % (brief, source))
        f.write("    @details DO NOT EDIT. Regenerate with tools/generate_tables.py.\n\n")
        f.write("             Generated from Windows SDK %s. Sorted by value, with\n" % sdk_version)
        f.write("             the primary name for each value first, so a lookup can\n")
        f.write("             binary search to it and walk forward over the aliases.\n\n")
        f.write("    @copyright Copyright (c) 2026 Ged Murphy\n\n")
        f.write(LICENCE)
        f.write("\n*/\n\n")
        f.write('#include "Tables.hpp"\n\n\n')
        f.write("static const TableEntry g_Entries[] =\n{\n")
        for value, flags, name, text in rows:
            f.write('    { 0x%08Xu, %s, "%s", %s },\n'
                    % (value, flags, name, c_string(text) if text else "nullptr"))
        f.write("};\n\n")
        if headers is None:
            f.write("const TableSpan g_%sTable =\n{\n    g_Entries,\n    ARRAYSIZE(g_Entries),\n    L\"%s\"\n};\n"
                    % (key, source.split()[0]))
            return path
        f.write("static const PCWSTR g_Headers[] =\n{\n")
        for _, _, name, _ in rows:
            f.write('    L"%s",\n' % headers[name])
        f.write("};\n\n")
        f.write("const TableSpan g_%sTable =\n{\n    g_Entries,\n    ARRAYSIZE(g_Entries),\n    L\"%s\",\n    g_Headers\n};\n"
                % (key, source.split()[0]))
    return path


def write_info(sdk_version):
    """The SDK the tables came from, for showing beside every match."""
    path = os.path.join(OUT_DIR, "TablesInfo.g.cpp")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("/*!\n\n")
        f.write("    @file Core/Tables/TablesInfo.g.cpp\n\n")
        f.write("    @brief Which Windows SDK the tables were generated from.\n\n")
        f.write("    @details DO NOT EDIT. Regenerate with tools/generate_tables.py.\n\n")
        f.write("    @copyright Copyright (c) 2026 Ged Murphy\n\n")
        f.write(LICENCE)
        f.write("\n*/\n\n")
        f.write('#include "Tables.hpp"\n\n\n')
        f.write('const WCHAR g_TablesSdkVersion[] = L"%s";\n' % sdk_version)
    return path


def main():
    sdk_dir = os.environ.get("WindowsSdkDir")
    sdk_version = os.environ.get("WindowsSDKVersion", "").strip("\\")
    if not sdk_dir or not sdk_version or not shutil.which("cl"):
        print("Run this from an x64 developer shell, so that cl and the Windows SDK are on the path.")
        return 1
    include = os.path.join(sdk_dir, "Include", sdk_version)

    skipped = collections.defaultdict(list)
    listed = read_wm_list(WM_LIST)

    found = {}
    for header, classify in (("shared\\winerror.h", classify_winerror),
                             ("shared\\ntstatus.h", classify_ntstatus),
                             ("shared\\bugcodes.h", classify_bugcodes),
                             ("um\\winuser.h", make_classify_winuser(listed))):
        for name, entry in collect(os.path.join(include, header), classify, skipped).items():
            found.setdefault(name, entry)

    includes = []
    include_dirs = []
    comment_texts = {}
    for header, table, macros in EXTRA_HRESULT_HEADERS:
        path = os.path.join(include, header)
        includes.append(os.path.basename(path))
        for name, entry in collect(path, make_classify_hresults(table, macros), skipped).items():
            found.setdefault(name, entry)
        if table in COMMENT_TEXT_TABLES:
            comment_texts.update(read_comment_texts(path))

    # Without the .NET Framework SDK the CLR table is left as it was
    clr = find_clr_header()
    if clr:
        includes.append(CLR_HEADER[0])
        include_dirs.append(os.path.dirname(clr))
        for name, entry in collect(clr, make_classify_hresults(CLR_HEADER[1], CLR_HEADER[2]), skipped).items():
            found.setdefault(name, entry)
    else:
        print("warning: corerror.h not found (install the .NET Framework SDK); %s is left as it was"
              % [t[1] for t in TABLES if t[0] == CLR_HEADER[1]][0])

    path = os.path.join(include, DEVICE_PROBLEM_HEADER)
    includes.append("cfg.h")
    for name, entry in collect(path, classify_device_problem, skipped).items():
        found.setdefault(name, entry)
    comment_texts.update(read_trailing_comments(path, "CM_PROB_"))

    path = os.path.join(include, USB_STATUS_HEADER)
    includes.append("usb.h")
    for name, entry in collect(path, classify_usb_status, skipped).items():
        found.setdefault(name, entry)

    path = os.path.join(include, LDAP_HEADER)
    includes.append("winldap.h")
    for order, name in enumerate(read_enum_members(path, LDAP_ENUM)):
        found.setdefault(name, ("LdapError", order))

    sdk_ioctls, wdk_ioctls, have_wdk = collect_ioctls(include, skipped)
    if not have_wdk:
        print("warning: %s not found (install the WDK); IoctlsWdk.g.cpp is left as it was"
              % os.path.join(include, IOCTL_WDK_DIR))
    ioctl_headers = {}
    winioctl_names = [name for name, (_, header) in sdk_ioctls.items() if header == "winioctl.h"]
    includes.append("winioctl.h")

    flag_families = collect_flag_families(include)
    probe_names = {name: TEXT_SOURCE[table] for name, (table, _) in found.items()}
    for name in winioctl_names:
        probe_names.setdefault(name, TEXT_NONE)
    for _, _, _, _, members in flag_families:
        for name in members:
            probe_names.setdefault(name, TEXT_NONE)

    values = run_probe(probe_names, skipped, includes, include_dirs)

    # The generator works IOCTLs out itself, so check it against the compiler
    # wherever the compiler can see them
    wrong = ["%s (0x%X, not 0x%X)" % (name, sdk_ioctls[name][0], values[name][0])
             for name in winioctl_names if name in values and values[name][0] != sdk_ioctls[name][0]]
    if wrong:
        raise SystemExit("IOCTL values disagree with the compiler: " + ", ".join(wrong))
    print("IOCTLs   %d checked against the compiler" % sum(1 for name in winioctl_names if name in values))

    members = collections.defaultdict(list)
    for name, (table, order) in found.items():
        if name in values:
            value, text = values[name]
            if not text:
                text = comment_texts.get(name, "")
            members[table].append((name, value, order, text, False))

    # Messages only the list knows about. The header wins any disagreement, but a
    # header name that didn't compile in this configuration still has the list's
    # value to fall back on.
    header_messages = {name for name, (table, _) in found.items() if table == "WindowMessage"}
    for index, (name, value) in enumerate(listed.items()):
        if name in header_messages and name in values:
            if values[name][0] != value:
                print("warning: %s is 0x%X in winuser.h but 0x%X in wm_messages.txt; using the header"
                      % (name, values[name][0], value))
            continue
        if MARKER.search(name) or WM_MARKER.search(name):
            skipped["range marker or mask"].append(name)
            continue
        members["WindowMessage"].append((name, value, 1000000 + index, "", True))

    # Pool tags come from a list rather than a header, so need no probe. Without
    # the Debugging Tools the table is left as it was.
    pool_tags = os.path.join(sdk_dir, POOL_TAGS)
    have_pool_tags = os.path.isfile(pool_tags)
    if have_pool_tags:
        for index, (name, value, text) in enumerate(read_pool_tags(pool_tags)):
            members["PoolTag"].append((name, value, index, text, False))
    else:
        print("warning: %s not found (install the Debugging Tools for Windows); PoolTags.g.cpp is left as it was"
              % pool_tags)

    for key, ioctls in (("IoctlSdk", sdk_ioctls), ("IoctlWdk", wdk_ioctls)):
        for index, (name, (value, header)) in enumerate(ioctls.items()):
            members[key].append((name, value, index, "", False))
            ioctl_headers[name] = header
    for index, (name, value) in enumerate(collect_device_types(include).items()):
        members["IoctlDeviceType"].append((name, value, index, "", False))

    os.makedirs(OUT_DIR, exist_ok=True)
    print("SDK      %s" % include)
    for key, file, brief, source, _ in TABLES:
        if ((key == CLR_HEADER[1] and not clr) or (key == "PoolTag" and not have_pool_tags) or
                (key == "IoctlWdk" and not have_wdk)):
            continue
        rows = build_rows(key, members[key])
        write_table(key, file, brief, source, sdk_version, rows,
                    ioctl_headers if key in ("IoctlSdk", "IoctlWdk") else None)
        primaries = sum(1 for r in rows if "Alias" not in r[1])
        texts = sum(1 for r in rows if r[3])
        print("%-22s %5d names, %5d values, %5d with text" % (file, len(rows), primaries, texts))

    write_flags(flag_families, values, sdk_version)
    write_info(sdk_version)

    print("skipped")
    for reason, names in sorted(skipped.items()):
        shown = names if reason.startswith(("unrecognised", "does not compile", "alias")) else names[:6]
        print("  %4d  %s: %s%s" % (len(names), reason, ", ".join(shown), " ..." if len(shown) < len(names) else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
