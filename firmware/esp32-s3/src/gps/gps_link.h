// Reads the FIX line protocol coming from the RP2040 over the inter-processor
// UART and exposes the latest GPS fix. See firmware/PROTOCOL.md.
#pragma once

#include <Arduino.h>

struct GpsFix {
  bool valid = false;        // GPS has a position lock
  double lat = 0;
  double lng = 0;
  double courseDeg = 0;      // course-over-ground
  double speedKmh = 0;
  int sats = 0;
  unsigned long updatedMs = 0;  // millis() of last successful parse
};

class GpsLink {
 public:
  // `serial` must already be begun at the link baud (see PROTOCOL.md).
  void begin(HardwareSerial& serial) { serial_ = &serial; }

  // Drain the UART; returns true if a new FIX line was parsed this call.
  bool poll();

  const GpsFix& fix() const { return fix_; }

 private:
  bool parseLine(const char* line);

  HardwareSerial* serial_ = nullptr;
  char buf_[128];
  size_t len_ = 0;
  GpsFix fix_;
};
