# External references

Only the sources that actually yielded real, used facts for the hardware notes in
this repo (pinouts, GPS specs, flashing). Pages consulted that gave nothing usable
are intentionally omitted. Captured 2026-06-03.

## Grove ports → RP2040 pinout

- [Seeed SenseCAP Indicator — ESPHome device DB](https://devices.esphome.io/devices/seeed-sensecap/)
  — `Grove(IIC)` = GPIO20 SDA / GPIO21 SCL with **GPIO18 power switch (active-high)**;
  `Grove(ADC)` = GPIO26/GPIO27; inter-processor UART on ESP32-S3 GPIO19/20.
- [Grove IIC — Seeed Wiki](https://wiki.seeedstudio.com/SenseCAP_Indicator_RP2040_Grove_IIC/)
  — confirms `Wire.setSDA(20); Wire.setSCL(21);`.
- [Grove ADC — Seeed Wiki](https://wiki.seeedstudio.com/SenseCAP_Indicator_RP2040_Grove_ADC/)
  — confirms `ADC0 = 26`, `ADC1 = 27`.
- [XOD on the SenseCAP Indicator pt.1 — Hackster](https://www.hackster.io/wayland2/xod-on-the-sensecap-indicator-part-1-b4daea)
  — the RP2040 (not the ESP32-S3) owns the sensors, buzzer, SD card and **both Grove
  connectors**. First confirmation the Grove ports are on the RP2040.
- RP2040 datasheet (GPIO function table) — GPIO20 = UART1 TX, GPIO21 = UART1 RX
  (function F2); the basis for reading the Grove(IIC) pins as a UART.

## Display & touch (ST7701S / FT5x06)

- [Seeed SenseCAP Indicator — ESPHome device DB](https://devices.esphome.io/devices/seeed-sensecap/)
  — full RGB panel pin map (HSYNC/VSYNC/DE/PCLK + R/G/B data), SPI init pins
  (GPIO41/48), CS/RST on the PCA9535 expander, backlight GPIO45, FT5x06 touch on
  GPIO39/40, and the `mipi_rgb` model `SEEED-INDICATOR-D1`.
- [Develop SenseCAP Indicator with Arduino — Seeed Wiki](https://wiki.seeedstudio.com/SenseCAP_Indicator_ESP32_Arduino/)
  — Arduino + LVGL bring-up reference for the ESP32-S3.
- [Arduino_GFX discussion #334 (SenseCAP Indicator SPI/CS)](https://github.com/moononournation/Arduino_GFX/discussions/334)
  — notes that SPI_CS and LCD_RST are routed through the PCA9535 I²C expander.

## Grove – GPS (Air530Z)

- [Grove – GPS (Air530 / Air530Z) — Seeed Wiki](https://wiki.seeedstudio.com/Grove-GPS-Air530/)
  — UART interface, default **9600 baud**, and the Grove pin mapping
  (yellow = pin1 = module TX; white = pin2 = module RX).
- [Air530Z — LuatOS documentation](https://wiki.luatos.org/chips/gnss/air530z.html)
  — the Air530Z GNSS chip (GPS + BeiDou + Galileo + QZSS).

## Flashing the RP2040

- [Update and Flash Firmware — Seeed Wiki](https://wiki.seeedstudio.com/SenseCAP_Indicator_How_To_Flash_The_Default_Firmware/)
  — RP2040 BOOTSEL = long-press the internal pinhole button with a needle, plug in
  USB, release; copy a `.uf2` to the `RPI-RP2` drive.
- [Seeed factory RP2040 firmware `terminal_rp2040_v1.0.0.uf2`](https://github.com/Seeed-Solution/sensecap_indicator_rp2040/releases/download/v1.0.0/terminal_rp2040_v1.0.0.uf2)
  — recovery image to restore the stock RP2040 firmware.

## Verified on-device

Beyond the docs above, the port side (right = IIC), the GPIO18 power switch, the
9600/`$GNRMC` stream and the no-crossover wiring were **confirmed empirically** with
the probe firmware in [`../tools/rp2040-gps-probe/`](../tools/rp2040-gps-probe/).
