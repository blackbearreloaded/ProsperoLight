# Build output formats

Every application or package build creates and validates
`dist/<TITLE_ID>/`. Releases and every CI build carry that folder as a ZIP
with its `SHA256SUMS`, and nothing else; the images below are local options.
The Make targets map to the same PowerShell `-OutputFormat` selections where
one exists:

| Make target / selection | Additional output | Packaging tool |
| --- | --- | --- |
| `make app` / `Folder` | None | None |
| `make ffpkg` / `Ffpkg` | `dist/<TITLE_ID>.ffpkg` | UFS2Tool |
| `make exfat` | `dist/<TITLE_ID>.exfat` | MkPFS (its exFAT writer) |
| `make packages` / `All` | Both images | Both tools |

```bash
make app
make ffpkg
make exfat
make packages
```

`-Ffpkg` remains accepted as a compatibility alias for
`-OutputFormat Ffpkg` in the Windows PowerShell frontend.

## Raw exFAT image

`tools/pack-exfat.py` writes the validated app folder into a raw exFAT image
and reads every file back. On first use, `tools/setup-packaging-dependencies.sh`
fetches the pinned [PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) revision,
whose exFAT writer the packer uses, into the ignored `.deps/MkPFS` cache and
installs its dependencies under that ignored checkout (`.venv-linux`). The
repository does not distribute MkPFS source or binaries. Python 3.9 or newer
with `venv` support is required.

## UFS2 FFPKG

The `.ffpkg` option creates and checks an uncompressed UFS2 filesystem image:

```text
ufs2tool makefs -S 4096 -b 20% -t ffs \
  -o version=2,bsize=32768,fsize=4096,minfree=0,softupdates=0,optimization=space \
  <title.ffpkg> <app-directory>
```

On first use, `tools/setup-packaging-dependencies.sh` or the equivalent
PowerShell bootstrapper fetches
[SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) at commit
`b5307a60d5b4e3a68ba680e0e33cfadf05017c77`, builds its CLI with the .NET SDK
8 or newer, and caches it under ignored `.deps/UFS2Tool/`. The repository does
not distribute UFS2Tool source or binaries. The build reserves allocation
slack and verifies the resulting UFS2 superblock magic.

The same UFS2Tool-generated image was mounted through ShadowMountPlus and
launched successfully on PS5 system software 6.02 and 12.70.

Despite the similar names, `.ffpkg` here is a mountable filesystem image. This
project does not create a signed retail PKG/FPKG container.

Package files from older builds are not automatically deleted when a different
format is selected. Rebuild the exact format immediately before deployment so
an old image is not mistaken for the current app.

The compressed `.ffpfsc` image of earlier versions is no longer built, and
`make ffpfsc` is gone with it. If an old `<TITLE_ID>.ffpfsc` is still in
`/data/homebrew`, delete it before installing the folder: a folder and an
image with the same title ID must not both be in the loader's scan paths.
