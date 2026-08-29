# Port validation

## Host gate

Before deploying, run:

```sh
make check
make ffpfsc
```

Confirm the package uses the expected `titleId`, `contentVersion`, and
ProsperoLight title from `sce_sys/param.json`.

## PS5 smoke test

Before touching the console, check the shared PS5 workspace `lock.txt`. If it
exists, wait and re-check every 15 seconds. If it is absent, create it for the
duration of this test; delete it immediately afterwards. Do not change PS5
settings.

1. Install the fresh title and open the launcher. Confirm the version in the
   top bar matches `contentVersion`.
   A saved Sunshine host must be reported online when its `/serverinfo`
   endpoint is reachable; an offline result fails this gate.
2. Move focus across every item on all four launcher screens, including the
   host card and action buttons. This exercises lazy texture loading; the title
   must remain alive and the detected host state must not regress.
3. Pair with Sunshine and restart the app once to confirm pairing persistence.
4. Launch Desktop at default H.264 1080p. Check video, stereo audio, and
   DualSense input.
5. Toggle the metrics overlay with `Select + R1`, then return using
   `Select + L1`. Confirm no debug text is left in the launcher.
6. At Windows sign-in, press `Select + Triangle` and confirm ProsperoLight's
   stream keyboard appears without a PS5 notification. Use the D-pad and Cross
   to enter private text, Triangle for Shift, Square for Backspace, and Options
   for Enter. Confirm the password is never present in ProsperoLight
   diagnostics or storage. Toggle the keyboard off again.
7. Launch and stop a non-Desktop Sunshine application. Repeat a launch after
   returning to the launcher; a failed or stalled session must return with a
   useful error rather than remain black.
8. Test mouse emulation using `Select + Square`, then return to controller
   mode with the same shortcut. Confirm both modes still accept input.
9. If the host supports them, test HEVC and HDR separately. Verify image,
   audio, input, cleanup, and recovery; do not treat HDR audio-only output as a
   pass.

For release readiness, add a longer 1080p60 gameplay soak, network interruption
recovery, repeated launch/stop cycles, and a check that no decoded-video CPU
copy or frame backlog appears in the metrics.

## Latest hardware evidence

On 2026-08-28, `PPSA77003` content version `01.000.011` passed a clean PS5
launch/close cycle after the C++ runtime migration. The console reported the
restored HDR-capable title profile as `HDR:o`, and the launcher reached the
saved Sunshine host `Gaming-PC` at `192.168.4.20`, changing it from the
regressed offline state to `ONLINE / NOT PAIRED`. The fix routes network-handle
nonblocking configuration through the `libSceNet` `ioctl(FIONBIO)` adapter
instead of libc `fcntl()`.

The tested `eboot.bin` SHA-256 is
`539c0a0cd7bc16c41f40aee6c24bbc58416d4d5597c45e69c61e5d70d4361a36`.
No application fatal signal appeared in the bounded kernel log, and FTP, klog,
and loader services remained reachable after close. Pairing and physical-pad
focus traversal remain explicit manual gates for this fresh application data.

Content version `01.000.013` was also exercised against Sunshine state
`SUNSHINE_SERVER_BUSY` with active app ID `881448767`. The launcher remained
online, labeled the host `BUSY`, disabled pairing before opening the PIN modal,
and displayed the unpaired-client recovery instruction without clipping. Its
tested `eboot.bin` SHA-256 is
`aaa47276ba5844e6aee625ed7def176149631a92ea950ec3e82eba8d80eaf7f9`.

Content version `01.000.014` moves controller initialization and the
`Select + L1` escape monitor ahead of decoder allocation and keeps the monitor
alive through the Moonlight connection handshake. Startup is bounded to 20
seconds and the first video frame to 10 seconds; either failure closes the
Sunshine session, reports the reason, and returns to the launcher. Host tests
(`make test` and `make lint`) pass. The folder build was deployed, but its PS5
launch/recovery gate remains pending because the console's loader, log, and
file-service ports became unavailable before the test could be completed.

Content version `01.000.017` isolated the later connection freeze precisely.
The encrypted RTSP OPTIONS, DESCRIBE, and audio SETUP exchanges completed,
Sunshine supplied audio port `48000`, the PS5 UDP socket bound successfully,
and `pthread_create()` returned success. The new audio-ping thread then entered
the common-c wrapper but stopped inside `pthread_setname_np()` before invoking
its entry point. This explains why the connecting animation eventually ended
in a process failure and why video SETUP never began.

