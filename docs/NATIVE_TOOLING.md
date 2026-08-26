# Native build tooling

The normal build has no C# or .NET dependency. Repository-owned host tools are
C++20 and compile natively on Linux/WSL or through WSL from PowerShell.

## Division of responsibility

LLVM/Clang and lld handle the standard work that mature native tools already
solve: C/C++ compilation, archives, COMDAT, symbol resolution, TLS, unwind
records, and x86-64 relocations. The repository-owned converter handles only
PS5-specific output requirements:

- FreeBSD OS ABI 9, ABI version 2, and executable type `0xFE10`;
- execute-only, read-only, RELRO, writable, and flags-zero linking segments;
- process-parameter and parameter-block records;
- SDK import discovery from the installed public `.so` stubs;
- PS5 NIDs, module/library IDs, SysV hash, and dynamic tags;
- development FSELF wrapping and integrity metadata.

The converter reads imports directly from the PIE and the SDK stubs. It does
not contain a copied import catalog or firmware offsets.

Native third-party libraries are allowed when they solve a standard problem
and the build can acquire and link them reproducibly. zlib is used directly by
the FSELF tool; LLVM/lld handles standard object linking. This boundary keeps
the repository free of managed build tooling without reimplementing mature
native libraries such as zlib or OpenSSL.

## Files

| File | Purpose |
| --- | --- |
| `tooling/native/app_crt.c` | Native process startup and constructor handling |
| `tooling/native/ps5-pie.ld` | Non-overlapping intermediate PIE layout |
| `tooling/native/elf_object.*` | ELF and SDK-stub reader |
| `tooling/native/sce_module_writer.*` | PS5 executable converter |
| `tooling/native/self_container.*` | FSELF reader, writer, and verifier |
| `tooling/native/libc_builder.cpp` | Deterministic clean-room runtime emitter |
| `tooling/native/hash.hpp` | Project-owned SHA-1/SHA-256 implementation |

## Build the host utility manually

The normal build invokes the dependency bootstrap automatically. For a manual
PowerShell build, use the returned WSL paths rather than a global library:

```powershell
$deps = ./tools/setup-native-dependencies.ps1 | ConvertFrom-Json
wsl.exe --exec clang++ -std=c++20 -O2 -Wall -Wextra -Werror `
  -I $deps.zlibInclude `
  tooling/native/native_app_builder.cpp `
  tooling/native/self_container.cpp `
  tooling/native/elf_object.cpp `
  tooling/native/sce_module_writer.cpp `
  $deps.zlibArchive -o build/ps5-native-tool
```

zlib is the only directly linked host library.

## Source quality

The repository uses the same focused Clang policy as the CPython PS5 project:
`make format-check` enforces `.clang-format`, while `make tidy` runs the Clang
core and security analyzers configured by `.clang-tidy`. `make lint` runs both
plus attribution, JSON, shell-syntax, local-path, and whitespace checks.

## Commands

```text
ps5-native-tool link --in <llvm-pie> --out <ps5-elf> --stub-dir <sdk-lib>
ps5-native-tool self --sign --in <ps5-elf> --out <fself>
ps5-native-tool self --extract --file <fself> --out <ps5-elf>
ps5-native-tool self --inspect --file <module>
```

`link` expects the repository linker script’s page-separated PIE. The normal
build invokes it correctly; the command is documented for debugging and CI.

## Hardware validation

The C++ pipeline's signed graphical Hello World output was tested unchanged on
firmware 6.02 and 12.70. It entered `eboot`, rendered the CPU VideoOut scene,
loaded `/app0/assets/banner.txt`, remained stable for observation, and closed
normally. The exact validation artifact had:

```text
Raw ELF SHA-256:  49a0d657708d633ce32233361b9d147d16387e63e9e67f2a09800684fa4ead67
FSELF SHA-256:    ff5135cd279b2c58f53e1c0c86671edcba97a4434e802872fa5c2347929ec3db
Runtime SHA-256:  e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036
```

The loader-visible comment record and the unmapped trailing note intentionally
have zero memory size. Firmware 6.02 rejects those records before entry when
their file size is incorrectly copied into `p_memsz`; the static validator now
enforces the tested convention.
