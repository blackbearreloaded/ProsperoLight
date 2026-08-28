# Troubleshooting

## Linux, WSL, or the native toolchain is missing

On Linux or WSL, run:

```bash
make doctor
```

On Windows PowerShell, run:

```powershell
./tools/doctor.ps1
```

On Linux or inside WSL, confirm the native toolchain:

```bash
test -x /usr/bin/clang-18
test -x /usr/bin/clang++
test -x /usr/bin/curl
test -x /usr/bin/wget
test -x /usr/bin/unzip
test -x /usr/bin/tar
test -x /usr/bin/llvm-ar-18
```

On Ubuntu, install missing host packages with:

```bash
sudo apt install clang-18 clang-format-18 clang-tidy-18 curl lld-18 make python3 python3-pip python3-venv tar unzip wget
```

## The generated `libc.prx` is missing or has the wrong hash

Run the source reproducer, which verifies both release digests:

```bash
make libc
```

Normal `make` builds it automatically when absent. Do not replace it with a
module extracted from a game or firmware.

## The linker reports unresolved symbols

Check spelling, C versus C++ linkage, and whether the needed static archive is
listed in `APP_STATIC_ARCHIVES` or the relevant `PACBREW_*` variable. Platform
imports must exist in the public SDK stubs under
`.deps/native/ps5-payload-sdk/target/lib`. Do not silence unresolved symbols;
update the SDK or provide a legitimate native implementation.

## Native dependency bootstrap fails

The first build needs network access to download the hash-pinned public PS5
payload SDK and upstream zlib source archive. Retry:

```bash
make deps
```

The script writes only to `.deps/native/` and never installs packages globally.

## Optional package setup fails

- `.ffpkg` requires Git, the .NET SDK 8 or newer, and network access on first
  use. UFS2Tool and its build output are stored under `.deps/UFS2Tool`.
- `.ffpfsc` requires Git and Python 3.9 or newer with `venv` support. MkPFS and
  its isolated environment are stored under `.deps/MkPFS`.
- Folder output has neither optional dependency. Use `make app` to isolate
  packaging from compilation.

Nothing is installed globally by these optional bootstrappers.

## FTP deployment fails

- Confirm `PS5_HOST` identifies the intended console and its FTP service is
  already running on `FTP_PORT` (default `2121`).
- Run `make deploy PS5_HOST=192.0.2.1 DEPLOY_DRY_RUN=1` to validate the local
  build and destination without sending a network request.
- Fully close the previous title and any crash dialog before replacing its
  files. Never launch while deployment is still running.
- A remaining hidden `.upload` file indicates that a transfer or rename did
  not finish. Rerunning deployment safely overwrites that temporary file.
- Folder deployment intentionally does not delete remote files absent from the
  new build. Manually clean the title directory if a removed asset or module
  must disappear.
- Do not leave both a title folder and an image with the same title ID under
  active scan paths. ShadowMountPlus may prefer its existing mounted source.
- Wait for the mount service to report the title ready before launching. The
  Make target uploads only; it does not launch the app.

## The title does not appear

- Confirm `dist/<TITLE_ID>/sce_sys/param.json` and `icon0.png` exist.
- Confirm another title is not still active in the loader.
- Use a title ID not already registered by another application.
- Wait for the directory loader's explicit ready/installed message.
- Stage the whole title directory, not only `eboot.bin`.

## The icon, background, or selection audio does not update

- Run `make`; both the Make and PowerShell builds validate the tracked
  presentation assets before compiling.
- Confirm `icon0.png`, `pic0.dds`, and `pic1.dds` reached
  `dist/<TITLE_ID>/sce_sys/`.
- Selection and launch pictures must be 3840x2160 DX10 DDS files using BC7 UNORM. A PNG
  renamed to `.dds` is not sufficient.
- Audio must be ATRAC9 in a RIFF container named exactly `snd0.at9`; renaming
  MP3 or AAC input does not convert it.
- The RIFF must contain one `smpl` loop. If selecting the app stops default
  home-screen music but remains silent, inspect the chunk list.
- `Base.BgmController: Invalid file size` means Shell rejected the file. The
  observed limit is 2,097,152 bytes (2 MiB), not a fixed duration. At stereo
  192 kb/s, keep input at or below 4,193,024 samples (87.354666667 seconds) so
  frame padding stays below the ceiling.
- Presentation metadata may be cached for an already registered title. Follow
  the loader's documented refresh procedure after structural changes.
- Retail-style custom logos and descriptions are Internet catalog metadata,
  not package assets for a synthetic homebrew concept.

## The app immediately crashes

- Do not return from `main` or call an exit function.
- Keep the generated runtime digest unchanged while testing the baseline.
- Keep the default FSELF magic and SDK pair until the baseline launches.
- Run `tools/inspect.ps1 dist/<TITLE_ID>/eboot.bin` and resolve every error.
- Consult loader diagnostics; the home-screen message alone is not a root
  cause.

## Moving focus crashes the launcher

Symbolicate the fatal instruction and backtrace against `build/llvm-pie.elf`.
RmlUi loads hidden button-state textures lazily when focus changes, so a trap in
`operator new` followed by `SdlRenderInterface::LoadTexture` indicates that the
large-allocation mapping path was lost or failed. The launcher must keep
allocations of 64 KiB or more outside the bounded libc heap.

## `/download0` is missing

Keep a positive `downloadDataSize` in `sce_sys/param.json`, rebuild, and stage the
new generated directory. Do not attempt to write to `/app0`.
