# PS5 Native App Boilerplate

[![Build](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/actions/workflows/tooling.yml/badge.svg)](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/actions/workflows/tooling.yml)
[![Release](https://img.shields.io/github/v/release/blackbearreloaded/ps5-native-app-boilerplate)](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/releases)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

Build native C and C++ homebrew applications for PlayStation 5 from Windows.
The template compiles with Clang in WSL, links a native PS5 ELF, wraps it as a
development FSELF, and assembles a complete title directory for a
directory-based homebrew loader.

The generated application is native code. C# and .NET are used only by the
host-side build tools. Pinned upstream dependencies are fetched on demand into
the ignored `.deps/` cache; no Sony SDK files or proprietary runtime modules
are included.

## Project status

| Area | Status |
| --- | --- |
| Host build | Verified on Windows with PowerShell, WSL, Clang 18, and .NET 10 |
| PS5 hardware | Exact V7 runtime verified on firmware 6.02 and 12.70 with ShadowMountPlus |
| Runtime shim | Project-authored, reproducible V7 artifact with no proprietary implementation code |
| Output formats | Title folder, UFS2 `.ffpkg`, and compressed `.ffpfsc` |
| CI | Builds the host tooling and reproduces the bundled runtime shim |

Firmware and homebrew-loader behavior vary. The bundled V7 artifact is verified
on 6.02 and 12.70; validate the exact artifact on other target environments
before distribution. Do not present two tested versions as universal firmware
compatibility.

## What is included

| Feature | Included implementation |
| --- | --- |
| Native build | C11 and basic C++20 compilation through the PS5 payload SDK |
| Linking and FSELF | Pinned SharpProspero revision plus a focused compatibility patch |
| Runtime companion | Source-reproducible, independently authored `libc.prx` loader shim |
| Packaging | Folder, optional UFS2 `.ffpkg`, and optional compressed `.ffpfsc` outputs |
| App assets | Recursive read-only `assets/` packaging at `/app0/assets/` |
| Presentation | Replaceable icon, 4K BC7 backgrounds, and ATRAC9 selection audio |
| Example | Graphical Hello World with CPU-rendered text, shapes, and packaged data |
| Validation | Host prerequisite checks and static ELF/FSELF inspection before release |

## Quick start

### Prerequisites

- [Git for Windows](https://git-scm.com/download/win)
- [WSL](https://learn.microsoft.com/windows/wsl/install)
- Clang 18 inside WSL
- [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) at
  `/opt/ps5-payload-sdk`
- [.NET 10 SDK](https://dotnet.microsoft.com/download/dotnet/10.0)

### Build the template

1. Give the application a unique title ID, content ID, and name in
   [`project.json`](project.json).
2. Check the host prerequisites:

   ```powershell
   ./tools/doctor.ps1
   ```

3. Build the application:

   ```powershell
   ./build.ps1
   ```

The finished title directory is written to `dist/<TITLE_ID>/`. Stage that
entire directory with a compatible loader such as
[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus); `eboot.bin`
cannot be deployed by itself.

Select an optional package format when needed:

```powershell
./build.ps1 -OutputFormat Ffpkg
./build.ps1 -OutputFormat Ffpfsc
./build.ps1 -OutputFormat All
```

If `dotnet` is not on `PATH`, pass its executable explicitly:

```powershell
./build.ps1 -Dotnet C:\path\to\dotnet.exe
```

Read [Getting started](docs/GETTING_STARTED.md) before the first build.

## Customize the application

### Source and metadata

Edit `project.json` to define the app identity, sources, compiler definitions,
include paths, static archives, and runtime modules. The default program is
[`src/main.c`](src/main.c); the complete graphical sample lives under
[`examples/hello-world`](examples/hello-world/README.md).

### Read-only application assets

Put fonts, images, configuration defaults, shaders, and other packaged data
under `assets/`. The build copies the directory recursively without
conversion:

```text
assets/fonts/ui.bin  ->  /app0/assets/fonts/ui.bin
```

Open packaged files through absolute `/app0/assets/...` paths. `/app0` is
read-only; writable application state belongs under `/download0`. The Hello
World example loads and renders `assets/banner.txt` at runtime.

### Presentation assets

Replace the icon and background with one command:

```powershell
./tools/prepare-assets.ps1 `
    -Icon C:\art\icon.png `
    -Background C:\art\background.png
```

The asset tool validates the required dimensions and prepares the PS5-facing
formats. Audio conversion and the verified Shell limits are documented in
[Presentation assets](docs/PRESENTATION_ASSETS.md).

## Build outputs

| Output | Purpose |
| --- | --- |
| `dist/<TITLE_ID>/` | Complete directory-style application |
| `dist/<TITLE_ID>.ffpkg` | Optional uncompressed UFS2 image |
| `dist/<TITLE_ID>.ffpfsc` | Optional compressed UFS2 image |
| `build/` | Generated compiler, linker, and validation intermediates |

`build/`, `dist/`, `.deps/`, and `.local/` are intentionally ignored.

## Repository layout

```text
project.json                  App identity and build inputs
src/main.c                    Minimal native application
assets/                       Optional files mounted at /app0/assets/
sce_sys/                      Param, icon, backgrounds, and selection audio
examples/hello-world/         Complete graphical native example
build.ps1                     Windows/WSL build entry point
tools/doctor.ps1              Read-only prerequisite check
tools/inspect.ps1             Static ELF/FSELF validator
tools/prepare-assets.ps1      Presentation conversion and validation
tooling/NativeAppBuilder/     Host-side link and FSELF frontend
tooling/ConventionalLibcBuilder/ Clean-room V7 runtime emitter and manifests
tooling/patches/              SharpProspero compatibility delta
runtime/libc.prx              Bundled cross-firmware-tested V7 loader shim
tools/rebuild-libc.ps1        Deterministic shim reproduction check
```

## Documentation

| Document | Purpose |
| --- | --- |
| [Getting started](docs/GETTING_STARTED.md) | Host setup, first configuration, build, and output inspection |
| [Project configuration](docs/CONFIGURATION.md) | `project.json`, sources, compiler inputs, archives, and runtime modules |
| [Presentation assets](docs/PRESENTATION_ASSETS.md) | Icons, backgrounds, ATRAC9 audio, and format limits |
| [Build output formats](docs/FFPKG.md) | Folder, `.ffpkg`, and `.ffpfsc` generation |
| [C# build tooling](docs/CSHARP_TOOLING.md) | Host-side linker and FSELF command surface |
| [Clean-room runtime shim](docs/RUNTIME_SHIM.md) | Design, hashes, compatibility, and deterministic reproduction |
| [Deployment](docs/DEPLOYMENT.md) | Directory staging and smoke testing |
| [Platform constraints](docs/PLATFORM_NOTES.md) | Loader, filesystem, presentation, and capability boundaries |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Common setup, build, packaging, and launcher failures |
| [Contributing](CONTRIBUTING.md) | Change requirements and release checks |

## External projects and tools

| Project | Role |
| --- | --- |
| [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) | Public PS5 headers, sysroot, and Clang target support |
| [SvenGDK/SharpProspero](https://github.com/SvenGDK/SharpProspero) | Linker, import catalog, ELF inspection, and development FSELF writer |
| [SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) | Optional uncompressed UFS2 `.ffpkg` generation |
| [PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) | Optional compressed `.ffpfsc` generation |
| [sinajet/PSFFPKG](https://github.com/sinajet/PSFFPKG) | Public `.ffpkg` procedure used as a format reference |
| [LLVM/Clang](https://github.com/llvm/llvm-project) | Native compiler |
| [.NET SDK](https://github.com/dotnet/sdk) | Host-side build runtime |
| [Microsoft DirectXTex](https://github.com/microsoft/DirectXTex) | `texconv` presentation-image preparation |
| [FFmpeg](https://ffmpeg.org/) | Developer-supplied selection-audio preparation |
| [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus) | Directory-style deployment and hardware validation |

Exact dependency pins and license notes are recorded in [NOTICE.md](NOTICE.md).

## Scope

This project builds a directory-style homebrew application and optional UFS2
images. It does not create a signed retail PKG/FPKG, automate an exploit, alter
console configuration, or bundle Sony files. GPU decoding and a general-purpose
C library are outside this foundation.

## Contributing

Contributions are welcome. Keep the template small, reproducible, and useful to
a first-time native-app developer. See [CONTRIBUTING.md](CONTRIBUTING.md) for
the required checks.

## License and attribution

Repository-authored code is licensed under GPL-3.0-or-later. The build fetches
[SharpProspero](https://github.com/SvenGDK/SharpProspero) at pinned commit
[`e36e610`](https://github.com/SvenGDK/SharpProspero/commit/e36e610fa5b4be23ad38b9c8429f11f11750cc0c)
and applies the tracked compatibility patch. The fetched dependency remains
under its upstream GPL-3.0 license. See [LICENSE](LICENSE) and
[NOTICE.md](NOTICE.md).

This project was developed with assistance from OpenAI Codex. Project
maintainers reviewed and validated the resulting code and documentation.

PlayStation and PS5 are trademarks of Sony Interactive Entertainment. This
project is independent and is not affiliated with or endorsed by Sony.
