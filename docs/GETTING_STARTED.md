# Getting started

This guide starts from a Windows development PC with no PS5 build tools
installed. It does not configure or modify the PS5.

## 1. Install WSL and Clang 18

Follow Microsoft's [WSL installation guide](https://learn.microsoft.com/windows/wsl/install).
Inside the WSL Linux distribution, install the compiler, archive, and optional
audio-preparation tools:

```bash
sudo apt update
sudo apt install clang-18 lld-18 unzip wget ffmpeg
```

Confirm the expected compiler exists:

```bash
/usr/bin/clang-18 --version
```

## 2. Install the PS5 payload SDK in WSL

This template expects the SDK at `/opt/ps5-payload-sdk`, matching the official
[ps5-payload-dev/sdk quick start](https://github.com/ps5-payload-dev/sdk):

```bash
wget https://github.com/ps5-payload-dev/sdk/releases/latest/download/ps5-payload-sdk.zip
sudo unzip -d /opt ps5-payload-sdk.zip
```

Verify this directory exists:

```bash
test -d /opt/ps5-payload-sdk/target/include && echo SDK ready
```

The payload SDK supplies public headers and Clang target support.

## 3. Install Git and .NET 10 on Windows

Install [Git for Windows](https://git-scm.com/download/win). The first build
uses it to fetch the pinned SharpProspero source into the ignored `.deps/`
cache; later builds reuse that checkout.

Install the Windows x64 SDK from the official
[.NET 10 download page](https://dotnet.microsoft.com/download/dotnet/10.0).
Open a new PowerShell window and check:

```powershell
dotnet --version
```

The C# tooling runs only on the PC. The produced PS5 application contains no
.NET runtime or managed code.

Compressed `.ffpfsc` output additionally requires Python 3.9 or newer. MkPFS
is fetched and installed in the ignored `.deps/` cache only when that format is
selected.

## 4. Use the bundled clean-room loader shim

No proprietary runtime module is required. The repository includes a
4,898-byte `runtime/libc.prx` artifact plus its complete clean-room emitter.
`tools/doctor.ps1` verifies its SHA-256 before a build. See
[Clean-room runtime shim](RUNTIME_SHIM.md) for its scope and reproduction
procedure.

## 5. Choose a unique app identity

Edit [`project.json`](../project.json). Change these fields together:

```json
{
  "titleName": "My Native App",
  "titleId": "PPSA99999",
  "conceptId": "99999",
  "contentId": "UP9000-PPSA99999_00-NATIVEAPP0000001"
}
```

The title ID must be unique among applications already registered on your
console. `contentId` must contain the same title ID and end with exactly 16
uppercase letters or digits.

The template includes an original BlackBear presentation set. The easiest way
to give the app its own identity is:

```powershell
./tools/prepare-assets.ps1 `
    -Icon C:\art\my-icon.png `
    -Background C:\art\my-background.png
```

The generated console files are:

- `sce_sys/icon0.png`: 512x512 launcher icon.
- `sce_sys/pic0.dds`: 3840x2160 BC7 selection background.
- `sce_sys/pic1.dds`: 3840x2160 BC7 selection-background fallback.

`sce_sys/background-source.png`, `pic0.png`, and `pic1.png` are editable
previews; the build deploys only the DDS files. A PNG renamed to `.dds` is not
sufficient.

The normal directory-promotion path displays `titleName` as Shell-rendered text
over this artwork. Retail custom-font Game Hub logos and descriptions are
downloaded asynchronously as Internet catalog metadata; the supported
package-local fields cannot define them for a synthetic homebrew concept. See
[Platform Notes](PLATFORM_NOTES.md).

The default presentation set also includes original selection music. Preparing
MP3/M4A/AAC/WAV input requires FFmpeg plus a legally obtained compatible
ATRAC9 encoder; neither a Sony encoder nor any proprietary SDK tool is bundled
or downloaded. The script also accepts an already encoded `.at9` file.

See [Presentation assets](PRESENTATION_ASSETS.md) for source recommendations,
conversion commands, the supported format profile, licensing guidance,
and the catalog-owned logo/description limitation.

## 6. Check and build

From PowerShell in the repository root:

```powershell
./tools/doctor.ps1
./build.ps1
```

Pass `-Dotnet` to either command if .NET is installed in a nonstandard place:

```powershell
./tools/doctor.ps1 -Dotnet C:\path\to\dotnet.exe
./build.ps1 -Dotnet C:\path\to\dotnet.exe
```

Successful output ends with an app directory such as:

```text
dist/PPSA99999/
  eboot.bin
  sce_module/libc.prx
  sce_sys/icon0.png
  sce_sys/pic0.dds
  sce_sys/pic1.dds
  sce_sys/param.json
  sce_sys/snd0.at9
```

Choose the final output with `-OutputFormat`:

```powershell
./build.ps1 -OutputFormat Folder
./build.ps1 -OutputFormat Ffpkg
./build.ps1 -OutputFormat Ffpfsc
./build.ps1 -OutputFormat All
```

The optional packaging tools are fetched only on first use. See
[Build output formats](FFPKG.md).

Continue with [Deployment](DEPLOYMENT.md).
