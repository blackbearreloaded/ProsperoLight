# Clean-room runtime shim

`libc.prx` was independently authored by BlackBearReloaded for
`ps5-native-app-boilerplate` and is distributed under GPL-3.0-or-later. It
contains no Sony runtime implementation, proprietary SDK binary, or game file.

The release artifact has SHA-256:

```text
af5dbb1c778135f63daf07f225f84fb948b07034d6d0cd2e393528510f2236b4
```

Verify it from this directory with:

```sh
sha256sum -c libc.prx.sha256
```

The complete source, reproduction procedure, and compatibility scope are in
[`tooling/MinimalLibcBuilder`](../tooling/MinimalLibcBuilder) and
[`docs/RUNTIME_SHIM.md`](../docs/RUNTIME_SHIM.md).
