# Inter-processor link protocol (RP2040 → ESP32-S3)

The RP2040 reads the GPS and forwards clean fixes to the ESP32-S3 over the
dedicated inter-processor UART. We define our own simple ASCII line protocol
(both firmwares are ours; we do not need Seeed's COBS framing).

## Physical link

| Side | UART | TX | RX |
|------|------|----|----|
| RP2040 | UART0 (`Serial1`) | GPIO16 | GPIO17 |
| ESP32-S3 | UART1 (`Serial1`) | **GPIO19** | **GPIO20** |

Cross-wired: RP2040 TX(16) → ESP32 RX(20); ESP32 TX(19) → RP2040 RX(17).
Baud: **115200**.

> **Confirmed empirically 2026-06-03** with `tools/esp32-link-probe/`: the RP2040's
> `FIX` lines arrive on ESP32-S3 **GPIO20** (GPIO19 receives nothing). So the ESP32
> link RX = GPIO20, TX = GPIO19. The full GPS → RP2040 → ESP32 path works end-to-end.

## Line format

One newline-terminated ASCII line per GPS update (~1 Hz):

```
FIX,<valid>,<lat>,<lng>,<cog>,<speed_kmh>,<sats>
```

| Field | Meaning |
|-------|---------|
| `valid` | `1` if the GPS has a position fix, else `0` |
| `lat` / `lng` | decimal degrees, 6 dp (empty when never seen) |
| `cog` | course-over-ground, degrees 0–359.99 (empty when unknown) |
| `speed_kmh` | ground speed in km/h |
| `sats` | satellites used in the fix |

Example: `FIX,1,28.538336,-81.379234,142.30,63.4,9`

No checksum for v1 — the link is short and reliable. Add one if noise appears.
Lines that do not start with `FIX,` (e.g. debug text) must be ignored by the reader.
