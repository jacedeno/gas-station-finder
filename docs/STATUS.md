# Project status & next steps

_Snapshot: 2026-06-04. Resume point for the next session._

## Where we are

The core of the Fuel Finder works end-to-end, **and the display is now clean** — the
esp_lcd port (below) fixed the LVGL blur. Only touch + folding the UI into the app
remain.

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

### ✅ Display: clean LVGL via the esp_lcd port (2026-06-04)
The LVGL blur is **fixed**. Ported Seeed's esp_lcd path into `tools/esp32-esplcd-lvgl/`
and verified on-device: the three boot R/G/B framebuffer fills are **sharp & correct**,
and the LVGL mock UI (header + station list + `geo:` QR) is **sharp & stable** — no
blur, no tearing, correct orientation. The Arduino_GFX route (`tools/esp32-lvgl-test`,
single-fb + bounce buffer) stays as a documented dead-end.

What made it work (all in `tools/esp32-esplcd-lvgl/`, spec in `docs/hardware/display.md`):
- **esp_lcd RGB panel** with `num_fbs=2` (double FB) + `refresh_on_demand` + `on_vsync`,
  **no bounce buffer**; a refresh task mirrors Seeed's `lcd_task` (vsync → 40 ms → refresh).
- **ST7701 init** transcribed verbatim from `lcd_panel_st7701s_init` (incl. `0x36=0x10`,
  `0xC7=0x04`, `0x3A=0x60`, `0x21`), bit-banged 9-bit SPI, CS/RST on the PCA9535.
- **LVGL `full_refresh=1`** with the two framebuffers as the draw buffers; flush =
  zero-copy `draw_bitmap` swap synced to vsync. Orientation is in the init (no SW rotation).

## ▶️ Next session: touch + fold the UI into the app

1. **Touch (FT5x06, I²C GPIO39/40)** — add an `indev` reader (port Seeed's
   `touchpad_read`; note the `W - x`, `H - y` mapping for the 180° mount) and register it
   as an LVGL pointer device in `tools/esp32-esplcd-lvgl/` first to verify taps.
2. **Migrate the app to core 3.x + esp_lcd** — `firmware/esp32-s3/` still uses the
   `espressif32` 6.x platform (core 2.0.17). Switch it to the pioarduino 55.03.x platform
   used by the tool, so esp_lcd is available.
3. **Port the display + UI into `firmware/esp32-s3/src/ui/`** — lift the panel/LVGL setup
   from the tool into the app's `ui/` module (replacing the serial-log stub) and feed it
   real `Station` data from the existing pipeline (GPS → TomTom → geo).

Estimate: touch ≈ short; app migration + UI wiring ≈ one focused session.

## Housekeeping / reminders
- 🔑 **Rotate the TomTom API key** used for testing (it's in the ESP32 flash + chat
  history). `config.h` (real Wi-Fi/key) is git-ignored and never committed.
- Device currently runs: RP2040 = GPS reader; ESP32-S3 = `tools/esp32-esplcd-lvgl/`
  (the clean esp_lcd LVGL build — sharp UI). Reflash the app with
  `cd firmware/esp32-s3 && pio run -e esp32-s3 -t upload` when resuming app work, or the
  display tool with `cd tools/esp32-esplcd-lvgl && pio run -e esplcd-lvgl -t upload`.
- Lesson logged: for finicky hardware, port a known-good vendor reference first instead
  of looping on blind one-variable guesses.
