# PacBrew dependencies

The build can consume prebuilt PS5 libraries from
[ps5-payload-dev/pacbrew-repo](https://github.com/ps5-payload-dev/pacbrew-repo)
without installing Pacman, using `sudo`, or rebuilding the ports collection.

## Select a library

List the available `pkg-config` module names:

```bash
make pacbrew-list
```

The first request downloads PacBrew's complete prebuilt ports image because
upstream publishes one sysroot rather than separate per-library archives. The
approximately 346 MB download is cached under `.deps/pacbrew/`.

Add one or more names from that list to `project.json`:

```json
"pacbrewPackages": ["sdl2", "SDL2_image", "openssl"]
```

Then build normally:

```bash
make
```

The bootstrapper verifies the pinned release, keeps it separate from the
project's newer SDK, and asks `pkg-config --static` for include paths, archives,
and transitive dependencies. Those flags are passed to the existing Clang and
LLD pipeline. Package names are case-sensitive `pkg-config` modules, not
necessarily PacBrew repository directory names.

## Libraries without `pkg-config`

Some ports install headers and static archives without a `.pc` file. Select
their paths relative to PacBrew's `/user/homebrew` prefix:

```json
"pacbrewPackages": [],
"pacbrewIncludePaths": ["include"],
"pacbrewStaticArchives": ["lib/libsqlite3.a"]
```

List multiple archives in linker order when a manually selected library has
transitive dependencies. Prefer `pacbrewPackages` whenever a module exists;
automatic static dependency ordering is safer.

## Reproducibility and scope

The integration pins PacBrew `v0.40.2` and verifies
`ps5-payload-dev.tar.gz` with SHA-256
`a85f65de418a8e6a898c6c3e3c870d50fff7618a200e4dd59ea9692af6ecec4d`.
Only its `target/user/homebrew` ports prefix is extracted into the ignored
cache. It never overwrites the pinned SDK under `.deps/native/` and never
changes the host system.

PacBrew integration establishes build-time header and static-library access;
it cannot guarantee that every upstream port is compatible with this
application container or the clean-room runtime shim. A library may require
additional system-module imports, runtime assets, or libc APIs. Test the exact
dependency set on each target firmware before distribution, and comply with
the licenses of every statically linked library.
