> **Disclaimer:** This is an AI-assisted project developed using OpenAI Codex.

<p align="center">
  <img src="sce_sys/icon0.png" width="128" alt="ProsperoLight icon">
</p>

<h1 align="center">ProsperoLight</h1>

<p align="center">
  <strong>A native Moonlight client for PlayStation 5 homebrew</strong><br>
  Stream Sunshine applications with hardware video decoding, low-latency input,
  stereo audio, and a controller-first interface.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-PlayStation%205-003791?logo=playstation&amp;logoColor=white" alt="PlayStation 5">
  <img src="https://img.shields.io/badge/video-H.264%20%7C%20HEVC-70E1DC" alt="H.264 and HEVC">
  <img src="https://img.shields.io/badge/UI-RmlUi-5DDFA4" alt="RmlUi">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue" alt="GPL-3.0-or-later"></a>
</p>

ProsperoLight is a native PS5 client for the open Moonlight/Sunshine streaming
protocol. Its RmlUi launcher discovers and pairs with Sunshine hosts, browses
their applications, and starts a native streaming session. Video access units
are decoded by PS5 VideoDec2 and the resulting GPU-visible surfaces are
presented by AGC without copying decoded pixels through a CPU framebuffer.

> [!IMPORTANT]
> ProsperoLight does not run on an unmodified retail console. It is intended
> for consoles you own with an already configured, compatible homebrew loader.
> This repository does not include an exploit, proprietary Sony SDK, system
> module, encryption key, firmware file, or game asset.

## Project foundation

> [!IMPORTANT]
> **Built on the [PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate).**
> ProsperoLight preserves the template's C++20 structure, `.hpp` interfaces,
> reproducible clean-room runtime, native FSELF tooling, tests, safe folder
> deployment, and release automation.

