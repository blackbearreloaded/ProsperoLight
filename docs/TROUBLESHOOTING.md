# Troubleshooting

## `dotnet` was not found

Install the [.NET 10 SDK](https://dotnet.microsoft.com/download/dotnet/10.0),
open a new PowerShell window, or pass the executable directly:

```powershell
./build.ps1 -Dotnet C:\path\to\dotnet.exe
```

## WSL, Clang 18, or the SDK is missing

Run:

```powershell
./tools/doctor.ps1
```

Inside WSL, confirm both paths:

```bash
test -x /usr/bin/clang-18
test -d /opt/ps5-payload-sdk/target/include
```

## SharpProspero setup failed

Confirm Git for Windows can reach
`https://github.com/SvenGDK/SharpProspero.git`. The bootstrapper accepts only
the revision pinned in `tools/setup-tooling.ps1`. If `.deps/SharpProspero` is an
incomplete checkout or contains experimental edits, move it aside and rerun:

```powershell
./tools/setup-tooling.ps1
```

The bootstrapper never changes global Git configuration.

## The bundled `libc.prx` is missing or has the wrong hash

Restore `runtime/libc.prx` from Git. To reproduce it from the tracked emitter
and verify both release hashes, run:

```powershell
./tools/rebuild-libc.ps1
```

Do not replace the public artifact with a module extracted from a game or
firmware.

## The linker reports unresolved symbols

The source called a function absent from the fetched import catalog or supplied
archives. Check spelling and C/C++ linkage first. Then either add the required
static library in `project.json` or update the tracked SharpProspero
compatibility patch deliberately. Do not silence unresolved symbols.

## The title does not appear

- Confirm `dist/<TITLE_ID>/sce_sys/param.json` and `icon0.png` exist.
- Confirm another title is not still active in the loader.
- Use a title ID not already registered by another application.
- Wait for the directory loader's explicit ready/installed message.
- Stage the whole title directory, not only `eboot.bin`.

## The icon, background, or selection audio does not update

- Run `./tools/prepare-assets.ps1 -ValidateOnly`; `build.ps1` also runs this
  check before compiling.
- Confirm `icon0.png`, `pic0.dds`, and `pic1.dds` reached the generated
  `dist/<TITLE_ID>/sce_sys/` directory.
- Selection pictures must be 3840x2160 DX10 DDS files using BC7 UNORM. PNG
  pictures can be kept as editable sources, but the PS5 promoter may copy and
  then ignore them, leaving `pic0Info` and `pic1Info` unset.
- Audio must be ATRAC9 in a RIFF container named exactly `snd0.at9`; renaming
  an MP3 or AAC file does not convert it.
- The RIFF must contain one `smpl` loop. If selecting the app stops the default
  home-screen music but the replacement remains silent, inspect the chunk list;
  this is the observed signature of an acknowledged but non-playing track.
- `Base.BgmController: Invalid file size` indicates Shell rejection before
  playback. The exact accepted total-file maximum is 2,097,152 bytes (2 MiB),
  not 15 seconds; shorten and re-encode the track rather than debugging
  application AudioOut. For stereo 192 kb/s output from the documented encoder,
  keep the source at or below 4,193,024 samples (87.354666667 seconds) so frame
  padding does not cross the file ceiling.
- Presentation metadata may be cached for an already registered title. Follow
  the loader's documented refresh or unregister procedure after changing the
  presentation structure.
- A retail-style custom logo and description are Internet catalog metadata,
  not package assets. Their absence on a synthetic homebrew concept is expected
  and cannot be fixed by renaming or adding another local image.

## The app immediately crashes

- Do not return from `main` or call an exit function.
- Keep the bundled shim unchanged while testing the baseline.
- Keep the default FSELF magic and SDK pair until the baseline launches.
- Run `tools/inspect.ps1 dist/<TITLE_ID>/eboot.bin` and resolve every error.
- Consult the loader's diagnostics; the home-screen message alone is not a
  root cause.

## `/download0` is missing

Keep a positive `downloadDataSize` in `project.json`, rebuild, and stage the
new generated directory. Do not attempt to write to `/app0`.
