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

## ✅ Verified-working driver config (2026-06-03)

The panel was brought up cleanly (no striping, no flicker) with `tools/esp32-display-test/`.
The decisive findings:

- **Use arduino-esp32 core 3.x** (pioarduino). The official `espressif32` 6.x ships
  core 2.0.17, whose Arduino_GFX build has **no RGB bounce buffer** → unavoidable
  striping. → The ESP32-S3 app must migrate to core 3.x for the UI.
- **Driver:** `GFX Library for Arduino` 1.4.x + `hideakitai/PCA95x5`, `Arduino_RGB_Display`
  + `Arduino_ESP32RGBPanel` with `st7701_type1_init_operations`.
- **Timing that works:** hfp=10, hpw=8, hbp=50; vfp=10, vpw=8, vbp=20;
  `pclk_active_neg=1`; **PCLK 16 MHz** (~56 Hz); **`bounce_buffer_size_px = 480*10`**
  (the key fix — smooths PSRAM framebuffer reads).
- **CS/RST via PCA9535:** P04=CS (held low for init), P05=RST (pulse); I²C SDA=39/SCL=40.
- **Backlight:** GPIO45 high.

## Seeed SDK reference (the authoritative config — port this)

Extracted from Seeed's ESP-IDF SDK (`Seeed-Solution/SenseCAP_Indicator_ESP32`):
`components/bsp/src/boards/lcd_panel_config.c` (`lcd_panel_st7701s_init`) +
`sensecap_indicator_board.c` + `peripherals/bsp_lcd.c`.

- **RGB timings (match what we used):** HBP=50, HFP=10, HPW=8; VBP=20, VFP=10, VPW=8.
- **PCLK = 18 MHz** (Kconfig default), `pclk_active_neg` per board.
- **Framebuffer:** `fb_in_psram = 1`. **No bounce buffer.** To avoid tearing Seeed uses
  **`double_fb = 1` + `refresh_on_demand = 1` + an `on_vsync` callback** (esp_lcd
  RGB) — *not* a bounce buffer.
- **ST7701 init:** their `lcd_panel_st7701s_init` is the canonical sequence. Two pieces
  Arduino_GFX's `st7701_type1_init_operations` is MISSING:
  - **`0x36` (MADCTL) = `0x10`** — orientation (the panel is mounted 180°), in hardware.
  - **`0xC7` (SDIR) = `0x04`** — source direction.
  - Pixel format `0x3A = 0x60` (RGB666); `0x21` display-inversion on.
  A verbatim translation is in `tools/esp32-lvgl-test` as `st7701_indicator_init`.

### Port spec — authoritative values extracted from the Seeed SDK (2026-06-04)

Pulled verbatim from `Seeed-Solution/SenseCAP_Indicator_ESP32` so we never re-derive
them. This is what `tools/esp32-esplcd-lvgl/` implements (the clean esp_lcd path).

