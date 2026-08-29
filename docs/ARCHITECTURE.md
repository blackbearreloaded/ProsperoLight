# Architecture

ProsperoLight keeps the application boundary small and explicit:

```text
RmlUi launcher (main.cpp, moonlight_app.cpp)
        |
configuration, discovery, pairing, application control
        |
Moonlight stream coordinator (moonlight_stream.cpp)
   |                 |                 |
VideoDec2 -> AGC     Opus -> AudioOut   DualSense + USB HID -> Moonlight input
```

`moonlight_stream.cpp` owns the live session and its cleanup. Video access
units are decoded by VideoDec2 into GPU-visible resources and handed to AGC for
presentation. The application does not copy decoded pixels through a CPU frame
buffer on this path.

While a stream is active, the same 4 ms input loop polls `libScePad`,
`libSceKeyboard`, and `libSceMouse`. Physical keyboard USB-HID usages are
translated to the Windows virtual-key values used by Moonlight Qt, while
physical mouse deltas, five buttons, and both wheel axes use Moonlight's native
input packets. Key repeat remains host-driven. Teardown raises every tracked
key and mouse button before the connection closes so returning to the launcher
cannot leave input stuck on the host.

The AGC presenter selects its VideoOut target at the stream boundary. A 1080p
stream uses two 1920x1080 scanout buffers. Both 1440p and 2160p streams acquire
the public 4K-buffer privilege and use two 3840x2160 scanout buffers; 2160p is
therefore presented 1:1, while 1440p is scaled by AGC. The Connecting animation,
TV-safe viewport, metrics HUD, and stream keyboard use the same selected output
geometry so presentation cannot become fixed at the animation's 1080p source
size. Per-stream teardown unregisters and releases the dynamically sized pool.
The recovered zero-copy shader currently uses integer point sampling. Native
2160p removes the former destructive 4K-to-1080p reduction, but 1440p-to-4K
filtered scaling remains a separate shader-quality milestone.

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
