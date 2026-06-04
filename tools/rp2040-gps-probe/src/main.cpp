// RP2040 GPS probe — SenseCAP Indicator
//
// The Indicator's Grove(IIC) port is wired to RP2040 GPIO20/21, which are also the
// RP2040's hardware UART1 (TX=GPIO20, RX=GPIO21). A Grove Air530Z GPS plugged into
// Grove(IIC) with a straight cable lands its TX on GPIO21 (UART1 RX).
//
// In the earlephilhower arduino-pico core the UART instances are:
//   Serial  = USB CDC      (our debug console -> /dev/ttyACM0)
//   Serial1 = UART0
//   Serial2 = UART1        <-- this is the one on GPIO20/21
//
// This sketch powers Grove(IIC) (GPIO18 active-high) and echoes whatever arrives on
// UART1 (Serial2) to USB. NMEA ($GxRMC/$GxGGA) => GPS is in this port. Silence =>
// GPS is in the OTHER (ADC) Grove port; move it and reset.

#include <Arduino.h>

static const uint8_t GROVE_IIC_PWR = 18;    // Grove(IIC) power switch, active-high
static const uint8_t UART1_RX_GPS_TX = 21;  // RP2040 UART1 RX  <- GPS TX
static const uint8_t UART1_TX_GPS_RX = 20;  // RP2040 UART1 TX  -> GPS RX

static bool sawData = false;
static unsigned long lastBeat = 0;

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) { /* wait for USB CDC host */ }

  // Banner FIRST, so we always get output even if UART init misbehaves.
  Serial.println();
  Serial.println("=== RP2040 GPS probe (UART1/Serial2) ===");
  Serial.println("Grove(IIC) powered (GPIO18 HIGH). Reading UART1 RX=GPIO21 @ 9600.");
  Serial.println("Expect $GxRMC / $GxGGA if the GPS is in THIS port.");
  Serial.println("No data in ~10 s -> GPS is in the OTHER Grove port; move it.");
  Serial.println("------------------------------------------------------------");

  pinMode(GROVE_IIC_PWR, OUTPUT);
  digitalWrite(GROVE_IIC_PWR, HIGH);  // power the Grove(IIC) port

  Serial2.setRX(UART1_RX_GPS_TX);
  Serial2.setTX(UART1_TX_GPS_RX);
  Serial2.begin(9600);                // UART1 on GPIO20/21
}

void loop() {
  while (Serial2.available()) {
    Serial.write(Serial2.read());
    sawData = true;
  }
  if (millis() - lastBeat > 2000) {
    lastBeat = millis();
    if (!sawData) Serial.println("[probe] no UART1 data yet on this port...");
  }
}
