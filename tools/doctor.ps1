<#
  ps5-native-app-boilerplate - Read-only prerequisite checker.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later

  Verifies the host toolchain and bundled clean-room runtime artifact.
#>

#requires -Version 5.1
param(
    [string]$Dotnet = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$failed = $false

function Report([string]$Name, [bool]$Passed, [string]$Detail) {
    $script:failed = $script:failed -or -not $Passed
    $status = if ($Passed) { "OK" } else { "MISSING" }
    Write-Host ("[{0}] {1}: {2}" -f $status, $Name, $Detail)
}

try {
    Get-Content -LiteralPath (Join-Path $root "project.json") -Raw |
        ConvertFrom-Json | Out-Null
    Report "project.json" $true "valid JSON"
} catch {
    Report "project.json" $false $_.Exception.Message
}

if (-not $Dotnet) {
    $command = Get-Command dotnet -ErrorAction SilentlyContinue
    if ($command) {
        $Dotnet = $command.Source
    }
}
$dotnetExists = $Dotnet -and (Test-Path -LiteralPath $Dotnet -PathType Leaf)
if ($dotnetExists) {
    $version = & $Dotnet --version
    $dotnetExists = $LASTEXITCODE -eq 0 -and $version -match '^10\.'
    Report ".NET SDK 10" $dotnetExists $version
} else {
    Report ".NET SDK 10" $false "install it or pass -Dotnet"
}

$wslExists = $null -ne (Get-Command wsl.exe -ErrorAction SilentlyContinue)
Report "WSL" $wslExists $(if ($wslExists) { "wsl.exe found" } else { "wsl.exe not found" })
if ($wslExists) {
    & wsl.exe --exec sh -lc "test -x /usr/bin/clang-18"
    Report "Clang 18 in WSL" ($LASTEXITCODE -eq 0) "/usr/bin/clang-18"
    & wsl.exe --exec sh -lc "test -d /opt/ps5-payload-sdk/target/include"
    Report "PS5 payload SDK" ($LASTEXITCODE -eq 0) "/opt/ps5-payload-sdk"
}

$git = Get-Command git -ErrorAction SilentlyContinue
Report "Git" ($null -ne $git) $(if ($git) { $git.Source } else { "Git for Windows is required for the on-demand SharpProspero checkout" })

$libc = Join-Path $root "runtime/libc.prx"
$expectedLibcHash = "247A8BAD5764D3134FB8470653AE8BD72BD200170BD132F6881C73A375D5533A"
$libcReady = Test-Path -LiteralPath $libc -PathType Leaf
if ($libcReady) {
    $libcReady = (Get-FileHash -LiteralPath $libc -Algorithm SHA256).Hash -eq $expectedLibcHash
}
Report "Bundled clean-room libc.prx" $libcReady $libc

$libcBuilder = Join-Path $root "tooling/MinimalLibcBuilder/MinimalLibcBuilder.csproj"
Report "Clean-room libc source" (Test-Path -LiteralPath $libcBuilder -PathType Leaf) $libcBuilder

if ($failed) {
    Write-Error "One or more prerequisites are missing. See docs/GETTING_STARTED.md."
    exit 1
}

Write-Host "All build prerequisites are ready."
