# WinCode — plan

WinCode was called Message Translator until the rewrite; see open question 13.

Status: **agreed. Phases 0 and 1 are done. Phase 2 is under way: reverse mapping, a first working
window (step 2a), the drawn details pane with links and history (step 2b), the launcher popup
with its Win+Shift+E hotkey (step 3), and the tray, settings.json, the settings dialog and
starting at sign-in (step 4), and the per-user Inno Setup installer and zip (step 5, x64 only)
are done. Steps 3 to 5 are waiting for review, and for testing in the VM: the elevated WinDbg
focus test, and installing, upgrading and uninstalling. After that, phase 2 is complete.** ARM64
and CI are set up but not yet verified: the ARM64 build tools
aren't installed on the dev box, and CI won't run until this repo is pushed.

## Purpose

Look up Windows error codes and related values while debugging or reading logs, in both
user mode and kernel mode. It will be native C++ with no .NET, run on Windows 10/11, and ship
as a single exe.

## Goals

- Paste anything that looks like a code and get every interpretation of it, straight away.
- Treat kernel mode as fully as user mode: NTSTATUS, bugchecks and driver values, not
  only Win32/HRESULT.
- Be quick to reach while debugging: a hotkey, the clipboard, the CLI, WinDbg.
- Accept codes in the messy forms they take in logs.
- Table data comes from the real SDK/WDK headers, generated rather than hand-maintained.

## Non-goals

- ReactOS: JIRA lookup, branding, registry path, reactos.org links. All removed.
- Windows versions before 10.
- A faithful copy of the old UI.

## Features

### 1. Lookup engine

- **Input parsing:** decimal; hex with or without `0x`; negative decimals (`-1073741819`);
  sign-extended 64-bit values (`0xFFFFFFFF80070057`); C suffixes (`0x80004005L`); codes inside
  text (`(HRESULT: 0x80070005)`, `Status=0xC0000022`).
- **Every interpretation of a value** shown together: Win32, HRESULT, NTSTATUS, bugcheck and so on.
- **Name search:** exact, prefix or substring across all tables (`ACCESS_DENIED` finds
  `ERROR_ACCESS_DENIED`, `STATUS_ACCESS_DENIED`, `E_ACCESSDENIED`, ...).
- **Cross-references:**
  - An NTSTATUS and the Win32 error it maps to (`RtlNtStatusToDosError`).
  - The reverse: every NTSTATUS that maps to a given Win32 error. It's many to one, and no
    other tool shows it. Built by running every NTSTATUS in the table through
    `RtlNtStatusToDosError` once.
  - `HRESULT_FROM_WIN32`/`HRESULT_FROM_NT` in both directions: looking up Win32 `5` also offers
    `0x80070005`.
  - Severity, customer bit and facility decoded, with facility names for both the HRESULT and
    NTSTATUS facility sets.
- **Message text** from the running OS: `FormatMessage`, and ntdll's message table for NTSTATUS.

### 2. Data sources

| Source | Values | Header | Needs |
|---|---|---|---|
| Win32 errors (includes WSA*) | `ERROR_*`, `WSAE*` | winerror.h | SDK |
| HRESULTs (includes DXGI etc.) | `E_*`, `*_E_*`, `*_S_*` | winerror.h | SDK |
| HRESULT facilities | `FACILITY_*` | winerror.h | SDK |
| NTSTATUS, NT facilities | `STATUS_*`, `DBG_*`, `FACILITY_*` | ntstatus.h | SDK |
| Bugchecks | codes | bugcodes.h | SDK |
| WinHTTP / WinInet | `ERROR_WINHTTP_*`, `ERROR_INTERNET_*` | winhttp.h, wininet.h | SDK |
| Config Manager / SetupAPI | `CR_*`, SetupAPI `ERROR_*` | cfgmgr32.h, setupapi.h | SDK |
| Media Foundation | `MF_E_*` | mferror.h | SDK |
| IO error log codes | `IO_ERR_*` | ntiologc.h | SDK |
| NDIS status | `NDIS_STATUS_*` | ndis.h | WDK |
| Window messages | `WM_*` plus the undocumented ones | winuser.h, wm_messages.txt | SDK |
| Control-class messages | `LVM_*`, `TVM_*`, `EM_*`, ... | commctrl.h, richedit.h | SDK |
| IOCTL names, device types | `IOCTL_*`, `FILE_DEVICE_*` | devioctl.h, ntdd*.h, winioctl.h | SDK/WDK |
| IRP function codes | `IRP_MJ_*`, `IRP_MN_*` | wdm.h | WDK |