The client uses the established
[moonlight-common-c](https://github.com/moonlight-stream/moonlight-common-c)
protocol implementation rather than reimplementing the wire protocol. The
launcher, stream coordination, PS5 input/audio/video integration, UI, and build
tooling are maintained in this repository.

| Identity | Value |
| --- | --- |
| Shell title | `ProsperoLight` |
| Title ID | `PPSA77003` |
| Category | Game |
| Current version | `01.000.029` |
| Version source | [`sce_sys/param.json`](sce_sys/param.json) |
| Writable data | `/download0` only |

## Features

- Discover Sunshine hosts on the LAN or add an IPv4 address manually.
- Remember up to eight PCs, pairing identities, and stream preferences under
  `/download0` across application restarts.
- Pair with a two-minute PIN dialog and unpair through explicit two-press
  confirmation.
- Browse up to 64 advertised Sunshine applications with paged artwork,
  launch/resume feedback, and active-application stop controls.
- Decode H.264 High and HEVC Main streams through VideoDec2 at 1080p60,
  1440p60, and 2160p60; higher-resolution modes remain beta.
- Present decoded GPU surfaces directly through AGC, with edge-to-edge and
  television-safe display modes. The presenter uses native 1080p scanout for
  1080p streams and a 3840x2160 target for 1440p and 2160p streams.
- Select bitrate presets up to 500 Mbps. The best setting depends on the host,
  encoder, network, and selected codec rather than link speed alone.
- Enable experimental 1080p60 HEVC Main10 HDR10 output when the Sunshine host
  advertises support.
- Decode Moonlight Opus audio and output 48 kHz stereo through PS5 AudioOut.
- Forward low-latency DualSense controls, with controller/mouse switching and
  a stream keyboard that works at Windows sign-in.
- Show Moonlight-style stream metrics for resolution, codec, frame rates,
  packet loss, network/host latency, and decode time.
- Recover from connection failures and return from a stream to the launcher
  without leaving a stale session running.
- Use original 4K launcher artwork, icon, loading presentation, and selection
  music in a controller-first RmlUi interface.

## Current status

The complete 1080p60 path—pairing, application launch, VideoDec2/AGC video,
stereo audio, DualSense input, mouse mode, Windows sign-in keyboard, metrics,
return-to-launcher, relaunch, and cleanup—has been exercised on PS5 hardware
with Sunshine.

ProsperoLight is still alpha software. HEVC, 1440p, 2160p, HDR, very high
bitrates, network recovery, and long gameplay sessions need broader validation
across GPUs, Sunshine configurations, networks, TVs, firmware versions, and
homebrew loaders. See the evidence and open acceptance items in
[Validation](docs/VALIDATION.md).

## Requirements

Build from Linux, WSL, or a Linux CI runner. On Ubuntu, Debian, or WSL:

```bash
sudo apt update
sudo apt install curl git make pkg-config python3 python3-venv tar unzip wget \
  clang-18 clang-format-18 clang-tidy-18 lld-18
```

The build downloads and verifies its public PS5 Payload SDK, zlib, GoogleTest,
and packaging inputs below ignored `.deps/` directories. Initialize the pinned
streaming sources after cloning:

```bash
git submodule update --init --recursive
make doctor
```

Compressed `.ffpfsc` output requires Python 3.9 or newer with `venv` support.
The optional local `.ffpkg` target additionally requires .NET 8 or newer.
Nothing is installed globally by the project build.

See [Getting started](docs/GETTING_STARTED.md) and
[Native tooling](docs/NATIVE_TOOLING.md) for clean-machine setup details.

## Build

```bash
# Production release image; also assembles the complete title folder.
make ffpfsc

# Faster folder-only development build.
make
```

Outputs are written to:

```text
dist/PPSA77003/           complete title folder
dist/PPSA77003.ffpfsc     compressed installation image
```

Useful development gates are:

```bash
make test       # C++ unit/runtime tests and Python tooling regressions
make lint       # formatting, static analysis, metadata, asset, and shell checks
make check      # lint + every host test + complete folder build
make ffpfsc     # production folder + compressed image
```

An optional `make ffpkg` target remains available for local development. The
`.ffpkg` output is intentionally excluded from GitHub Actions and Releases.
See [Package formats](docs/FFPKG.md).

## GitHub Actions and releases

The [Build workflow](.github/workflows/tooling.yml) runs on every push to
`main`, pull request, version tag, and manual dispatch. It:

1. checks out all pinned submodules;
2. installs the public Linux/PS5 build prerequisites;
3. validates metadata and the release tag;
4. runs lint, GoogleTest, runtime-allocation, and Python integration checks;
5. independently reproduces and verifies `runtime/libc.prx`;
6. builds `PPSA77003.ffpfsc`; and
7. generates `SHA256SUMS` and uploads both files as the Actions artifact.

A tag matching the exact `contentVersion` verifies that build-time checksum
again, then publishes exactly the `.ffpfsc` image and `SHA256SUMS`. Folder and
`.ffpkg` builds are never attached to a release.

## Deploy

For an already-running PS5 FTP service, stage the development folder with:

```bash
make deploy PS5_HOST=192.168.1.100
```

Fully close ProsperoLight before deploying. The deployer writes only the
current title below `/data/homebrew`, uploads through temporary names, and
publishes `eboot.bin` and `sce_sys/param.json` last. Upload the complete folder;
`eboot.bin` alone is not a valid deployment.

To test the packaged form instead:

```bash
make deploy PS5_HOST=192.168.1.100 DEPLOY_FORMAT=ffpfsc
```

ProsperoLight never changes PS5 system settings or configures a loader. See
[Deployment](docs/DEPLOYMENT.md) for the safe development loop and removal
behavior.

## Pairing and first stream

1. Start Sunshine on a PC connected to the same trusted LAN.
2. Open ProsperoLight and choose a discovered PC, or select **Add PC** and enter
   its IPv4 address.
3. Select **Pair PC**, then enter the displayed PIN in Sunshine within two
   minutes.
4. Open **Games**, choose Desktop or another advertised application, and press
   Cross.
5. Use `Select + L1` to end the stream and return to ProsperoLight.

Pairing credentials and settings are title-scoped. Installing under a different
title ID intentionally requires pairing again.

## Controls

### Launcher

| Input | Action |
| --- | --- |
| D-pad / left analog stick | Move focus or change the selected PC/application/setting |
| Cross | Activate, pair, launch, resume, or change a setting |
| Circle | Return to the PCs page |
| Square | Stop the active Sunshine application |
| Triangle | Refresh the selected Sunshine host |
| L1 / R1 | Change between PCs, Games, Settings, and Diagnostics |
| Options | Open Settings |

### Streaming

| Input | Action |
| --- | --- |
| `Select + R1` | Toggle the metrics overlay |
| `Select + L1` | End the stream and return to ProsperoLight |
| `Select + Square` | Toggle mouse/controller mode |
| `Select + Triangle` | Toggle ProsperoLight's stream keyboard |
| Either analog stick in mouse mode | Move the pointer |
| Cross / Circle / Square in mouse mode | Left / right / middle mouse button |
| L1 / R1 in mouse mode | Mouse X1 / X2 button |
| D-pad in mouse mode | Vertical / horizontal scroll |
| D-pad while keyboard is open | Move between keys |
| Cross while keyboard is open | Type the selected key |
| Triangle while keyboard is open | Toggle Shift |
| Square while keyboard is open | Send Backspace |
| Options while keyboard is open | Send Enter and close the keyboard |
| Circle while keyboard is open | Close the keyboard |

The stream keyboard contains every printable US-ASCII character used by
standard passwords. It is not currently a multilingual or Unicode input
method. Keyboard text is sent directly as Moonlight key events and is not
stored in ProsperoLight diagnostics or configuration.

## Source layout

```text
src/main.cpp                         SDL2/RmlUi lifetime and stream handoff
src/moonlight_app.cpp                launcher state, navigation, and feedback
src/moonlight_backend.cpp            pairing, app listing, artwork, and control
src/moonlight_discovery.cpp          LAN discovery
src/moonlight_config.cpp             /download0 host and preference persistence
src/moonlight_stream.cpp             Moonlight session, VideoDec2, audio, input
src/native_agc_present.cpp           zero-copy AGC presentation and overlays
src/gamestream/                      retained Moonlight-compatible C boundary
include/*.hpp                        application-owned public interfaces
platform/ps5/                        narrow Moonlight PS5 compatibility adapters
third_party/                         pinned Moonlight, mbedTLS, and Opus sources
ui/                                  RML, RCSS, fonts, icons, and chrome assets
sce_sys/                             launcher metadata, 4K artwork, icon, music
runtime/libc.prx                     generated clean-room loader runtime
tooling/native/                      native ELF/FSELF and runtime build tools
tests/                               GoogleTest and Python host regressions
docs/                                architecture, setup, testing, and evidence
```

Application-owned code is C++20 with `.hpp` interfaces. The retained
`src/gamestream` C code and pinned upstream dependencies preserve their native
language and public headers; they are dependency boundaries, not a second
application architecture. See [Architecture](docs/ARCHITECTURE.md) and
[Porting notes](docs/PORTING.md).

## Versioning

[`sce_sys/param.json`](sce_sys/param.json) is the only application identity and
release-version source. Its PS5-format `contentVersion` is injected into the
top bar, checked against the release tag, and used as the GitHub Release name.
Do not add a `v` prefix.

```bash
# After updating param.json and passing all local gates:
git tag 01.000.029
git push origin main 01.000.029
```

Keep `PPSA77003`, `conceptId`, and `contentId` stable for updates to this title.
Changing the title ID creates a separate PS5 application and separate pairing
storage. See [Configuration](docs/CONFIGURATION.md).

## Documentation

| Document | Purpose |
| --- | --- |
| [Getting started](docs/GETTING_STARTED.md) | Clean-machine prerequisites and first build |
| [Architecture](docs/ARCHITECTURE.md) | Launcher, protocol, video, audio, and input flow |
| [Configuration](docs/CONFIGURATION.md) | Identity, versioning, settings, and build variables |
| [Testing](docs/TESTING.md) | Host test boundaries and commands |
| [Validation](docs/VALIDATION.md) | Hardware acceptance checklist and recorded evidence |
| [Deployment](docs/DEPLOYMENT.md) | Safe folder/image staging and smoke tests |
| [Package formats](docs/FFPKG.md) | Folder, `.ffpkg`, and `.ffpfsc` outputs |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Common build, launch, and runtime failures |
| [Platform notes](docs/PLATFORM_NOTES.md) | PS5 filesystem, loader, and presentation constraints |
| [Runtime shim](docs/RUNTIME_SHIM.md) | Clean-room `libc.prx` scope and reproduction |
| [Presentation assets](docs/PRESENTATION_ASSETS.md) | Icon, backgrounds, and selection audio |
| [Contributing](CONTRIBUTING.md) | Change, test, and release requirements |
| [Notices](NOTICE.md) | Dependency, asset, and license attribution |

## Credits, third-party software, and licenses

ProsperoLight exists thanks to the maintainers and contributors of:

- [Moonlight](https://github.com/moonlight-stream/moonlight-common-c) and
  [Sunshine](https://github.com/LizardByte/Sunshine) for the open streaming
  protocol ecosystem;
- [PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
  and the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) for the
  reproducible native foundation and public target integration;
- [RmlUi](https://github.com/mikke89/RmlUi),
  [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2), and
  [FreeType](https://freetype.org/) for the launcher interface;
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) and
  [Opus](https://github.com/xiph/opus) for secure protocol and audio support;
- [MkPFS](https://github.com/PSBrew/MkPFS),
  [UFS2Tool](https://github.com/SvenGDK/UFS2Tool), LLVM/Clang, Python, zlib,
  and GoogleTest for build, packaging, and validation tooling.

The original ProsperoLight artwork and selection music are distributed under
the project license. Complete revisions, checksums, copyright notices, and
third-party terms are recorded in [NOTICE.md](NOTICE.md) and the corresponding
source trees.

ProsperoLight is distributed under [GPL-3.0-or-later](LICENSE). PlayStation and
PS5 are trademarks of Sony Interactive Entertainment. Moonlight and Sunshine
retain their respective project identities. ProsperoLight is an independent
homebrew project and is not affiliated with or endorsed by Sony Interactive
Entertainment, Moonlight, or Sunshine.
