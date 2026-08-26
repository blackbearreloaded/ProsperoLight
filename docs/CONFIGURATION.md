# Project configuration

[`project.json`](../project.json) is the only normal build configuration file.

The repository root is the application skeleton. Fork the project, edit this
file and `src/main.cpp`, and list additional source files explicitly below.

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
  "src/main.cpp",
  "src/network.cpp",
  "src/ui.cpp"
]
```

Source paths are explicit so stale or experimental files cannot enter a build
accidentally.

C++20 is the application baseline, with exceptions and RTTI disabled. The
public SDK supplies libc++ headers; the template exposes allocation-free
facilities and supplies project-owned `new`/`delete` operators for
`std::unique_ptr`. It does not link the complete libc++ or libc++abi archives.

Prefer stack values, RAII, `std::array`, `std::span`, `std::string_view`, and
`std::unique_ptr`. Treat raw pointers as non-owning. Throwing allocation traps
on exhaustion because exceptions are disabled; use a temporary
`std::nothrow_t{}` when allocation failure must be handled locally. C11 sources
remain supported for C libraries and narrow platform ABI code. Declare C and
platform entry points with `extern "C"` from C++.

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
