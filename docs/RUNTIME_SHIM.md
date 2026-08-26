# Clean-room runtime module

This repository includes [`runtime/libc.prx`](../runtime/libc.prx), the
project-authored loader companion used by the default template and the Hello
World example. The same binary is hardware-validated on PS5 firmware 6.02 and
12.70 with ShadowMountPlus.

It is not copied from a game, application, console firmware, Sony SDK, or
another developer's runtime module.

## Provenance

BlackBearReloaded designed and implemented the emitter, startup code,
compatibility stubs, relocation population, manifests, and metadata in this
repository. The complete build inputs are:

- [`tooling/LibcBuilder/Program.cs`](../tooling/LibcBuilder/Program.cs):
  deterministic clean-room ELF emitter;
- [`tooling/LibcBuilder/api-surface.txt`](../tooling/LibcBuilder/api-surface.txt):
  loader-visible export ABI manifest;
- [`tooling/LibcBuilder/runtime-imports.txt`](../tooling/LibcBuilder/runtime-imports.txt):
  named system imports and relocation roles;
- [`tooling/NativeAppBuilder`](../tooling/NativeAppBuilder): the host-side
  frontend that wraps the raw module in a development FSELF.
- [`tooling/patches/sharpprospero-native-app.patch`](../tooling/patches/sharpprospero-native-app.patch):
  the tracked GPL compatibility delta applied to the pinned SharpProspero host
  linker and container writer.

The emitter does not accept a reference binary and does not read, import,
transform, embed, or link proprietary runtime code. Reverse engineering was
used to learn interface facts and loader-visible structural requirements. The
implementation of those requirements was written independently.

The generated binary retains `BlackBearReloaded` in non-exported metadata. All
repository-authored source is GPL-3.0-or-later.

## Purpose

The tested application layout requires a loader-visible module named
`sce_module/libc.prx`. The bundled runtime satisfies that contract so a native application
can reach its own entry point without requiring users to supply a proprietary
game or application runtime.

It is a compatibility shim, not a complete C standard library. Most of its
large export surface consists of independently authored compatibility stubs or
zero-initialized object/TLS storage. Application functionality such as files,
networking, input, audio, and video continues to use the platform modules and
public SDK interfaces selected by the application linker.

Do not assume that every exported libc function has production libc semantics.
Code that needs a real implementation must supply one explicitly or bind to an
appropriate platform API.

## Loader-visible design

The raw module is an x86-64 PS5 dynamic module (`e_type=0xFE18`) with 14 program
headers. The development FSELF reports 12 container segments, authority
`0x3100000000000002`, and program type `1`.

The builder emits:

- 2,566 loader-visible exports;
- 102 named system imports;
- 100 `R_X86_64_JUMP_SLOT` relocations;
- 1,790 project-authored `R_X86_64_RELATIVE` relocations;
- three `R_X86_64_DTPMOD64` TLS-module relocations;
- three `R_X86_64_GLOB_DAT` relocations;
- the required `Need_sceLibc` marker and `libc`/`libc_setjmp` export-library
  identities;
- a valid GNU unwind header describing an empty FDE table;
- independently authored module, version, dynamic-table, TLS, and segment
  metadata.

Its startup path registers three inert thread-atexit callbacks with libkernel
and installs a nine-word application heap API table. The populated table entries
delegate `malloc`, `free`, and `posix_memalign` to the system
`libSceLibcInternal` exports; unused entries remain zero. This is why the shim
can participate in normal runtime initialization without containing those
allocator implementations itself.

The file size comes from the loader geometry and complete symbol/relocation
tables required across the tested firmware versions. It does not represent a
copied libc implementation.

## Host linker and container requirements

The build depends on several host-side format capabilities that are not present in the
pinned upstream SharpProspero revision. The tracked compatibility patch adds
them without copying the upstream repository into this project:

- separate module, import-library, and per-symbol export-library identities;
- the sparse PRX virtual-address and file-offset grid, including capacity
  checks for every fixed slot;
- the canonical libc init, fini, pre-init-array, GOT, TLS, and module-parameter
  positions;
- configurable module/companion SDK records and version-component names;
- correct ordering and accounting for relative, jump-slot, global-data, and TLS
  relocations;
- an empty but structurally valid GNU unwind header when the profile has no
  frame-description entries;
- support in the development FSELF writer for both recognized container
  layouts and explicit inclusion of a process-parameter segment when required.

The patch also carries regression tests for these behaviors. It is applied only
to the pinned upstream commit recorded by `tools/setup-tooling.ps1`; the setup
script rejects a different revision or unexplained cache changes.

## Deterministic artifact

```text
Raw ELF size:     1,335,962 bytes
Raw ELF SHA-256:  8ee6e124993e1af26420cb455890fd002f5d6c7e78883c860ce45734e7d002bb

Bundled FSELF size:    1,284,674 bytes
Bundled FSELF SHA-256: e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036
```

The FSELF digest is tracked in
[`runtime/libc.prx.sha256`](../runtime/libc.prx.sha256). From the `runtime`
directory, verify it with:

```sh
sha256sum -c libc.prx.sha256
```

Both default project files pin this exact SHA-256. The normal build refuses to
package a different runtime under the same configuration.

## Reproduce it

Requirements:

- Windows PowerShell 5.1 or newer;
- .NET SDK 10;
- Git and network access on the first run so the pinned SharpProspero source can
  be fetched into the ignored `.deps/` cache.

From the repository root:

```powershell
./tools/rebuild-libc.ps1
```

The script:

1. fetches or verifies the pinned open-source host tooling;
2. emits the raw runtime twice from the tracked source and manifests;
3. requires byte-identical raw ELF outputs and the recorded raw hash;
4. wraps both outputs as development FSELF containers;
5. requires byte-identical FSELF outputs, the recorded signed hash, and the
   expected size;
6. requires the non-exported `BlackBearReloaded` attribution marker and rejects
   known proprietary/reference build-path strings;
7. replaces `runtime/libc.prx` and its checksum only after every check passes.

The equivalent low-level commands are:

```powershell
dotnet run --project tooling/LibcBuilder/LibcBuilder.csproj `
  -c Release -- `
  tooling/LibcBuilder/api-surface.txt `
  tooling/LibcBuilder/runtime-imports.txt `
  build/libc.raw.elf

dotnet run --project tooling/NativeAppBuilder/NativeAppBuilder.csproj `
  -c Release -- self --sign `
  --in build/libc.raw.elf `
  --out build/libc.prx
```

The private modules used during research are neither build inputs nor repository
content.

## Hardware validation

The exact bundled FSELF completed the Hello World application on both tested
consoles:

- firmware 6.02: the CPU-rendered scene and packaged `/app0` asset appeared,
  the application closed normally, and FTP/elfldr/klog/websrv remained healthy;
- firmware 12.70: directory and compressed `.ffpfsc` deployments both entered
  `eboot`, stopped normally, left FTP/elfldr/klog/websrv healthy, and produced
  no loader, fatal-signal, crash, or panic marker. The compressed-image test
  also verified the uploaded image hash before launch.

These results validate the exact recorded artifact on those two environments.
They do not prove compatibility with every firmware, loader, or application.
Preserve the hash when comparing future results.

## Distribution

The shim, manifests, and emitter may be redistributed with this repository
under GPL-3.0-or-later. No Sony runtime implementation, proprietary SDK binary,
encryption key, or game file is included. The fetched SharpProspero host tooling
remains subject to its upstream GPL-3.0 license; see [`NOTICE.md`](../NOTICE.md).
