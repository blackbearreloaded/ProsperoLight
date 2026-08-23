<#
  ps5-native-app-boilerplate - SharpProspero dependency bootstrapper.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later

  Fetches the pinned upstream source and applies the native-app compatibility delta.
#>

#requires -Version 5.1

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$dependencyRoot = Join-Path $root ".deps"
$checkout = Join-Path $dependencyRoot "SharpProspero"
$patch = Join-Path $root "tooling/patches/sharpprospero-native-app.patch"
$repository = "https://github.com/SvenGDK/SharpProspero.git"
$revision = "e36e610fa5b4be23ad38b9c8429f11f11750cc0c"

function Fail([string]$Message) {
    throw "ps5-native-app-boilerplate: $Message"
}

function Invoke-Git([string[]]$Arguments) {
    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        Fail "Git command failed: git $($Arguments -join ' ')"
    }
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Fail "Git was not found. Install Git for Windows and retry."
}
if (-not (Test-Path -LiteralPath $patch -PathType Leaf)) {
    Fail "SharpProspero compatibility patch not found: $patch"
}

if (-not (Test-Path -LiteralPath $checkout)) {
    New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $checkout | Out-Null
    Invoke-Git @("-C", $checkout, "init", "--quiet")
    Invoke-Git @("-C", $checkout, "config", "core.autocrlf", "false")
    Invoke-Git @("-C", $checkout, "remote", "add", "origin", $repository)
    Invoke-Git @("-C", $checkout, "fetch", "--quiet", "--depth", "1", "origin", $revision)
    Invoke-Git @("-C", $checkout, "checkout", "--quiet", "--detach", "FETCH_HEAD")
} elseif (-not (Test-Path -LiteralPath (Join-Path $checkout ".git") -PathType Container)) {
    Fail "$checkout exists but is not a managed Git checkout."
}

$actualRevision = (& git -C $checkout rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualRevision -ne $revision) {
    Fail "SharpProspero cache is at $actualRevision; expected $revision. Remove .deps/SharpProspero and retry."
}

& git -C $checkout apply --reverse --ignore-space-change --check $patch 2>$null
if ($LASTEXITCODE -ne 0) {
    & git -C $checkout diff --quiet
    if ($LASTEXITCODE -ne 0) {
        Fail "SharpProspero cache has unexpected local changes. Remove .deps/SharpProspero and retry."
    }
    Invoke-Git @("-C", $checkout, "apply", "--ignore-space-change", "--check", $patch)
    Invoke-Git @("-C", $checkout, "apply", "--ignore-space-change", $patch)
}

Write-Host "SharpProspero tooling ready at revision $revision."
