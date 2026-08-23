<#
  ps5-native-app-boilerplate - Native application build orchestrator.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later

  Compiles, links, signs, validates, and assembles a directory-style PS5 app.
#>

#requires -Version 5.1
<#
.SYNOPSIS
    Builds a native PS5 application directory from project.json.
.DESCRIPTION
    Compiles C/C++ in WSL with the PS5 payload SDK, links the application with
    the repository-local C# builder, wraps it as FSELF, and assembles dist/.
#>
param(
    [string]$Dotnet = "",
    [string]$Python = "",
    [string]$Configuration = "Release",
    [string]$ProjectDirectory = "",
    [ValidateSet("Folder", "Ffpkg", "Ffpfsc", "All")]
    [string]$OutputFormat = "Folder",
    [switch]$Ffpkg
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = [IO.Path]::GetFullPath($here).TrimEnd('\', '/')
if (-not $ProjectDirectory) {
    $ProjectDirectory = $repoRoot
} elseif (-not [IO.Path]::IsPathRooted($ProjectDirectory)) {
    $ProjectDirectory = Join-Path $repoRoot $ProjectDirectory
}
$ProjectDirectory = [IO.Path]::GetFullPath($ProjectDirectory).TrimEnd('\', '/')
$repoPrefix = $repoRoot + [IO.Path]::DirectorySeparatorChar
if ($ProjectDirectory -ne $repoRoot -and
    -not $ProjectDirectory.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ps5-native-app-boilerplate: ProjectDirectory must stay inside the repository."
}
$projectRelative = if ($ProjectDirectory -eq $repoRoot) {
    ""
} else {
    $ProjectDirectory.Substring($repoPrefix.Length).Replace('\', '/')
}
$projectPath = Join-Path $ProjectDirectory "project.json"
$builderProject = Join-Path $here "tooling/NativeAppBuilder/NativeAppBuilder.csproj"
$setupTooling = Join-Path $here "tools/setup-tooling.ps1"
$setupFfpkgTooling = Join-Path $here "tools/setup-ffpkg-tooling.ps1"
$setupMkpfsTooling = Join-Path $here "tools/setup-mkpfs-tooling.ps1"
$prepareAssets = Join-Path $here "tools/prepare-assets.ps1"
$baseParamPath = Join-Path $ProjectDirectory "sce_sys/param.json"
$iconPath = Join-Path $ProjectDirectory "sce_sys/icon0.png"
$buildRoot = Join-Path $here "build"

function Fail([string]$Message) {
    throw "ps5-native-app-boilerplate: $Message"
}

function Invoke-Dotnet([string[]]$Arguments) {
    & $Dotnet @Arguments
    if ($LASTEXITCODE -ne 0) {
        Fail "The .NET application builder failed."
    }
}

if ($Ffpkg) {
    if ($OutputFormat -notin @("Folder", "Ffpkg")) {
        Fail "-Ffpkg cannot be combined with -OutputFormat $OutputFormat."
    }
    $OutputFormat = "Ffpkg"
}
$buildFfpkg = $OutputFormat -in @("Ffpkg", "All")
$buildFfpfsc = $OutputFormat -in @("Ffpfsc", "All")

foreach ($required in @($projectPath, $builderProject, $setupTooling, $prepareAssets, $baseParamPath, $iconPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        Fail "Required file not found: $required"
    }
}

$project = Get-Content -LiteralPath $projectPath -Raw | ConvertFrom-Json
if ($project.titleId -notmatch '^PPSA[0-9]{5}$') {
    Fail "project.json titleId must match PPSA followed by five digits."
}
if ($project.conceptId -notmatch '^[0-9]{5}$') {
    Fail "project.json conceptId must contain five digits."
}
if ($project.contentId -notmatch '^[A-Z]{2}[0-9]{4}-PPSA[0-9]{5}_00-[A-Z0-9]{16}$') {
    Fail "project.json contentId is not a valid 36-character content ID."
}
if (-not $project.contentId.Contains($project.titleId)) {
    Fail "project.json contentId must contain the configured titleId."
}
foreach ($field in @("moduleSdkVersion", "companionSdkVersion")) {
    if ($project.$field -notmatch '^0x[0-9a-fA-F]{8}$') {
        Fail "project.json $field must be an eight-digit hexadecimal value."
    }
}
if ($project.fselfMagic -notin @("0x1D3D154F", "0xEEF51454")) {
    Fail "project.json fselfMagic is unsupported."
}
if ([string]::IsNullOrWhiteSpace($project.titleName)) {
    Fail "project.json titleName cannot be empty."
}
if ([long]$project.downloadDataSize -lt 0) {
    Fail "project.json downloadDataSize cannot be negative."
}
if (@($project.sources).Count -eq 0) {
    Fail "project.json must list at least one C or C++ source."
}

& $prepareAssets -ValidateOnly -OutputDirectory (Join-Path $ProjectDirectory "sce_sys")

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

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    Fail "WSL was not found. Install WSL and a Linux distribution first."
}
& wsl.exe --exec sh -lc "test -x /usr/bin/clang-18 && test -d /opt/ps5-payload-sdk/target/include"
if ($LASTEXITCODE -ne 0) {
    Fail "WSL needs /usr/bin/clang-18 and /opt/ps5-payload-sdk/target/include."
}

& $setupTooling

foreach ($runtimeModule in @($project.runtimeModules)) {
    if ($runtimeModule.name -notmatch '^[A-Za-z0-9._-]+\.prx$') {
        Fail "Runtime module names must end in .prx."
    }
    if ($runtimeModule.source -notmatch '^(runtime|\.local/runtime)/[A-Za-z0-9._-]+\.prx$') {
        Fail "Runtime module sources must be .prx files under runtime or .local/runtime."
    }
    $runtimeSource = [IO.Path]::GetFullPath((Join-Path $here $runtimeModule.source))
    if (-not (Test-Path -LiteralPath $runtimeSource -PathType Leaf)) {
        Fail "Required runtime module not found: $runtimeSource. See docs/GETTING_STARTED.md."
    }
    if ($runtimeModule.sha256) {
        if ($runtimeModule.sha256 -notmatch '^[0-9a-fA-F]{64}$') {
            Fail "Runtime module sha256 must contain exactly 64 hexadecimal digits."
        }
        $runtimeHash = (Get-FileHash -LiteralPath $runtimeSource -Algorithm SHA256).Hash
        if ($runtimeHash -ne $runtimeModule.sha256) {
            Fail "Runtime module hash mismatch for $($runtimeModule.source)."
        }
    }
}

if (-not $env:DOTNET_CLI_HOME) {
    $env:DOTNET_CLI_HOME = Join-Path $buildRoot "dotnet-home"
}
$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = "1"
$env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"
$env:DOTNET_NOLOGO = "1"

if ($here -notmatch '^([A-Za-z]):\\(.*)$') {
    Fail "The repository must be on a Windows drive visible to WSL."
}
$drive = $Matches[1].ToLowerInvariant()
$tail = $Matches[2].Replace('\', '/')
$wslRoot = "/mnt/$drive/$tail"

$compileDefinitions = @($project.compileDefinitions) | Where-Object { $_ }
foreach ($definition in $compileDefinitions) {
    if ($definition -notmatch '^[A-Za-z_][A-Za-z0-9_]*(=[A-Za-z0-9_]+)?$') {
        Fail "Invalid compile definition: $definition"
    }
}
$includePaths = @($project.includePaths) | Where-Object { $_ }
foreach ($includePath in $includePaths) {
    if ($includePath -notmatch '^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$') {
        Fail "Invalid include path: $includePath"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $here $includePath) -PathType Container)) {
        Fail "Include directory not found: $includePath"
    }
}
$staticArchives = @($project.staticArchives) | Where-Object { $_ }
foreach ($archive in $staticArchives) {
    if ($archive -notmatch '^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*\.a$') {
        Fail "Invalid static archive path: $archive"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $here $archive) -PathType Leaf)) {
        Fail "Static archive not found: $archive"
    }
}

