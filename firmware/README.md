# Firmware

Two firmwares, one per MCU on the SenseCAP Indicator (see `docs/hardware/`).

```
Air530Z GPS ──UART1 9600── RP2040 ──UART0 115200 (FIX lines)── ESP32-S3 ──HTTPS── TomTom
            (Grove(IIC))   firmware/rp2040          firmware/esp32-s3
```

| Project | MCU | Role |
|---------|-----|------|
| [`rp2040/`](./rp2040/) | RP2040 | Reads the GPS on UART1, parses NMEA, forwards clean fixes to the ESP32-S3. |
| [`esp32-s3/`](./esp32-s3/) | ESP32-S3 | App: Wi-Fi + TomTom POI query, on-device geometry/heading filter, UI (LVGL, stubbed), alerts. |

The link between them is a simple ASCII line protocol: [`PROTOCOL.md`](./PROTOCOL.md).

There is also a throwaway diagnostic in [`../tools/rp2040-gps-probe/`](../tools/rp2040-gps-probe/)
that was used to confirm the GPS wiring.

## Build / flash

Each project is a standalone PlatformIO project:

```bash
# RP2040 GPS reader
cd firmware/rp2040 && pio run -e rp2040 -t upload     # auto-reset if arduino-pico already running,
                                                      # else enter BOOTSEL first (see ../../tools/rp2040-gps-probe/README.md)

# ESP32-S3 app
cd firmware/esp32-s3
cp src/config/config.example.h src/config/config.h    # then fill in Wi-Fi + TomTom key (config.h is git-ignored)
pio run -e esp32-s3 -t upload
```

## Status

- **RP2040 reader** — built and verified on-device (emits `FIX,...` at ~1 Hz).
- **ESP32-S3 app** — geometry, provider abstraction (`IFuelProvider` + `TomTomProvider`),
  link parser and main pipeline implemented and compiling. **Deferred:** the LVGL UI
  and RGB-panel driver (CLAUDE.md Pending #4/#5) — `ui/` logs to serial for now; and
  confirming the ESP32-side link TX/RX pins (`PIN_LINK_RX/TX` in `config.example.h`).
