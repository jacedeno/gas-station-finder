# Project status & next steps

_Snapshot: 2026-06-04. Resume point for the next session._

> **Update 2026-06-05 (branch `feature/restaurants-foursquare`):** the per-station
> **QR was dropped** (judged not useful) and replaced by a **restaurants section**.
> The screen is now a 50/50 split — 2 nearest-ahead **fuel** (TomTom) on top, 2
> **top-rated restaurants ahead** (Foursquare Premium, `rating ≥ 8.6`, cuisine-
> filtered) below. Provider layer generalised (`Station`→`Place`,
> `IFuelProvider`→`IPlaceProvider`). Builds clean; geo tests pass. **Pending:**
> on-device verification, and Foursquare **billing must be active** for ratings
> (credits were purchased — see `docs/foursquare-refund-claim.md` re: a duplicate
> charge). Design spec: `docs/superpowers/specs/2026-06-05-restaurants-section-design.md`.

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

### ✅ App integration: real data on the panel (2026-06-04)
The display is now **in the app**, not just the tool — verified on-device with the live
TomTom self-test (10 real Orlando stations on screen):
- `firmware/esp32-s3/` migrated to the **pioarduino core-3.x platform** (esp_lcd in core).
- `src/ui/ui.cpp` is the real LVGL backend (esp_lcd panel ported from the tool) — replaces
  the serial-log stub. Shows the **top 5 nearest-ahead** as two-line rows (brand +
  **distance in miles** + N/S/E/W on top, full address in grey below; nearest in green),
  the **geo: QR** for the nearest, and the **geekendzone.com logo** top-right
  (`src/ui/gz_logo.c`, generated from the site's `logo-onblack.png`).
- **Wi-Fi + RGB flicker fixed** — see `docs/hardware/display.md` "Wi-Fi + the RGB panel":
  bounce buffer + `WiFi.persistent(false)` + `setSleep(false)` + PCLK 16 MHz. No 120 MHz
  PSRAM (not reachable on the precompiled core).
- Device currently runs the **production** build (GPS-driven; queries on ~3 km movement).
  Build the bench variant with `-DSELFTEST_TOMTOM` to query a fixed Orlando location.

## ▶️ Next session: touch (optional) + polish

1. **Touch (FT5x06, I²C GPIO39/40)** — only needed if we want tap-to-scroll or a tappable
   detail screen. The driving UI deliberately shows the top 5 with no scroll, so this is
   now a nice-to-have, not a blocker. Port Seeed's `touchpad_read` (note `W - x`, `H - y`
   for the 180° mount) and register it as an LVGL pointer `indev`.
2. **Polish ideas:** a "no fuel ahead / searching" state while moving; a low-fuel/buzzer
   path (BUZZ to the RP2040); brand filter from `config.h`.

## 🔑 Before the trip
- **Rotate the TomTom API key** (it's in flash + chat history). `config.h` is git-ignored.
- Confirm the phone hotspot (`GeekSpot`) is 2.4 GHz and stays associated while driving.

## Housekeeping / reminders
- 🔑 **Rotate the TomTom API key** used for testing (it's in the ESP32 flash + chat
  history). `config.h` (real Wi-Fi/key) is git-ignored and never committed.
- Device currently runs: RP2040 = GPS reader; ESP32-S3 = `tools/esp32-esplcd-lvgl/`
  (the clean esp_lcd LVGL build — sharp UI). Reflash the app with
  `cd firmware/esp32-s3 && pio run -e esp32-s3 -t upload` when resuming app work, or the
  display tool with `cd tools/esp32-esplcd-lvgl && pio run -e esplcd-lvgl -t upload`.
- Lesson logged: for finicky hardware, port a known-good vendor reference first instead
  of looping on blind one-variable guesses.
