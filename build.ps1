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
    Compiles C/C++ in WSL with the PS5 payload SDK, links with LLVM lld,
    converts and wraps the result with the repository's C++ host tool, and
    assembles dist/.
#>
param(
    [string]$Python = "",
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
$nativeToolDirectory = Join-Path $here "tooling/native"
$setupNativeDependencies = Join-Path $here "tools/setup-native-dependencies.ps1"
$rebuildLibc = Join-Path $here "tools/rebuild-libc.ps1"
$setupFfpkgTooling = Join-Path $here "tools/setup-ffpkg-tooling.ps1"
$setupMkpfsTooling = Join-Path $here "tools/setup-mkpfs-tooling.ps1"
$prepareAssets = Join-Path $here "tools/prepare-assets.ps1"
$baseParamPath = Join-Path $ProjectDirectory "sce_sys/param.json"
$iconPath = Join-Path $ProjectDirectory "sce_sys/icon0.png"
$buildRoot = Join-Path $here "build"

function Fail([string]$Message) {
    throw "ps5-native-app-boilerplate: $Message"
}

function Invoke-WslTool([string[]]$Arguments) {
    & wsl.exe --exec @Arguments
    if ($LASTEXITCODE -ne 0) {
        Fail "A native build tool failed."
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

foreach ($required in @($projectPath, $prepareAssets, $baseParamPath, $iconPath,
        $setupNativeDependencies,
        $rebuildLibc,
        (Join-Path $nativeToolDirectory "native_app_builder.cpp"),
        (Join-Path $nativeToolDirectory "libc_builder.cpp"),
        (Join-Path $nativeToolDirectory "ps5-pie.ld"))) {
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
if ($project.applicationCategory -notin @("game", "media")) {
    Fail "project.json applicationCategory must be game or media."
}
if ($project.contentVersion -notmatch '^[0-9]{2}\.[0-9]{3}\.[0-9]{3}$') {
    Fail "project.json contentVersion must use NN.NNN.NNN."
}
if ($project.masterVersion -notmatch '^[0-9]{2}\.[0-9]{2}$') {
    Fail "project.json masterVersion must use NN.NN."
}
if ([long]$project.downloadDataSize -lt 0) {
    Fail "project.json downloadDataSize cannot be negative."
}
if (@($project.sources).Count -eq 0) {
    Fail "project.json must list at least one C or C++ source."
}

$defaultRuntime = Join-Path $here "runtime/libc.prx"
$usesDefaultRuntime = @($project.runtimeModules | Where-Object {
        $_.source -eq "runtime/libc.prx"
    }).Count -gt 0
if ($usesDefaultRuntime -and -not (Test-Path -LiteralPath $defaultRuntime -PathType Leaf)) {
    & $rebuildLibc
}

& $prepareAssets -ValidateOnly -OutputDirectory (Join-Path $ProjectDirectory "sce_sys")

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    Fail "WSL was not found. Install WSL and a Linux distribution first."
}
& wsl.exe --exec sh -lc "test -x /usr/bin/clang-18 && test -x /usr/bin/clang++ && test -x /usr/bin/wget && test -x /usr/bin/unzip && test -x /usr/bin/apt-get && test -x /usr/bin/dpkg-deb"
if ($LASTEXITCODE -ne 0) {
    Fail "WSL needs Clang/Clang++, wget, unzip, apt-get, and dpkg-deb."
}
$dependencyJson = & $setupNativeDependencies
$nativeDependencies = ($dependencyJson -join "`n") | ConvertFrom-Json
$sdkRoot = [string]$nativeDependencies.sdkRoot
$zlibInclude = [string]$nativeDependencies.zlibInclude
$zlibArchive = [string]$nativeDependencies.zlibArchive
if (-not $sdkRoot -or -not $zlibInclude -or -not $zlibArchive) {
    Fail "Native dependency bootstrap returned incomplete paths."
}

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

if ($here -notmatch '^([A-Za-z]):\\(.*)$') {
    Fail "The repository must be on a Windows drive visible to WSL."
}
$drive = $Matches[1].ToLowerInvariant()
$tail = $Matches[2].Replace('\', '/')
$wslRoot = "/mnt/$drive/$tail"

function Convert-ToWslPath([string]$Path) {
    $absolute = [IO.Path]::GetFullPath($Path)
    if ($absolute -notmatch '^([A-Za-z]):\\(.*)$') {
        Fail "Path is not on a Windows drive visible to WSL: $absolute"
    }
    $pathDrive = $Matches[1].ToLowerInvariant()
    $pathTail = $Matches[2].Replace('\', '/')
    return "/mnt/$pathDrive/$pathTail"
}

$hostToolRoot = Join-Path $buildRoot "host"
New-Item -ItemType Directory -Path $hostToolRoot -Force | Out-Null
$nativeTool = Join-Path $hostToolRoot "ps5-native-tool"
$wslNativeTool = Convert-ToWslPath $nativeTool
$nativeSources = @(
    "native_app_builder.cpp",
    "self_container.cpp",
    "elf_object.cpp",
    "sce_module_writer.cpp"
) | ForEach-Object { Convert-ToWslPath (Join-Path $nativeToolDirectory $_) }
Invoke-WslTool (@("clang++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror") +
    @("-I", $zlibInclude) + $nativeSources +
    @($zlibArchive, "-o", $wslNativeTool))

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
    $compile = "cd '$wslRoot' && PS5_PAYLOAD_SDK='$sdkRoot' sh tooling/prospero-clang18 $languageFlags -O2 -Wall -Wextra -ffunction-sections -fdata-sections $definitionFlags $includeFlags -c '$sourceFromRepo' -o '$wslObject'"
    & wsl.exe --exec bash -lc $compile
    if ($LASTEXITCODE -ne 0) {
        Fail "PS5 compilation failed for $source."
    }
    $objects += $objectPath
}

$compileCrt = "cd '$wslRoot' && PS5_PAYLOAD_SDK='$sdkRoot' sh tooling/prospero-clang18 -std=c11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections -c tooling/native/app_crt.c -o build/obj/app_crt.o"
& wsl.exe --exec bash -lc $compileCrt
if ($LASTEXITCODE -ne 0) {
    Fail "PS5 startup object compilation failed."
}

$compileCppRuntime = "cd '$wslRoot' && PS5_PAYLOAD_SDK='$sdkRoot' sh tooling/prospero-clang18 -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -c tooling/native/app_cpp_runtime.cpp -o build/obj/app_cpp_runtime.o"
& wsl.exe --exec bash -lc $compileCppRuntime
if ($LASTEXITCODE -ne 0) {
    Fail "PS5 C++ allocation runtime compilation failed."
}

$rawModule = Join-Path $buildRoot "eboot.elf"
$intermediateModule = Join-Path $buildRoot "llvm-pie.elf"
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

$linkInputs = @("build/obj/app_crt.o", "build/obj/app_cpp_runtime.o")
foreach ($object in $objects) {
    $linkInputs += Convert-ToWslPath $object
}
foreach ($archive in $staticArchives) {
    $linkInputs += Convert-ToWslPath ([IO.Path]::GetFullPath((Join-Path $here $archive))
    )
}
$quotedInputs = ($linkInputs | ForEach-Object { "'$_'" }) -join " "
$nativeLink = "cd '$wslRoot' && '$sdkRoot/bin/prospero-lld' -T tooling/native/ps5-pie.ld --eh-frame-hdr --version-script tooling/native/app-symbols.map -e _start -o build/llvm-pie.elf $quotedInputs --as-needed '$sdkRoot'/target/lib/*.so"
& wsl.exe --exec bash -lc $nativeLink
if ($LASTEXITCODE -ne 0) {
    Fail "LLVM application link failed."
}

Invoke-WslTool @($wslNativeTool, "link",
    "--in", (Convert-ToWslPath $intermediateModule),
    "--out", (Convert-ToWslPath $rawModule),
    "--stub-dir", "$sdkRoot/target/lib",
    "--module-sdk", $project.moduleSdkVersion,
    "--companion-sdk", $project.companionSdkVersion,
    "--file-name", "eboot.elf")

$module = Join-Path $app "eboot.bin"
Invoke-WslTool @($wslNativeTool, "self", "--sign",
    "--in", (Convert-ToWslPath $rawModule), "--out", (Convert-ToWslPath $module),
    "--magic", $project.fselfMagic)

$param = Get-Content -LiteralPath $baseParamPath -Raw | ConvertFrom-Json
$param.titleId = $project.titleId
$param.conceptId = $project.conceptId
$param.contentId = $project.contentId
$param.contentVersion = $project.contentVersion
$param.masterVersion = $project.masterVersion
$param.localizedParameters.'en-US'.titleName = $project.titleName
$param.downloadDataSize = [long]$project.downloadDataSize
if ($project.applicationCategory -eq "game") {
    $param.applicationCategoryType = 0
    $param.contentBadgeType = 1
    $gameIntent = [PSCustomObject]@{
        permittedIntents = @([PSCustomObject]@{ intentType = "launchActivity" })
    }
    $param | Add-Member -NotePropertyName gameIntent -NotePropertyValue $gameIntent -Force
} else {
    $param.applicationCategoryType = 65536
    $param.contentBadgeType = 2
    $param.PSObject.Properties.Remove("gameIntent")
}
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
        Invoke-WslTool @($wslNativeTool, "self", "--sign",
            "--in", (Convert-ToWslPath $runtimeSource),
            "--out", (Convert-ToWslPath $runtimeOutput))
    }
    Invoke-WslTool @($wslNativeTool, "self", "--inspect",
        "--file", (Convert-ToWslPath $runtimeOutput))
}

Invoke-WslTool @($wslNativeTool, "self", "--inspect",
    "--file", (Convert-ToWslPath $module))
& (Join-Path $here "tools/inspect.ps1") $module
if ($LASTEXITCODE -ne 0) {
    Fail "Static FSELF compatibility inspection failed."
}

$ffpkgOutput = ""
if ($buildFfpkg) {
    if (-not (Test-Path -LiteralPath $setupFfpkgTooling -PathType Leaf)) {
        Fail "Optional FFPKG bootstrapper not found: $setupFfpkgTooling"
    }
    $makefs = & $setupFfpkgTooling
    if ($makefs -is [array]) { $makefs = $makefs[-1] }
    $ffpkgOutput = Join-Path $appRoot "$($project.titleId).ffpkg"
    $resolvedFfpkg = [IO.Path]::GetFullPath($ffpkgOutput)
    if (-not $resolvedFfpkg.StartsWith($resolvedDist, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetExtension($resolvedFfpkg) -ne ".ffpkg") {
        Fail "Refusing to write FFPKG outside the repository dist directory."
    }
    if (Test-Path -LiteralPath $resolvedFfpkg -PathType Leaf) {
        [IO.File]::Delete($resolvedFfpkg)
    }

    Invoke-WslTool @($makefs, "-S", "4096", "-b", "20%", "-t", "ffs", "-o",
        "version=2,bsize=32768,fsize=4096,minfree=0,optimization=space",
        (Convert-ToWslPath $resolvedFfpkg), (Convert-ToWslPath $app))
    if (-not (Test-Path -LiteralPath $resolvedFfpkg -PathType Leaf) -or
        (Get-Item -LiteralPath $resolvedFfpkg).Length -le 0) {
        Fail "Native makefs did not produce an FFPKG image."
    }
    $ufsHeader = [IO.File]::ReadAllBytes($resolvedFfpkg)
    if ($ufsHeader.Length -lt 0x2560 -or
        [BitConverter]::ToUInt32($ufsHeader, 0x255c) -ne 0x19540119) {
        Fail "Generated FFPKG does not contain the expected UFS2 superblock magic."
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
