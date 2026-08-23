<!--
  ps5-native-app-boilerplate - Example index.
  Copyright (C) 2026 BlackBearReloaded
  SPDX-License-Identifier: GPL-3.0-or-later
-->

# Examples

Each example is a complete application profile with its own source,
`project.json`, launcher icon, and selection background. Build one from its
directory; the shared repository tooling writes the result to
`dist/<TITLE_ID>/`.

| Example | Demonstrates |
| --- | --- |
| [Hello World](hello-world/README.md) | CPU-rendered bitmap text, rectangles, a circle, and a triangle through native VideoOut. |

Examples deliberately reuse the root build and clean-room runtime shim. This
keeps the working linker, FSELF writer, validation, and runtime contract in one
place.
