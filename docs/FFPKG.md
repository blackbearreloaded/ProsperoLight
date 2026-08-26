# Build output formats

Every application or package build creates and validates
`dist/<TITLE_ID>/`. The Make targets map to the same PowerShell
`-OutputFormat` selections:

| Make target / selection | Additional output | Packaging tool |
| --- | --- | --- |
| `make app` / `Folder` | None | None |
| `make ffpkg` / `Ffpkg` | `dist/<TITLE_ID>.ffpkg` | Native `makefs` |
| `make ffpfsc` / `Ffpfsc` | `dist/<TITLE_ID>.ffpfsc` | MkPFS |
| `make packages` / `All` | Both images | Both tools |

```bash
make app
make ffpkg
make ffpfsc
make packages
```

`-Ffpkg` remains accepted as a compatibility alias for
`-OutputFormat Ffpkg` in the Windows PowerShell frontend.

## Compressed FFPFSC

MkPFS creates the console-compatible, exFAT-wrapped compressed form directly
from the validated app folder:

```text
python -m mkpfs pack folder --no-adjust-output-file-extension \
  --version PS5 --verify \
  <app-directory> <title.ffpfsc>
```

On first use, `tools/setup-packaging-dependencies.sh` or the equivalent
PowerShell bootstrapper fetches the pinned
[PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) revision into the ignored
`.deps/MkPFS` cache and installs its dependencies under that ignored checkout;
the repository does not distribute MkPFS source or binaries. Python 3.9 or
newer is required.

The build uses MkPFS's default wrapped-folder mode because upstream documents
it as the maximum-compatibility `.ffpfsc` layout. It does not use the advanced
direct raw-PFS mode.

## UFS2 FFPKG

The `.ffpkg` option creates and checks an uncompressed UFS2 filesystem image:

```text
makefs -S 4096 -b 20% -t ffs \
  -o version=2,bsize=32768,fsize=4096,minfree=0,optimization=space \
  <title.ffpkg> <app-directory>
```

On first use, `tools/setup-packaging-dependencies.sh` or the equivalent
PowerShell bootstrapper downloads the native Ubuntu or Debian `makefs` package
into `.deps/makefs` and extracts it without root. The
build reserves allocation slack and verifies the UFS2 superblock magic. The
profile follows the public procedures documented by
[SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) and
[sinajet/PSFFPKG](https://github.com/sinajet/PSFFPKG); neither managed tool is
executed or fetched.

Despite the similar names, `.ffpkg` here is a mountable filesystem image. This
project does not create a signed retail PKG/FPKG container.

Package files from older builds are not automatically deleted when a different
format is selected. Rebuild the exact format immediately before deployment so
an old image is not mistaken for the current app.
