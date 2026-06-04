# esp32-esplcd-lvgl — clean display path (esp_lcd, the Seeed port)

Brings the Indicator's ST7701S 480×480 RGB panel up through Espressif's **esp_lcd**
RGB API with a **double framebuffer + refresh-on-demand + vsync**, and wires the two
framebuffers straight to **LVGL** as its draw buffers (full-refresh). This is the
replacement for the Arduino_GFX route (`tools/esp32-lvgl-test`), which could only do a
single PSRAM framebuffer + bounce buffer and therefore tore/blurred under LVGL — a
documented dead-end (see `docs/STATUS.md`).

Everything here is a faithful port of Seeed's ESP-IDF SDK
(`Seeed-Solution/SenseCAP_Indicator_ESP32`), transcribed in
`docs/hardware/display.md` → **"Port spec"**:

| Piece | Seeed source | Here |
|-------|--------------|------|
| RGB panel (double FB + vsync) | `peripherals/bsp_lcd.c` | `initRGBPanel()` / `lcdRefreshTask()` |
| ST7701 init via PCA9535 | `boards/lcd_panel_config.c` (`lcd_panel_st7701s_init`) | `ST7701_INIT[]` / `runST7701Init()` |
| LVGL full-refresh wiring | `examples/lvgl_demos/main/lv_port.c` | `flushCb()` + `full_refresh=1` |

## How it's structured (verify in layers)

`setup()` brings the stack up so each layer is independently checkable:

1. **esp_lcd RGB panel + ST7701 init** → three **direct framebuffer fills** (R/G/B, no
   LVGL). These must be sharp and the right colour. If they are, the panel, the init
   sequence, the timings and the FB→GDMA path are all correct.
2. **LVGL full-refresh** on top → the mock Fuel Finder UI (header + station list +
   `geo:` QR). The two framebuffers are LVGL's two draw buffers; the flush is a
   zero-copy framebuffer swap synced to vsync — the standard Espressif anti-tear
   pattern. No software rotation (orientation is baked into the ST7701 init).

## Run

```bash
pio run -e esplcd-lvgl -t upload    # ESP32-S3 on /dev/ttyUSB0
pio device monitor                  # 115200
```

Expected serial:

```
# esp_lcd + LVGL bring-up (Seeed port)
[disp] ST7701 SPI init...
[disp] esp_lcd RGB panel (double FB)...
[probe] direct fill RED / GREEN / BLUE
[lvgl] init + full-refresh draw buffers
[lvgl] UI built
[lvgl] alive
```

## If the direct fills are wrong

Stay at layer 1 — do **not** start tuning LVGL. The fills isolate the panel path:
- **Blank screen:** check the ST7701 SPI init reached the panel (CS=P04/RST=P05 on the
  PCA9535 @ 0x20, CLK=GPIO41, MOSI=GPIO48) and the backlight (GPIO45).
- **Stripes/garbage:** RGB timings or the data-pin order (`data_gpio_nums`, B/G/R).
- **Wrong colours:** pixel-format / byte order, not LVGL.

These all have known-good values in `docs/hardware/display.md`.
