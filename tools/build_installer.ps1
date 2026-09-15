#
# tools/build_installer.ps1: builds WinCode's installer and zip from a Release build.
#
# Copyright (c) 2026 Ged Murphy. Part of WinCode, GPLv3 or later; see LICENSE.
#
# Run it by hand after `cmake --build --preset x64-release`:
#
#   powershell -File tools\build_installer.ps1
#
# or through the build, which passes the version and the folder it just built
# into:
#
#   cmake --build --preset x64-release --target installer
#
# It needs Inno Setup 6.3 or later (winget install JRSoftware.InnoSetup). The
# version comes from project() in CMakeLists.txt, so the installer, the zip and
# the executables always agree.
#
# Output, in build\:
#   WinCode-<version>-setup.exe   the per-user installer
#   WinCode-<version>-x64.zip     the same files, for copying onto a VM or test
#                                 machine; a settings.json beside the exe there
#                                 keeps that copy portable
#

param(
    # Where the built executables are. The preset's Release folder by default.
    [string]$BuildDir = "",

    # What to call this build. Read from CMakeLists.txt by default.
    [string]$Version = "",

    # Where the installer and zip go.
    [string]$OutputDir = "",

    # The preset whose output is packaged, and what the zip is named after.
    [string]$Preset = "x64",
    [string]$Arch = "",

    # Guards against packaging a Debug build by mistake.
    [string]$Configuration = "Release",

    # Builds only the zip. ARM64 has no installer yet, but the zip is still
    # worth having.
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

if ($Configuration -ne "Release") {
    throw "Only a Release build is packaged; this was $Configuration."
}

if ($Version -eq "") {
    $cmakeLists = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
    if ($cmakeLists -notmatch 'project\(WinCode VERSION (\d+\.\d+\.\d+)') {
        throw "No version found in project() in CMakeLists.txt."
    }
    $Version = $Matches[1]
}

if ($BuildDir -eq "") { $BuildDir = Join-Path $root "build\$Preset\Release" }
if ($OutputDir -eq "") { $OutputDir = Join-Path $root "build" }
if ($Arch -eq "") { $Arch = $Preset }

foreach ($exe in "WinCodeUI.exe", "wincode.exe") {
    if (-not (Test-Path (Join-Path $BuildDir $exe))) {
        throw "$exe isn't in $BuildDir. Build the $Preset-release preset first."
    }
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$setup = ""

if (-not $SkipInstaller) {
    #
    # ISCC is on the PATH after some installs and not others, so the usual
    # places are tried too.
    #
    $iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
    if (-not $iscc) {
        $candidates = @(
            (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
            (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
            (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
        )
        $iscc = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    }
    if (-not $iscc) {
        throw "Inno Setup's ISCC.exe wasn't found. Install it with: winget install JRSoftware.InnoSetup"
    }

    & $iscc /Qp "/DAppVersion=$Version" "/DBuildDir=$BuildDir" "/DOutputDir=$OutputDir" (Join-Path $root "installer\WinCode.iss")
    if ($LASTEXITCODE -ne 0) {
        throw "ISCC failed with exit code $LASTEXITCODE."
    }

    $setup = Join-Path $OutputDir "WinCode-$Version-setup.exe"
}

$zip = Join-Path $OutputDir "WinCode-$Version-$Arch.zip"
Compress-Archive -Force -DestinationPath $zip -Path @(
    (Join-Path $BuildDir "WinCodeUI.exe"),
    (Join-Path $BuildDir "wincode.exe"),
    (Join-Path $root "LICENSE")
)

"Built:"
if ($setup -ne "") { "  $setup" }
"  $zip"
