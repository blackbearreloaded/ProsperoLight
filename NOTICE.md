# Notices

## Native build dependencies

The application build uses LLVM/Clang/lld and the public
[PS5 payload SDK](https://github.com/ps5-payload-dev/sdk). The bootstrapper
downloads SDK v0.42 after verifying SHA-256
`8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`.
It also downloads the Linux/WSL distribution's native `zlib1g-dev` package and links
its static archive. Both are extracted under ignored `.deps/native/`, retain
their upstream licenses, and are not distributed by this repository. No Sony
SDK file is included.

The project’s PS5 ELF converter and FSELF writer are independently authored
GPL-3.0-or-later code. SharpProspero was a useful public format reference during
development but is not fetched, copied, linked, or required by the build.

## Optional native UFS2 dependency

When `.ffpkg` output is requested, the platform bootstrapper downloads the
Ubuntu/Debian `makefs` binary package into the ignored `.deps/makefs` cache and
extracts it without root privileges. `makefs` is native code derived from the
NetBSD/MirBSD implementation and retains the copyright and license text shipped
inside that package. Nothing from the package is committed here.

The selected UFS2 profile follows the public procedures documented by
[SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) and
[sinajet/PSFFPKG](https://github.com/sinajet/PSFFPKG); neither project is a
runtime or build dependency.

## Optional MkPFS dependency

When `.ffpfsc` output is requested, the platform bootstrapper fetches
[PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) at commit
`6cb8313dfe0c988ac52617794553f343243d3a56` into the ignored `.deps/MkPFS`
cache and installs its Python dependencies there. MkPFS and its dependencies
retain their own licenses and are not distributed by this repository.

## Independently authored runtime shim

`tooling/native/libc_builder.cpp` and the manifests under
`tooling/native/runtime/` are independently authored for this project and
licensed under GPL-3.0-or-later. The generated `runtime/libc.prx` contains
project-authored compatibility stubs, startup code, and semantic loader
metadata. It contains no Sony runtime implementation.

Original ps5-native-app-boilerplate code is Copyright (C) 2026
BlackBearReloaded and licensed under GPL-3.0-or-later. Source and script files
carry matching SPDX identifiers.

## Original presentation assets

The BlackBear icon, selection artwork, and default selection track
`sce_sys/snd0.at9` are original assets supplied by BlackBearReloaded, Copyright
(C) 2026 BlackBearReloaded, and distributed under GPL-3.0-or-later. The track
is titled `Night Drive`.

No proprietary runtime module, encryption key, or game file is included.
