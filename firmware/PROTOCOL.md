# Inter-processor link protocol (RP2040 → ESP32-S3)

The RP2040 reads the GPS and forwards clean fixes to the ESP32-S3 over the
dedicated inter-processor UART. We define our own simple ASCII line protocol
(both firmwares are ours; we do not need Seeed's COBS framing).

## Physical link

| Side | UART | TX | RX |
|------|------|----|----|
| RP2040 | UART0 (`Serial1`) | GPIO16 | GPIO17 |
| ESP32-S3 | UART1 (`Serial1`) | GPIO19/20 ¹ | GPIO19/20 ¹ |

Cross-wired: RP2040 TX(16) → ESP32 RX, ESP32 TX → RP2040 RX(17).
Baud: **115200**.

¹ ESPHome places the link on ESP32-S3 GPIO19/20; the exact TX/RX split is not yet
confirmed against the schematic. The ESP32 firmware keeps these as named constants
(`PIN_LINK_RX` / `PIN_LINK_TX`) — swap them if no data arrives. See Pending in
`CLAUDE.md`.

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
