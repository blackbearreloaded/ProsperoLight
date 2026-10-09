# Measuring frame pacing on PS5

Enable **Diagnostic logs**, select the desired frame pacing mode and stop the
stream normally after 60–90 seconds. Compare the same codec, scene, resolution,
FPS, VSync setting and network conditions. Repeat each mode rather than using
one run as proof of improvement.

After stopping, download these files through FTP from
`/data/prosperolight/logs` with filesystem elevation, or from the mounted
app's `download0` directory in sandbox fallback mode:

- H.264 / HEVC: `performance-frames-modeN.csv` and `performance-last.json`.
- PyroWave: `pyrowave-output-modeN.csv` and `prosperolight-pyrowave.log`.

`N` is 0 (Unpaced), 1 (Paced), or 2 (Paced+VRR). Each new stream replaces
its previous trace for that backend and mode, so all three modes can be tested
before collecting files. H.264 and HEVC share native traces: collect one codec
before testing the other. There are at most three native and three PyroWave traces.
Recording stops at 32,768 samples (about 273 seconds at 120 FPS); the header
reports omitted samples. Capture uses bounded memory and does not write files
in the streaming loop. Turning logs off prevents export. The previous native
reports inside `moonlight/` are legacy files; new reports are outside that
restricted pairing directory. Pairing credentials are never included.

Keep copies with names such as `hevc-paced.csv`, `hevc-vrr.csv` and
`hevc-unpaced.csv`. Generate a standalone HTML graph and JSON statistics:

```sh
python3 tools/plot-frame-pacing.py hevc-unpaced.csv hevc-paced.csv hevc-vrr.csv \
    --fps 120 --output hevc-pacing.html
```

The green line is the requested frame period: 8.33 ms at 120 FPS, 16.67 ms at
60 FPS. Look for fewer long intervals followed by short intervals, lower p95,
p99 and RMS interval error, without more dropped frames or additional latency.
The summary excludes the first three seconds where the trace is long enough.
Plots preserve interval minima/maxima when reducing samples for display.
Counts above 1.5 periods and below 0.5 periods are indicators, not a universal
pass/fail threshold. Fixed-refresh output can legitimately alternate intervals
when the source FPS is not divisible into the physical refresh rate.

## What these clocks mean

Native submit is a CPU marker after the presentation call. Native completion is
the CPU observation after the completed flip wait; polling/scheduling delay is
included. This is stronger than measuring decode speed, but it is not an exact
hardware scanout timestamp.

PyroWave submit is the CPU callback immediately before `queuePresent`. Paced
modes wait for GPU preparation before this callback; Unpaced preserves its
original order and only records the marker. `observed_us` samples the VideoOut
flip count after present returns. Repeated counts do not represent new flips.
If the count jumps by more than one, the graph excludes that interval and reports
an aggregated observation: individual display intervals cannot be reconstructed.
A single prepared image is still used; this instrumentation adds no GPU pipeline.

CPU submission graphs alone cannot prove smooth display. GPU fence completion
is not scanout. VideoOut counter observations do not prove that the TV activated
VRR. For end-to-end confirmation, use a high-speed camera on a scrolling scene
or visible frame counter, alongside the TV's refresh-rate indicator. These traces
do not measure input-to-photon latency.


### Low-FPS host capture and HDMI VRR

Host VRR controls optional host-side capture/display behavior. It does not disable
PS5 system VRR. The high-refresh VideoOut preset can retain the HDMI VRR transport
when the console setting is enabled. A successful preset request is not proof
that the TV's VRR indicator is disabled. `preset_fallback` in the log reports only
the return code of that preset request; the unpeg result is logged separately.

When a host stops sending pictures (for example, a static desktop), both renderers
now repeat the last decoded image. Repetition begins after two missing source
periods, so it does not compete with an ordinary 30/60/90/120 FPS source. Native
AGC repeats its completed scanout buffer without holding a decoder slot and waits
for the repeat to complete before rendering again. PyroWave re-renders retained
planes without decoding the compressed frame again. Both run repetition on their
existing presentation thread and stop it during teardown. Actual display timing
still depends on VideoOut; the TV and live vblank counts must be checked on PS5.

