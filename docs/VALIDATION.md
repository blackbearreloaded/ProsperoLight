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
2. Pair with Sunshine and restart the app once to confirm pairing persistence.
3. Launch Desktop at default H.264 1080p. Check video, stereo audio, and
   DualSense input.
4. Toggle the metrics overlay with `Select + R1`, then return using
   `Select + L1`. Confirm no debug text is left in the launcher.
5. Launch and stop a non-Desktop Sunshine application. Repeat a launch after
   returning to the launcher; a failed or stalled session must return with a
   useful error rather than remain black.
6. Test mouse emulation by holding and releasing `Options`, then return to
   controller mode. Confirm both modes still accept input.
7. If the host supports them, test HEVC and HDR separately. Verify image,
   audio, input, cleanup, and recovery; do not treat HDR audio-only output as a
   pass.

For release readiness, add a longer 1080p60 gameplay soak, network interruption
recovery, repeated launch/stop cycles, and a check that no decoded-video CPU
copy or frame backlog appears in the metrics.