Content version `01.000.018` makes thread naming a PS5-only no-op in the forced
compatibility header, leaving thread creation and every stream worker entry
unchanged. The temporary common-c tracing was removed and the submodule was
restored to its fetchable upstream commit. Nine integration tests, eight
GoogleTests, the allocation-runtime test, lint, and the native app build pass.
The candidate `eboot.bin` SHA-256 is
`1d1e7bd949aeec4598b33f1640ca33d0932c6fc4a79e7ec3bbd6bcccd91d8eb1`.
The deployed build then completed all 11 Moonlight connection stages, started
H.264 video, stereo audio, and input, and presented at least 3,600 decoded
frames with `pending=0`. ShadowMount recorded a normal title stop with no new
crash candidate, and FTP, klog, and loader services remained reachable after
cleanup.

Content version `01.000.021` adds password-safe native PS5 text entry during a
stream. The frozen candidate at commit `a608b67` launched, rendered the paired
launcher, resumed the H.264 Desktop stream, and opened PS5 common-dialog title
`NPXS40093` when the targeted `Select + Triangle` chord was sent. Cancelling
the dialog sent no text; the existing `Select + L1` shortcut then returned to
the launcher, and the title closed normally. FTP, klog, and loader services
remained reachable after teardown. The tested `eboot.bin` SHA-256 is
`a7c0c7bb03d605c738d1db531b4923109e555fccc247bc144fc65bc8f09881b7`.
This is a `partial-pass`: native password-dialog launch, cancellation, and
stream recovery are hardware-proven, while text-plus-Enter delivery and an
actual private Windows sign-in remain the operator acceptance step.

Content version `01.000.022` replaced the native dialog with the documented
Windows `Win + Ctrl + O` shortcut. The PS5 notification proved that the
`Select + Triangle` handler ran, but the Windows On-Screen Keyboard did not
appear at the secure sign-in screen. This is a failed acceptance result for
that approach; Windows documents opening its Accessibility menu at sign-in
rather than relying on the desktop OSK shortcut.

Content version `01.000.023` therefore rendered a password-safe keyboard inside
the existing AGC stream overlay and sends each selected key through Moonlight's
ordinary keyboard event path. Host tests cover layout navigation, shifted
labels, and all stream shortcut chords. The frozen candidate `eboot.bin`
SHA-256 is
`d6d32540483262394695fffa0c302ef5192b90fa10d2066be9abe6c4cb717225`.
On PS5 the overlay opened without interrupting video and D-pad navigation moved
its selection. The operator subsequently confirmed that private Windows sign-in
typing worked perfectly. This is a `pass` for the input path.

Content version `01.000.024` moves the keyboard into the upper-center safe
region and enlarges its key labels. It also removes the keyboard toggle
notification, raises panel opacity to 82%, and adds the previously missing
backtick/tilde key. A host regression test proves that the layout contains all
printable US-ASCII characters. The frozen candidate `eboot.bin` SHA-256 is
`f5a7e8bf655141dfa53dae455bd1c2d11ba07339485d9fe32d8586e98933baf2`.
The folder candidate was deployed to `PPSA77003`; the launch request was
accepted and both payload and FTP services remained healthy after cleanup.
This Windows session could not capture Chiaki's render window, so the 82%
opacity and revised placement still require direct TV/Chiaki visual acceptance.

Content version `01.000.025` disables the optional blocking TCP telemetry sink
in normal builds. A `01.000.024` reproduction entered a black frame before the
Connecting renderer, produced no first-stage receipt, remained alive until an
explicit title-aware close, and left FTP, klog, and loader services healthy.
The first stream-stage action was a synchronous port-8767 telemetry connection
with no deadline. The production build now compiles that path to an immediate
no-op; `LAN_TELEMETRY=1` is reserved for bounded diagnostics with a receiver
already running.

The frozen `f3ad73f` candidate passed the corresponding hardware gate with no
listener on port `8767`. It started and presented H.264 1080p Desktop twice in
one process, returned through `Select + L1`, then survived a complete title
close/reopen and resumed the Sunshine-reported running Desktop session. Sunshine
recorded a client connection, disconnection, and completed encoder teardown for
each observed session. The title processes stopped normally and FTP, klog, and
loader services remained reachable. The tested `eboot.bin` SHA-256 is
`1a184f009fb138579644e3195ab5e2a7958f3c4db10306f50e9c7906b37b9fcc`.

The same run selected Steam after returning from Desktop. ProsperoLight returned
to the launcher instead of hanging; Sunshine identified the independent host
failure as `steam://open/bigpicture` being denied by the Windows service
account. Fixing that Sunshine application command remains a host-configuration
task, not a client lifecycle failure.

