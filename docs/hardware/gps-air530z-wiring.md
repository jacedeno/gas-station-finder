# GPS wiring — Grove – GPS (Air530Z) → SenseCAP Indicator

The owned module is a **Luat Air530Z** on a **Grove – GPS (Air530z) v1.1** carrier.

## Module facts

| Property | Value |
|----------|-------|
| GNSS chip | Luat Air530Z (GPS + BeiDou + Galileo + QZSS) |
| Interface | **UART**, NMEA 0183 |
| Default baud | **9600** |
| Supply | 3.3 V / 5 V tolerant |
| Grove pinout | pin1 (yellow) = **module TX** (NMEA out), pin2 (white) = **module RX** (config in), pin3 (red) = VCC, pin4 (black) = GND ¹ |

¹ Derived from the Seeed wiki direct-connection mapping (D2/yellow = host RX ← module
TX; D3/white = host TX → module RX), i.e. standard Grove-UART convention.

## Decision: Option A — plug into `Grove(IIC)`, read on the RP2040

**Plug the GPS into the `Grove(IIC)` port**, with a normal 4-pin Grove cable.
**No soldering, no crossover.**

> **Which physical socket (confirmed empirically 2026-06-03):** the two back Grove
> sockets are *not* silkscreen-labeled on the unit. With the **back of the device
> facing you, the `Grove(IIC)` port is the one on the RIGHT**; the LEFT one is
> `Grove(ADC)` and does **not** work for the GPS. Verified by flashing an RP2040
> probe (`tools/rp2040-gps-probe/`): NMEA (`$GNRMC`, `$GPGSV`/`$BDGSV`,
> `$GPTXT,...,ANTENNA OK`) streamed only when the GPS was in the right-hand socket.
> This also confirmed the **GPIO18 active-high power switch** (the GPS powered up
> and streamed once the probe drove GPIO18 high).

### Pin-by-pin

| Grove pin | Cable | Air530Z | `Grove(IIC)` → RP2040 | RP2040 function | Result |
|-----------|-------|---------|-----------------------|-----------------|--------|
| 1 | yellow | **TX** (NMEA out) | GPIO21 (SCL) | **UART1 RX** | ✅ GPS speaks → RP2040 listens |
| 2 | white | **RX** (config in) | GPIO20 (SDA) | **UART1 TX** | ✅ RP2040 can send commands |
| 3 | red | VCC | VCC (3.3 V) | — | ✅ Air530Z accepts 3.3 V |
| 4 | black | GND | GND | — | ✅ |

The GPS data output (TX) lands on GPIO21 = RP2040 UART1 RX with a straight cable,
so the mapping is correct as-is.

### Firmware to-do on the RP2040

> In the **earlephilhower arduino-pico** core, UART1 is exposed as **`Serial2`**
> (`Serial1` is UART0 and CANNOT use GPIO20/21). Use `Serial2.setRX(21);
> Serial2.setTX(20); Serial2.begin(9600);`. See the working probe in
> `tools/rp2040-gps-probe/src/main.cpp`.

1. **Drive GPIO18 high** to power the `Grove(IIC)` port (active-high power switch —
   confirmed). Without this the GPS never powers on.
2. **Open UART1 = `Serial2` (GPIO20 TX / GPIO21 RX) at 9600 baud** and read NMEA
   (need **RMC** for course-over-ground; the Air530Z emits `$GNRMC`).
3. Forward parsed position + CoG (or raw NMEA) to the **ESP32-S3 over the
   inter-processor UART** (ESP32-S3 GPIO19/20). The ESP32-S3 app runs the rest of
   the pipeline (geometry, TomTom, LVGL, buzzer).

### Why not the alternatives

- **`Grove(ADC)` port** — GPIO26/27 are ADC-only, not UART-capable. Unusable for NMEA.
- **I²C GPS** — the Air530Z is UART-only; no I²C/DDC mode to exploit.
- **Solder GPS UART straight to ESP32-S3** — would keep a single firmware and the
  original architecture, but the user has ruled out soldering. Rejected.

## Sources

See [`docs/references.md`](../references.md).
