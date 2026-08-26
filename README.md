# PS5 Native App Boilerplate

[![Build](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/actions/workflows/tooling.yml/badge.svg)](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/actions/workflows/tooling.yml)
[![Release](https://img.shields.io/github/v/release/blackbearreloaded/ps5-native-app-boilerplate)](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/releases)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

Build modern C++20 homebrew applications for PlayStation 5 from Linux, WSL, or
Windows. The repository root is a complete graphical Hello World skeleton:
fork it, edit `src/main.cpp`, and build a deployable title. C sources and C
libraries remain supported at explicit ABI boundaries.

The application and all repository-owned build tools are native C/C++. LLVM
handles ordinary linking; a small project-owned converter emits PS5 metadata
and the development FSELF. The public SDK, static zlib, and optional packaging
dependencies are fetched on demand into the ignored `.deps/` cache. No
C#/.NET toolchain, proprietary Sony SDK file, or proprietary runtime module is
required.

## Project status

| Area | Status |
| --- | --- |
| Host build | C/C++ pipeline verified through Make on Linux/WSL and PowerShell on Windows |
| PS5 hardware | Current C++20 skeleton and runtime verified on firmware 6.02 and 12.70 |
| Runtime shim | Project-authored, reproducible artifact with no proprietary implementation code |
| Output formats | Title folder, UFS2 `.ffpkg`, and compressed `.ffpfsc` |
| CI | Runs the native Linux Make workflow and reproduces the runtime shim |

Firmware and homebrew-loader behavior vary. The generated runtime is verified
on 6.02 and 12.70; validate the exact artifact on other target environments
before distribution. Do not present two tested versions as universal firmware
compatibility.

## What is included

| Feature | Included implementation |
| --- | --- |
| Native build | C++20 with RAII, libc++ headers, unique ownership, and C-library interoperability |
| Linking and FSELF | LLVM lld plus the repository-owned C++ PS5 converter and FSELF writer |
| Runtime companion | Source-reproducible, independently authored `libc.prx` loader shim |
| Packaging | Folder, optional UFS2 `.ffpkg`, and optional compressed `.ffpfsc` outputs |
| App assets | Recursive read-only `assets/` packaging at `/app0/assets/` |
| Presentation | Replaceable icon, 4K BC7 backgrounds, and ATRAC9 selection audio |
| Root skeleton | C++20 graphical Hello World with RAII, bounded views, unique ownership, CPU-rendered text, shapes, and packaged data |
| Validation | Host prerequisite checks and static ELF/FSELF inspection before release |

## Quick start

### Prerequisites

On Ubuntu, Debian, or WSL install Git, Make, Python 3 with virtual-environment
support, Clang 18, lld 18, clang-format, clang-tidy, `wget`, and `unzip`.
Windows users may use the same WSL path or the retained PowerShell frontend.

```bash
sudo apt update
sudo apt install git make python3 python3-pip python3-venv wget unzip \
  clang-18 clang-format-18 clang-tidy-18 lld-18
```

The optional presentation-asset converter also uses FFmpeg. Packaging tools
are fetched into `.deps/` automatically when their corresponding Make targets
are requested.

The first build fetches the pinned public PS5 payload SDK and native zlib
archive into the ignored `.deps/native/` cache, then generates the clean-room
`runtime/libc.prx` from source.

### Build the template

1. Give the application a unique title ID, content ID, and name in
   [`project.json`](project.json).
2. Build:

   ```bash
   make
   ```

Bare `make` fetches missing native dependencies, generates and verifies
`runtime/libc.prx`, then builds the complete application folder.

The finished title directory is written to `dist/<TITLE_ID>/`. Stage that
entire directory with a compatible loader such as
[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus); `eboot.bin`
cannot be deployed by itself.

Run `make help` to list the focused targets. `make deps` only prefetches native
dependencies, `make libc` forces runtime reproduction, `make lint` runs
clang-format and clang-tidy, and `make packages` emits the folder, `.ffpkg`,
and `.ffpfsc` forms. On Windows PowerShell, `./build.ps1` and
`./tools/rebuild-libc.ps1` remain equivalent supported entry points.

Read [Getting started](docs/GETTING_STARTED.md) before the first build.

## Customize the application

### Source and metadata

Edit `project.json` to define the app identity, sources, compiler definitions,
include paths, static archives, and runtime modules. The hardware-proven
graphical skeleton is [`src/main.cpp`](src/main.cpp); replace or extend it directly
after forking the repository.

The target uses C++20 with exceptions and RTTI disabled. Allocation-free
facilities such as `std::array`, `std::span`, and `std::string_view` are
available, and the repository-owned allocation bridge supports
`std::unique_ptr`, `new`, and `delete` through the clean-room runtime. Prefer
values and unique ownership; `std::shared_ptr` and the complete libc++ runtime
are intentionally outside the baseline.

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
| `dist/<TITLE_ID>.ffpfsc` | Optional compressed PFS image |
| `build/` | Generated compiler, linker, and validation intermediates |
| `runtime/libc.prx` | Generated loader shim; also copied to `sce_module/` |

`build/`, `dist/`, `.deps/`, and `.local/` are intentionally ignored.
`runtime/libc.prx` is generated by `make`, ignored by Git, copied into the app
folder, and published as a standalone GitHub Release asset. Its expected digest
remains tracked in `runtime/libc.prx.sha256`.

## Repository layout

```text
project.json                  App identity and build inputs
src/main.cpp                  Modern C++20 graphical Hello World skeleton
assets/                       Optional files mounted at /app0/assets/
sce_sys/                      Param, icon, backgrounds, and selection audio
Makefile                      Linux/WSL build, lint, dependency, and package targets
build.ps1                     Windows PowerShell build entry point
tools/build.sh                Native Linux/WSL build orchestrator
tools/doctor.ps1              Read-only prerequisite check
tools/inspect.ps1             Static ELF/FSELF validator
tools/prepare-assets.ps1      Presentation conversion and validation
tooling/native/               C++ linker converter, allocation bridge, C ABI CRT, FSELF, and runtime builder
runtime/libc.prx.sha256       Expected digest for the generated loader shim
tools/rebuild-libc.sh         Linux/WSL deterministic shim reproduction check
tools/rebuild-libc.ps1        Windows deterministic shim reproduction check
```

## Documentation

| Document | Purpose |
| --- | --- |
| [Getting started](docs/GETTING_STARTED.md) | Host setup, first configuration, build, and output inspection |
| [Project configuration](docs/CONFIGURATION.md) | `project.json`, sources, compiler inputs, archives, and runtime modules |
| [Presentation assets](docs/PRESENTATION_ASSETS.md) | Icons, backgrounds, ATRAC9 audio, and format limits |
| [Build output formats](docs/FFPKG.md) | Folder, `.ffpkg`, and `.ffpfsc` generation |
| [Native build tooling](docs/NATIVE_TOOLING.md) | LLVM boundary and C++ converter/FSELF commands |
| [Clean-room runtime shim](docs/RUNTIME_SHIM.md) | Design, hashes, compatibility, and deterministic reproduction |
| [Deployment](docs/DEPLOYMENT.md) | Directory staging and smoke testing |
| [Platform constraints](docs/PLATFORM_NOTES.md) | Loader, filesystem, presentation, and capability boundaries |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Common setup, build, packaging, and launcher failures |
| [Contributing](CONTRIBUTING.md) | Change requirements and release checks |

## External projects and tools

| Project | Role |
| --- | --- |
| [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) | Public PS5 headers, libc++ headers, sysroot, and Clang target support |
| [SvenGDK/SharpProspero](https://github.com/SvenGDK/SharpProspero) | Public format reference used during initial research; not a build dependency |
| [NetBSD/MirBSD makefs](http://cvs.mirbsd.de/src/usr.sbin/makefs/) | Native optional UFS2 `.ffpkg` generation |
| [SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) | Public UFS2 profile reference; not a build dependency |
| [PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) | Optional compressed `.ffpfsc` generation |
| [sinajet/PSFFPKG](https://github.com/sinajet/PSFFPKG) | Public `.ffpkg` procedure used as a format reference |
| [LLVM/Clang](https://github.com/llvm/llvm-project) | Native compiler |
| [Microsoft DirectXTex](https://github.com/microsoft/DirectXTex) | `texconv` presentation-image preparation |
| [FFmpeg](https://ffmpeg.org/) | Developer-supplied selection-audio preparation |
| [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus) | Directory-style deployment and hardware validation |

Exact dependency pins and license notes are recorded in [NOTICE.md](NOTICE.md).

## Scope

This project builds a directory-style homebrew application and optional
filesystem images. It does not create a signed retail PKG/FPKG, automate an
exploit, alter console configuration, or bundle Sony files. GPU decoding and a
general-purpose C library are outside this foundation.

## Contributing

Contributions are welcome. Keep the template small, reproducible, and useful to
a first-time native-app developer. See [CONTRIBUTING.md](CONTRIBUTING.md) for
the required checks.

## License and attribution

Repository-authored code is licensed under GPL-3.0-or-later. Optional fetched
tools remain under their upstream licenses. See [LICENSE](LICENSE) and
[NOTICE.md](NOTICE.md).

PlayStation and PS5 are trademarks of Sony Interactive Entertainment. This
project is independent and is not affiliated with or endorsed by Sony.

This project was developed with assistance from OpenAI Codex. Project
maintainers reviewed and validated the resulting code and documentation.
