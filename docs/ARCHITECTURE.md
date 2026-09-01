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

The AGC presenter selects its VideoOut target at the stream boundary. At
60 FPS, a 1080p stream uses two 1920x1080 buffers, while 1440p and 2160p use two
3840x2160 buffers; 2160p is presented 1:1 and 1440p is filtered by AGC. At 90
or 120 FPS, ProsperoLight retains the same resolution-aware output geometry.
The title advertises the high-resolution 120-Hz capability in `attribute3`,
then selects HFR through ordinary VideoOut request 15: 2160p/120 maps to the
native 3840x2160 119.88-Hz scanout and 1440p HFR is filtered into that 4K
target. This matches the public retail PS5 path and avoids restricted system
or VR output interfaces. The Connecting animation, TV-safe viewport, metrics
HUD, and stream keyboard use the same selected output geometry. Per-stream
teardown unregisters and releases the dynamically sized pool.

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

The launcher owns a small queued-audio sound-effect player backed by SDL's
hardware-validated PS5 AudioOut driver. All cues are 48 kHz stereo signed
16-bit PCM and are loaded from `assets/sfx`. The opening cue plays only on
process startup; returning from a stream does not replay it. Before returning a
stream command, the launcher clears the UI queue and tears down only SDL video.

Stream audio is independent of the launcher cues. Stereo uses two-channel
signed 16-bit AudioOut. The optional 5.1 mode negotiates Moonlight's standard
six-channel Opus layout (`FL FR FC LFE BL BR`) and writes it to PS5's validated
eight-channel AudioOut layout, with `SL` and `SR` zero-filled. If the
eight-channel port cannot be opened before negotiation, the session requests
stereo instead.
The SDL audio device remains open and silent for the process lifetime because
the PS5 SDL backend faults while closing a device that has played queued audio.
Moonlight uses its independent native Opus/AudioOut port during the stream.
