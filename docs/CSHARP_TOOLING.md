# C# build tooling

The PS5 application is native C/C++. C# is used only on the development PC to
turn Clang objects into the PS5-specific ELF and FSELF layout.

## Components

- `NativeAppBuilder`: small command-line frontend.
- `MinimalLibcBuilder`: independent deterministic emitter for the bundled
  loader shim; it has no reference-binary input.
- fetched `SharpProspero.Link`: ELF object/archive reader, symbol resolution,
  relocations, startup generation, and PS5 dynamic-module writer.
- fetched `SharpProspero.Prx`: import/NID catalog, PRX helpers, ELF inspection, and
  FSELF reader/writer.

SharpProspero source is not stored in this repository. `tools/setup-tooling.ps1`
fetches [SvenGDK/SharpProspero](https://github.com/SvenGDK/SharpProspero) commit
[`e36e610`](https://github.com/SvenGDK/SharpProspero/commit/e36e610fa5b4be23ad38b9c8429f11f11750cc0c)
into `.deps/SharpProspero` and applies the tracked native-app compatibility
patch. The dependency remains GPL-3.0 and is cached locally after the first
network fetch.

## Build the host tool directly

```powershell
./tools/setup-tooling.ps1
dotnet build tooling/NativeAppBuilder/NativeAppBuilder.csproj -c Release
```

Show its commands:

```powershell
dotnet run --project tooling/NativeAppBuilder/NativeAppBuilder.csproj -c Release -- --help
```

Inspect a generated FSELF:

```powershell
dotnet run --project tooling/NativeAppBuilder/NativeAppBuilder.csproj -c Release -- `
  self --inspect --file dist/PPSA99999/eboot.bin
```

The normal `build.ps1` command invokes the tool automatically. Developers only
need these direct commands when modifying or diagnosing the builder.

Reproduce the bundled runtime shim with its stricter two-build hash check:

```powershell
./tools/rebuild-libc.ps1
```

## Updating the tooling

Do not change the pinned revision blindly. The tracked patch is part of the
reproducible linker and FSELF baseline.

For any change:

1. Build the same source with the old and new tool.
2. Inspect both artifacts with `tools/inspect.ps1`.
3. Explain every byte/layout difference.
4. Test the generated application on the target hardware and loader.
