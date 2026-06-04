# Project status & next steps

_Snapshot: 2026-06-03. Resume point for the next session._

## Where we are

The core of the Fuel Finder works end-to-end; only the on-screen UI is unfinished.

### ✅ Done & verified on the device
- **GPS wiring** — Air530Z plugs into the **`Grove(IIC)`** socket (the RIGHT one with
  the back facing you). Both Grove ports are on the RP2040. See `docs/hardware/`.
- **RP2040 firmware** (`firmware/rp2040/`) — reads the GPS (UART1, 9600), forwards
  `FIX,...` lines to the ESP32-S3 at ~1 Hz. Flashed & running.
- **Inter-proc link** — confirmed: ESP32-S3 RX = GPIO20, TX = GPIO19.
- **ESP32-S3 app** (`firmware/esp32-s3/`) — boots; PSRAM (octal/OPI) fixed; geo math
  (6 passing unit tests); `IFuelProvider` + `TomTomProvider`; FIX-line parser.
- **Full pipeline** — with real Wi-Fi + TomTom key, a fixed-location query returned
  **10 real Orlando stations** (brand + address + distance + bearing, sorted). The
  `-DSELFTEST_TOMTOM` build flag reproduces it.
- **Display panel** — first light is clean (`tools/esp32-display-test`).

### ⚠️ Unfinished: the LVGL UI on the panel
LVGL renders + `lv_qrcode` works, but the output tears/blurs. **Root cause found:**
Arduino_GFX can't do what this panel needs (Seeed uses esp_lcd **double framebuffer +
vsync**, not a bounce buffer; Arduino_GFX is single-fb + bounce-buffer only). The
Arduino_GFX route (`tools/esp32-lvgl-test`) is a **documented dead-end**.

## ▶️ Next session: port Seeed's esp_lcd display (the clean path)

All reference values are in **`docs/hardware/display.md`** (timings, 18 MHz, MADCTL
`0x36=0x10`, SDIR `0xC7=0x04`, flags). Reference SDK: clone
`https://github.com/Seeed-Solution/SenseCAP_Indicator_ESP32` — the files that matter are
`components/bsp/src/boards/lcd_panel_config.c` (`lcd_panel_st7701s_init`),
`sensecap_indicator_board.c`, and `peripherals/bsp_lcd.c`.

1. **esp_lcd RGB panel** — create directly (double_fb + refresh_on_demand + on_vsync),
   drop `Arduino_GFX` for the panel.
2. **ST7701 init via the PCA9535** — bit-bang the 3-wire SPI with CS/RST on the expander
   (port Seeed's `SPI_WriteComm/SPI_WriteData`). *This is the one medium-risk step.*
3. **LVGL** — wire the two framebuffers as LVGL's draw buffers, vsync-synced flush.
4. **Touch (FT5x06, I²C GPIO39/40) + real UI** — port the mock from `esp32-lvgl-test`
   into the app's `ui/` module (replacing the serial-log stub) and feed it real data.

Estimate: steps 1–3 ≈ one focused session (a few on-screen checks); step 4 a short one.

## Housekeeping / reminders
- 🔑 **Rotate the TomTom API key** used for testing (it's in the ESP32 flash + chat
  history). `config.h` (real Wi-Fi/key) is git-ignored and never committed.
- Device currently runs: RP2040 = GPS reader; ESP32-S3 = the best Arduino_GFX LVGL
  test build (blurry-but-visible). Reflash the app with
  `cd firmware/esp32-s3 && pio run -e esp32-s3 -t upload` when resuming app work.
- Lesson logged: for finicky hardware, port a known-good vendor reference first instead
  of looping on blind one-variable guesses.