Commit `048d334` adds automatic Sunshine health recovery. The launcher checks
the saved address asynchronously every five seconds, retries transient failures
after one second, and requires three consecutive failures before displaying
`OFFLINE`. The policy has a focused host test and the complete `make check`
gate passed. The folder candidate was deployed to `PPSA77003`; its `eboot.bin`
SHA-256 is
`d0cd9430b421442008c404524f93cdf0946da71ddf45d009e8a21bbde2d0e2b9`.
It entered `eboot`, ran for the bounded ten-second observation without a loader
error or fatal signal, closed through the title-aware controller, and left FTP,
klog, and elfldr reachable. This is a `partial-pass`: launcher lifecycle and
cleanup passed, while an RDP/Windows-session transition must still verify the
visible `RECONNECTING` to `ONLINE` recovery behavior.

Commit `9b0e020` preserves the last complete Sunshine application catalogue
during stop-session teardown. The stop request now refreshes into a temporary
snapshot and only replaces the visible catalogue when the refresh succeeds or
returns application entries. A transient post-stop application-list failure
therefore shows `RECONNECTING` while retaining the existing game cards instead
of collapsing the Games page to an empty state. The complete `make check` gate
passed with 14 GoogleTests and 14 Python integration tests. The folder candidate
was deployed to `PPSA77003`; its `eboot.bin` SHA-256 is
`0f042240680ad4dba333fa1b8eeb93136c936b747690ba3af7507bb7f245f8ef`.
The title entered its native executable, remained alive during the bounded
observation, closed normally, and left FTP, klog, and elfldr reachable. This is
a `partial-pass`: startup and cleanup are hardware-proven, while the original
launch, return, then stop interaction remains the operator acceptance step.

Commit `9f981f7` coalesces each PS5 pad batch to its newest timestamped sample
before forwarding controller state to Sunshine. This matches the project's
validated native-input recipe and Moonlight Qt's practice of batching pending
axis motion before sending the resulting current state. It prevents stale pad
states from being replayed in a tight burst after a delayed poll. The same
candidate changes the HUD text and RTT refresh cadence from roughly four times
per second to roughly once per second, matching Moonlight Qt while retaining
per-frame counters and composition. The complete `make check` gate passed with
14 GoogleTests and 15 Python integration tests. The deployed `PPSA77003`
`eboot.bin` SHA-256 is
`3fcb7763cd7e59e4f3f9baebe7407d981c78526017ca2b61bb00fea4a606a278`.
The title entered its native executable, survived the bounded observation,
closed normally, and left FTP, klog, and elfldr reachable. This is a
`partial-pass`: lifecycle is hardware-proven; in-game D-pad navigation remains
the operator acceptance step.

The HDR launch-handshake candidate adds Moonlight Qt's standard HDR capability
parameters to both launch and resume requests whenever the selected stream
format is 10-bit. SDR requests continue to omit the HDR block, while every
request carries the selected width, height, frame rate, and SOPS flag so
Sunshine can apply its automatic display configuration. The complete
`make check` gate passed with 14 GoogleTests and 16 Python integration tests.
The folder candidate `eboot.bin` SHA-256 is
`08e30eddd7fa48f081d524133384a509e3fd8cba4d2d7113f540cd4e96328d3f`.
Windows exposes an HDR toggle for the selected VDD, so the remaining acceptance
step is to confirm that an HDR stream makes Sunshine enable it and negotiate
HEVC Main10/Rec.2020 on PS5.

The first `01.000.025` HDR reproduction completed that host-side acceptance
step. Sunshine's session log recorded a 1920x1080 capture in
`DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020`, 10 bits per color, and an
`hevc_nvenc` stream identified as HDR (Rec. 2020 + SMPTE 2084 PQ) and 10-bit.
The client disconnected roughly three seconds later. ProsperoLight was
terminating the stream when the first decoded frame arrived before
moonlight-common-c's asynchronous HDR control packet had updated `hdrActive`;
the pinned dependency explicitly permits that state to be stale for several
frames during transitions.

Commit `e0f52eb` changes the renderer gate to tolerate an unknown initial HDR
state. It still rejects a stream after the control channel explicitly confirms
HDR is disabled, or when decoded frames are not Rec.2020. A regression test
covers the delayed-confirmation case, and the complete `make check` gate passed
with 14 GoogleTests and 17 Python integration tests. Content version
`01.000.026` (commit `e33961c`) was deployed in folder form to `PPSA77003`; its
`eboot.bin` SHA-256 is
`d85061697b2df4f16ce6e338f90148d100b25c5a62ce4a4f5dc0ae5f8b61436e`.
This is a `partial-pass`: Sunshine/VDD HDR negotiation and the PS5 deployment
are proven; visible video and the HDR metrics overlay await operator acceptance.