Where the same value means different things in different sources (WinHTTP and WinInet both use
12xxx), every match is shown, labelled by source.

### 3. Driver value decoding

- IOCTLs: split a `CTL_CODE` into device type (with its name), function, method and access, and
  recognise named IOCTLs.
- IRP major/minor function names.
- Bugcheck parameter meanings: there's no header for these. Possibly hand-curate the common
  ones later (see open questions).

### 4. Front ends

- **GUI:**
  - One search box with a results list grouped by kind, and copy buttons.
  - Tray icon.
  - A global hotkey that opens it with the clipboard contents already looked up.
  - A "paste log" mode: multi-line input, returning a list of every recognised code in it
    with the surrounding text.
  - Per-monitor DPI; the theme follows Windows. The detailed design is under "GUI design" below.
- **CLI:** `wincode <value|name>`. Phase 3 adds `wincode --scan`, to annotate a log from stdin
  or a file, and `--json` output for scripts.
- **WinDbg extension:** `!wincode <value>`, a DbgEng extension DLL built on the same core.

## GUI design (phase 2)

Settled on 2026-09-15 from the mockups
(https://claude.ai/code/artifact/1f780d51-87a6-4d4e-8d4a-a1a6fd5468f3), after a survey of
existing tools. PowerToys Command Palette is the model; none of the error lookup tools
surveyed has clickable cross-references or a hotkey popup.

**Launcher popup**
- A global hotkey, **Win+Shift+E** by default and configurable in settings. It was free on the
  dev box. Win+Alt+Space belongs to Command Palette, Alt+Space to PowerToys Run, and Ctrl+Alt+E
  would take Visual Studio's Exception Settings away from it. If another program already holds
  the hotkey, WinCode says so and asks for another rather than failing silently.
- Opens on the monitor with the focused window. The hotkey toggles: pressed while the popup is
  in front, it hides it.
- Pre-filled from the clipboard when it holds a code or a name, with the text selected so typing
  replaces it. Otherwise it opens empty and shows recent lookups (kept for now, to see how they
  feel in use).
- The text is reset before the popup is shown, so an old query never flashes up.
- It grows downward as results arrive, all in one pass. Each match shows its kind, name and
  message, and its cross-references as links.
- Keys: Enter opens the window with the same query and selection (for now; see open question
  18), Ctrl+C copies a one-line summary, Up and Down move, and Esc clears the query and then
  hides the popup.

**Tool window: layout A, list and details**
- Title bar; back and forward; the search box; a pin for always-on-top; settings.
- Left: the matches, each with a kind tag, name and value. For a name search, the matched text
  is highlighted and whole-word matches come first, as in the CLI.
- Right, for the selected match:
  - kind, name and value (hex, decimal, and signed where the top bit is set)
  - Copy name, Copy value and Copy all
  - the message
  - fields
  - "Converts to": as HRESULT, maps to, and wraps
  - "NTSTATUS codes that map here", with the count and a Show all
  - the source line, e.g. "From winerror.h · Windows SDK 10.0.28000.0", on every match for now
    (it may move)
- Every code name is a link. Following one is a new lookup, with back and forward.
- It lives in the tray, and closing it hides it.

**Throughout**
- The theme follows Windows light/dark, with a dark title bar through DWM and FileMon++'s dark
  palette. Per-monitor DPI v2, and the system message font.
- Settings go in `settings.json`: the hotkey, recent lookups, the window position and
  always-on-top.
- WinCodeUI stays running in the tray, since a hotkey only works while its owner runs. Closing
  the window hides it, and Exit on the tray icon's menu quits. `--background` starts it with only
  the tray icon.
- Starting at sign-in is off until the user turns it on (decided 2026-09-15). It's an entry in
  the current user's Run key, read back each time rather than copied into `settings.json`, since
  Task Manager can switch it off there.
- The hotkey is chosen with Win/Ctrl/Alt/Shift check boxes and a key list, because Windows' hotkey
  control can't record the Win key. It's saved as text, as in `"Win+Shift+E"`.
- Built from owner-drawn `ListView`s for the lists and a custom-drawn details pane that does its
  own hit-testing for links.
- **Test early, in the VM:** the hotkey taking focus while an elevated WinDbg is in front. It was
  the most common complaint about launchers in the survey.

## Installer (phase 2, step 5)

Decided 2026-09-15: a small per-user installer built with Inno Setup, with a zip alongside for VMs
and test machines, where a `settings.json` beside the exe keeps a copy portable.

- **Why have one:** WinCodeUI now runs from sign-in, and the sign-in entry names wherever the exe
  was when the box was ticked. Run from the build folder, that locks the exe, and the next build
  fails. An installer puts it somewhere stable, and handles upgrades, uninstalling, the Start menu
  and the PATH.
- **Why Inno Setup:**
  - Per-user installs with no admin prompt.
  - One short script.
  - It closes running copies through Restart Manager.
  - One installer can carry x64 and ARM64.
  - It builds from the command line, so CI can build it, and winget accepts it.
- **Ruled out:**
  - MSIX: it needs a trusted signing certificate even to test, which would mean installing one on
    the dev box. It also wants its own startup mechanism instead of the Run key, and its install
    folder is read-only, which would break the portable `settings.json`.
  - WiX (MSI) and NSIS: more work for the same result.
- **What it does:**
  - Installs `WinCodeUI.exe` and `wincode.exe` to `%LOCALAPPDATA%\Programs\WinCode`, per user, with
    no admin prompt.
  - Adds a Start menu shortcut for WinCodeUI, and no desktop icon.
  - Adds the install folder to the user's PATH, so `wincode` works from any console (a check box,
    on by default).
  - Offers "Start WinCode when I sign in", off by default as in the app. It writes the same Run key
    entry the app does.
  - Offers to start WinCode at the end.
  - Upgrades in place: closes the running WinCodeUI first, and starts it again after.
  - Uninstalling removes the files, the shortcut, the PATH entry and the Run key entry. It leaves
    `%APPDATA%\WinCode\settings.json`, as most programs leave their settings.
