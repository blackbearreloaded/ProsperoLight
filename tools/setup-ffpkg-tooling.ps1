<#
  ps5-native-app-boilerplate - Optional UFS2 packaging bootstrapper.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later

  Fetches and builds the pinned UFS2Tool used by the optional FFPKG output.
#>

#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)]
    [string]$Dotnet
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$dependencyRoot = Join-Path $root ".deps"
$checkout = Join-Path $dependencyRoot "UFS2Tool"
$repository = "https://github.com/SvenGDK/UFS2Tool.git"
$revision = "b5307a60d5b4e3a68ba680e0e33cfadf05017c77"

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

if (-not (Test-Path -LiteralPath $checkout)) {
    New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $checkout | Out-Null
    Invoke-Git @("-C", $checkout, "init", "--quiet")
    Invoke-Git @("-C", $checkout, "config", "core.autocrlf", "false")
    Invoke-Git @("-C", $checkout, "remote", "add", "origin", $repository)
    Invoke-Git @("-C", $checkout, "fetch", "--quiet", "--depth", "1",
        "origin", $revision)
    Invoke-Git @("-C", $checkout, "checkout", "--quiet", "--detach",
        "FETCH_HEAD")
} elseif (-not (Test-Path -LiteralPath (Join-Path $checkout ".git") -PathType Container)) {
    Fail "$checkout exists but is not a managed Git checkout."
}

$actualRevision = (& git -C $checkout rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualRevision -ne $revision) {
    Fail "UFS2Tool cache is at $actualRevision; expected $revision. Remove .deps/UFS2Tool and retry."
}
& git -C $checkout diff --quiet
if ($LASTEXITCODE -ne 0) {
    Fail "UFS2Tool cache has local changes. Remove .deps/UFS2Tool and retry."
}

$project = Join-Path $checkout "UFS2Tool.csproj"
$license = Join-Path $checkout "LICENSE"
foreach ($required in @($project, $license)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        Fail "Required UFS2Tool file not found: $required"
    }
}

& $Dotnet build $project -c Release --nologo | Out-Host
if ($LASTEXITCODE -ne 0) {
    Fail "UFS2Tool build failed."
}

$assembly = Join-Path $checkout "bin/Release/net8.0/UFS2Tool.dll"
if (-not (Test-Path -LiteralPath $assembly -PathType Leaf)) {
    Fail "Built UFS2Tool assembly not found: $assembly"
}

Write-Host "UFS2Tool ready at revision $revision."
Write-Output $assembly
