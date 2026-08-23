# Build output formats

Every build creates and validates `dist/<TITLE_ID>/`. Select the additional
package output with `-OutputFormat`:

| Selection | Additional output | Packaging tool |
| --- | --- | --- |
| `Folder` | None | None |
| `Ffpkg` | `dist/<TITLE_ID>.ffpkg` | UFS2Tool |
| `Ffpfsc` | `dist/<TITLE_ID>.ffpfsc` | MkPFS |
| `All` | Both images | Both tools |

```powershell
./build.ps1 -OutputFormat Folder
./build.ps1 -OutputFormat Ffpkg
./build.ps1 -OutputFormat Ffpfsc
./build.ps1 -OutputFormat All
```

The Hello World wrapper accepts the same option:

```powershell
./examples/hello-world/build.ps1 -OutputFormat Ffpfsc
```

`-Ffpkg` remains accepted as a compatibility alias for
`-OutputFormat Ffpkg`.

## Compressed FFPFSC

MkPFS creates the console-compatible, exFAT-wrapped compressed form directly
from the validated app folder:

```text
python -m mkpfs pack folder --no-adjust-output-file-extension \
  --version PS5 --verify \
  <app-directory> <title.ffpfsc>
```

On first use, `tools/setup-mkpfs-tooling.ps1` fetches the pinned
[PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) revision into the ignored
`.deps/MkPFS` cache. It creates `.deps/MkPFS/.venv` and installs the tool there;
the repository does not distribute MkPFS source or binaries. Python 3.9 or
newer is required. Pass a nonstandard interpreter with
`-Python C:\path\to\python.exe`.

The build uses MkPFS's default wrapped-folder mode because upstream documents
it as the maximum-compatibility `.ffpfsc` layout. It does not use the advanced
direct raw-PFS mode.

## UFS2 FFPKG

The `.ffpkg` option creates and checks an uncompressed UFS2 filesystem image:

```text
newfs -O 2 -b 32768 -f 4096 -D <app-directory> <title.ffpkg>
```

On first use, `tools/setup-ffpkg-tooling.ps1` fetches the pinned
[SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) revision into the
ignored `.deps/UFS2Tool` cache and builds its command-line assembly. The
profile follows the public procedure documented by
[sinajet/PSFFPKG](https://github.com/sinajet/PSFFPKG).

Despite the similar names, `.ffpkg` here is a mountable filesystem image. This
project does not create a signed retail PKG/FPKG container.

Package files from older builds are not automatically deleted when a different
format is selected. Rebuild the exact format immediately before deployment so
an old image is not mistaken for the current app.