- **What the app needs first:**
  - WinCodeUI has to really exit when asked, since closing its window only hides it. It should exit
    when Restart Manager asks (WM_QUERYENDSESSION and WM_ENDSESSION with ENDSESSION_CLOSEAPP).
    `WinCodeUI --exit` should tell a running copy to quit, handed over like any other command line.
  - The installer's version comes from `project()` in CMakeLists.txt, as the exe's does.
- **Building it:**
  - `installer/WinCode.iss`, compiled by Inno Setup's ISCC after a Release build.
  - A `tools/build_installer.ps1` passes it the version and the build folder.
  - It produces `WinCode-<version>-setup.exe` and the zip under `build\`.
  - Inno Setup has to be installed on the dev box and in CI.
- **Testing:** building it on the dev box is fine. Installing, upgrading and uninstalling are done
  in the VM, since they change the Run key and the PATH.
- **Later:**
  - Signing, with Azure Trusted Signing, when it's published.
  - A winget manifest (phase 6).
  - The WinDbg extension DLL as an optional component, once phase 5 exists.

## Log scanning (phase 3)

Drafted 2026-09-15 and not yet agreed; the questions it raises are 6 and 21 to 24 under "Open
questions". It covers reading whole logs: setup and installer logs, driver traces, service logs,
event log exports and debugger output.

**The goal:** point WinCode at a log and get back every error code in it, explained, without a wall
of false alarms. A scan that reports every process ID as a Win32 error is worse than no scan, so
precision matters more than catching everything. A code the scan misses can still be looked up by
hand.

**The scanner, in the core**
- `ScanText` takes text and returns findings. Each finding has:
  - the line and column it was found at;
  - the token as written, and how it was read;
  - why it counts (below);
  - its matches, the same CodeMatch the rest of WinCode uses.
- It reads numbers exactly as `ParseQuery` does: the same separators, C suffixes, sign extension
  and eight-digit hex guess. The difference is that it judges each token by the text around it,
  rather than treating the input as one query.
- Only exact names are reported (`STATUS_ACCESS_DENIED`, `E_FAIL`). Logs are full of identifiers,
  so partial name matches would be noise.
- It works a line at a time and caches each value's lookup, so a log of tens of megabytes scans
  in seconds.

**Telling codes from other numbers (open question 6).** Each number falls into one of three
groups:
1. **Taken on its own.** Hex with its top bit set, in any form (`0xC0000022`, `80070005`,
   `C0000005`, `0xFFFFFFFF80070005`), or a negative decimal (`-2147024891`). Values like that are
   almost always failure HRESULTs or NTSTATUS codes.
2. **Taken only with a cue.** Smaller values, and plain decimals, need the text just before them to
   name a code. Cues include `error`, `err`, `status`, `hr`, `hresult`, `ntstatus`, `result`,
   `code`, `rc`, `GetLastError`, `LastError` and `exit code`, as in `error 5`, `status=5`,
   `GetLastError() returned 5` and `exited with code 1603`. The cue also narrows the kind:
   - window messages need a message cue (`msg`, `message`, `WM_`);
   - bugchecks need a bugcheck cue (`bugcheck`, `stop code`);
   - otherwise `0x0000001F` in a trace would be read as WM_CANCELMODE.
3. **Ignored.** Everything else:
   - timestamps and dates, GUIDs, IP addresses and version numbers (`10.0.26100.1`);
   - numbers inside paths;
   - sizes and counts with no cue;
   - 64-bit addresses (`00007ffb1c2e4f3e`);
   - any value that matches nothing.

By default only named matches are reported. Values that decode without a name, such as a customer
HRESULT or `WM_USER + 1`, need `--all` (open question 24).

**A test set of logs.** The rules get tuned against logs kept in `Tests/logs/`, each with a file
beside it saying what should be found: the line, the token and the code it means. Precision is
measured against them, and a false alarm found later becomes a new case.

There were no real logs to hand when this started (open question 21), so the first set is written
to match the shapes these logs really take:
- a device install log, as setupapi.dev.log is, in UTF-16 with a byte order mark, which also
  tests the encodings;
- a servicing log in the shape of CBS.log, full of HRESULTs;
- an MSI verbose log, with `Return value 3` and `error 1603`;
- a WinDbg session: `!gle`, a stack, `.lastevent` and an `!analyze` bugcheck;
- a driver trace with NTSTATUS values;
- **a log with no codes in it at all**, but full of what a scanner mistakes for them: process and
  thread IDs, timestamps, dates, GUIDs, version numbers, addresses, sizes and handles. Anything
  reported from this one is a false alarm, which makes it the precision test.

Between them they carry the awkward cases: `Status=0xC0000022.`, `hr = 0x80070005`,
`GetLastError() returned 5`, `exited with code 1603`, a bare `ERROR_ACCESS_DENIED`, and values
such as `0x0000001F` with no cue, which must be left alone.

Real logs replace or join these as they turn up, and anything private is stripped before they go
in the repo.

**The CLI: `wincode --scan`**
- `wincode --scan [file...]`. With no file, or `-`, it reads stdin, so logs can be piped through it.
- **Output:**
  - By default, only the lines with findings, each with its line number and followed by what the
    codes on it mean. A code is explained in full the first time and by name after that
    (open question 22).
  - A summary at the end: each distinct code, how many times it appeared, and its first line.
  - `--summary` prints only the summary. `--all` also reports unnamed values.
- **Exit codes:** as for lookups. 0 if anything was found, 1 if nothing was, 2 for a usage error.
- **Encodings:** Windows logs come in UTF-16 (usually with a byte order mark), UTF-8 and the ANSI
  code page. A byte order mark decides it; otherwise the file is read as UTF-8, falling back to
  ANSI if it isn't valid UTF-8.

**`--json`**
- For lookups and scans alike:
  - `wincode --json 0xC0000022` gives an array of matches;
  - `wincode --scan --json` gives the findings and the summary.
- **A match** has:
  - `value`, `hex`, `kind` (`win32`, `hresult`, `ntstatus`, `bugcheck`, `message`), `name` and
    `aliases`;
  - `message`, and `messageSource` (`system` or `captured`);
  - `fields`, `related`, `mappedFrom` and `header`.
- **A finding** adds `line`, `column`, `token`, `reading` and `reason`.
- The top level carries `"version": 1`, so scripts can tell if the shape ever changes. The fields
  are documented in the README.
- Output is always UTF-8 and never wrapped.
- The JSON writer moves from `Gui/` into the core so the CLI can use it. Both front ends share it.

**The GUI: paste mode**
- Pasting several lines into the window's search box scans them instead of treating them as one
  query. A single-line edit would otherwise keep only the first line, so the paste is caught before
  the box sees it. The box then says what it's holding, as in "Pasted log: 1,234 lines".
- A log file dropped on the window is scanned the same way.
- **The results list:**
  - one row per distinct code, with how many times it appears and the first line it was on
    (open question 23);
  - the details pane shows the code as usual, plus the lines it was found on.
- **The popup:** it stops ignoring a multi-line clipboard. It shows the codes found, with "Open in
  window" for the full list.

**Steps, each reviewed:**
1. The scanner in the core, and its tests with a first set of logs, tuned until the noise rules
   hold up.
2. `--scan` in the CLI: files and stdin, the encodings, annotated output and the summary.
3. `--json` for lookups and scans, with the JSON writer moved into the core.
4. Paste mode in the window, dropped files, and the popup.

## Architecture

- **`core`** (static library, no UI): parsing, lookup, decoding, name search, log scanning.
  Pure functions returning structured results. Every front end is a thin layer over it.
- **Data generation:**
  - A script collects the names from the headers.
  - It then compiles and runs a small generated C++ program that `#include`s the real headers
    and prints each name's value.
  - The compiler therefore evaluates expressions like `(LVM_FIRST + 5)` and `CTL_CODE(...)`,
    and `#if` branches, and we never write a C preprocessor in a script.
  - Output is sorted tables, committed to the repo, so normal builds need no SDK/WDK-specific
    step. Only regenerating needs the SDK/WDK.
