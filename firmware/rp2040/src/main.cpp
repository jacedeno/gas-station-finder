// SenseCAP Indicator — RP2040 GPS reader / forwarder
//
// Role (Option A, see docs/hardware/): the Grove ports are wired to the RP2040, so
// the RP2040 owns the GPS. It parses NMEA from the Air530Z and forwards clean fixes
// to the ESP32-S3, which runs the rest of the app.
//
// arduino-pico UART map on this board (variant seeed_indicator_rp2040):
//   Serial  = USB CDC                         debug console (/dev/ttyACM0)
//   Serial1 = UART0, GPIO16 TX / GPIO17 RX    link to the ESP32-S3
//   Serial2 = UART1, remapped to GPIO20/21    the GPS on Grove(IIC)
//
// Wire protocol to the ESP32-S3: see firmware/PROTOCOL.md ("FIX,..." lines).

#include <Arduino.h>
#include <TinyGPSPlus.h>

static const uint8_t GROVE_IIC_PWR = 18;    // Grove(IIC) power switch, active-high
static const uint8_t GPS_RX = 21;           // UART1 RX <- GPS TX
static const uint8_t GPS_TX = 20;           // UART1 TX -> GPS RX
static const uint32_t GPS_BAUD = 9600;      // Air530Z default
static const uint32_t LINK_BAUD = 115200;   // RP2040 <-> ESP32-S3

#define GPS   Serial2   // UART1 on GPIO20/21
#define LINK  Serial1   // UART0 on GPIO16/17 -> ESP32-S3

static TinyGPSPlus gps;
static unsigned long lastEmit = 0;
static const unsigned long EMIT_PERIOD_MS = 1000;  // forward at ~1 Hz

// Build and send one "FIX,..." line on both the link and the USB console.
static void emitFix() {
  char line[96];
  const bool valid = gps.location.isValid();
  // Course/speed are only meaningful while moving; send what we have.
  const double lat = valid ? gps.location.lat() : 0.0;
  const double lng = valid ? gps.location.lng() : 0.0;
  const double cog = gps.course.isValid() ? gps.course.deg() : 0.0;
  const double kmh = gps.speed.isValid() ? gps.speed.kmph() : 0.0;
  const int sats = gps.satellites.isValid() ? gps.satellites.value() : 0;

  // FIX,<valid>,<lat>,<lng>,<cog>,<speed_kmh>,<sats>
  snprintf(line, sizeof(line), "FIX,%d,%.6f,%.6f,%.2f,%.1f,%d",
           valid ? 1 : 0, lat, lng, cog, kmh, sats);
  LINK.println(line);
  Serial.println(line);   // mirror for debugging
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) { /* wait briefly for USB host */ }

  pinMode(GROVE_IIC_PWR, OUTPUT);
  digitalWrite(GROVE_IIC_PWR, HIGH);  // power the Grove(IIC) port (confirmed)

  LINK.begin(LINK_BAUD);              // UART0 -> ESP32-S3 (default GPIO16/17)

  GPS.setRX(GPS_RX);
  GPS.setTX(GPS_TX);
  GPS.begin(GPS_BAUD);               // UART1 on GPIO20/21

  Serial.println();
  Serial.println("# RP2040 GPS reader up. GPS=UART1@9600 (GPIO20/21), LINK=UART0@115200 (GPIO16/17).");
}

void loop() {
  // Feed every GPS byte into the parser.
  while (GPS.available()) {
    gps.encode(GPS.read());
  }

  // Forward a fix line at a steady ~1 Hz (whether or not we have a lock, so the
  // ESP32 can show "searching" vs "fix").
  if (millis() - lastEmit >= EMIT_PERIOD_MS) {
    lastEmit = millis();
    emitFix();
  }
}
