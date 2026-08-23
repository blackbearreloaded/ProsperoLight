# Clean-room runtime shim

`libc.prx` was independently authored by BlackBearReloaded for
`ps5-native-app-boilerplate` and is distributed under GPL-3.0-or-later. It
contains no Sony runtime implementation, proprietary SDK binary, or game file.

The release artifact has SHA-256:

```text
cd961ee6ed3d08117459b0fe70d86fe322672ebe0103678ee7c3f15af7e00504
```

Verify it from this directory with:

```sh
sha256sum -c libc.prx.sha256
```

The complete source, reproduction procedure, and compatibility scope are in
[`tooling/MinimalLibcBuilder`](../tooling/MinimalLibcBuilder) and
[`docs/RUNTIME_SHIM.md`](../docs/RUNTIME_SHIM.md).
