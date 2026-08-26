# Getting started

This guide supports native Linux, WSL, and Windows PowerShell hosts. It does
not configure or modify the PS5.

## 1. Install the Linux toolchain

Use an Ubuntu or Debian host directly, or follow Microsoft's
[WSL installation guide](https://learn.microsoft.com/windows/wsl/install).
Install the compiler, linker, Make, Python, and download/archive tools:

```bash
sudo apt update
sudo apt install clang-18 clang-format-18 clang-tidy-18 lld-18 make python3 python3-pip python3-venv unzip wget
```

Confirm the expected compiler exists:

```bash
/usr/bin/clang-18 --version
```

## 2. Native dependencies

The first build downloads the pinned public
[PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) and a native static
zlib package into `.deps/native/`. It verifies the SDK archive digest and
extracts both dependencies without administrator privileges.

Nothing is installed globally. Bare `make` invokes this bootstrapper
automatically and later builds reuse the ignored cache. To prefetch without
building:

```bash
make deps
```

Windows PowerShell users can run `./tools/setup-native-dependencies.ps1`
instead.

## 3. Install optional packaging prerequisites

The normal folder build needs no managed runtime or external host project.
Repository-owned tools are compiled from C/C++ source automatically.

The root application is C++20. It uses the libc++ headers already present in
the fetched public SDK while keeping exceptions and RTTI disabled. The build
links a small project-owned allocation bridge rather than the complete libc++
runtime; see [Native build tooling](NATIVE_TOOLING.md).

Compressed `.ffpfsc` output uses Python 3.9 or newer with `venv` support. The
build fetches MkPFS and installs it into an ignored virtual environment under
`.deps/MkPFS/` when selected.

Uncompressed `.ffpkg` output uses a native `makefs` binary downloaded into
`.deps/` on first use. It does not require administrator access or a global
installation.

## 4. Generate the clean-room loader shim

No proprietary runtime module is required. The repository includes the
complete clean-room emitter, input manifests, and expected digest. The first
`make` generates the 1,284,674-byte `runtime/libc.prx` locally and verifies its
SHA-256 before packaging. `make libc` forces an independent two-pass
reproduction. See
[Clean-room runtime shim](RUNTIME_SHIM.md) for its scope and reproduction
procedure.

## 5. Choose a unique app identity

Edit [`project.json`](../project.json). Change these fields together:

```json
{
  "titleName": "My Native App",
  "titleId": "PPSA99999",
  "conceptId": "99999",
  "contentId": "UP9000-PPSA99999_00-MYNATIVEAPP00001"
}
```

The title ID must be unique among applications already registered on your
console. `contentId` must contain the same title ID and end with exactly 16
uppercase letters or digits.

The template includes an original BlackBear presentation set. The easiest way
to give the app its own identity is:

```powershell
./tools/prepare-assets.ps1 `
    -Icon C:\art\my-icon.png `
    -Background C:\art\my-background.png
```

The generated console files are:

- `sce_sys/icon0.png`: 512x512 launcher icon.
- `sce_sys/pic0.dds`: 3840x2160 BC7 selection background.
- `sce_sys/pic1.dds`: 3840x2160 BC7 selection-background fallback.

`sce_sys/background-source.png`, `pic0.png`, and `pic1.png` are editable
previews; the build deploys only the DDS files. A PNG renamed to `.dds` is not
sufficient.

The normal directory-promotion path displays `titleName` as Shell-rendered text
over this artwork. Retail custom-font Game Hub logos and descriptions are
downloaded asynchronously as Internet catalog metadata; the supported
package-local fields cannot define them for a synthetic homebrew concept. See
[Platform Notes](PLATFORM_NOTES.md).

The default presentation set also includes original selection music. Preparing
MP3/M4A/AAC/WAV input requires FFmpeg plus a legally obtained compatible
ATRAC9 encoder; neither a Sony encoder nor any proprietary SDK tool is bundled
or downloaded. The script also accepts an already encoded `.at9` file.

See [Presentation assets](PRESENTATION_ASSETS.md) for source recommendations,
conversion commands, the supported format profile, licensing guidance,
and the catalog-owned logo/description limitation.

## 6. Check and build

From Linux or WSL in the repository root:

```bash
make lint
make
```

Only `make` is required for a normal folder build. It fetches missing native
dependencies, generates and verifies `runtime/libc.prx`, and builds the root
application. Linting remains an explicit development check.

Windows PowerShell remains supported:

```powershell
./tools/doctor.ps1
./build.ps1
```

Successful output ends with an app directory such as:

```text
dist/PPSA99999/
  eboot.bin
  sce_module/libc.prx
  sce_sys/icon0.png
  sce_sys/pic0.dds
  sce_sys/pic1.dds
  sce_sys/param.json
  sce_sys/snd0.at9
```

Choose the final output with Make:

```bash
make app
make ffpkg
make ffpfsc
make packages
```

The equivalent PowerShell selections are:

```powershell
./build.ps1 -OutputFormat Folder
./build.ps1 -OutputFormat Ffpkg
./build.ps1 -OutputFormat Ffpfsc
./build.ps1 -OutputFormat All
```

The optional packaging tools are fetched only on first use. See
[Build output formats](FFPKG.md).

`runtime/libc.prx` is a generated, ignored file. Tagged GitHub Releases also
publish the verified runtime as a standalone convenience download.

Continue with [Deployment](DEPLOYMENT.md).
