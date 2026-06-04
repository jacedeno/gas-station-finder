// ESP32-S3 inter-processor link probe — SenseCAP Indicator
//
// Confirms which ESP32-S3 GPIO receives the RP2040's "FIX,..." stream. ESPHome
// places the link on GPIO19/20 but does not say which is RX. This listens on each
// candidate RX pin for a few seconds and reports where FIX lines arrive.
//
// `Serial` (UART0, GPIO43/44) is the CH340 USB console -> /dev/ttyUSB0.
// We probe with UART1 (`Link`) remapped to each candidate pin; RX-only (TX = -1).
//
// Prereq: the RP2040 must be running firmware/rp2040 (emitting FIX at ~1 Hz).

#include <Arduino.h>

static const int kCandidates[] = {20, 19};  // ESPHome: link is on GPIO19/20
static HardwareSerial Link(1);

static void testPin(int rx) {
  Link.begin(115200, SERIAL_8N1, rx, /*tx=*/-1);  // receive-only
  unsigned long t0 = millis();
  size_t bytes = 0;
  int fixLines = 0;
  String line;
  while (millis() - t0 < 4000) {
    while (Link.available()) {
      char c = (char)Link.read();
      bytes++;
      if (c == '\n' || c == '\r') {
        if (line.startsWith("FIX,")) fixLines++;
        line = "";
      } else if (line.length() < 120) {
        line += c;
      }
    }
  }
  Link.end();
  Serial.printf("[probe] RX=GPIO%-2d : %4u bytes, %d FIX line(s)  %s\n", rx,
                (unsigned)bytes, fixLines,
                fixLines > 0 ? "<<< THIS is the link RX pin" : "");
}

void setup() {
  Serial.begin(115200);
  delay(600);
  Serial.println("\n=== ESP32-S3 inter-proc link probe ===");
  Serial.println("RP2040 should be emitting FIX lines. Testing candidate RX pins...");
}

void loop() {
  for (int rx : kCandidates) testPin(rx);
  Serial.println("--- cycle done ---\n");
}