- **Front ends:** `gui` (exe), `cli` (exe), `windbg` (dll).
- **Tests:** core unit tests, plus table checks (sorted, no duplicates, known values present),
  in a test executable.
- **Build:** CMake with presets, MSVC, C++20, static CRT. CI on GitHub Actions (the Windows
  runners have VS and the SDK).
- **Generators:** Python, in `tools/`, following FileMon++'s `generate_ntstatus.py`.
- **Settings:** `settings.json` beside the exe if one is there (portable), otherwise in
  `%APPDATA%\WinCode\`. Same approach as FileMon++, including atomic writes and
  leaving a file that won't parse untouched.
- **Code from FileMon++:** copied in, not shared: the JSON reader/writer, dark mode
  (`Theme.cpp`), and the NTSTATUS generator as a starting point. Each project stays
  self-contained.

## Code conventions

C++ follows the style in `~/.claude/CLAUDE.md`, and FileMon++ is the working example of it.
In short:

- Every file starts with the Doxygen header (`@file`, `@brief`, `@details` only when needed,
  `@author Ged Murphy`, `@copyright Copyright (c) 2026 Ged Murphy`). Then comes the GPLv3
  notice, worded as in FileMon++ with "WinCode" in place of "FileMon++".
- Every non-trivial function gets a Doxygen block. `@details` only holds reasoning.
- Allman braces, always. Return type and function name each on their own line. One
  SAL-annotated parameter per line, and the closing `)` on its own line.
- Naming by scope: `g_`, `m_`, `k_` prefixes; PascalCase for functions, types and parameters;
  camelCase for locals.
- No bare casts. Use `SCAST`/`RCAST`/`CCAST`, defined once in the project-wide header.
- Comments explain why, never what. Multi-line comments are `//` blocks with a blank `//` line
  above and below.
