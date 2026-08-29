# Testing ProsperoLight

Run host checks from WSL:

```sh
make test
make lint
make app
```

`make test` contains focused GoogleTest coverage for stream input shortcuts,
configuration defaults, host-list updates, health failure debouncing and
recovery, and failure fallback. Python
regressions cover identity and safe deploy path resolution. These tests do not
require a console or a Sunshine host.

`make lint` checks formatting, static analysis, shell syntax, PS5 metadata,
assets, and accidental local-path leaks. `make check` is the combined host
gate.

Hardware validation is intentionally separate. Use the sequence in
[VALIDATION.md](VALIDATION.md), acquire the shared PS5 lock only for a console
test, and release it immediately when the test ends. Never change console
settings as part of the test process.
