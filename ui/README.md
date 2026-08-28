# Radio Browser UI

Static RmlUi/RCSS assets for the first PS5 UI pass. The default preview state is
`browse`; `boot`, `empty`, and `error` are included as alternate panels for the
future controller/UI bridge to activate by moving `is-active`.

## Files

- `main.rml` — controller-first structure and mock station content.
- `styles/app.rcss` — 10-foot layout, signal-themed palette, spatial focus, and
  the persistent player strip.

No networking, audio, image assets, or runtime behavior is assumed here.
