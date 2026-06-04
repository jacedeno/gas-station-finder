# SenseCAP Indicator D1L — hardware notes

Seeed SKU `114993532`. Dual-MCU IoT touchscreen development platform. This is the
target device for the Indicator Fuel Finder firmware.

## Processors

| MCU | Role | Notes |
|-----|------|-------|
| **ESP32-S3** | Primary. Display, touch, Wi-Fi/BLE/LoRa, app logic | The MCU we flash with the fuel-finder app. |
| **RP2040** | Co-processor. Sensors, buzzer, microSD, **both Grove ports** | Talks to the ESP32-S3 over a private inter-processor UART. |

The two MCUs are linked by a dedicated UART: **RP2040 ↔ ESP32-S3 on ESP32-S3
GPIO19 / GPIO20**.

## On-device probe (2026-06-03, this dev machine)

Both MCUs enumerate over a single USB-C cable through an internal USB hub
(Terminus `1a40:0101`):

| Linux device | MCU | USB ID | Serial bridge |
|--------------|-----|--------|---------------|
| `/dev/ttyUSB0` | ESP32-S3 | `1a86:7523` | CH340 |
| `/dev/ttyACM0` | RP2040 | `2886:0050` | native USB (SN `4250305031363913`) |

`esptool flash_id` on `/dev/ttyUSB0` (non-destructive) reported:

- ESP32-S3 (QFN56) **rev v0.2** — Wi-Fi + BLE
- **8 MB embedded PSRAM** (3.3 V)
- **8 MB flash** — Winbond `ef:4017` (W25Q64), quad SPI, 3.3 V
- Crystal 40 MHz
- MAC `d8:3b:da:75:74:78`

The dev user is in the `dialout` group, so both ports open without `sudo`.
PlatformIO is installed at `~/.platformio`.

## Other onboard hardware (per spec)

- Display: 3.95" RGB capacitive touch, 480 × 480
- Radios: SX1262 LoRa (862–930 MHz), Wi-Fi 2.4 GHz only (no 5 GHz), BLE 5.0
- Buzzer: MLT-8530
- microSD up to 32 GB (not included)
- Power: USB-C 5 V / 1 A — **no battery, no GPS, no cellular onboard**

## Flashing safety

Meshtastic lives on the ESP32-S3 as the factory app and **must stay recoverable**.
`esptool flash_id` only resets + reads identity; it does not erase flash. Any real
flashing of fuel-finder firmware must target the ESP32-S3 app partition without
wiping the Meshtastic partition table unintentionally.

## Sources

See [`docs/references.md`](../references.md).
