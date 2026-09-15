;
; installer/WinCode.iss: WinCode's installer, built with Inno Setup 6.3 or later.
;
; Copyright (c) 2026 Ged Murphy. Part of WinCode, GPLv3 or later; see LICENSE.
;
; Built by tools/build_installer.ps1, which passes AppVersion (from project() in
; CMakeLists.txt), BuildDir and OutputDir. The defaults below are for building
; it by hand from this folder.
;
; Per user, with no admin prompt: WinCode is one exe and a CLI, and nothing in it
; needs to be installed for every user of the machine. See "Installer" in
; docs/PLAN.md for why it's Inno Setup and not MSIX or an MSI.
;

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#ifndef BuildDir
  #define BuildDir "..\build\x64\Release"
#endif

#ifndef OutputDir
  #define OutputDir "..\build"
#endif

[Setup]
; Never change the AppId: it's how an upgrade finds the copy it replaces.
AppId={{6B2E9C4A-3F71-4D8B-9E25-A1C7D04F8B36}
AppName=WinCode
AppVersion={#AppVersion}
AppVerName=WinCode {#AppVersion}
AppPublisher=Ged Murphy
AppPublisherURL=https://github.com/gedmurphy/wincode
AppSupportURL=https://github.com/gedmurphy/wincode/issues
VersionInfoVersion={#AppVersion}

; {autopf} is %LOCALAPPDATA%\Programs for a per-user install.
PrivilegesRequired=lowest
DefaultDirName={autopf}\WinCode
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
UsePreviousTasks=yes

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

; The PATH changes, and running programs are told so.
ChangesEnvironment=yes

; WinCodeUI is asked to quit with --exit before its files are replaced (see
; PrepareToInstall), so Restart Manager is only the fallback.
CloseApplications=yes
RestartApplications=no

WizardStyle=modern
SetupIconFile=..\Gui\WinCode.ico
UninstallDisplayIcon={app}\WinCodeUI.exe
UninstallDisplayName=WinCode
OutputDir={#OutputDir}
OutputBaseFilename=WinCode-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "path"; Description: "Add wincode.exe to my &PATH, for the command line"
Name: "startup"; Description: "&Start WinCode when I sign in, so the hotkey always works"; Flags: unchecked

[Files]
Source: "{#BuildDir}\WinCodeUI.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\wincode.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\WinCode"; Filename: "{app}\WinCodeUI.exe"; Comment: "Look up Windows error codes and messages"

[Registry]
; The same entry WinCode's own "Start when I sign in" writes.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "WinCode"; \
    ValueData: """{app}\WinCodeUI.exe"" --background"; Tasks: startup

; Removed on uninstall however it was turned on, here or in WinCode, and with
; any switch-off Task Manager left. ValueType none writes nothing on install.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "WinCode"; \
    Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run"; ValueType: none; \
    ValueName: "WinCode"; Flags: uninsdeletevalue

[Run]
; An upgrade puts WinCode back as it was: running in the tray.
Filename: "{app}\WinCodeUI.exe"; Parameters: "--background"; Flags: nowait; Check: WasRunningBefore
Filename: "{app}\WinCodeUI.exe"; Description: "Start WinCode now"; Flags: nowait postinstall skipifsilent; \
    Check: not WasRunningBefore

[UninstallRun]
Filename: "{app}\WinCodeUI.exe"; Parameters: "--exit"; RunOnceId: "ExitWinCode"; Flags: runhidden waituntilterminated

[Code]
const
  EnvironmentKey = 'Environment';

var
  WasRunning: Boolean;

{ Whether WinCode was running when setup started, for the [Run] entries. }
function WasRunningBefore: Boolean;
begin
  Result := WasRunning;
end;

{ Asks a running WinCodeUI to quit, whichever copy it is, before its files are
  replaced. Closing its window only hides it, so only --exit really ends it.
  The new copy is used to ask, so this works on a first install too. }
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  WasRunning := FindWindowByClassName('WinCodeMainWindow') <> 0;

  if WasRunning then
  begin
    ExtractTemporaryFile('WinCodeUI.exe');
    Exec(ExpandConstant('{tmp}\WinCodeUI.exe'), '--exit', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;

{ Whether a PATH-style list holds a folder, ignoring case. }
function PathContains(Paths, Folder: String): Boolean;
begin
  Result := Pos(';' + Uppercase(Folder) + ';', ';' + Uppercase(Paths) + ';') > 0;
end;

procedure AddToPath(Folder: String);
var
  Paths: String;
begin
  if not RegQueryStringValue(HKCU, EnvironmentKey, 'Path', Paths) then
    Paths := '';

  if PathContains(Paths, Folder) then
    exit;

  if (Paths <> '') and (Copy(Paths, Length(Paths), 1) <> ';') then
    Paths := Paths + ';';

  RegWriteExpandStringValue(HKCU, EnvironmentKey, 'Path', Paths + Folder);
end;

procedure RemoveFromPath(Folder: String);
var
  Paths: String;
  Position: Integer;
begin
  if not RegQueryStringValue(HKCU, EnvironmentKey, 'Path', Paths) then
    exit;

  Paths := ';' + Paths + ';';
  Position := Pos(';' + Uppercase(Folder) + ';', Uppercase(Paths));
  if Position = 0 then
    exit;

  Delete(Paths, Position, Length(Folder) + 1);
  RegWriteExpandStringValue(HKCU, EnvironmentKey, 'Path', Copy(Paths, 2, Length(Paths) - 2));
end;

{ The PATH follows the task, so unticking it on an upgrade takes WinCode off it. }
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then
    exit;

  if WizardIsTaskSelected('path') then
    AddToPath(ExpandConstant('{app}'))
  else
    RemoveFromPath(ExpandConstant('{app}'));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromPath(ExpandConstant('{app}'));
end;
