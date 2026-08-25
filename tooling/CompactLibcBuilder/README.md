# Compact clean-room runtime-shim emitter

This project preserves the exact source for the independently authored compact
`libc.prx` experiment. It is separate from the release emitter in
[`../MinimalLibcBuilder`](../MinimalLibcBuilder) and does not replace the
default [`runtime/libc.prx`](../../runtime/libc.prx).

## Provenance

BlackBearReloaded designed and implemented this emitter from scratch for
`ps5-native-app-boilerplate`. It constructs the ELF from project-owned semantic
constants and does not read, import, transform, embed, or link a Sony
`libc.prx`, firmware module, proprietary SDK file, or game file. No proprietary
code or data is present in the source or generated artifact.

Loader-visible interfaces and ELF/FSELF structure informed the compatibility
requirements. Their implementation here is independently written and licensed
under GPL-3.0-or-later.

## Reproduce the experiment

After running `./tools/setup-tooling.ps1`, build and sign it from PowerShell:

```powershell
dotnet run --project tooling/CompactLibcBuilder/CompactLibcBuilder.csproj `
  -c Release -- build/compact-libc.raw.elf
dotnet run --project tooling/NativeAppBuilder/NativeAppBuilder.csproj `
  -c Release -- self --sign `
  --in build/compact-libc.raw.elf --out build/compact-libc.prx
```

Expected output:

```text
Raw ELF:     98,442 bytes
Raw SHA-256: 9ad5970f037abfa673fb369a2d6ed8be650eb2ebc810d7080ca8420e1d0cbac3
FSELF:       4,898 bytes
FSELF SHA-256:
6dc325bfb9c3c9af48ef9b424960b78cc92edc1703df81ecf982b40d559fffb2
```

The exact FSELF passed the firmware 6.02 diagnostic gate. It did not reach
application checkpoints in the tested firmware 12.70 loader environment and
caused a probable console reboot, so it must not be presented as generally
firmware-compatible.
