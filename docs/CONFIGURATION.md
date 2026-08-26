# Project configuration

[`project.json`](../project.json) is the only normal build configuration file.

The repository root is the application skeleton. Fork the project, edit this
file and `src/main.c`, and list additional source files explicitly below.

| Field | Purpose |
| --- | --- |
| `titleName` | Name displayed on the home screen. |
| `titleId` | Unique `PPSA` plus five-digit application identifier. |
| `conceptId` | Five numeric characters; normally the numeric title-ID portion. |
| `contentId` | Package identity containing the title ID and a 16-character suffix. |
| `moduleSdkVersion` | Loader-visible SDK value. Keep the release default unless the target requires another value. |
| `companionSdkVersion` | Companion SDK value paired with the module SDK. |
| `fselfMagic` | Loader-compatible FSELF magic. Keep the release default `0x1D3D154F`. |
| `downloadDataSize` | Reservation that makes `/download0` available for writable persistent app data. |
| `sources` | C, `.cc`, or `.cpp` files under `src/`. |
| `compileDefinitions` | Optional preprocessor definitions such as `FEATURE_AUDIO=1`. |
| `includePaths` | Optional repository-relative header directories. |
| `staticArchives` | Optional repository-relative `.a` libraries. |
| `runtimeModules` | Generated or optional local PRXs copied into the app; the default pins the clean-room shim hash. |

## Adding code

Add files under `src/`, then list each file explicitly:

```json
"sources": [
  "src/main.c",
  "src/network.c",
  "src/ui.cpp"
]
```

Source paths are explicit so stale or experimental files cannot enter a build
accidentally.

Basic C++20 sources are supported with exceptions and RTTI disabled. The
template does not bundle a C++ standard library. Export platform functions with
`extern "C"` in C++ and add any required static runtime archive explicitly.
C11 is the baseline language mode.

## Adding headers or static libraries

Repository-local include paths and archives are passed through the build:

```json
"includePaths": ["include", "vendor/example/include"],
"staticArchives": ["lib/example.a"]
```

Do not put proprietary Sony headers or libraries in a public repository.

## Persistent configuration

Use `/download0` for configuration, pairing state, caches, and logs. Write a
temporary file and rename it into place to avoid partial configuration writes.
Provide an application-level export/import mechanism for important data because
retention after title deletion or cache-management actions is not guaranteed.

Native SaveData initialization is not part of this baseline; see
[Platform findings](PLATFORM_NOTES.md).

## Runtime modules

The default entry points at the generated `runtime/libc.prx` FSELF and pins its
release SHA-256. `make` creates it automatically when needed. Keep that entry
unchanged for the baseline. A pre-signed module is copied byte-for-byte; an
optional raw module under ignored `.local/runtime/` is wrapped by the build.
Never move an extracted proprietary module into the repository.
