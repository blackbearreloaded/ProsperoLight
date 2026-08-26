<#
  ps5-native-app-boilerplate - Optional native FFPKG tooling bootstrapper.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later

  Downloads and extracts the Debian/Ubuntu native makefs package without root
  privileges. The ignored cache is reused by later builds.
#>

#requires -Version 5.1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$cache = Join-Path $root ".deps/makefs"
$binary = Join-Path $cache "root/usr/sbin/makefs"

function Fail([string]$Message) {
    throw "ps5-native-app-boilerplate: $Message"
}

function WslPath([string]$Path) {
    $absolute = [IO.Path]::GetFullPath($Path)
    if ($absolute -notmatch '^([A-Za-z]):\\(.*)$') {
        Fail "Path is not on a Windows drive visible to WSL: $absolute"
    }
    return "/mnt/$($Matches[1].ToLowerInvariant())/$($Matches[2].Replace('\', '/'))"
}

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    Fail "WSL was not found."
}
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
    New-Item -ItemType Directory -Path $cache -Force | Out-Null
    $wslCache = WslPath $cache
    $command = "cd '$wslCache' && apt-get download makefs >/dev/null && dpkg-deb -x makefs_*.deb root"
    & wsl.exe --exec bash -lc $command
    if ($LASTEXITCODE -ne 0) {
        Fail "Unable to download and extract the native makefs package."
    }
}
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
    Fail "Native makefs executable was not found after extraction."
}

Write-Output (WslPath $binary)
