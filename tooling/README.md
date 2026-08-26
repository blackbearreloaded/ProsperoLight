# Repository-local application builder

This folder contains the project-owned host-side frontend and clean-room runtime
emitter. The root `build.ps1` invokes `../tools/setup-tooling.ps1`, which fetches
the pinned SharpProspero source into the ignored `.deps/` cache when needed.

Requirements:

- Git for Windows and network access for the first dependency fetch.
- Windows .NET SDK 10 for `NativeAppBuilder`.
- WSL `/usr/bin/clang-18` and `/opt/ps5-payload-sdk` for compilation.
- The bundled clean-room V7 `libc.prx`; its independent emitter and manifests
  are in `ConventionalLibcBuilder`, and its reproduction command is
  `../tools/rebuild-libc.ps1`.
- `ConventionalLibcBuilder` contains the sole supported runtime profile: the
  deterministic V7 artifact validated on firmware 6.02 and 12.70. Its stubs
  provide loader compatibility, not a complete behavioral libc.

The bootstrapper pins
[SharpProspero](https://github.com/SvenGDK/SharpProspero) commit
`e36e610fa5b4be23ad38b9c8429f11f11750cc0c` and applies
[`patches/sharpprospero-native-app.patch`](patches/sharpprospero-native-app.patch).
See [`../docs/CSHARP_TOOLING.md`](../docs/CSHARP_TOOLING.md).
