# The app folder and the release ZIP

Every build creates and validates `dist/<TITLE_ID>/`, the complete title
folder. It is the only build output:

```bash
make app
```

On Windows, `./build.ps1` runs the same build through WSL.

Tagged GitHub Releases and every CI build attach a `.zip` containing that
folder and its `SHA256SUMS`, and nothing else. To install a release, extract
the ZIP and upload its `<TITLE_ID>/` folder to `/data/homebrew`; see
[Deployment](DEPLOYMENT.md).

No filesystem image is built. The `.ffpkg` (UFS2) image, the raw `.exfat` image
and the compressed `.ffpfsc` image of earlier versions are gone, with
`make ffpkg`, `make exfat`, `make ffpfsc`, `make packages` and the
`DEPLOY_FORMAT` variable. If an old `<TITLE_ID>.ffpkg`, `<TITLE_ID>.exfat` or
`<TITLE_ID>.ffpfsc` is still in a directory the loader scans, delete it before
installing the folder: a folder and an image with the same title ID must not
both be in the loader's scan paths.

This project does not create a signed retail PKG/FPKG container.
