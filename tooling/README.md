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
- `MinimalLibcBuilder` preserves the original 4,898-byte experiment; it is no
  longer the bundled release runtime.
- The later compact-layout clean-room experiment is preserved separately in
  `CompactLibcBuilder`; it is not the bundled release shim.
- The conventional-slot clean-room history is preserved in
  `ConventionalLibcBuilder`; its V7 profile is now the bundled release runtime.
  The same emitter also offers an explicitly experimental version-2 profile
  with complete loader-visible API-surface parity; its stubs are not a
  behavioral libc implementation. Version 3 adds cross-firmware-stable system
  forwarders; version 4 additionally matches the analyzed jump-buffer ABI and
  return-from-main behavior. Version 5 conditionally delegates the two
  thread-destructor APIs on firmwares that export them while preserving the
  version-4 fallbacks on 6.02. V5 passed 6.02 but produced a probable pre-main
  kernel panic on 12.70, so it is not a cross-firmware candidate. The separate
  V6 profile is a firmware-6.02-proven, reference-shaped experiment that keeps
  the full export surface but reduces startup to seven cross-firmware imports
  and matches the working module's loader geometry. Its 12.70 gate produced a
  probable kernel panic after eboot exec and before the first application
  checkpoint, so V6 is not a cross-firmware candidate. V7 retains the
  V6-proven startup routine while adding the analyzed 102-import and full
  relocation-table topology. Its deterministic signed artifact passed the
  stable loop on firmware 6.02 and 12.70 and is the first project-authored
  candidate validated on both. None of these profiles is a complete behavioral
  libc.

The bootstrapper pins
[SharpProspero](https://github.com/SvenGDK/SharpProspero) commit
`e36e610fa5b4be23ad38b9c8429f11f11750cc0c` and applies
[`patches/sharpprospero-native-app.patch`](patches/sharpprospero-native-app.patch).
See [`../docs/CSHARP_TOOLING.md`](../docs/CSHARP_TOOLING.md).
