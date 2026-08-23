<#
  ps5-native-app-boilerplate - Native Hello World build wrapper.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later
#>

#requires -Version 5.1
param(
    [string]$Dotnet = "",
    [string]$Python = "",
    [string]$Configuration = "Release",
    [ValidateSet("Folder", "Ffpkg", "Ffpfsc", "All")]
    [string]$OutputFormat = "Folder",
    [switch]$Ffpkg
)

$ErrorActionPreference = "Stop"
$repoBuild = Join-Path $PSScriptRoot "../../build.ps1"
& $repoBuild -ProjectDirectory $PSScriptRoot -Dotnet $Dotnet `
    -Python $Python -Configuration $Configuration -OutputFormat $OutputFormat `
    -Ffpkg:$Ffpkg
