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
]
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


def run_probe(names, skipped):
    """Compile and run the probe. names: name -> text source.
    Returns name -> (value, text)."""
    work = tempfile.mkdtemp(prefix="wincode-probe-")
    source = os.path.join(work, "probe.cpp")
    remaining = dict(names)

    for attempt in range(10):
        line_of = {}
        with open(source, "w", encoding="utf-8", newline="\n") as f:
            f.write(PROBE_HEAD)
            line = PROBE_HEAD.count("\n") + 1
            for name, text in remaining.items():
                f.write("    { (uint32_t)(%s), \"%s\", %d },\n" % (name, name, text))
                line_of[line] = name
                line += 1
            f.write(PROBE_TAIL)

        result = subprocess.run(
            ["cl", "/nologo", "/EHsc", "/std:c++20", "/utf-8", "/W0", "/DUNICODE", "/D_UNICODE",
             "probe.cpp", "/Fe:probe.exe", "/link", "user32.lib"],
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


def write_table(key, file, brief, source, sdk_version, rows):
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
        f.write("const TableSpan g_%sTable =\n{\n    g_Entries,\n    ARRAYSIZE(g_Entries),\n    L\"%s\"\n};\n"
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

    values = run_probe({name: TEXT_SOURCE[table] for name, (table, _) in found.items()}, skipped)

    members = collections.defaultdict(list)
    for name, (table, order) in found.items():
        if name in values:
            value, text = values[name]
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

    os.makedirs(OUT_DIR, exist_ok=True)
    print("SDK      %s" % include)
    for key, file, brief, source, _ in TABLES:
        rows = build_rows(key, members[key])
        write_table(key, file, brief, source, sdk_version, rows)
        primaries = sum(1 for r in rows if "Alias" not in r[1])
        texts = sum(1 for r in rows if r[3])
        print("%-22s %5d names, %5d values, %5d with text" % (file, len(rows), primaries, texts))

    write_info(sdk_version)

    print("skipped")
    for reason, names in sorted(skipped.items()):
        shown = names if reason.startswith(("unrecognised", "does not compile", "alias")) else names[:6]
        print("  %4d  %s: %s%s" % (len(names), reason, ", ".join(shown), " ..." if len(shown) < len(names) else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
