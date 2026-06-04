# esp32-lvgl-test  (WORK IN PROGRESS)

LVGL 8.4 render test on the SenseCAP Indicator, reusing the verified panel config from
`tools/esp32-display-test`. Draws a mock Fuel Finder UI (header + station list + a
`geo:` QR via `lv_qrcode`) with dummy data, to validate the LVGL stack before porting
the UI into the main app.

## Status

- ✅ Builds; LVGL initialises; `lv_qrcode` works; the UI renders and persists.
- ✅ The panel + driver are proven good — direct `gfx->fillScreen()` R/G/B at boot is
  clean (a probe is left in `setup()`).
- ⚠️ **Not yet clean.** The LVGL output is blurry / occasionally unstable and the
  180° orientation vs sharpness trade-off is unresolved. The direct fills are sharp,
  so the issue is specific to the LVGL → framebuffer path, not the panel.

## What was tried (none fully clean)

| Lever | Values tried | Result |
|-------|--------------|--------|
| Orientation | software `rotation=2`; hardware SDIR `0xC7,0x04` in a custom init (`st7701_indicator_init`) | SW rot = right-side-up but blurry; the HW-SDIR attempt regressed to vertical stripes (init likely needs the gate-direction bit too, not just source) |
| `LV_COLOR_16_SWAP` | 0 vs 1 | 0 = recognisable but colours look off; 1 = byte-swapped noise |
| PCLK | 12 / 14 / 16 MHz | lower = less tearing, more flicker |
| Bounce buffer | 10 / 20 lines | bigger helped the moving stripes |
| Redraw | forced each second vs one-shot | forced = "trembling" text (tearing); one-shot = stable but sometimes blank until primed |

Best so far (current code): `rotation=2` + `st7701_type1_init_operations` + `swap=0`,
one-shot draw — oriented, but text blurry.

## Researched next steps

- **Hardware 180° rotation done right**: extend `st7701_indicator_init` to flip BOTH
  source (`0xC7` SDIR) and gate direction (the `0xC0` LNSET scan-direction bits), then
  use `rotation=0`. Software rotation smears partial flushes — the likely blur cause.
  Ref: Arduino_GFX discussion #334 (this exact board) and the Seeed ESP-IDF reference
  (`SenseCAP_Indicator_ESP32`, its `lv_port` + ST7701 init) — port its init verbatim.
- Consider zero-copy: render LVGL straight into `gfx->getFramebuffer()` (full-refresh)
  to remove the copy and any format mismatch.

## Run

```bash
pio run -e lvgl-test -t upload    # ESP32-S3 on /dev/ttyUSB0
```