PyroWave uses the result of the actual unpeg call for its pacing policy, rather
than treating the selected Paced+VRR setting as proof of VRR availability. Repeat
flips are logged separately and must not be interpreted as new incoming frames or
as successful decodes. PyroWave's shown/flip count includes repeats.

Console regression checklist (system VRR on, logs on):

- HEVC HDR 4K120, V-sync on, Classic decoder, 3 cores: test Paced, then Paced+VRR.
  Leave a static desktop for 60 seconds; scroll and move windows for 60 seconds.
  Record the TV refresh reading and any brightness flicker in each part.
- Repeat the static/moving test with Sunshine and Vibepollo, Host VRR off.
- Repeat with PyroWave and H.264 SDR; verify no black frames or stream failures.
- Test 60 FPS and 120 FPS motion, then leave/reconnect/switch codecs without
  restarting the app. Check that idle repeats stop on exit and pairing is intact.
- Compare system VRR off as a control. A VRR logo alone does not show whether
  refresh is varying; report its numeric range as well.

Firmware 13.00 and 13.60 require console confirmation. The exact meaning of
`0x8029001c` has not been established from public documentation; do not identify
it as a firmware limitation based only on that code.


VideoOut ABI correction: Peg takes a handle and two reserved 64-bit arguments,
both zero. This was verified against the local console's firmware 13.60 library:
nonzero reserved arguments return InvalidValue (`0x80290001`). The prior
one-argument declaration left these registers undefined and explains the test
receipts. Peg also requires the process-local Unpeg state; the wrapper now
attempts Unpeg then Peg, recording both results even when Unpeg fails. These are
public exports, without hidden entry points or memory patches. Firmware 13.00
still requires confirmation. A successful API result is not physical HDMI timing
proof; record the TV's numeric refresh range during motion and transitions.

Kodi's current PS5 port documents unsupported Unpeg and system VRR following
flip cadence. Idle frame repetition is retained when an explicit output request
fails; it fixes static low-cadence flicker but does not yet guarantee constant
refresh during motion. Public emulator stubs establish export names, not ABI.

With diagnostic logs enabled, `videoout-policy.log` records each peg/unpeg return
code, selected mode, handle and API-active state. This bounded (64 KiB) receipt is
written only during output setup, directly to the log directory, independently
of redirected stdout. It is deliberately separate from frame timing and the old
component logs, which current main routes to the launcher log.


### Adaptive VRR repeats and scanout observations

VRR now has a separate repeat schedule anchored to source-frame submissions.
An eight-interval source timestamp window estimates cadence; hysteresis enters
low-rate compensation below about 45 FPS and leaves it above about 54 FPS.
Low rates use integer multiplication targeting intervals no longer than 20 ms
(e.g. 16 FPS -> four shows per source period). Normal 60/120 FPS has no
periodic repeat; a 20 ms gap watchdog can repeat on a host stall. Ready frames
preempt repeats; RADV must confirm the previous flip before a repeat is queued.
Missed deadlines do not trigger catch-up bursts. Actual firmware timing and
panel flicker must be tested; this policy is not proof of a minimum HDMI rate.

