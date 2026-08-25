# Platform constraints

These notes describe the compatibility baseline used by the template. PS5
firmware and homebrew loaders vary, so applications must verify their target
environment.

## Loader and runtime

- The application is a native PS5 ELF of type `0xFE10` inside a development
  FSELF.
- FSELF magic `0x1D3D154F` and SDK pair
  `0x02000009 / 0x08050001` are the template defaults.
- The supported title layout includes `sce_module/libc.prx`. The bundled
  clean-room shim supplies its minimal loader contract and contains no Sony
  implementation code.
- The bundled shim is hardware-verified on firmware 6.02 with ShadowMountPlus.
  In the tested firmware 12.70 loader environment it failed before `main` and
  caused a probable console reboot; that environment is not supported by the
  current release.
- The shim is not a C library. Application imports bind to platform modules
  selected by the linker.
- The examples keep `main` alive and rely on the host application lifecycle
  for process closure.

## Filesystem

- `/app0` is the read-only application image.
- `/download0` is available for writable application data when
  `downloadDataSize` is positive in `param.json`.
- Applications should use the sandbox path rather than relying on its host
  backing-file location.
- Availability of `/temp0` and other mounts depends on the loader and title
  environment.
- SaveData setup is not included. Use `/download0` for ordinary configuration
  and provide export/import for data that must survive title removal or cache
  management.

## Home-screen presentation

- `icon0.png` is a 512x512 launcher icon.
- `pic0.dds` and `pic1.dds` are 3840x2160, single-surface,
  `DXGI_FORMAT_BC7_UNORM` DX10 DDS images.
- The package controls `titleName`, the launcher icon, selection backgrounds,
  and optional selection audio.
- Retail-style custom logos and descriptions are online catalog metadata and
  cannot be defined by the supported package-local fields for a synthetic
  homebrew concept.
- Selection audio uses a 48 kHz stereo ATRAC9 RIFF file named `snd0.at9` with
  one `smpl` loop. The asset tool enforces a conservative 15-second,
  approximately 360 KB profile.
- The Shell's file-size ceiling for selection audio is exactly 2,097,152 bytes
  (2 MiB); it rejects files starting at 2,097,153 bytes. It has no independent
  duration limit. At the template's stereo 192 kb/s profile, ATRAC9 frame
  granularity makes 87.354666667 seconds (2,096,808 bytes) the largest
  constructible whole-loop file below that ceiling.
- The same validator accepts 48 kHz mono up to 96 kb/s and 48 kHz stereo up to
  192 kb/s. These are documented platform observations, not additional output
  profiles promised by the asset-preparation script.
- Presentation metadata may be cached by the shell or loader. Follow the
  loader's refresh procedure after structural asset changes.

## Application capabilities

The template includes notification and basic lifecycle code. The graphical
example adds CPU-rendered VideoOut output. Filesystem, networking, controller
input, AudioOut, and third-party libraries can be added through the public SDK
interfaces and explicit build inputs.

GPU decoding is outside this repository's scope.
