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
6. Launch and stop a non-Desktop Sunshine application. Repeat a launch after
   returning to the launcher; a failed or stalled session must return with a
   useful error rather than remain black.
7. Test mouse emulation by holding and releasing `Options`, then return to
   controller mode. Confirm both modes still accept input.
8. If the host supports them, test HEVC and HDR separately. Verify image,
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
Hardware validation is pending because FTP, klog, and the loader stopped after
the diagnostic run; the PS5 remained reachable and returned to its home screen.
