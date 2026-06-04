# Hardware documentation

Notes captured while resolving the open hardware unknowns from `CLAUDE.md`
(Pending #1 / #2). Start here.

- [`sensecap-indicator-d1l.md`](./sensecap-indicator-d1l.md) — the device, its two
  MCUs, and the on-device probe results (chip identity, flash, serial ports).
- [`grove-ports.md`](./grove-ports.md) — the two Grove connectors, their pinout, and
  the fact that **both go to the RP2040, not the ESP32-S3**.
- [`gps-air530z-wiring.md`](./gps-air530z-wiring.md) — the owned Air530Z GPS, and the
  **chosen wiring (Option A)**: plug into `Grove(IIC)`, read on the RP2040.
- [`display.md`](./display.md) — ST7701S RGB panel + FT5x06 touch pin map, the
  PCA9535 CS/RST wrinkle, and the LVGL 8.x decision (Pending #4/#5).

External manuals/datasheets: [`../references.md`](../references.md).

## TL;DR

- Plug the GPS into the **`Grove(IIC)`** socket = **the RIGHT one with the device
  back facing you** (the sockets are not labeled on the unit). The LEFT socket is
  `Grove(ADC)` and won't work. Straight Grove cable, no soldering. *(Confirmed
  empirically — see `tools/rp2040-gps-probe/`.)*
- The GPS is a **UART/NMEA @ 9600** module and lands on the **RP2040's UART1**
  (GPIO20/21 = `Serial2` in arduino-pico, **not** `Serial1`). The RP2040 must enable
  port power on **GPIO18 (active-high)** and forward NMEA to the ESP32-S3 over the
  inter-processor UART.
