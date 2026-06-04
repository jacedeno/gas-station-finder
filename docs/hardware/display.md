# SenseCAP Indicator — display & touch

Resolves CLAUDE.md Pending #4 (panel controller + pin map) and #5 (LVGL version).
Pin assignments from the ESPHome device DB; the panel needs verification on first
bring-up but the values are consistent across Seeed's SDK and ESPHome.

## Panel

- **Controller:** Sitronix **ST7701S**, 480 × 480, **16-bit RGB565 parallel** interface.
- The ST7701S is initialised over a **3-wire SPI** side-channel, then streams pixels
  over the RGB bus. The RGB bus is driven by the ESP32-S3 LCD peripheral (esp_lcd).

### RGB parallel bus (ESP32-S3 GPIO)

| Signal | GPIO |
|--------|------|
| HSYNC | 16 |
| VSYNC | 17 |
| DE | 18 |
| PCLK | 21 |
| R4..R0 | 4, 3, 2, 1, 0 |
| G5..G0 | 10, 9, 8, 7, 6, 5 |
| B4..B0 | 15, 14, 13, 12, 11 |

### SPI init side-channel + control

| Signal | Where |
|--------|-------|
| SPI CLK | GPIO41 |
| SPI MOSI (SDA) | GPIO48 |
| **SPI CS** | **PCA9535 I²C expander, pin 4** |
| **Panel RST** | **PCA9535 I²C expander, pin 5** |
| Backlight | GPIO45 (PWM, 100 Hz) |

> ⚠️ **The catch:** SPI **CS** and panel **RST** are *not* on ESP32 GPIOs — they hang
> off a **PCA9535 I²C port expander**. So the ST7701S init sequence must toggle CS/RST
> by writing the PCA9535 over I²C while clocking the init bytes out on GPIO41/48. This
> is the main bring-up complexity; Seeed's SDK does exactly this.

### Panel timing

Porch/PCLK values are not published in the ESPHome page; take them from Seeed's
ESP-IDF SDK (`esp_lcd` panel config) or the ESPHome `mipi_rgb` model
`SEEED-INDICATOR-D1`, which is pre-configured for this board. To confirm during
bring-up.

## Touch

- **Controller:** Focaltech **FT5x06** (capacitive), I²C.
- I²C: **SDA = GPIO39, SCL = GPIO40**. Apply `mirror_x` + `mirror_y` to match the
  panel orientation.

## No pin conflicts with the GPS link

The inter-processor UART (RP2040 ↔ ESP32) uses **GPIO19 (TX) / GPIO20 (RX)** — see
`firmware/PROTOCOL.md`. The display bus uses GPIO0–18 and 21; touch uses 39/40; SPI
init 41/48; backlight 45. **GPIO19/20 are free of the display**, so the GPS link and
the screen coexist cleanly.

## LVGL decision (Pending #5)

- **Pin LVGL 8.x** (target **8.4.0**, the last 8.x). CLAUDE.md requires `lv_qrcode`,
  which ships *inside* LVGL 8.x (`LV_USE_QRCODE`, under `src/extra/libs/qrcode/`). In
  LVGL 9 the QR code widget was moved out to a separate component, so 8.x is the
  low-friction choice for the QR-centric detail screen.
- **Display driver:** Espressif **`esp_lcd`** RGB panel API (bundled with the
  arduino-esp32 core) + an ST7701S vendor init that drives CS/RST via the PCA9535.
  LVGL's `flush_cb` blits into the RGB framebuffer (PSRAM-backed; the device has 8 MB).
- **Touch driver:** an FT5x06 I²C reader feeding LVGL's `indev` pointer device.

## Bring-up checklist (next coding task)

1. PCA9535 I²C driver: set CS/RST lines.
2. ST7701S init sequence over GPIO41/48 SPI + PCA9535 CS/RST.
3. esp_lcd RGB panel config with the pins/timing above; framebuffer in PSRAM.
4. LVGL 8.4 `lv_conf.h` (enable `LV_USE_QRCODE`); wire `flush_cb` + FT5x06 `indev`.
5. Backlight on (GPIO45 PWM).

## Sources

ESPHome device DB and Seeed SDK — see [`../references.md`](../references.md).
