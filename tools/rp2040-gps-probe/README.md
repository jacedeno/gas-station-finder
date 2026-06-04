# rp2040-gps-probe

Diagnostic firmware for the RP2040 on the SenseCAP Indicator. It confirms that the
Air530Z GPS is plugged into the correct Grove socket and that the wiring is right,
by reading the GPS on **UART1** and echoing the NMEA stream to USB serial.

It was used to empirically resolve which back socket is `Grove(IIC)` (the right one,
device back facing you) and to confirm the GPIO18 power switch — see
[`../../docs/hardware/gps-air530z-wiring.md`](../../docs/hardware/gps-air530z-wiring.md).

## What it does

- Drives **GPIO18 high** to power the `Grove(IIC)` port (active-high switch).
- Opens **UART1 = `Serial2`** (RX = GPIO21, TX = GPIO20) at **9600 baud**.
- Echoes everything received to the USB CDC console, with a heartbeat when idle.

NMEA lines (`$GNRMC`, `$GPGSV`, …) ⇒ the GPS is in this (IIC) socket. Silence ⇒ it
is in the other (`Grove(ADC)`) socket; UART1 does not exist there.

## Build & flash

```bash
pio run -e probe                 # build
# Enter BOOTSEL: hold the internal pinhole button, plug in USB-C, release.
# The RP2040 mounts as the RPI-RP2 drive; copy the UF2:
cp .pio/build/probe/firmware.uf2 /run/media/$USER/RPI-RP2/
```

Once this (arduino-pico) firmware is running, later flashes can use
`pio run -e probe -t upload` (1200 bps auto-reset). To restore the stock firmware,
flash Seeed's `terminal_rp2040_v1.0.0.uf2` (see `docs/references.md`).

## Read the output

```bash
pio device monitor -p /dev/ttyACM0 -b 115200
# or: stty -F /dev/ttyACM0 115200 raw -echo && cat /dev/ttyACM0
```

> A position fix needs sky view; indoors the RMC stays `V` (void) with 0 satellites,
> but the data stream and `ANTENNA OK` still confirm the wiring.
