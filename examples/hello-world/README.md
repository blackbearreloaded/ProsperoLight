<!--
  ps5-native-app-boilerplate - Native Hello World guide.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later
-->

# Native Hello World

A complete graphical first app. It writes a 1920x1080 frame with the CPU,
renders a tiny 5x7 bitmap font, draws three geometric figures, submits the
frame through native VideoOut, reads a packaged text asset from `/app0`, and
stays alive until the shell closes it.

It uses VideoOut directly. No SDL build or other third-party runtime is needed.

## Build

From Windows PowerShell:

```powershell
cd examples/hello-world
./build.ps1
```

Build a compressed `.ffpfsc` image:

```powershell
./build.ps1 -OutputFormat Ffpfsc
```

Use `-OutputFormat Ffpkg` for the UFS2 image or `-OutputFormat All` for both
package formats.

If `dotnet` is not on `PATH`:

```powershell
./build.ps1 -Dotnet C:\path\to\dotnet.exe
```

The complete app folder appears at `../../dist/PPSA99998/`. Package selections
also create `../../dist/PPSA99998.ffpkg`, `../../dist/PPSA99998.ffpfsc`, or
both. Stage one complete form supported by the loader.

## What to edit

- `src/main.c` contains the renderer, bitmap glyphs, and shapes.
- `assets/banner.txt` is copied to `/app0/assets/banner.txt`, read at startup,
  and rendered on the frame.
- `project.json` owns this example's title identity.
- `art/` contains the editable original icon and background artwork.
- `sce_sys/` contains the converted files consumed by the PS5 shell.

Regenerate the shell assets after replacing the artwork:

```powershell
../../tools/prepare-assets.ps1 `
  -Icon ./art/icon-source.png `
  -Background ./art/background-source.png `
  -Texconv ../../.local/tools/directxtex/texconv.exe `
  -OutputDirectory ./sce_sys
```

`texconv` may instead be installed on `PATH`. The checked-in example includes
the same original 15-second `Night Drive` selection loop as the root template.
Replace it with developer-owned audio using
[`docs/PRESENTATION_ASSETS.md`](../../docs/PRESENTATION_ASSETS.md).

## Runtime rules illustrated here

- The framebuffer uses the template's tiled 1920x1080 RGBA8 layout.
- Direct memory type 3 backs two 16 MiB VideoOut buffers.
- CPU cache lines are flushed before the first flip.
- Read-only files under `assets/` are available under `/app0/assets/`.
- `main` never returns and does not call `exit`; shell-mediated closure owns
  process termination in this launch context.

This is deliberately a first-frame example, not a reusable graphics library.
Extract an abstraction only after the next application needs one.