$objectRoot = Join-Path $buildRoot "obj"
New-Item -ItemType Directory -Path $objectRoot -Force | Out-Null
$objects = @()
foreach ($source in @($project.sources)) {
    if ($source -notmatch '^src/[A-Za-z0-9_./-]+\.(c|cc|cpp)$') {
        Fail "Invalid source path: $source"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $ProjectDirectory $source) -PathType Leaf)) {
        Fail "Source file not found: $source"
    }

    $sourceFromRepo = if ($projectRelative) {
        "$projectRelative/$source"
    } else {
        $source
    }
    $objectName = ($sourceFromRepo -replace '[^A-Za-z0-9_.-]', '_') + ".o"
    $objectPath = Join-Path $objectRoot $objectName
    $wslObject = "build/obj/$objectName"
    $languageFlags = if ([IO.Path]::GetExtension($source) -eq ".c") {
        "-std=c11"
    } else {
        "-std=c++20 -fno-exceptions -fno-rtti"
    }
    $definitionFlags = ($compileDefinitions | ForEach-Object { "-D$_" }) -join " "
    $includeFlags = ($includePaths | ForEach-Object { "-I$_" }) -join " "
    $compile = "cd '$wslRoot' && sh tooling/prospero-clang18 $languageFlags -O2 -Wall -Wextra -ffunction-sections -fdata-sections $definitionFlags $includeFlags -c '$sourceFromRepo' -o '$wslObject'"
    & wsl.exe --exec bash -lc $compile
    if ($LASTEXITCODE -ne 0) {
        Fail "PS5 compilation failed for $source."
    }
    $objects += $objectPath
}

