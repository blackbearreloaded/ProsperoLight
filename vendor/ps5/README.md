# PS5 dependency snapshot

This directory contains the small set of PS5 build artifacts needed by the
first RmlUi + SDL smoke target.

- SDL2 headers and `libSDL2.a` come from the local PS5 SDL build.
- RmlUi headers and `librmlui.a` come from the local RmlUi 6.2 build with
  its FreeType font engine enabled, Lua bindings disabled, and
  `-fno-exceptions` to remove unused exception metadata.
- FreeType 2.13.2 is built as a static PS5 archive without optional
  compression or image libraries. Its FreeType Project License is retained
  next to the archive.
- The PS5 compiler emits `.ctors`; the checked-in SharpProspero compatibility
  patch handles constructor arrays and COMDAT section symbols.
- C++ runtime archives and libc/kernel stub catalogs come from the installed
  PS5 Payload SDK in WSL.

The bundled Lato and Noto Emoji font faces are distributed under the SIL Open
Font License retained in `ui/fonts/LICENSE.txt`. DejaVu Sans provides the
symbol fallback and retains its license in `ui/fonts/DejaVuSans-LICENSE.txt`.
