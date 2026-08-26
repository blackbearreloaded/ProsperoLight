<#
  ps5-native-app-boilerplate - Read-only prerequisite checker.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later

  Verifies the native host toolchain, dependency bootstrap, and runtime.
#>

#requires -Version 5.1
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

$wslExists = $null -ne (Get-Command wsl.exe -ErrorAction SilentlyContinue)
Report "WSL" $wslExists $(if ($wslExists) { "wsl.exe found" } else { "wsl.exe not found" })
if ($wslExists) {
    & wsl.exe --exec sh -lc "test -x /usr/bin/clang-18 && test -x /usr/bin/clang++"
    Report "Native C/C++ compiler" ($LASTEXITCODE -eq 0) "Clang 18 and Clang++ in WSL"
    & wsl.exe --exec sh -lc "test -x /usr/bin/wget && test -x /usr/bin/unzip && test -x /usr/bin/apt-get && test -x /usr/bin/dpkg-deb"
    Report "Native dependency bootstrap" ($LASTEXITCODE -eq 0) "wget, unzip, apt-get, and dpkg-deb in WSL"
}

$git = Get-Command git -ErrorAction SilentlyContinue
Report "Git" ($null -ne $git) $(if ($git) { $git.Source } else { "required only for optional MkPFS bootstrap" })

$libc = Join-Path $root "runtime/libc.prx"
$expectedLibcHash = "E6FF45D16ADF687855CC3B33B0C8A4132B6504360B221E0A34C7E99FB3BA0036"
$libcReady = Test-Path -LiteralPath $libc -PathType Leaf
if ($libcReady) {
    $libcReady = (Get-FileHash -LiteralPath $libc -Algorithm SHA256).Hash -eq $expectedLibcHash
    Report "Generated clean-room libc.prx" $libcReady $libc
} else {
    Write-Host "[GENERATED] Clean-room libc.prx: make will create $libc"
}

$nativeBuilder = Join-Path $root "tooling/native/libc_builder.cpp"
Report "Clean-room libc source" (Test-Path -LiteralPath $nativeBuilder -PathType Leaf) $nativeBuilder

if ($failed) {
    Write-Error "One or more prerequisites are missing. See docs/GETTING_STARTED.md."
    exit 1
}

Write-Host "All native build prerequisites are ready."