**esp_lcd RGB panel** (`peripherals/bsp_lcd.c`, the `LCD_AVOID_TEAR` branch):
- `data_width = 16`, `bits_per_pixel = 16`, `clk_src = PLL160M`.
- `flags.fb_in_psram = 1`, **`num_fbs = 2` (double FB)**, **`flags.refresh_on_demand = 1`**.
  **No bounce buffer** (that was the Arduino_GFX crutch; esp_lcd doesn't need it here).
- Register an `on_vsync` callback that gives two binary semaphores (`trans_ready`,
  `flush_ready`) from ISR.
- A dedicated `lcd_task`: `take(trans_ready)` → `vTaskDelayUntil(CONFIG_LCD_TASK_REFRESH_TIME)`
  → `esp_lcd_rgb_panel_refresh()` → `take(flush_ready, 0)`. `LCD_TASK_REFRESH_TIME = 40 ms`,
  task priority 5.
- Pins (RGB bus): HSYNC=16, VSYNC=17, DE=18, PCLK=21, DISP_EN=NC, BL=45 (active-high).
  Data (`data_gpio_nums[0..15]` = **B0..B4, G0..G5, R0..R4**): 15,14,13,12,11,
  10,9,8,7,6,5, 4,3,2,1,0.
- Timings: `pclk_hz = 18 MHz`, h_res=v_res=480, HBP=50, HFP=10, HPW=8, VBP=20,
  VFP=10, VPW=8, `flags.pclk_active_neg = 0`.

**LVGL wiring** (`examples/lvgl_demos/main/lv_port.c`, `LCD_LVGL_FULL_REFRESH`):
- The **two esp_lcd framebuffers are LVGL's two draw buffers**
  (`esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &buf0, &buf1)`), each `W*H` px.
- `disp_drv.full_refresh = 1`.
- `flush_cb` → `esp_lcd_panel_draw_bitmap(panel, x1,y1,x2+1,y2+1, color_p)` (full-frame
  swap; draw_bitmap handles the PSRAM cache writeback) → `take(flush_ready)` (wait one
  vsync) → `lv_disp_flush_ready()`. No software rotation (orientation is in the init).

**ST7701S init via the PCA9535** (`boards/lcd_panel_config.c::lcd_panel_st7701s_init`):
- 3-wire 9-bit SPI bit-banged: **CS = expander P04, RST = expander P05** (I²C @ SDA39/SCL40,
  addr `0x20`); **CLK = GPIO41, MOSI/SDA = GPIO48** (real GPIOs). 9-bit frame = 1 D/C bit
  (0=cmd, 1=data) + 8 data bits, MSB first, sampled on the rising edge.
- Sequence (BK0 → BK1 → gamma → `0x36=0x10` MADCTL → `0x3A=0x60` RGB666 → `0x21` inversion
  → `0x11` sleep-out → `0x29` display-on). Orientation (180°) is **fully in hardware**
  here (`0x36`/`0xC7`), so LVGL/esp_lcd use no rotation. Transcribed in
  `tools/esp32-esplcd-lvgl/src/main.cpp`.
- **Order:** PCA9535 RST pulse → run the SPI register init → *then* create/init the
  esp_lcd RGB panel → backlight on. (Configure panel registers before streaming RGB.)

### Why Arduino_GFX struggled (architecture)

Arduino_GFX's `Arduino_ESP32RGBPanel` hardcodes `num_fbs = 1` and offers only a bounce
buffer — it can't do Seeed's `double_fb` + vsync-synced refresh. With a single PSRAM
framebuffer + LVGL writes, it tears/stripes/blurs. **Recommended: drop Arduino_GFX for
the panel and port Seeed's `esp_lcd` setup directly** (double framebuffer + the two LVGL
draw buffers = the two FBs, the standard Espressif "avoid tearing" pattern), keeping the
PCA9535 CS/RST init. That is the clean path; the Arduino_GFX route in `esp32-lvgl-test`
was a dead-end for a tear-free UI.

## Bring-up status — LVGL is clean via the esp_lcd port ✅ (2026-06-04)

1. ✅ ST7701S init + RGB panel — clean, sharp direct framebuffer fills.
2. ✅ **LVGL is now sharp & stable.** The blur was the Arduino_GFX single-fb + bounce
   buffer architecture, not the panel. Fixed by porting Seeed's esp_lcd path (the **Port
   spec** above): `num_fbs=2` double framebuffer + `refresh_on_demand` + `on_vsync`, the
   two framebuffers wired as LVGL's two draw buffers (`full_refresh=1`), flush = zero-copy
   `draw_bitmap` swap synced to vsync. Orientation is baked into the ST7701 init
   (`0x36=0x10`/`0xC7=0x04`), so **no software rotation**. Implemented + verified on-device
   in **`tools/esp32-esplcd-lvgl/`** (R/G/B fills sharp/correct; mock UI + `lv_qrcode`
   sharp/stable). The Arduino_GFX route (`tools/esp32-lvgl-test`) is the documented
   dead-end.
3. ⏭️ Next: wire FT5x06 touch (`indev`) and fold the panel/LVGL setup + the list +
   QR-detail screens into the app's `ui/` module (see `docs/STATUS.md`).

## Sources

ESPHome device DB and Seeed SDK — see [`../references.md`](../references.md).