- Guard clauses and early returns. No speculative abstractions.

Also taken from FileMon++, though not stated in CLAUDE.md:

- PascalCase file names and `.hpp` headers. Each binary has one project-wide header carrying
  its includes, the cast macros and shared declarations.
- File-scope functions are `static`, on a line of its own, rather than in anonymous namespaces.
- Locals are declared at the top of the function.
- Windows-facing code uses Win32 types (`WCHAR`, `PCWSTR`, `PWSTR`).
- Long calls take one argument per line, aligned after the `(`.
- Generated sources are named `*.g.cpp`, say "DO NOT EDIT. Regenerate with ..." in `@details`,
  and are committed.
- British spelling in identifiers and comments (`Initialise`, `Colour`).
- The README explains why things are the way they are, as well as how to use them.

## Phases

Each phase ends with something usable, and we review it together before starting the next.

0. **Groundwork:**
   - Install the C++ toolchain.
   - Leave the C# and every ReactOS reference behind. The rewrite started as a branch of
     Message_Translator and then moved to its own repo; see open question 14.
   - CMake skeleton, CI, licence, README.
1. **Core and SDK data:**
   - Parsing, the Win32/HRESULT/NTSTATUS/bugcheck/WM tables, cross-references, name search.
   - The CLI as the first front end, since it's the easiest to test.
   - Built in steps, each reviewed: (1) generator and tables; (2) parsing and lookup by value,
     with a minimal CLI so each step can be tried for real; (3) name search and
     cross-references; (4) the CLI's final output.
