# WinCode

A native C++ rewrite of an old .NET error lookup tool, built in phases set out in
`docs/PLAN.md`. Read the plan and its decided open questions before starting
work. Don't start a new phase without the user's go-ahead.

## Style

C++ follows the global `~/.claude/CLAUDE.md`, plus the FileMon++ conventions listed
under "Code conventions" in `docs/PLAN.md`. Every source file carries the GPLv3
header; copy it from an existing file.

## Build and test

From an x64 developer shell (or Visual Studio, which sets the environment itself):

```
cmake --preset x64
cmake --build --preset x64-debug
ctest --preset x64-debug
```

ARM64 needs a shell for `x64_arm64` and `--preset arm64`. Output goes to
`build\<preset>\<Config>\`. MSVC only, `/W4 /WX`.

The installer and zip: after the x64-release build, run
`powershell -File tools/build_installer.ps1` (needs Inno Setup, installed with
winget). They land in `build\`. Test installing, upgrading and uninstalling in
the VM, not on the dev box: they change the Run key and the user's PATH.

In a non-interactive shell, get the environment with:

```
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
```

## Layout

- `Core/`: the core static library; `WinCode.hpp` is the project-wide header
- `Cli/`: `wincode.exe`
- `Tests/`: `WinCodeTests.exe`, a plain exe; exit code is the failure count
- `Gui/`: `WinCodeUI.exe`, the GUI. `WinCodeUI.hpp` is its shared header,
  `MainWindow.cpp` the window and `Popup.cpp` the hotkey popup. Not
  `WinCode.exe`: filenames ignore case, so it would clash with `wincode.exe`.
  There are no automated GUI tests; check it by launching it with a query on
  the command line, e.g. `WinCodeUI 5`, or `WinCodeUI --popup 5` for the
  popup. Only one WinCodeUI runs at a time: a second hands its command line to
  the first, so close any running copy before checking a new build
- `tools/`: Python table generators and their inputs, e.g. `wm_messages.txt`

## Rules

- Generated tables (`Core/Tables/*.g.cpp`) are committed and never edited by hand.
  Regenerate them from an x64 developer shell with `python tools/generate_tables.py`,
  which reads the SDK that shell points at. Its "skipped" report lists every name it
  left out and why; check it after an SDK update.
