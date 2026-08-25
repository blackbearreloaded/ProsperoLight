# Repository-local application builder

This folder contains the project-owned host-side frontend and clean-room runtime
emitter. The root `build.ps1` invokes `../tools/setup-tooling.ps1`, which fetches
the pinned SharpProspero source into the ignored `.deps/` cache when needed.

Requirements:

- Git for Windows and network access for the first dependency fetch.
- Windows .NET SDK 10 for `NativeAppBuilder`.
- WSL `/usr/bin/clang-18` and `/opt/ps5-payload-sdk` for compilation.
- The bundled clean-room `libc.prx`; its independent emitter is in
  `MinimalLibcBuilder` and its reproduction command is
  `../tools/rebuild-libc.ps1`.
- The later compact-layout clean-room experiment is preserved separately in
  `CompactLibcBuilder`; it is not the bundled release shim.

The bootstrapper pins
[SharpProspero](https://github.com/SvenGDK/SharpProspero) commit
`e36e610fa5b4be23ad38b9c8429f11f11750cc0c` and applies
[`patches/sharpprospero-native-app.patch`](patches/sharpprospero-native-app.patch).
See [`../docs/CSHARP_TOOLING.md`](../docs/CSHARP_TOOLING.md).