2. **GUI:** reverse mapping in the core first, which the clickable cross-references need; then
   the launcher popup and full window (open question 15), tray, hotkey and clipboard, settings.
   Last, the per-user installer (step 5; see "Installer"), brought forward from phase 6 once
   WinCodeUI started running from sign-in.
3. **Log scanning:** a scanner in core, `--scan` in the CLI, paste mode in the GUI, and
   `--json` output from the CLI, which suits piping logs through it. Moved here from phase 1.
4. **Kernel and driver extras:**
   - The WDK sources (IO_ERR, NDIS).
   - IOCTL decoding and named IOCTLs.
   - IRP codes.
   - Control-class window messages.
   - Window message ranges (`WM_AFXFIRST`…`WM_AFXLAST` and the like), so a value inside one is
     named by its range. The phase 1 generator drops these markers.
5. **WinDbg extension:**
   - `wincode.dll`, a DbgEng extension built on the same core, so `!wincode <value>` gives the
     CLI's answer without leaving the debugger.
   - It has to match the debugger's architecture, so both the x64 and the ARM64 builds matter
     here.
   - The installer gains an optional component for it. How WinDbg finds a per-user extension
     (`_NT_DEBUGGER_EXTENSION_PATH`, `.extpath` or an extension gallery) is to be checked then.
6. **Polish:** remaining data sources, packaging for winget, signing, bugcheck parameter data.

## Open questions

