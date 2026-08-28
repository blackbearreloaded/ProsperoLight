# ProsperoLight

ProsperoLight is a native PS5 Moonlight client for Sunshine. It is built on
[`ps5-native-app-boilerplate`](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
and retains its reproducible native build, runtime validation, packaging, and
deployment workflow.

The client provides a controller-first RmlUi launcher, Sunshine discovery and
pairing, application launch/stop control, DualSense input, Opus audio, and
hardware video presentation through VideoDec2 and AGC. The streaming path is
designed to keep decoded video on the GPU until presentation.

## Current port status

The port builds and passes a native PS5 launch/close smoke test from this
repository. The current identity is `PPSA77003`, content version `01.000.014`,
and title `ProsperoLight`.

This fresh title has its own PS5 application data. Pair it once after
installation; existing pairing data from another title is intentionally not
reused.

## Architecture

Application-owned code is C++20. The only retained C boundary is
`src/gamestream`, the established Moonlight-compatible protocol core, plus its
pinned upstream C dependencies. This deliberately avoids a risky protocol
rewrite while keeping the launcher, stream integration, UI, platform glue, and
build integration in modern C++.

`sce_sys/param.json` is the source of package identity and content version. At
build time, the same `contentVersion` value replaces the UI version token, so
the launcher and the PS5 shell cannot drift.

See [architecture](docs/ARCHITECTURE.md), [porting notes](docs/PORTING.md), and
the [validation plan](docs/VALIDATION.md).

## Build

Use WSL on a host with the PS5 SDK environment expected by the boilerplate.

```sh
make app       # native app folder in dist/PPSA77003/
make test      # focused host tests and tooling tests
make lint      # format, static analysis, metadata, and asset checks
make ffpfsc    # compressed installation image
```

`make check` runs lint, tests, and an app build. Build products are written to
`dist/` and are ignored by Git.

The build creates host-only import stubs for the PS5 modules used by VideoDec2,
AGC, PNG decoding, and dialogs. The generated stubs only let the native linker
record imports; they are not packaged and contain no replacement runtime code.

For common native build and deployment tasks, start with
[Getting started](docs/GETTING_STARTED.md), [testing](docs/TESTING.md), and
[deployment](docs/DEPLOYMENT.md).

## Streaming shortcuts

- `Select + R1`: toggle the metrics overlay.
- `Select + L1`: end the stream and return to ProsperoLight.
- Hold then release `Options`: toggle mouse emulation.

## Scope

The current UI exposes H.264/HEVC, resolution, bitrate, aspect behavior, and
HDR preferences. Codec and HDR availability remain dependent on the selected
Sunshine host configuration and the PS5 hardware path. Validate a setting on
hardware before depending on it for a play session.

## License and notices

ProsperoLight is GPL-3.0-or-later. The pinned Moonlight, mbedTLS, Opus, RmlUi,
SDL, FreeType, and PS5 SDK integration components retain their own licenses;
see [NOTICE.md](NOTICE.md) and the corresponding source trees.
