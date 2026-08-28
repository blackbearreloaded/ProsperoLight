# Capability recipes

The root app intentionally proves only the smallest graphical baseline. Add
capabilities one at a time, keep their lifetime explicit, and test the resulting
artifact on every supported firmware. The native linker discovers referenced
platform imports from the public SDK stubs; no firmware offsets belong in app
code.

## Persistent files under `/download0`

Set a positive `downloadDataSize` in `sce_sys/param.json`. Write a complete
temporary file, flush it, and rename it over the stable path so interruption
cannot leave a partially written configuration:

```cpp
const char temporary[] = "/download0/settings.tmp";
const char current[] = "/download0/settings.bin";

int descriptor = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0600);
// Loop on write() until every byte is stored, then fsync() and close().
if (rename(temporary, current) != 0) {
    // Preserve the previous current file and report the failure.
}
```

Use versioned, bounded records and validate every length before loading them.
Treat `/download0` as title-local persistent application data, not as a path to
its host backing image. Provide export/import for state that must survive title
removal or cache management.

## Controller input

The direct sequence validated by the project is:

1. initialize User Service and obtain the initial or foreground user;
2. call `scePadInit()` and `scePadOpen(user, standard_port, 0, nullptr)`;
3. once per frame, drain `scePadRead(handle, records, 64)` and use the newest
   timestamped record;
4. close the pad during an orderly application shutdown.

Use `scePadReadState()` only when the latest cached state is sufficient. Do not
mix it with batch reads in the same frame, because both enter the same read
path. Platform pad structures are ABI declarations, so take them from a
reviewed public header or use SDL instead of recreating layouts from memory.

## Networking and TLS

For sockets, include the public SDK's standard networking headers and follow the
usual bounded BSD-socket lifecycle: create, set timeouts, connect, loop on
partial send/receive, and close on every error path. Keep network work off the
render/input thread.

For production HTTPS or TLS, use the maintained OpenSSL port rather than a
custom protocol implementation:

```bash
make PACBREW_PACKAGES=openssl
```

Resolve hostnames and certificate failures explicitly. Never disable peer or
hostname verification in a distributed application.

## AudioOut

The hardware-proven baseline initializes AudioOut, obtains the active user, and
opens the main output at 48 kHz. Feed complete interleaved PCM grains and let
`sceAudioOutOutput()` provide pacing. A 256-frame grain is a practical starting
point. Keep decode/resample work outside the submission loop, bound the queue,
and output silence on underrun rather than reusing stale samples.

Treat the AudioOut handle as an RAII resource: finish or cancel pending output,
close the handle, and release buffers in reverse initialization order.

## SDL

SDL is the shortest route to a portable CPU-rendered app with events and audio.
Fetch the pinned PacBrew sysroot and select its module only for builds that use
it:

```bash
make pacbrew
make PACBREW_PACKAGES=sdl2
```

Check every SDL initialization stage separately—memory, video driver, events,
renderer, and audio—so a loader/import failure is distinguishable from an SDL
subsystem failure. Keep the root boilerplate independent of SDL so developers
who need only VideoOut do not inherit the dependency.

## Adding a native library

Prefer a PacBrew module when available:

```bash
make pacbrew-list
make PACBREW_PACKAGES="sdl2 openssl"
```

For a project-owned archive, put headers and `.a` files in repository-relative
directories and declare them explicitly:

```bash
make APP_INCLUDE_PATHS=vendor/include APP_STATIC_ARCHIVES=vendor/lib/libexample.a
```

Paths cannot contain spaces. Review every dependency's license and PS5 build
profile before distributing the result.