1. ~~**Licence**~~ — **decided: GPLv3.** Add `LICENSE` and per-file headers in phase 0.
2. ~~**Architectures**~~ — **decided: x64 and ARM64.**
3. ~~**Message text**~~ — **decided: the running OS first**, falling back to English text
   captured at generation time when the OS has none.
4. **Bugcheck parameters:** worth hand-curating the common bugchecks, or leave it to WinDbg's
   `!analyze`?
5. ~~**WDK on the dev box**~~ — **decided: use the installed WDK** (the eWDK has been removed).
   Only needed to regenerate tables.
6. **Log scanning noise:** logs are full of numbers that aren't codes (process IDs, addresses,
   timestamps, sizes). Proposal, set out under "Log scanning": hex with its top bit set counts on
   its own; smaller values and decimals need a cue such as `error` or `status=` just before them;
   everything else is ignored; and only named matches are reported. To be tuned against real logs
   (question 21).
7. ~~**The first draft port**~~ — **decided: deleted.** A 1:1 port written before this plan
   was thrown away, and the rewrite started fresh from the plan.
8. ~~**Build system**~~ — **decided: CMake.**
9. ~~**Generator language**~~ — **decided: Python.**
10. ~~**Settings storage**~~ — **decided: JSON file**, as FileMon++ does.
11. ~~**Code shared with FileMon++**~~ — **decided: copy it in.**
12. ~~**A project `CLAUDE.md`**~~ — **decided: yes, in phase 0.** Build, test, layout and
    regeneration steps only. The style stays in the global one.
13. ~~**Name**~~ — **decided: WinCode**, replacing Message Translator, which sounded like
    language translation and undersold what the tool covers. The CLI is `wincode.exe` and the GUI
    will be `WinCodeUI.exe`, since Windows filenames ignore case and `WinCode.exe` would clash
    with the CLI.
14. ~~**Where it lives**~~ — **decided: its own repo**, github.com/gedmurphy/wincode. Its history
    starts with an import of the two files that carried over from Message Translator, the window
    message list and the icon, followed by the rewrite commits brought across from a branch of
    Message_Translator. That repo is left as it was.
15. ~~**GUI shape**~~ — **decided: a launcher that expands into a tool window.** A global hotkey
    pops up a compact search box, pre-filled from the clipboard, that grows as results arrive and
    expands into a full tray window with a details pane, clickable cross-references and
    back/forward. It's modelled on PowerToys Command Palette, after a survey of existing tools.
    The detailed design is under "GUI design", with layout A chosen from the mockups.
16. ~~**Theme**~~ — **decided: follow the Windows light/dark setting.** This differs from
    FileMon++, which is light by default.
17. ~~**Reverse mapping**~~ — **decided: yes**, as a core feature (see Features, cross-references).
18. **Enter in the popup:** it opens the window for now. The alternative is copying the name.
    Try both once the GUI exists, and keep whichever feels right in use.
19. ~~**Installer architectures**~~ — **decided: x64 only at first**, since the ARM64 build tools
    aren't installed and the ARM64 build hasn't been checked. ARM64 joins the same installer
    later.
20. ~~**Inno Setup on the dev box**~~ — **decided: installed with winget**
    (`JRSoftware.InnoSetup`), for building the installer. Installing, upgrading and
    uninstalling are still tested in the VM.
21. ~~**Logs for the scanner's tests**~~ — **decided: start with a written set** (2026-09-16).
    The user had no logs to hand, so the first set is written to match the shapes real logs take,
    including one with no codes in it as the precision test; see "Log scanning". Real logs join
    or replace them later, and every false alarm found in use becomes a new case.
22. ~~**What `--scan` prints by default**~~ — **decided: only the lines with codes on them**,
    then the summary.
23. ~~**Rows in paste mode**~~ — **decided: one row per distinct code**, with a count and the
    lines it was found on.
24. ~~**Unnamed values in scans**~~ — **decided: left out unless `--all` is given.**
