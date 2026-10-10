# Repeatable frame-pacing validation

The host stimulus and log collector are in [`tools/pacing-test`](../tools/pacing-test).
No application installation, browser extensions or external web services are required for the scene.
The collector requires Python 3 and uses only its standard library.

## Preparation

1. Fully close ProsperoLight and remount the new PPSA99002 image. Verify the About build label.
2. Enable Debug Log and diagnostic logs. Use wired networking. Keep the host display at 120 Hz or higher and the browser visible on the captured display. Do not change TV settings between runs.
3. Baseline: PS5 system VRR On, “Apply to Unsupported Games” Off, 120 Hz output enabled; VSync On in ProsperoLight. Record firmware and TV model/settings.
4. Open `tools/pacing-test/index.html` on the PC; a local file works. The canvas is always 3840×2160; the selected stream resolution determines the encoded resolution. The scene is SDR even when the stream transport is HDR, so it does not test HDR colour accuracy.
5. Start the collector on any LAN computer: `python collect.py --watch --host PS5_IP --output pacing-logs`. Allow FTP through the PS5 environment. It archives completed-session traces so subsequent runs do not overwrite them. Ctrl+C stops it. Each archive includes the current build label and bounded logs.

## Each run

1. Apply the matrix row's settings in ProsperoLight and connect to Desktop.
2. Set matching labels on the host test page. These labels **do not configure the client or host**.
3. Start the page in full screen. Let its complete 90-second schedule finish: warm-up 10 s, motion 20 s, static 20 s, motion 20 s, then motion/static/motion/static for 5 s each. Remain in the stream without the PS5 system menu.
4. Download the page's JSON report, then stop streaming. Wait **15 seconds** before starting the next stream so the collector can capture completed traces. Check its “Saved” message.
5. Record the matrix ID, visible flicker/stutter, TV Hz during motion/static/return to motion, and any error. Match the JSON settings to the actual client settings.

The page animates according to elapsed time at every browser refresh. It deliberately does not cap its drawing to the client FPS: otherwise a 45 FPS page on a 120 Hz host would introduce its own uneven timing. Static phases do not repaint or update a visible timer. The JSON reports browser refresh and warns about hidden/background tabs or insufficient host refresh. These measurements describe the **host browser**, not PS5 HDMI scanout. PC and PS5 clocks are not assumed to be synchronized.

## Matrix

The explicit cases and blank result columns are supplied in [`matrix.csv`](../tools/pacing-test/matrix.csv).
Start with **A49, A50, A51 for all three codecs**: nine runs, about 14 minutes plus reconnect time.

| Group | Resolution / codec | FPS | Policy | Purpose |
|---|---|---|---|---|
| A | 1080p SDR, PyroWave / HEVC / H.264 | 45, 48, 49, 50, 51 | Paced+VRR, VSync On | Boundary behaviour and parity of decoders; 15 runs |
| B | 1080p SDR, PyroWave | 30, 40, 60, 75, 90, 100, 120 | Paced+VRR, VSync On | Other Custom values; seven runs |
| C | 1080p SDR, PyroWave / HEVC | 45, 75, 90, 120 | Paced, VSync On | Fixed-refresh regression; eight runs |
| D | 4K HDR, PyroWave / HEVC | 45, 49, 50, 90, 120 | Paced+VRR, VSync On | Production-resolution/HDR regression; ten runs |
| E | 1080p SDR, all three codecs | 120 | Unpaced, VSync On | Baseline; three runs |
| F1 | 1080p SDR, PyroWave / HEVC, PS5 VRR Off | 45 | Paced+VRR, VSync On | Fixed-output/API-fallback behaviour; two runs |
| F2 | 1080p SDR, PyroWave / HEVC, PS5 VRR On | 90 | Paced+VRR, saved VSync Off | Synchronization forced On while using VRR; two runs |
| F3 | Same codecs, immediately after F2 | 90 | Paced, VSync Off | Saved Off preference restored; two runs |
| G | 4K HDR PyroWave or HEVC | 120 | Paced+VRR, VSync On | 30–60 minutes in an actual game; choose one, then the other if needed |

Use fixed bitrates throughout: 1080p PyroWave **200 Mbps**, HEVC/H.264 **20 Mbps**; 4K PyroWave **500 Mbps**, HEVC **80 Mbps**. These are test conditions, not general quality recommendations. Avoid H.264 4K120 in the primary pacing matrix: its known decoder load can obscure presentation problems. Test it separately if investigating decoder performance.

## Expected results

With successful variable output, a **stable incoming source** of 30–59 FPS uses two scanouts per source picture (30→60, 40→80, 45→90, 48→96, 49→98, 50→100, 51→102 Hz). 60–120 FPS uses one. A 1% timestamp tolerance prevents nominal 60 FPS drifting to 2× due to clock rounding. At sparse ~16 FPS desktop capture the factor can rise to four (~64 Hz). These are scheduler targets, not guarantees of the TV's reported frequency.

Initialization and static→motion recovery must converge to the same rate/factor. Allow the short cadence-learning window; there must be no persistent 60 FPS ceiling when input is 90/120, no large sustained stale growth in steady motion, no crash/hang, and no visible transition flicker. Source frames are repeated, not interpolated.

Paced uses fixed nominal 60 Hz for FPS ≤60, 120 Hz for FPS >60. Non-divisible source rates naturally occupy alternating refresh counts (e.g. 45 FPS on 60 Hz). This is expected and is not evidence of a decoding failure. Paced+VRR requests a 120 Hz-capable mode and then schedules variable flips; if the API refuses VRR it uses fixed-output pacing. With system VRR disabled, also record the TV's actual fixed refresh: API receipts alone do not prove physical VRR.

Compare **unique pictures**, repetitions, and network losses separately. Native `shown` telemetry counts unique pictures; PyroWave's live `shown` includes repeats. Do not compare total shown counts directly. Shared `VRR cadence` receipts report source FPS, repetition factor and target refresh for both backends. CSV files contain software counter observations; they are not a calibrated HDMI measurement. Ignore the first warm-up window when estimating steady intervals, but retain transition outliers in the report.

Automated host regression tests cover every integer Custom FPS from 30 to 120, including recovery from sparse input and locally skipped frames. They validate scheduling calculations, not firmware/TV acceptance. The hardware matrix above remains necessary.

Small losses from a requested 120 FPS versus nominal 119.88 Hz, source jitter or decoder load must not be mistaken for the former ~60 FPS scheduling cap. Record their rate and cause instead of requiring absolutely zero discarded pictures.

For requested 30–59 FPS, the duplicated scanout grid is now held across static/moving transitions. Sparse capture changes source FPS, not this grid. The logged sparse repeat factor is approximate; it does not imply interpolation. Compare the new reference scene with HEVC 60 FPS / Paced / VSync On before evaluating HEVC/PyroWave 51 FPS / Paced+VRR. Prefer a 120 Hz host display for the 60 FPS reference; rAF at 200 Hz sampled at 51 FPS is not a perfectly uniform motion generator.
