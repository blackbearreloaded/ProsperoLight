# Changelog

## 01.000.060

### Smoother 4K120 streaming

- Enabled bounded decode/presentation overlap: the next picture can decode while
  the previous native flip completes, with at most one outstanding submission.
  Source surfaces and shared GPU memory remain protected until completion.
- Improved stale-frame handling, short-window FPS metrics, and fractional-refresh
  negotiation (for example, a 120 FPS request on 119.88 Hz output).
- Added a small local post-session performance summary and regression checks for
  presentation ownership, timeout handling, and incomplete report writes. No
  remote telemetry or streamed content is collected by this summary.

### 4K120 before / after

One paired Hollow Knight test on PS5 firmware 6.02, using wired Ethernet,
3840×2160 at a requested 120 FPS, HEVC SDR, 80 Mbps, stereo, and 119.88 Hz output:

| Measurement | Before: overlap off | After: overlap on |
| --- | ---: | ---: |
| Decoded / presented frames | 16,769 / 12,686 | 14,058 / 14,058 |
| Stale presentation skips | 4,083 (24.3%) | 0 (0%) |
| Mean enqueue-to-decoder-callback wait | 5.468 ms | 0.016 ms |
| Sampled pending video queue peak | 5 frames | 0 frames |
| Mean decoder-call time | 6.869 ms | 6.934 ms |
| p99 CPU-observed completion interval | 75.499 ms | 31.499 ms |

The tester reported **lots of stuttering before** and **much smoother playback
after**. Neither run reported network frame gaps or audio errors. The improvement
is in queueing and presentation, not faster hardware decoding.

These are whole-session measurements from one comparison, including startup and
menus, with different session lengths. The baseline was instrumented with overlap
disabled, not the exact public `01.000.055` binary. This is not a guarantee of
locked 120 FPS, lower input-to-photon latency, or a fix for every high-bitrate/Wi-Fi
setup. CPU-observed frame age increased from 12.560 to 16.854 ms; overlap changes
when completion is observed, and the baseline excludes skipped pictures from
that metric. It cannot establish an end-to-end latency change.

See the [Round 2 measurements and methodology](https://github.com/blackbearreloaded/ProsperoLight/blob/01.000.060/docs/PERFORMANCE_ROUND_2.md#release-checkpoint--01000060).
Further performance work is deferred to the next version: FEC SIMD, Opus SIMD,
audio catch-up, and shorter flip polling are **not enabled** in this release.

### Installation / update

Title ID remains `PPSA99002`; existing pairing and preferences are preserved.
Choose `PPSA99002.ffpfsc` or extract `PPSA99002.zip` and copy its `PPSA99002/`
folder to `/data/homebrew`. Do not keep both formats installed for the same
title. Close the app before replacing its files, then restart ShadowMountPlus
or the console. `SHA256SUMS` covers both downloads.

## 01.000.055

- Added a persistent Stereo / 5.1 surround setting.
- Added Moonlight-compatible 5.1 Opus negotiation and PS5 eight-channel AudioOut,
  with the unused side channels silenced and automatic stereo fallback when the
  surround port is unavailable.
- Compacted the Settings rows so all seven controls and the shortcut note remain
  visible above the footer.
- Replaced the temporary demo app cards with a clear loading state while the
  Sunshine application catalog refreshes.
- Unified the mbedTLS structure configuration across the C and C++ stream code,
  preventing identity initialization from overwriting the live PS5 pad state.

## 01.000.050

- Added independently selectable 90 and 120 FPS streaming at 1080p, 1440p,
  and 2160p, with native 4K/119.88 Hz output validated on PS5 hardware.
- Preserved native 4K presentation for high-resolution HFR streams and restored
  the launcher output cleanly when a stream ends.
- Added an application-side startup transition for TVs that resynchronize when
  an HFR-capable title opens.
- Moved Sunshine discovery behind the first rendered launcher frame for faster
  visible startup.
- Documented that HFR setting changes should be applied before launching, or
  followed by stopping and relaunching the active Sunshine application.
- Clarified that wired Ethernet is recommended for high-resolution and HFR
  streaming.
- Added guidance to tune bitrate per configuration because maximum bitrate can
  reduce smoothness at 4K or high frame rates.