The source-clock/presentation-floor separation is inspired by Nonary's
[VRR worker](https://github.com/Nonary/moonlight-qt/blob/master/app/streaming/video/ffmpeg-renderers/pacer/vrrpacingworker.cpp).
Qt renderer code is not copied; the source playout controller already used in
ProsperoLight is retained, with a PS5-specific low-cadence adapter.

`pyrowave-scanout-modeN.csv` and `native-scanout-modeN.csv` contain bounded
software observations of VideoOut counts, including repeats. `kind=coalesced`
and `count_delta>1` identify observations that missed intervening flips.
They must not be interpolated into synthetic hardware timestamps. Logs-off
skips collection; logs-on keeps the last 32768 observations per mode, replacing
the prior file on exit. Use `tools/plot-frame-pacing.py` on these CSV files for
counter-observation interval plots alongside the original per-picture traces.


### Shared compensation slots

New source pictures and repeats now share the same low-cadence scanout floor
and slot phase. Observe source cadence before waiting; prepare the new image,
then use its closest legal slot instead of restarting the repeat phase. Both
operations respect the last observed physical flip plus the compensation
interval. RADV retires the prior requested flip before placing a compensated
new picture. This avoids a fresh picture shortly after a repeat, the 8/16 ms
pairing observed in the 60 FPS test. A late scanout moves the future floor
without a burst. Normal-rate playout retains the existing source clock.

The unsigned initial VideoOut flip argument can be `UINT64_MAX` (no picture
shown); it must not be accepted as evidence that every requested image retired.
The same check is used for repeat eligibility and prior-picture retirement.


VRR recovery now leaves low-rate compensation after three consecutive source
intervals below 18.5 ms (with a 6 ms lower sanity bound). On recovery, discard
the mixed old/static cadence window and reset source pacing to the measured
30–120 FPS cadence. This prevents the old compensation phase from backing up
new frames during motion resumption. Isolated capture bursts retain compensation.
Both backends emit a `VRR recovery` receipt with the recovered source rate.

### Stable recovery and admission (2026-10-09)

Fast VRR activity recovery still exits low-rate compensation after three short
source intervals, but does not use that burst as a new FPS estimate. Cadence
windows must span at least 200 ms of source time. Recovery retains the last
moving cadence and the pacer's learned readiness reserve and counters.
Ordinary fitted-rate changes correct phase by at most 1 ms per picture rather
than resetting readiness history. A sparse-to-moving transition rebases the
phase once, without imposing an additional source period on the first image.

Queue admission retains its successor requirement and bounded queue. PyroWave
also considers the queued successor's RTP interval; both backends allow for an
active compensation slot before declaring an image stale. Admission remains
bounded at 100 ms. This does not turn CPU-observed flip times into hardware HDMI
timestamps; real-console comparisons are still required.

### Single VRR admission clock (2026-10-09)

Active VRR now admits fresh pictures and idle repeats through VrrRepeatPolicy
alone. The fixed-refresh/source pacer no longer adds a second deadline to a
prepared VRR picture. Both backends retire the previous physical flip before
admission; a ready source picture takes priority over a pending idle repeat.
Fixed Paced and VRR fallback retain the existing FramePacing path.

Cadence fitting restarts when source intervals spread beyond 25%, and requires
at least 200 ms and eight homogeneous intervals. Sparse-to-motion detection is
still fast, but mixed windows cannot replace the last confirmed moving rate.
Normal-motion admission uses that rate and the display ceiling; compensation
uses the existing integral repeat grid. This is intended to remove scheduling
holes, not guarantee that every TV panel tolerates variable refresh transitions.

New bounded `pyrowave-decisions-modeN.csv` records arrival, dequeue, capacity/age
loss, planned target, submit and repeat events. `source` is raw 90 kHz RTP for
arrival/dequeue/drop events and unwrapped microseconds for target/submit events.
Native `native-decisions-modeN.csv` records ready, age loss, target and submit
with source timestamps in microseconds. Each event ring holds 32768 records,
reports overwritten records, and replaces its file after teardown. No disk I/O
occurs on the input/presentation paths; logging Off skips ring allocation.
Physical-counter CSVs remain CPU observations rather than hardware timestamps.

### Late counter observation regression (2026-10-09)

The b77e0c2 console trace exposed two faults: steady 120 FPS admission fell to
about 103 FPS, and a prepared sparse-source image could receive a full 20 ms
wait when an older repeat was first observed as completed. Software counter
poll time was incorrectly treated as scanout start time. The scheduler now
keeps the submission timeline as its phase anchor. Counter retirement still
gates eligibility, but does not move a deadline forward. An already due fresh
image replaces the due repeat immediately after retirement. Repeat scheduling
uses submission-start time rather than GPU/flip completion time in both paths.

Replay covers delayed counter polls at steady 120 Hz and the frame-813-style
repeat-to-fresh transition. It preserves the bounded compensation grid and
requires no additional interval after a late completion observation. These
checks validate scheduling, not panel brightness or actual HDMI timestamps.
