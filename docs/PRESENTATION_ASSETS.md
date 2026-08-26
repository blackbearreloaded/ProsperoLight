# Presentation assets

This repository can turn ordinary developer-owned artwork and audio into the
launcher formats supported by this template. Conversion changes only the
repository's `sce_sys` files; it does not connect to or configure a console.

## What you can customize

| Experience | Source to provide | Generated console file |
| --- | --- | --- |
| Launcher tile | Square PNG, JPEG, or other texconv-readable image | `sce_sys/icon0.png`, 512x512 PNG |
| Selection background | 16:9 image, preferably 3840x2160 | `sce_sys/pic0.dds`, 4K BC7 DX10 DDS |
| Launch/loading background | 16:9 image, preferably 3840x2160 | `sce_sys/pic1.dds`, 4K BC7 DX10 DDS |
| Selection music | MP3, M4A, AAC, WAV, FLAC, or a ready AT9 file | `sce_sys/snd0.at9`, 48 kHz stereo ATRAC9 |
| Displayed app name | `localizedParameters.<language>.titleName` in `sce_sys/param.json` | Shell-rendered text |

Retail-style custom-font logos and descriptions are catalog metadata, not
package assets. A synthetic homebrew concept has no retail catalog record, and
the supported `param.json` and image fields cannot create one. Catalog-database
modification is outside this portable template.

If desired, make a graphical logo part of the background. The Shell-rendered
`titleName` will still be present, so compose around it. Keep the main subject
on the right and leave breathing room on the left for the title and Play
button. Always verify the final composition on a TV because the Shell adds its
own crop, gradient, dimming, and controls.

## Install the image converter

On Windows, install Microsoft's open-source
[DirectXTex `texconv`](https://github.com/microsoft/DirectXTex/wiki/texconv)
utility:

```powershell
winget install Microsoft.DirectXTex.Texconv
```

Open a new PowerShell window after installation. Alternatively, pass the full
executable path with `-Texconv`.

## Prepare the icon and backgrounds

From the repository root:

```powershell
./tools/prepare-assets.ps1 `
    -Icon C:\art\my-icon.png `
    -Background C:\art\my-background.png
```

The command normalizes the icon to 512x512 and the background to 3840x2160,
then produces the single-surface `DXGI_FORMAT_BC7_UNORM` (98), DX10 DDS
profile required by the template. `-Background` is a shorthand that uses the
same image for both `pic0.dds` and `pic1.dds`.

Use separate artwork when the selected-app view and launch transition should
look different:

```powershell
./tools/prepare-assets.ps1 `
    -SelectionBackground C:\art\selected-app.png `
    -LaunchBackground C:\art\launching.png
```

Hardware traces from the validated launcher path show `pic0.dds` in the
selected-app presentation and `pic1.dds` during the launching-game transition.
Shell and loader caching can delay visible changes, so refresh the title using
the loader's normal procedure after replacement.

The converter deliberately creates no mipmaps. Supply artwork with the correct
1:1 and 16:9 aspect ratios; resizing does not invent a good crop. Editable
previews are kept as `pic0.png` and `pic1.png`; the legacy `-Background`
shorthand also keeps `background-source.png`. Only `icon0.png`, `pic0.dds`, and
`pic1.dds` are deployed.

## Prepare selection music

Selection music is optional. If `sce_sys/snd0.at9` is absent, the build simply
omits it.

For an MP3, M4A, AAC, WAV, or FLAC source, the script needs:

- [FFmpeg](https://ffmpeg.org/) on Windows `PATH`, or FFmpeg inside WSL
  (`sudo apt install ffmpeg`).
- A compatible `ps4_at9tool.exe` that you are legally permitted to use.

No ATRAC9 encoder is included, downloaded, or linked by this repository.
FFmpeg prepares the source but does not encode the final ATRAC9 stream.

Convert the first 15 seconds:

```powershell
./tools/prepare-assets.ps1 `
    -Audio C:\music\selection.mp3 `
    -At9Tool C:\tools\ps4_at9tool.exe
```

Choose a different excerpt with `-AudioStart`:

```powershell
./tools/prepare-assets.ps1 `
    -Audio C:\music\selection.m4a `
    -AudioStart 42.5 `
    -AudioDuration 15 `
    -At9Tool C:\tools\ps4_at9tool.exe
```

The script strips metadata, normalizes toward -28 LUFS, creates 48 kHz stereo
16-bit PCM, encodes ATRAC9 at 192 kb/s, and adds a whole-track RIFF `smpl`
loop. It also writes `pubtools.loudnessSnd0` as `-28.00` when `param.json` is
present. The default 15-second, approximately 360 KB profile is intentionally
conservative, but `-AudioDuration` accepts values through 87.3 seconds.

The 15-second value is a template policy, not a Shell duration limit. Offline
inspection of the Shell audio validator established these file-level limits:

- ATRAC9 in a RIFF/WAVE container at exactly 48 kHz.
- Mono at no more than 96 kb/s, or stereo at no more than 192 kb/s.
- A total RIFF file length no greater than 2,097,152 bytes (2 MiB).

The validator has no separate duration comparison. Duration follows from the
chosen channel count, bitrate, and ATRAC9 frame/container overhead. With the
template's 48 kHz stereo, 192 kb/s, whole-loop encoding, the largest complete
file below the ceiling is 2,096,808 bytes: 4,193,024 samples, or
87.354666667 seconds. One additional sample requires another 512-byte frame,
producing a 2,097,320-byte file that the Shell rejects. The converter caps
requested excerpts at 87.3 seconds to remain below that boundary. Short clips
remain quicker to prepare and smaller to distribute.

If you already have a correctly encoded AT9 file, no encoder or FFmpeg is
needed:

```powershell
./tools/prepare-assets.ps1 -Audio C:\music\selection.at9
```

Only publish audio you own or have permission to distribute.

## Validate and build

Validate the current presentation files at any time:

```powershell
./tools/prepare-assets.ps1 -ValidateOnly
```

The normal build runs the same validation automatically:

```powershell
make
```

The validator rejects renamed PNG-as-DDS files, mipmapped or non-BC7 DDS
images, wrong dimensions, non-ATRAC9 audio, missing loop metadata, and audio
outside the supported sample-rate/channel/bitrate/size profile.
