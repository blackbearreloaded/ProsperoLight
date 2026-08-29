# Architecture

ProsperoLight keeps the application boundary small and explicit:

```text
RmlUi launcher (main.cpp, moonlight_app.cpp)
        |
configuration, discovery, pairing, application control
        |
Moonlight stream coordinator (moonlight_stream.cpp)
   |                 |                 |
VideoDec2 -> AGC     Opus -> AudioOut   DualSense -> Moonlight input
```

`moonlight_stream.cpp` owns the live session and its cleanup. Video access
units are decoded by VideoDec2 into GPU-visible resources and handed to AGC for
presentation. The application does not copy decoded pixels through a CPU frame
buffer on this path.

The launcher and all PS5-specific application integration are C++20, with
application interfaces named `*.hpp`. The proven Moonlight-compatible HTTP,
pairing, and RTSP implementation remains in `src/gamestream` as a C ABI
library; its `*.h` headers and the small PS5 C adapters deliberately retain
C-compatible names. Its upstream dependencies are pinned as submodules and
compiled in their native languages. This is a dependency boundary, not a
second application architecture.

The launcher treats Sunshine availability as a sampled health state rather
than a permanent result. It refreshes the saved host address on a worker
thread every five seconds without repeating mDNS discovery. A failed check
retains the last successful host and application snapshot, displays
`RECONNECTING`, and retries after one second. Three consecutive failures are
required before the launcher displays `OFFLINE`; polling continues so a host
that restarts during an RDP or Windows-session transition recovers without a
manual refresh. Pairing, artwork downloads, and health checks are serialized
because the compact NVHTTP transport has process-global timeout/error state.

The build creates minimal linker-only import stubs from
`vendor/ps5/sdk/stubs/*_link_stub.c` for PS5 system modules that are not in the
bundled SDK stub set. They only describe unresolved imports to the native
linker; the console resolves the actual system modules at run time.

`sce_sys/param.json` provides title identity and `contentVersion`. `tools/build.sh`
stages `ui/main.rml` and replaces `@PROSPEROLIGHT_VERSION@` with that same
version before packaging.
