# PS5 Native App Boilerplate

[![Build](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/actions/workflows/tooling.yml/badge.svg)](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/actions/workflows/tooling.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

Build modern C++20 homebrew applications for PlayStation 5 from Linux, WSL, or
Windows. The repository root is a complete graphical Hello World skeleton:
create a repository from the template, edit `src/main.cpp`, and build a
deployable title. C sources and C libraries remain supported at explicit ABI
boundaries.

The application and all repository-owned build tools are native C/C++. LLVM
handles ordinary linking; a small project-owned converter emits PS5 metadata
and the development FSELF. The public SDK, static zlib, and optional packaging
dependencies are fetched on demand into the ignored `.deps/` cache. No
proprietary Sony SDK file or proprietary runtime module is required. The
optional `.ffpkg` target builds the external UFS2Tool with .NET 8 or newer;
the application and repository-owned tooling remain C/C++.

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
| Third-party libraries | Optional pinned PacBrew sysroot with declarative static linking |
| Root skeleton | C++20 graphical Hello World with RAII, bounded views, unique ownership, CPU-rendered text, shapes, and packaged data |
| Validation | C++ unit tests, host integration tests, prerequisites, and static ELF/FSELF inspection |

## Quick start

### Create a project from the template

For a new application, select **Use this template** on GitHub, then choose
**Create a new repository**. This creates an independent repository with a
clean history. The same workflow is available through the GitHub CLI:

```bash
gh repo create my-ps5-app \
  --template blackbearreloaded/ps5-native-app-boilerplate \
  --public \
  --clone
```

Use `--private` instead of `--public` when appropriate. Fork this repository
only when preparing a contribution back to the boilerplate. A direct clone is
best reserved for temporary local experiments because it retains the
boilerplate remote and history.

The checked-in skeleton deliberately uses the development identity
`PPSA99999`, with concept code `99999`. Keep it for a single local development
copy; assign a unique identity before distributing an app or deploying
multiple projects derived from this template.

### Prerequisites

On Ubuntu, Debian, or WSL install Git, Make, Python 3 with virtual-environment
support, Clang 18, lld 18, clang-format, clang-tidy, `curl`, `wget`, and
`unzip`.
Windows users may use the same WSL path or the retained PowerShell frontend.

```bash
sudo apt update
sudo apt install curl git make pkg-config python3 python3-pip python3-venv tar wget unzip \
  clang-18 clang-format-18 clang-tidy-18 lld-18
```

The optional presentation-asset converter uses DirectXTex `texconv` and
Windows PowerShell, either directly or through its Bash frontend in WSL.
FFmpeg may run on Windows or inside WSL. ATRAC9 output additionally
requires a compatible `ps4_at9tool.exe` that you are legally permitted to use;
the repository neither bundles nor downloads that encoder. Packaging tools are
fetched into `.deps/` automatically when their Make targets are requested.
`make ffpkg` additionally requires the .NET SDK 8 or newer to build UFS2Tool.

The first build fetches the pinned public PS5 payload SDK and zlib 1.3.2 source
archives, verifies both digests, compiles zlib into the ignored
`.deps/native/` cache, then generates the clean-room `runtime/libc.prx` from
source. Host unit tests similarly fetch a verified GoogleTest release into
`.deps/test/`; GoogleTest is never linked into PS5 output. Check the host first
with `make doctor`.

### Build the template

1. Initialize a distinct identity. The command coordinates title ID, concept
   ID, content ID, name, and category in [`sce_sys/param.json`](sce_sys/param.json):

   ```bash
   make init TITLE_ID=PPSA12345 APP_NAME="My Native App"
   ```

   Add `APP_CATEGORY=media` for a Media-area application.
2. Build:

   ```bash
   make
   ```

Bare `make` fetches missing native dependencies, generates and verifies
`runtime/libc.prx`, then builds the complete application folder.

To use a library from PacBrew, run `make pacbrew-list`, then build with a
space-separated module list such as `make PACBREW_PACKAGES="sdl2 openssl"`.
The prebuilt ports image is verified and cached inside `.deps/`; no global
installation is performed. See [PacBrew dependencies](docs/PACBREW.md).

The finished title directory is written to `dist/<TITLE_ID>/`. Stage that
entire directory with a compatible loader such as
[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus); `eboot.bin`
cannot be deployed by itself.

For a short development loop, build and update the title folder under
`/data/homebrew` through an already-running FTP service:

```bash
make deploy PS5_HOST=192.168.1.100
```

Remove only this title's staged folder and same-ID image artifacts when the
development copy is no longer needed:

```bash
make undeploy PS5_HOST=192.168.1.100
```

The default FTP port is `2121`. Each file is uploaded under a hidden temporary
name and promoted only after its transfer completes; `eboot.bin` and
`sce_sys/param.json` are published last. Use `DEPLOY_FORMAT=ffpfsc` or
`DEPLOY_FORMAT=ffpkg` when an image is specifically required. Launching and
closing the app remain explicit manual steps. `undeploy` does not unregister a
Shell entry. See [Deployment](docs/DEPLOYMENT.md).

Do not relaunch an `.ffpfsc` immediately after replacing the same pathname:
ShadowMountPlus may still have the previous image mounted. Fully close the
title, deploy the completed replacement, then either restart ShadowMountPlus
cleanly or restart the PS5. Afterward, start the approved services normally and
wait for ShadowMountPlus to rediscover the title before launching it. Keeping
the same title ID preserves separate `/download0` and save data; `/temp0` is
temporary, `/app0` comes from the new image, and Shell presentation metadata
may remain cached.

For values used repeatedly on one workstation, copy the tracked example to the
hidden, ignored local configuration and edit it:

```bash
cp .env.example .env
```

GNU Make reads `.env`; it is appropriate for host addresses and build choices,
not app identity or release metadata. Keep those in `sce_sys/param.json`.

Run `make help` to list the focused targets. `make deps` only prefetches native
dependencies, `make libc` forces runtime reproduction, `make lint` runs
clang-format and clang-tidy, `make test` runs the host unit and integration
suites, and `make packages` emits the folder, `.ffpkg`, and `.ffpfsc` forms. On Windows
PowerShell, `./build.ps1` and
`./tools/rebuild-libc.ps1` remain equivalent supported entry points.

Read [Getting started](docs/GETTING_STARTED.md) before the first build.

## Customize the application

### Source and metadata

Edit `sce_sys/param.json` to define the app identity, Games/Media category, and
PS5-format release version. Every C or C++ file under `src/` is compiled
automatically; optional build inputs use documented Make variables. The
hardware-proven
graphical skeleton is [`src/main.cpp`](src/main.cpp); edit its short scene and
entry point after creating a project. The reusable VideoOut setup and bitmap
font live in `src/demo_renderer.*` so application logic does not start inside a
large platform implementation file.

### Versioning from `param.json`

[`sce_sys/param.json`](sce_sys/param.json) is the only release-version source;
there is no `project.json` or separate Semantic Version. The relevant structure
is:

```json
{
  "titleId": "PPSA99999",
  "conceptId": "99999",
  "contentId": "UP9000-PPSA99999_00-HELLOWORLD000001",
  "contentVersion": "01.000.000",
  "masterVersion": "01.00"
}
```

- `contentVersion` is the application version, Git tag, and GitHub Release name.
  It must use `NN.NNN.NNN` exactly, without a `v` prefix. Increment it for each
  release; this project does not assign Semantic Versioning meaning to its
  fields.
- `masterVersion` is the compatible release baseline. Keep `01.00` unless a
  release intentionally changes that compatibility baseline.
- `titleId`, `conceptId`, and `contentId` identify the application rather than
  its version. Keep them stable for updates to the same installed title; change
  them together when creating a separate application.
- `sdkVersion` and `requiredSystemSoftwareVersion` are loader/platform metadata,
  not project release numbers.

For a release, update `contentVersion`, commit the exact source, run the local
gates, then tag and push that same commit:

```bash
git tag 01.002.003
git push origin 01.002.003
```

The workflow rejects a tag that differs from `contentVersion` and publishes
the verified compressed FFPFSC image and its `SHA256SUMS` under that version.
The image already
contains the complete application and generated `libc.prx`. See [Application
configuration](docs/CONFIGURATION.md) for every metadata field.

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

### Application filesystem paths

Access depends on the title metadata, loader, firmware, and successful mount
APIs. These statuses describe this project's hardware-tested baseline; do not
treat a conditional path as available until the application verifies it.

| Path | Access | Status and intended use |
| --- | --- | --- |
| `/app0/` | Read-only | The application image ran on firmware 6.02 and 12.70; packaged asset reading was directly observed on 6.02. Contains `eboot.bin`, `sce_sys/`, `sce_module/`, and packaged assets. |
| `/download0/` | Read/write, persistent | Validated across relaunches on 6.02 when `downloadDataSize` is positive. Use for configuration, pairing state, caches, and logs. Its host backing file is `/user/download/<TITLE_ID>/download0.dat`; applications must use `/download0`, not that host path. |
| `/temp0/` | Read/write when mounted; temporary | Not available in the tested 6.02 ShadowMount native-title environment: opening a file returned `0x80020002`. Do not depend on it without a successful runtime probe. |
| `/savedata0/`, `/savedata1/`, ... | Read/write after a successful SaveData mount | Not part of this baseline. The investigated ShadowMount title could not complete SaveData initialization, so no save-data mount or write was validated. |
| `/common/lib/` | Read-only | System-provided shared modules; observed in process module paths. Never modify or package replacements there. |
| `/common_ex/lib/` | Read-only when present | Firmware/loader-dependent extended shared-library namespace; not validated by this template. |
| `/addcont0/`, ... | Normally read-only when mounted | Conditional add-on-content mounts requiring matching content and entitlement; untested here. |
| `/trophy/` | System-managed, conditional | Not general application storage and untested here. Use the platform trophy APIs only when supported. |
| `/usb0/`, `/usb1/`, ... | Conditional | External-storage visibility and write access depend on title permissions and loader context; untested here. |
| `/data/`, `/user/` | Outside the supported app sandbox | Do not use from a normal application. Paths seen by payloads or FTP services do not imply application access. |

`/download0` and mounted save data are separate from `/app0`, so replacing an
application folder or `.ffpfsc` does not inherently replace them. Provide an
export/import path for data that must survive title removal or cache clearing.

### Presentation assets

Replace the icon and background with one command:

```powershell
./tools/prepare-assets.ps1 `
    -Icon C:\art\icon.png `
    -Background C:\art\background.png
```

From WSL, use the equivalent Bash frontend and POSIX paths:

```bash
./tools/prepare-assets.sh \
    --icon /mnt/c/art/icon.png \
    --background /mnt/c/art/background.png
```

The asset tool validates the required dimensions and prepares the PS5-facing
formats. Use `-SelectionBackground` and `-LaunchBackground` when the selected
app and launch/loading screens should differ.

Prepare a 15-second selection-audio excerpt from MP3, M4A, AAC, WAV, or FLAC:

```powershell
./tools/prepare-assets.ps1 `
    -Audio C:\music\selection.mp3 `
    -AudioDuration 15 `
    -At9Tool C:\tools\ps4_at9tool.exe
```

The WSL equivalent is:

```bash
./tools/prepare-assets.sh \
    --audio /mnt/c/music/selection.mp3 \
    --audio-duration 15 \
    --at9-tool /mnt/c/tools/ps4_at9tool.exe
```

The default is 15 seconds; the tool and validator support the documented
2 MiB Shell envelope up to a safe 87.3-second stereo excerpt. Exact audio
constraints and ready-made `.at9` handling are documented in
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
`runtime/libc.prx` is generated by `make`, ignored by Git, and copied into the
application image. Its expected digest remains tracked in
`runtime/libc.prx.sha256`.

## Repository layout

```text
src/main.cpp                  Concise Modern C++20 app entry point and scene
src/demo_renderer.*           Reusable CPU VideoOut and bitmap-font demo support
assets/                       Optional files mounted at /app0/assets/
sce_sys/param.json            App identity, metadata, and release version
sce_sys/                      Icon, backgrounds, and selection audio
Makefile                      Linux/WSL build, lint, dependency, package, and deploy targets
.env.example                  Copyable, ignored local Make defaults
build.ps1                     Windows PowerShell build entry point
tools/build.sh                Native Linux/WSL build orchestrator
tools/deploy.sh               FTP deployment and title-scoped cleanup helper
tools/doctor.sh               Canonical read-only Linux/WSL prerequisite check
tools/doctor.ps1              Windows frontend for the same doctor
tools/init-project.sh         Coordinated param.json identity initializer
tools/inspect.ps1             Static ELF/FSELF validator
tools/prepare-assets.ps1      Presentation conversion and validation
tools/prepare-assets.sh       WSL Bash frontend for presentation conversion
tools/validate-assets.sh      Portable Linux/WSL presentation validator
tools/setup-pacbrew-dependencies.sh  Isolated PacBrew sysroot and flag resolver
tooling/native/               C++ linker converter, allocation bridge, startup runtime, FSELF, and runtime builder
runtime/libc.prx.sha256       Expected digest for the generated loader shim
tools/rebuild-libc.sh         Linux/WSL deterministic shim reproduction check
tools/rebuild-libc.ps1        Windows deterministic shim reproduction check
tests/                        Host-native C++ unit and tooling integration tests
```

## Documentation

| Document | Purpose |
| --- | --- |
| [Getting started](docs/GETTING_STARTED.md) | Host setup, first configuration, build, and output inspection |
| [Testing](docs/TESTING.md) | Unit, host integration, and PS5 hardware-validation practices |
| [Application configuration](docs/CONFIGURATION.md) | `param.json`, release tags, Games/Media category, sources, and libraries |
| [Presentation assets](docs/PRESENTATION_ASSETS.md) | Icon, selection/launch images, ATRAC9 conversion, and format limits |
| [PacBrew dependencies](docs/PACBREW.md) | Third-party PS5 libraries, selection, caching, and limits |
| [Build output formats](docs/FFPKG.md) | Folder, `.ffpkg`, and `.ffpfsc` generation |
| [Native build tooling](docs/NATIVE_TOOLING.md) | LLVM boundary and C++ converter/FSELF commands |
| [Clean-room runtime shim](docs/RUNTIME_SHIM.md) | Design, hashes, compatibility, and deterministic reproduction |
| [Deployment](docs/DEPLOYMENT.md) | FTP staging, title-scoped cleanup, and smoke testing |
| [Platform constraints](docs/PLATFORM_NOTES.md) | Loader, filesystem, presentation, and capability boundaries |
| [Capability recipes](docs/RECIPES.md) | Focused patterns for storage, input, networking, AudioOut, SDL, and native libraries |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Common setup, build, packaging, and launcher failures |
| [Contributing](CONTRIBUTING.md) | Change requirements and release checks |

## External projects and tools

| Project | Role |
| --- | --- |
| [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) | Public PS5 headers, libc++ headers, sysroot, and Clang target support |
| [ps5-payload-dev/pacbrew-repo](https://github.com/ps5-payload-dev/pacbrew-repo) | Optional prebuilt PS5 ports and static libraries |
| [SvenGDK/SharpProspero](https://github.com/SvenGDK/SharpProspero) | Public format reference used during initial research; not a build dependency |
| [SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) | Optional UFS2 `.ffpkg` generation |
| [PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) | Optional compressed `.ffpfsc` generation |
| [sinajet/PSFFPKG](https://github.com/sinajet/PSFFPKG) | Public `.ffpkg` procedure used as a format reference |
| [LLVM/Clang](https://github.com/llvm/llvm-project) | Native compiler |
| [GoogleTest](https://github.com/google/googletest) | Pinned host-only C++ unit-test framework |
| [zlib](https://zlib.net/) | Pinned source-built compression library used by the host FSELF tool |
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
