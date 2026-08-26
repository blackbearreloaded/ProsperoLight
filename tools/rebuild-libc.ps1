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
$emitter = Join-Path $root "tooling/ConventionalLibcBuilder/ConventionalLibcBuilder.csproj"
$apiManifest = Join-Path $root "tooling/ConventionalLibcBuilder/api-surface-v2.txt"
$importManifest = Join-Path $root "tooling/ConventionalLibcBuilder/startup-imports-v7.txt"
$signer = Join-Path $root "tooling/NativeAppBuilder/NativeAppBuilder.csproj"
$setupTooling = Join-Path $root "tools/setup-tooling.ps1"
$work = Join-Path $root "build/runtime-shim"
$rawA = Join-Path $work "libc-a.raw.elf"
$rawB = Join-Path $work "libc-b.raw.elf"
$signedA = Join-Path $work "libc-a.prx"
$signedB = Join-Path $work "libc-b.prx"
$emitterOutput = Join-Path $work "dotnet/emitter"
$signerOutput = Join-Path $work "dotnet/signer"
$emitterDll = Join-Path $emitterOutput "ConventionalLibcBuilder.dll"
$signerDll = Join-Path $signerOutput "NativeAppBuilder.dll"
$output = Join-Path $root "runtime/libc.prx"
$manifest = Join-Path $root "runtime/libc.prx.sha256"
$expectedRaw = "FD18A0C7C18BC62144890294DC1BB85C780757D2ED425B1D7FE0BD58AED1ACE2"
$expectedSigned = "E2292D285565937F1DAC09EF5AB742B6027C28D38BA775AD56465AA5594E2A10"

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

foreach ($required in @($emitter, $apiManifest, $importManifest, $signer, $setupTooling)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        Fail "Required source project not found: $required"
    }
}

& $setupTooling

$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = "1"
$env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"
$env:DOTNET_NOLOGO = "1"
New-Item -ItemType Directory -Path $work -Force | Out-Null

Invoke-Dotnet @("build", $emitter, "-c", $Configuration, "-o", $emitterOutput)
Invoke-Dotnet @("build", $signer, "-c", $Configuration, "-o", $signerOutput)
Invoke-Dotnet @($emitterDll,
    "--startup-v7", $apiManifest, $importManifest, $rawA)
Invoke-Dotnet @($emitterDll,
    "--startup-v7", $apiManifest, $importManifest, $rawB)
$rawHashA = (Get-FileHash -LiteralPath $rawA -Algorithm SHA256).Hash
$rawHashB = (Get-FileHash -LiteralPath $rawB -Algorithm SHA256).Hash
if ($rawHashA -ne $rawHashB -or $rawHashA -ne $expectedRaw) {
    Fail "The clean-room raw module does not match the deterministic release artifact."
}

Invoke-Dotnet @($signerDll,
    "self", "--sign", "--in", $rawA, "--out", $signedA)
Invoke-Dotnet @($signerDll,
    "self", "--sign", "--in", $rawB, "--out", $signedB)
$signedHashA = (Get-FileHash -LiteralPath $signedA -Algorithm SHA256).Hash
$signedHashB = (Get-FileHash -LiteralPath $signedB -Algorithm SHA256).Hash
if ($signedHashA -ne $signedHashB -or $signedHashA -ne $expectedSigned) {
    Fail "The signed module does not match the deterministic release artifact."
}
if ((Get-Item -LiteralPath $signedA).Length -ne 1284674) {
    Fail "The signed module is not 1,284,674 bytes."
}

foreach ($artifact in @($rawA, $signedA)) {
    $text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($artifact))
    if (-not $text.Contains("BlackBearReloaded")) {
        Fail "Generated output is missing the BlackBearReloaded attribution marker."
    }
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
