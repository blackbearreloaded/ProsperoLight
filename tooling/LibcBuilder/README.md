# Clean-room runtime builder

`LibcBuilder` produces the independently authored raw ELF used to create
[`runtime/libc.prx`](../../runtime/libc.prx). It has one supported build path
and consumes only the two tracked semantic manifests in this directory.

For the loader design, distribution statement, and hardware-validation scope,
see [`docs/RUNTIME_SHIM.md`](../../docs/RUNTIME_SHIM.md).

## Inputs

- `Program.cs` emits the ELF layout, startup routine, exports, imports,
  relocations, zero-initialized storage, and loader metadata.
- `api-surface.txt` records each exported NID, ELF binding/type, and advertised
  size.
- `runtime-imports.txt` records each named system import and its relocation
  classes.

The manifests describe interfaces only. They contain no proprietary
instructions, initialized object contents, encryption material, or SDK binary
data.

## Direct use

```powershell
dotnet run --project tooling/LibcBuilder/LibcBuilder.csproj `
  -c Release -- `
  tooling/LibcBuilder/api-surface.txt `
  tooling/LibcBuilder/runtime-imports.txt `
  build/libc.raw.elf
```

The builder emits:

- an x86-64 PS5 dynamic module (`e_type=0xFE18`) with 14 program headers;
- 2,566 exports in the `libc` and `libc_setjmp` export libraries;
- 102 named system imports;
- 100 jump-slot, 1,790 relative, three TLS-module, and three global-data
  relocations;
- the module parameter, TLS template, unwind index, SysV hash, dynamic tables,
  and SCE dynamic tags required by the loader;
- project-authored startup code that registers inert thread callbacks and
  supplies the application heap API through stable system imports.

Most exported functions are safe local stubs and exported objects are
zero-initialized. This is loader-visible API compatibility, not a complete C
standard-library implementation.

## Reproduce the bundled module

Run:

```powershell
./tools/rebuild-libc.ps1
```

The script emits and signs twice, checks deterministic hashes and size, rejects
known private build-path strings, verifies the `BlackBearReloaded` attribution,
and only then updates the bundled artifact.

```text
Raw ELF size:         1,335,962 bytes
Raw ELF SHA-256:      8ee6e124993e1af26420cb455890fd002f5d6c7e78883c860ce45734e7d002bb
Signed FSELF size:    1,284,674 bytes
Signed FSELF SHA-256: e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036
```

The exact signed artifact is validated with the Hello World application on
firmware 6.02 and 12.70 with ShadowMountPlus. The 12.70 validation covered both
directory and compressed `.ffpfsc` deployment. Preserve its hash when testing
another firmware or loader.
