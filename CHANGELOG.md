# Changelog

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
