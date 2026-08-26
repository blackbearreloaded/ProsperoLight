# Notices

## SharpProspero build dependency

SharpProspero source is not distributed in this repository. During setup,
`tools/setup-tooling.ps1` fetches
[SvenGDK/SharpProspero](https://github.com/SvenGDK/SharpProspero), originally by
SvenGDK, at commit `e36e610fa5b4be23ad38b9c8429f11f11750cc0c` and applies
`tooling/patches/sharpprospero-native-app.patch`. The generated checkout is
ignored under `.deps/SharpProspero`.

The fetched source and the compatibility delta remain subject to
SharpProspero's upstream GPL-3.0 terms.

## Optional UFS2Tool build dependency

When `-OutputFormat Ffpkg` (or its legacy `-Ffpkg` alias) is requested,
`tools/setup-ffpkg-tooling.ps1` fetches
[SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) at commit
`b5307a60d5b4e3a68ba680e0e33cfadf05017c77` into the ignored
`.deps/UFS2Tool` cache. No UFS2Tool source or binary is distributed here.
The fetched dependency remains subject to its upstream BSD-2-Clause license.

The optional command profile follows the public procedure documented by
[sinajet/PSFFPKG](https://github.com/sinajet/PSFFPKG). PSFFPKG is not fetched,
copied, or distributed by this repository.

## Optional MkPFS build dependency

When `.ffpfsc` output is requested, `tools/setup-mkpfs-tooling.ps1` fetches
[PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) at commit
`6cb8313dfe0c988ac52617794553f343243d3a56` into the ignored `.deps/MkPFS`
cache and installs its Python dependencies in `.deps/MkPFS/.venv`. No MkPFS
source or binary is distributed here. The fetched dependency remains subject
to its upstream GPL-3.0 license; its Python dependencies retain their own
licenses.

## Independently authored runtime shim

`runtime/libc.prx`, `tooling/ConventionalLibcBuilder`, and its tracked manifests
are independently authored for this project and licensed under
GPL-3.0-or-later. The module contains project-authored compatibility stubs,
startup code, and semantic loader metadata. It contains no Sony runtime
implementation. `MinimalLibcBuilder` and `CompactLibcBuilder` remain as
historical clean-room experiments.

Original ps5-native-app-boilerplate code is Copyright (C) 2026
BlackBearReloaded and licensed under GPL-3.0-or-later. Source and script files
carry matching SPDX identifiers.

## Original presentation assets

The BlackBear icon, selection artwork, and default selection track
`sce_sys/snd0.at9` are original project assets supplied by BlackBearReloaded,
Copyright (C) 2026 BlackBearReloaded, and distributed under
GPL-3.0-or-later. The track is titled `Night Drive`. Its distributable ATRAC9
master is a 15-second whole-track loop at 48 kHz stereo and
192 kb/s. The measured integrated loudness of the final encoded excerpt is
approximately -28.6 LUFS. The lossy source and uncompressed intermediate are
intentionally not included.

No Sony runtime module, SDK binary, encryption key, or game file is included.