$rawModule = Join-Path $buildRoot "eboot.elf"
$appRoot = Join-Path $here "dist"
$app = Join-Path $appRoot $project.titleId
$resolvedApp = [IO.Path]::GetFullPath($app)
$resolvedDist = [IO.Path]::GetFullPath($appRoot) + [IO.Path]::DirectorySeparatorChar
if (-not $resolvedApp.StartsWith($resolvedDist, [StringComparison]::OrdinalIgnoreCase)) {
    Fail "Refusing to write outside the repository dist directory."
}
if (Test-Path -LiteralPath $resolvedApp) {
    Remove-Item -LiteralPath $resolvedApp -Recurse -Force
}
New-Item -ItemType Directory -Path (Join-Path $app "sce_sys") -Force | Out-Null

$linkArguments = @("run", "--project", $builderProject, "-c", $Configuration, "--",
    "link", "--self-contained")
foreach ($object in $objects) {
    $linkArguments += @("--obj", $object)
}
foreach ($archive in $staticArchives) {
    $linkArguments += @("--lib", [IO.Path]::GetFullPath((Join-Path $here $archive)))
}
$linkArguments += @(
    "--out", $rawModule,
    "--entry", "_start",
    "--module-sdk", $project.moduleSdkVersion,
    "--companion-sdk", $project.companionSdkVersion
)
Invoke-Dotnet $linkArguments

$module = Join-Path $app "eboot.bin"
Invoke-Dotnet @("run", "--project", $builderProject, "-c", $Configuration, "--",
    "self", "--sign", "--in", $rawModule, "--out", $module,
    "--magic", $project.fselfMagic)

$param = Get-Content -LiteralPath $baseParamPath -Raw | ConvertFrom-Json
$param.titleId = $project.titleId
$param.conceptId = $project.conceptId
$param.contentId = $project.contentId
$param.localizedParameters.'en-US'.titleName = $project.titleName
$param.downloadDataSize = [long]$project.downloadDataSize
$param | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath (Join-Path $app "sce_sys/param.json") -Encoding utf8
[IO.File]::Copy($iconPath, (Join-Path $app "sce_sys/icon0.png"), $true)
foreach ($assetName in @("pic0.dds", "pic1.dds", "snd0.at9")) {
    $assetPath = Join-Path $ProjectDirectory "sce_sys/$assetName"
    if (Test-Path -LiteralPath $assetPath -PathType Leaf) {
        [IO.File]::Copy($assetPath, (Join-Path $app "sce_sys/$assetName"), $true)
    }
}
$assetDirectory = Join-Path $ProjectDirectory "assets"
if (Test-Path -LiteralPath $assetDirectory -PathType Container) {
    Copy-Item -LiteralPath $assetDirectory -Destination (Join-Path $app "assets") -Recurse -Force
}

