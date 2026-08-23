<#
  ps5-native-app-boilerplate - Clean-room runtime-shim reproducer.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later

  Rebuilds twice and installs the shim only after deterministic hash checks.
#>

#requires -Version 5.1
param(
    [string]$Dotnet = "",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$emitter = Join-Path $root "tooling/MinimalLibcBuilder/MinimalLibcBuilder.csproj"
$signer = Join-Path $root "tooling/NativeAppBuilder/NativeAppBuilder.csproj"
$setupTooling = Join-Path $root "tools/setup-tooling.ps1"
$work = Join-Path $root "build/runtime-shim"
$rawA = Join-Path $work "libc-a.raw.prx"
$rawB = Join-Path $work "libc-b.raw.prx"
$signedA = Join-Path $work "libc-a.prx"
$signedB = Join-Path $work "libc-b.prx"
$output = Join-Path $root "runtime/libc.prx"
$manifest = Join-Path $root "runtime/libc.prx.sha256"
$expectedRaw = "202C41C485CACE159D354216818EB69AF9843698D695D4FB611D6BB13A4C85FB"
$expectedSigned = "247A8BAD5764D3134FB8470653AE8BD72BD200170BD132F6881C73A375D5533A"

function Fail([string]$Message) {
    throw "ps5-native-app-boilerplate: $Message"
}

function Invoke-Dotnet([string[]]$Arguments) {
    & $Dotnet @Arguments
    if ($LASTEXITCODE -ne 0) {
        Fail "The .NET runtime-module build failed."
    }
}

if (-not $Dotnet) {
    $command = Get-Command dotnet -ErrorAction SilentlyContinue
    if ($command) {
        $Dotnet = $command.Source
    }
}
if (-not $Dotnet -or -not (Test-Path -LiteralPath $Dotnet -PathType Leaf)) {
    Fail ".NET SDK 10 was not found. Install it or pass -Dotnet C:\path\to\dotnet.exe."
}
$Dotnet = (Resolve-Path -LiteralPath $Dotnet).Path

foreach ($required in @($emitter, $signer, $setupTooling)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        Fail "Required source project not found: $required"
    }
}

& $setupTooling

$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = "1"
$env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"
$env:DOTNET_NOLOGO = "1"
New-Item -ItemType Directory -Path $work -Force | Out-Null

Invoke-Dotnet @("run", "--project", $emitter, "-c", $Configuration, "--", $rawA)
Invoke-Dotnet @("run", "--project", $emitter, "-c", $Configuration, "--", $rawB)
$rawHashA = (Get-FileHash -LiteralPath $rawA -Algorithm SHA256).Hash
$rawHashB = (Get-FileHash -LiteralPath $rawB -Algorithm SHA256).Hash
if ($rawHashA -ne $rawHashB -or $rawHashA -ne $expectedRaw) {
    Fail "The clean-room raw module does not match the deterministic release artifact."
}

Invoke-Dotnet @("run", "--project", $signer, "-c", $Configuration, "--",
    "self", "--sign", "--in", $rawA, "--out", $signedA)
Invoke-Dotnet @("run", "--project", $signer, "-c", $Configuration, "--",
    "self", "--sign", "--in", $rawB, "--out", $signedB)
$signedHashA = (Get-FileHash -LiteralPath $signedA -Algorithm SHA256).Hash
$signedHashB = (Get-FileHash -LiteralPath $signedB -Algorithm SHA256).Hash
if ($signedHashA -ne $signedHashB -or $signedHashA -ne $expectedSigned) {
    Fail "The signed module does not match the deterministic release artifact."
}
if ((Get-Item -LiteralPath $signedA).Length -ne 4898) {
    Fail "The signed module is not 4,898 bytes."
}

foreach ($artifact in @($rawA, $signedA)) {
    $text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($artifact))
    foreach ($forbidden in @("W:/Build", "J013", "Prospero_Release", "sys/internal")) {
        if ($text.Contains($forbidden)) {
            Fail "Generated output contains forbidden historical path text: $forbidden"
        }
    }
}

New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force | Out-Null
[IO.File]::Copy($signedA, $output, $true)
Set-Content -LiteralPath $manifest `
    -Value "$($signedHashA.ToLowerInvariant()) *libc.prx" -Encoding ascii
Write-Host "Rebuilt deterministic clean-room runtime module."
Write-Host "Raw SHA-256:    $rawHashA"
Write-Host "Signed SHA-256: $signedHashA"
Write-Host "Output:         $output"
Write-Host "Manifest:       $manifest"
