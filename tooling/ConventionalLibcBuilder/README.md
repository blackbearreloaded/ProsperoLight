# V7 clean-room runtime-shim emitter

This project emits the independently authored V7 runtime bundled as
[`runtime/libc.prx`](../../runtime/libc.prx). It consumes semantic manifests and
writes a deterministic PS5 dynamic ELF; it does not consume a reference binary.

For the complete loader design, artifact hashes, hardware-validation scope, and
distribution statement, see
[`docs/RUNTIME_SHIM.md`](../../docs/RUNTIME_SHIM.md).

## Source inputs

- `Program.cs` implements the raw ELF emitter, compatibility code, relocation
  tables, startup routine, storage allocation, and metadata writers.
- `api-surface-v2.txt` records loader-visible NIDs, ELF binding/type classes,
  and advertised sizes for 2,565 observed libc symbols.
- `startup-imports-v7.txt` records the named system imports and whether each
  participates in a jump-slot or global-data relocation.
- `ConventionalLibcBuilder.csproj` references the patched, pinned
  SharpProspero host linker from the ignored `.deps/` cache.

Every tracked input is GPL-3.0-or-later and attributed to BlackBearReloaded.
The manifests describe interfaces; they contain no proprietary instructions,
initialized object contents, encryption material, or SDK binary data.

## V7 profile

Invoke the emitter with `--startup-v7` followed by the API manifest, startup
manifest, and output path:

```powershell
dotnet run --project tooling/ConventionalLibcBuilder/ConventionalLibcBuilder.csproj `
  -c Release -- --startup-v7 `
  tooling/ConventionalLibcBuilder/api-surface-v2.txt `
  tooling/ConventionalLibcBuilder/startup-imports-v7.txt `
  build/conventional-libc-v7.raw.elf
```

The profile produces:

- an x86-64 PS5 dynamic module (`e_type=0xFE18`) with 14 program headers;
- 2,566 exports across the `libc` and `libc_setjmp` export libraries;
- 102 named system imports;
- 100 jump-slot, 1,790 relative, three TLS-module, and three global-data
  relocations;
- fixed sparse text, read-only, RELRO, writable, dynamic-data, comment, note,
  and version regions;
- a module-parameter record, TLS template, empty GNU unwind index, SysV hash,
  dynamic string/symbol tables, and loader-required SCE dynamic tags;
- a project-authored startup routine that registers inert thread callbacks and
  supplies the application heap API table through stable platform imports.

Compatibility functions use local project-authored stubs unless startup needs a
specific system import. Objects and TLS symbols receive zero-initialized local
storage. This is loader-visible API compatibility, not a complete behavioral C
library.

## Build the signed artifact

The supported reproduction entry point is:

```powershell
./tools/rebuild-libc.ps1
```

That script prepares the pinned host tooling, builds this emitter in an isolated
output directory, emits twice, signs twice, verifies byte-for-byte determinism,
checks both recorded SHA-256 values and the signed size, rejects known private
build-path strings, requires the non-exported `BlackBearReloaded` attribution
marker, and only then updates `runtime/libc.prx` and its checksum.

The expected outputs are:

```text
Raw ELF size:         1,335,962 bytes
Raw ELF SHA-256:      fd18a0c7c18bc62144890294dc1bb85c780757d2ed425b1d7fe0bd58aed1ace2
Signed FSELF size:    1,284,674 bytes
Signed FSELF SHA-256: e2292d285565937f1dac09ef5ab742b6027c28d38ba775ad56465aa5594e2a10
```

The low-level signing command, after emitting the raw ELF, is:

```powershell
dotnet run --project tooling/NativeAppBuilder/NativeAppBuilder.csproj `
  -c Release -- self --sign `
  --in build/conventional-libc-v7.raw.elf `
  --out build/conventional-libc-v7.prx
```

## Compatibility scope

The exact signed artifact above completed the same 26-checkpoint diagnostic,
stable render loop, and clean-close test on firmware 6.02 and 12.70 with
ShadowMountPlus. This validates those two tested environments only; preserve the
hash when testing another firmware or loader.

Earlier profiles remain in the emitter for research reproducibility, but they
are not release runtimes and are intentionally undocumented here. The original
minimal and compact experiments are preserved in their own sibling projects.
