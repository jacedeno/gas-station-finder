# esp32-display-test

Display "first light" bring-up for the SenseCAP Indicator ST7701S 480×480 RGB panel.
Cycles solid colours + colour bars via Arduino_GFX, with the panel CS/RST driven over
the PCA9535 I²C expander. Used to find the working timing/driver config before adding
LVGL.

## Verified-working configuration (2026-06-03)

Confirmed on-device: **clean image, no striping, no flicker.**

- **Core:** arduino-esp32 **3.x** via pioarduino (the official `espressif32` 6.x ships
  core 2.0.17, whose Arduino_GFX has **no RGB bounce buffer** → striping).
- **Lib:** `GFX Library for Arduino` **1.4.x** + `hideakitai/PCA95x5`.
- **Panel:** ST7701S, `st7701_type1_init_operations`, RGB pins per `docs/hardware/display.md`.
- **Timing:** hfp=10, hpw=8, hbp=50; vfp=10, vpw=8, vbp=20; `pclk_active_neg=1`;
  **PCLK = 16 MHz** (~56 Hz refresh).
- **Key fix:** `bounce_buffer_size_px = 480*10` — an SRAM bounce buffer that smooths the
  framebuffer reads from PSRAM. Without it the panel shows moving stripes; lowering the
  PCLK reduces stripes but drops the refresh rate into visible flicker. The bounce
  buffer lets us keep 16 MHz *and* a clean image.
- **CS/RST:** PCA9535 P04 = CS (held low for init), P05 = RST (pulse). I²C on SDA=39,
  SCL=40 (shared with the FT5x06 touch).
- **Backlight:** GPIO45 high.

## Run

```bash
pio run -e display-test -t upload
pio device monitor -b 115200
```
