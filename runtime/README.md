# Clean-room runtime shim

`libc.prx` was independently authored by BlackBearReloaded for
`ps5-native-app-boilerplate` and is distributed under GPL-3.0-or-later. It
contains no Sony runtime implementation, proprietary SDK binary, or game file.

The release artifact has SHA-256:

```text
e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036
```

Verify it from this directory with:

```sh
sha256sum -c libc.prx.sha256
```

The complete source, reproduction procedure, and compatibility scope are in
[`tooling/LibcBuilder`](../tooling/LibcBuilder) and
[`docs/RUNTIME_SHIM.md`](../docs/RUNTIME_SHIM.md).
