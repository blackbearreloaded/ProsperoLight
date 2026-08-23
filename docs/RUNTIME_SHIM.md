# Clean-room runtime shim

This repository includes [`runtime/libc.prx`](../runtime/libc.prx), an
independently authored 4,898-byte loader companion. It is not copied from a
game, application, console firmware, or SDK.

## What it does

The supported application layout includes a loader-visible file named
`sce_module/libc.prx`. The shim implements the release compatibility contract:

- `Need_sceLibc=1` under libc export library ID 3/module ID 0;
- inert `_setjmp` and `_longjmp` exports under library ID 4;
- the required module, segment, version, and dynamic-table geometry;
- empty import and relocation tables.

It contains four independently written `xor eax,eax; ret` stubs and semantic
metadata. It does not provide malloc, stdio, filesystem, networking, threading,
or other C library APIs. Application imports continue to bind to the platform
system modules selected by the pinned build-time linker.

## Artifact integrity

```text
Raw ELF SHA-256:
1f176192c7e55cdd6477d11b2290d05fad63e803dab057216f6f92f4b4b37867

Bundled FSELF SHA-256:
cd961ee6ed3d08117459b0fe70d86fe322672ebe0103678ee7c3f15af7e00504
```

The FSELF digest is tracked in
[`runtime/libc.prx.sha256`](../runtime/libc.prx.sha256). From the `runtime`
directory, verify it with `sha256sum -c libc.prx.sha256`.

Compatibility depends on the target firmware and loader.

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
