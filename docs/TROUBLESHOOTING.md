# Troubleshooting

## Linux, WSL, or the native toolchain is missing

On Linux or WSL, run:

```bash
make deps
make lint
```

On Windows PowerShell, run:

```powershell
./tools/doctor.ps1
```

On Linux or inside WSL, confirm the native toolchain:

```bash
test -x /usr/bin/clang-18
test -x /usr/bin/clang++
test -x /usr/bin/wget
test -x /usr/bin/unzip
test -x /usr/bin/apt-get
test -x /usr/bin/dpkg-deb
```

On Ubuntu, install missing host packages with:

```bash
sudo apt install clang-18 clang-format-18 clang-tidy-18 lld-18 make python3 python3-pip python3-venv unzip wget
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
listed in `project.json`. Platform imports must exist in the public SDK stubs
under `.deps/native/ps5-payload-sdk/target/lib`. Do not silence unresolved symbols;
update the SDK or provide a legitimate native implementation.

## Native dependency bootstrap fails

The first build needs network access to download the hash-pinned public PS5
payload SDK and the distribution's native zlib development package. Retry:

```bash
make deps
```

The script writes only to `.deps/native/` and never installs packages globally.

## Optional package setup fails

- `.ffpkg` requires network access once to download and extract native
  `makefs` into `.deps/makefs`.
- `.ffpfsc` requires Git and Python 3.9 or newer with `venv` support. MkPFS and
  its isolated environment are stored under `.deps/MkPFS`.
- Folder output has neither optional dependency. Use `make app` to isolate
  packaging from compilation.

Nothing is installed globally by these optional bootstrappers.

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
- Selection pictures must be 3840x2160 DX10 DDS files using BC7 UNORM. A PNG
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

## `/download0` is missing

Keep a positive `downloadDataSize` in `project.json`, rebuild, and stage the
new generated directory. Do not attempt to write to `/app0`.
