# Clean-room runtime shim

`libc.prx` was independently authored by BlackBearReloaded for
`ps5-native-app-boilerplate` and is distributed under GPL-3.0-or-later. It
contains no Sony runtime implementation, proprietary SDK binary, or game file.

The release artifact has SHA-256:

```text
e2292d285565937f1dac09ef5ab742b6027c28d38ba775ad56465aa5594e2a10
```

Verify it from this directory with:

```sh
sha256sum -c libc.prx.sha256
```

The complete source, reproduction procedure, and compatibility scope are in
[`tooling/ConventionalLibcBuilder`](../tooling/ConventionalLibcBuilder) and
[`docs/RUNTIME_SHIM.md`](../docs/RUNTIME_SHIM.md).
