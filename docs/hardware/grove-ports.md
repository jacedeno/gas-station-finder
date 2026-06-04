# SenseCAP Indicator — Grove ports

**Key fact that overturns the original architecture:** both Grove connectors on
the back of the Indicator are wired to the **RP2040**, *not* the ESP32-S3. Neither
is a dedicated UART port — one is I²C, the other ADC.

## The two ports (silkscreen-labeled on the back)

| Silkscreen | Type | RP2040 GPIO | Notes |
|------------|------|-------------|-------|
| **`Grove(IIC)`** | I²C | GPIO20 = SDA, GPIO21 = SCL | Power-enable on **GPIO18 (active-high)** — port is unpowered until firmware drives it high. ¹ **Physical: RIGHT-hand socket with the device back facing you.** ² |
| **`Grove(ADC)`** | ADC / configurable analog-digital | GPIO26 = ADC0, GPIO27 = ADC1 | Also usable as digital / PWM. **Physical: LEFT-hand socket** (back facing you). |

¹ GPIO18 active-high power switch — documented in the ESPHome device DB and
**confirmed empirically** (the GPS powered up only when the probe drove GPIO18 high).

² The two sockets are **not silkscreen-labeled on the unit**; the IIC/ADC labels
only exist in Seeed's manual diagram. Side mapping confirmed empirically on
2026-06-03 via `tools/rp2040-gps-probe/` (NMEA appeared only on the right socket).

## Why this matters for the GPS

The owned GPS (Grove – GPS **Air530Z**) is a **UART / NMEA** module — see
[`gps-air530z-wiring.md`](./gps-air530z-wiring.md). There is therefore **no
plug-and-play UART Grove port** on this device. The chosen path (Option A) reuses
the `Grove(IIC)` port because its pins double as the RP2040's hardware **UART1**:

- RP2040 **UART1 TX = GPIO20**, **UART1 RX = GPIO21** (RP2040 GPIO function F2).
- These are exactly the `Grove(IIC)` SDA/SCL pins, so a straight Grove cable lands
  the GPS on UART1 with **no TX/RX crossover** (see wiring doc for the full table).

Consequence: the GPS is read by **RP2040 firmware**, which forwards NMEA to the
ESP32-S3 over the inter-processor UART. This is why the project now includes
custom RP2040 firmware (no soldering required).

## Sources

See [`docs/references.md`](../references.md).
