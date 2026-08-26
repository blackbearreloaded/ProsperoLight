<#
  ps5-native-app-boilerplate - Native dependency bootstrapper.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later

  Fetches the public PS5 payload SDK and a native static zlib package into the
  ignored repository cache. Nothing is installed globally.
#>

#requires -Version 5.1
param(
    [switch]$SkipSdk
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$cache = Join-Path $root ".deps/native"
$sdk = Join-Path $cache "ps5-payload-sdk"
$sdkArchive = Join-Path $cache "ps5-payload-sdk.zip"
$zlib = Join-Path $cache "zlib"
$zlibRoot = Join-Path $zlib "root"
$sdkUrl = "https://github.com/ps5-payload-dev/sdk/releases/download/v0.42/ps5-payload-sdk.zip"
$sdkHash = "8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da"

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

function Invoke-Wsl([string]$Command) {
    & wsl.exe --exec bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        Fail "Native dependency bootstrap failed."
    }
}

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    Fail "WSL was not found."
}
New-Item -ItemType Directory -Path $cache -Force | Out-Null

if (-not $SkipSdk) {
    $sdkReady = Test-Path -LiteralPath (Join-Path $sdk "bin/prospero-lld") -PathType Leaf
    if (-not $sdkReady) {
        $wslCache = WslPath $cache
        $archiveName = Split-Path -Leaf $sdkArchive
        $temporaryName = "$archiveName.download"
        Invoke-Wsl "cd '$wslCache' && wget -q '$sdkUrl' -O '$temporaryName' && echo '$sdkHash  $temporaryName' | sha256sum --check --strict && mv '$temporaryName' '$archiveName' && unzip -q -o '$archiveName' -d ."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $sdk "bin/prospero-lld") -PathType Leaf) -or
        -not (Test-Path -LiteralPath (Join-Path $sdk "target/include") -PathType Container)) {
        Fail "The pinned PS5 payload SDK is incomplete after extraction."
    }
}

$zlibArchive = Get-ChildItem -LiteralPath $zlibRoot -Filter "libz.a" -File `
    -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $zlibArchive -or
    -not (Test-Path -LiteralPath (Join-Path $zlibRoot "usr/include/zlib.h") -PathType Leaf)) {
    New-Item -ItemType Directory -Path $zlib -Force | Out-Null
    $wslZlib = WslPath $zlib
    Invoke-Wsl "cd '$wslZlib' && apt-get download zlib1g-dev >/dev/null && for package in zlib1g-dev_*.deb; do dpkg-deb -x `"`$package`" root; done"
    $zlibArchive = Get-ChildItem -LiteralPath $zlibRoot -Filter "libz.a" -File `
        -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $zlibArchive) {
    Fail "The native zlib archive was not found after extraction."
}

[pscustomobject]@{
    sdkRoot = if ($SkipSdk) { "" } else { WslPath $sdk }
    zlibInclude = WslPath (Join-Path $zlibRoot "usr/include")
    zlibArchive = WslPath $zlibArchive.FullName
} | ConvertTo-Json -Compress