foreach ($runtimeModule in @($project.runtimeModules)) {
    $runtimeSource = [IO.Path]::GetFullPath((Join-Path $here $runtimeModule.source))
    $runtimeDirectory = Join-Path $app "sce_module"
    $runtimeOutput = Join-Path $runtimeDirectory $runtimeModule.name
    New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null
    $runtimeBytes = [IO.File]::ReadAllBytes($runtimeSource)
    $runtimeMagic = if ($runtimeBytes.Length -ge 4) {
        [BitConverter]::ToUInt32($runtimeBytes, 0)
    } else {
        [uint32]0
    }
    if ($runtimeMagic -in @([uint32]0x1D3D154F, [uint32]4009038932)) {
        [IO.File]::Copy($runtimeSource, $runtimeOutput, $true)
    } else {
        Invoke-Dotnet @("run", "--project", $builderProject, "-c", $Configuration, "--",
            "self", "--sign", "--in", $runtimeSource, "--out", $runtimeOutput)
    }
    Invoke-Dotnet @("run", "--project", $builderProject, "-c", $Configuration, "--",
        "self", "--inspect", "--file", $runtimeOutput)
}

Invoke-Dotnet @("run", "--project", $builderProject, "-c", $Configuration, "--",
    "self", "--inspect", "--file", $module)
& (Join-Path $here "tools/inspect.ps1") $module
if ($LASTEXITCODE -ne 0) {
    Fail "Static FSELF compatibility inspection failed."
}

$ffpkgOutput = ""
if ($buildFfpkg) {
    if (-not (Test-Path -LiteralPath $setupFfpkgTooling -PathType Leaf)) {
        Fail "Optional FFPKG bootstrapper not found: $setupFfpkgTooling"
    }
    $ufs2Assembly = & $setupFfpkgTooling -Dotnet $Dotnet
    $ffpkgOutput = Join-Path $appRoot "$($project.titleId).ffpkg"
    $resolvedFfpkg = [IO.Path]::GetFullPath($ffpkgOutput)
    if (-not $resolvedFfpkg.StartsWith($resolvedDist, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetExtension($resolvedFfpkg) -ne ".ffpkg") {
        Fail "Refusing to write FFPKG outside the repository dist directory."
    }
    if (Test-Path -LiteralPath $resolvedFfpkg -PathType Leaf) {
        [IO.File]::Delete($resolvedFfpkg)
    }

    $previousRollForward = $env:DOTNET_ROLL_FORWARD
    try {
        $env:DOTNET_ROLL_FORWARD = "Major"
        & $Dotnet $ufs2Assembly newfs -O 2 -b 32768 -f 4096 -D $app $resolvedFfpkg
        if ($LASTEXITCODE -ne 0) {
            Fail "UFS2Tool FFPKG creation failed."
        }
        & $Dotnet $ufs2Assembly fsck_ufs -n $resolvedFfpkg
        if ($LASTEXITCODE -ne 0) {
            Fail "Generated FFPKG failed its read-only UFS2 consistency check."
        }
    } finally {
        if ($null -eq $previousRollForward) {
            Remove-Item Env:DOTNET_ROLL_FORWARD -ErrorAction SilentlyContinue
        } else {
            $env:DOTNET_ROLL_FORWARD = $previousRollForward
        }
    }
}

$ffpfscOutput = ""
if ($buildFfpfsc) {
    if (-not (Test-Path -LiteralPath $setupMkpfsTooling -PathType Leaf)) {
        Fail "Optional MkPFS bootstrapper not found: $setupMkpfsTooling"
    }
    $mkpfsPython = & $setupMkpfsTooling -Python $Python
    $ffpfscOutput = Join-Path $appRoot "$($project.titleId).ffpfsc"
    $resolvedFfpfsc = [IO.Path]::GetFullPath($ffpfscOutput)
    if (-not $resolvedFfpfsc.StartsWith($resolvedDist, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetExtension($resolvedFfpfsc) -ne ".ffpfsc") {
        Fail "Refusing to write FFPFSC outside the repository dist directory."
    }
    if (Test-Path -LiteralPath $resolvedFfpfsc -PathType Leaf) {
        [IO.File]::Delete($resolvedFfpfsc)
    }

    & $mkpfsPython -m mkpfs pack folder --no-adjust-output-file-extension `
        --version PS5 --verify $app $resolvedFfpfsc
    if ($LASTEXITCODE -ne 0) {
        Fail "MkPFS FFPFSC creation or verification failed."
    }
}

Write-Host ""
Write-Host "Build complete."
Write-Host "Raw ELF:     $rawModule"
Write-Host "App folder:  $app"
if ($ffpkgOutput) {
    Write-Host "FFPKG image: $ffpkgOutput"
}
if ($ffpfscOutput) {
    Write-Host "FFPFSC image: $ffpfscOutput"
}
Write-Host "Stage one complete output supported by your loader, not only eboot.bin."
