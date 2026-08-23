# ps5-native-app-boilerplate

A beginner-oriented C/C++ template for building native PS5 homebrew
applications. It compiles application code with Clang, links a native PS5 ELF,
wraps it as a development FSELF, and assembles a complete title directory ready
for a directory-based homebrew loader.

The application itself is native C/C++. C# and .NET are used only on the
development PC during the build. On the first build, the bootstrapper fetches
a pinned [SharpProspero](https://github.com/SvenGDK/SharpProspero) revision into
the ignored `.deps/` cache and applies this repository's focused native-app
compatibility patch. No SharpProspero source is vendored here.

## What you get

- A minimal native application that displays a notification and stays alive.
- Original BlackBear launcher icon, 4K BC7 selection artwork, and selection
  music, ready to replace with an application's own identity.
- One asset-preparation command for developer-supplied artwork and audio.
- One editable [`project.json`](project.json) for title metadata and sources.
- One-command PowerShell build: `./build.ps1`.
- Selectable folder, UFS2 `.ffpkg`, and compressed `.ffpfsc` outputs.
- A complete [graphical Hello World example](examples/hello-world/README.md)
  with CPU-rendered text and geometric figures.
- A small local C# frontend for the on-demand SharpProspero linker and FSELF
  writer.
- A source-reproducible clean-room `libc.prx` loader shim.
- Static ELF/FSELF validation before an artifact is accepted.
- A ready-to-stage output directory under `dist/<TITLE_ID>/`.
- Documentation for setup, configuration, C# tooling, deployment, and known
  platform constraints.

PS5 firmware and homebrew loaders vary. Validate the generated application on
the target environment before distributing it.

## Quick start

1. Install [Git for Windows](https://git-scm.com/download/win),
   [WSL](https://learn.microsoft.com/windows/wsl/install), Clang 18,
   the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk), and the
   [.NET 10 SDK](https://dotnet.microsoft.com/download/dotnet/10.0).
2. Give the app a unique title ID and name in [`project.json`](project.json).
3. Optionally replace the presentation assets:

   ```powershell
   ./tools/prepare-assets.ps1 -Icon C:\art\icon.png -Background C:\art\background.png
   ```

4. Check the host:

   ```powershell
   ./tools/doctor.ps1
   ```

5. Build:

   ```powershell
   ./build.ps1
   ```

   Or select a package format:

   ```powershell
   ./build.ps1 -OutputFormat Ffpfsc
   ./build.ps1 -OutputFormat All
   ```

If `dotnet` is not on `PATH`, pass it explicitly:

```powershell
./build.ps1 -Dotnet C:\path\to\dotnet.exe
```

The finished app is `dist/<TITLE_ID>/`. Stage that entire directory with your
existing directory-based loader, such as
[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus); do not upload
only `eboot.bin`.

Read [Getting started](docs/GETTING_STARTED.md) before the first build.

## Repository layout

```text
project.json                App identity, sources, and build inputs
src/main.c                  Minimal native application
sce_sys/                    Param, icon, BC7 selection art, and optional snd0.at9
examples/hello-world/       Complete native CPU-rendered graphical example
build.ps1                   Complete Windows/WSL build
tools/doctor.ps1            Read-only prerequisite check
tools/inspect.ps1           Static ELF/FSELF validator
tools/prepare-assets.ps1     Presentation conversion and validation
tools/setup-ffpkg-tooling.ps1  Optional pinned UFS2Tool bootstrap
tools/setup-mkpfs-tooling.ps1  Optional pinned MkPFS bootstrap
tooling/NativeAppBuilder/   C# command-line build frontend
tooling/MinimalLibcBuilder/ Clean-room runtime-shim emitter
tooling/patches/             Native-app compatibility delta
tools/setup-tooling.ps1     Pinned SharpProspero fetch/bootstrap
.deps/SharpProspero/        Generated upstream checkout; ignored
runtime/libc.prx            Bundled clean-room loader shim
tools/rebuild-libc.ps1      Deterministic shim reproduction check
build/                      Generated intermediates; ignored
dist/                       Generated title directory; ignored
.local/runtime/             Optional private extra modules; ignored
```

## Documentation

- [Getting started](docs/GETTING_STARTED.md)
- [Presentation assets](docs/PRESENTATION_ASSETS.md)
- [Project configuration](docs/CONFIGURATION.md)
- [Build output formats](docs/FFPKG.md)
- [C# build tooling](docs/CSHARP_TOOLING.md)
- [Clean-room runtime shim](docs/RUNTIME_SHIM.md)
- [Deployment and smoke test](docs/DEPLOYMENT.md)
- [Platform findings](docs/PLATFORM_NOTES.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Contributing](CONTRIBUTING.md)

## Scope

This template produces a directory-style homebrew application and, optionally,
a mountable UFS2 `.ffpkg` image or compressed `.ffpfsc` image. It deliberately
does not create a signed retail PKG/FPKG container, automate an exploit, alter
console configuration, or bundle Sony files. GPU decode is outside this
foundation.

## License and attribution

Code authored for this repository is distributed under GPL-3.0-or-later.
The build fetches [SvenGDK/SharpProspero](https://github.com/SvenGDK/SharpProspero)
at the pinned commit
[`e36e610`](https://github.com/SvenGDK/SharpProspero/commit/e36e610fa5b4be23ad38b9c8429f11f11750cc0c)
and applies the tracked compatibility patch. The fetched dependency remains
under its upstream GPL-3.0 license. See [NOTICE.md](NOTICE.md).

This project was developed with assistance from OpenAI Codex. Project
maintainers reviewed and validated the resulting code and documentation.

PlayStation and PS5 are trademarks of Sony Interactive Entertainment. This
project is independent and is not affiliated with or endorsed by Sony.
