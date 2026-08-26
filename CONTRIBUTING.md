# Contributing

Keep the boilerplate small, reproducible, and useful to a first-time native-app
developer.

Before opening a change:

1. Run `./tools/check-headers.ps1` and `./tools/doctor.ps1`.
2. Run `./build.ps1` with the bundled clean-room `runtime/libc.prx` unchanged.
3. Confirm the build reports zero static FSELF errors.
4. Do not commit `build/`, `dist/`, `.local/`, proprietary PRXs, game files, SDK
   binaries, console dumps, keys, or credentials. The independently generated
   `runtime/libc.prx` is the sole intentional tracked PRX.
5. Include the firmware and loader context for platform-specific behavioral
   claims.

Every comment-capable code, script, project, workflow, tooling configuration,
manifest, and patch file must retain the project copyright and
`GPL-3.0-or-later` SPDX header. JSON and binary formats cannot carry comments;
their licensing is covered by `LICENSE` and `NOTICE.md`.

Changes to the SharpProspero compatibility patch should include a deterministic
output comparison or a narrowly scoped regression check. Do not silently
change its pinned upstream revision.
