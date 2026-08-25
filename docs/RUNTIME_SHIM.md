# Clean-room runtime shim

This repository includes [`runtime/libc.prx`](../runtime/libc.prx), an
independently authored 4,898-byte loader companion. It is not copied from a
game, application, console firmware, or SDK.

## Provenance and clean-room method

BlackBearReloaded designed and implemented the shim specifically for this
repository. Its complete source is the tracked
[`tooling/MinimalLibcBuilder/Program.cs`](../tooling/MinimalLibcBuilder/Program.cs)
emitter, which constructs the module from project-owned semantic constants.

The emitter does not read, import, transform, embed, or link any Sony
`libc.prx`, firmware module, proprietary SDK file, or game file. No proprietary
code or data was copied into the source or generated artifact. Loader-visible
interfaces and ELF/FSELF structure informed the compatibility requirements;
the implementation of those requirements was written independently from
scratch.

## What it does

The supported application layout includes a loader-visible file named
`sce_module/libc.prx`. The shim implements the release compatibility contract:

- `Need_sceLibc=1` under libc export library ID 3/module ID 0;
- inert `_setjmp` and `_longjmp` exports under library ID 4;
- the required module, segment, version, and dynamic-table geometry;
- a nonempty GNU EH header that validly describes an empty FDE table;
- empty import and relocation tables.

It contains four independently written `xor eax,eax; ret` stubs and semantic
metadata. It does not provide malloc, stdio, filesystem, networking, threading,
or other C library APIs. Application imports continue to bind to the platform
system modules selected by the pinned build-time linker.

## Artifact integrity

```text
Raw ELF SHA-256:
a71cdfb56c76545f6ecef6bd2bb4f6e670046fc5a654ca8212c8e599cdb16728

Bundled FSELF SHA-256:
af5dbb1c778135f63daf07f225f84fb948b07034d6d0cd2e393528510f2236b4
```

The FSELF digest is tracked in
[`runtime/libc.prx.sha256`](../runtime/libc.prx.sha256). From the `runtime`
directory, verify it with `sha256sum -c libc.prx.sha256`.

Compatibility depends on the target firmware and loader.

The release artifact passed the firmware 6.02 direct-entry control with all 26
diagnostic checkpoints and no EH corruption warning. The byte-identical shim
did not reach `main` in the tested firmware 12.70 loader environment and caused
a probable console reboot. It is therefore not compatible with that environment
and must not be described as universally firmware-compatible.

The separately tracked
[`tooling/CompactLibcBuilder`](../tooling/CompactLibcBuilder) preserves the
exact source and reproduction hashes for the later compact-layout experiment.
It is retained for research provenance and is not the bundled release shim.

## Reproduce it

The complete emitter is in `tooling/MinimalLibcBuilder`. It accepts only an
output path and cannot read a reference module. Rebuild and verify both raw and
signed outputs from PowerShell:

```powershell
./tools/rebuild-libc.ps1
```

The script emits twice, requires byte-identical results, checks the recorded
release hashes and size, rejects proprietary/reference build-path text, and
only then replaces `runtime/libc.prx` and its checksum manifest.

## Distribution and attribution

The shim and its source are GPL-3.0-or-later and may be redistributed with this
repository under its license. No Sony runtime implementation, proprietary SDK
binary, key, or game file is present.

The implementation is authored by BlackBearReloaded. Attribution is retained in
[`runtime/README.md`](../runtime/README.md), in the emitter's source headers,
and as a non-exported `BlackBearReloaded` metadata string in the binary.
