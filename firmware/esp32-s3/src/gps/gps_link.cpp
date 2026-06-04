#include "gps_link.h"

#include <stdlib.h>
#include <string.h>

bool GpsLink::poll() {
  if (!serial_) return false;
  bool parsedNew = false;

  while (serial_->available()) {
    const char c = (char)serial_->read();
    if (c == '\n' || c == '\r') {
      if (len_ > 0) {
        buf_[len_] = '\0';
        if (parseLine(buf_)) parsedNew = true;
        len_ = 0;
      }
    } else if (len_ < sizeof(buf_) - 1) {
      buf_[len_++] = c;
    } else {
      len_ = 0;  // overflow: drop the malformed line
    }
  }
  return parsedNew;
}

// Expected: FIX,<valid>,<lat>,<lng>,<cog>,<speed_kmh>,<sats>
bool GpsLink::parseLine(const char* line) {
  if (strncmp(line, "FIX,", 4) != 0) return false;  // ignore debug/other lines

  // Tokenise a mutable copy.
  char tmp[128];
  strncpy(tmp, line, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';

  char* save = nullptr;
  char* tok = strtok_r(tmp, ",", &save);  // "FIX"
  if (!tok) return false;

  const char* fields[6] = {nullptr};
  for (int i = 0; i < 6; ++i) {
    fields[i] = strtok_r(nullptr, ",", &save);
    if (!fields[i]) return false;  // malformed: too few fields
  }

  GpsFix f;
  f.valid = atoi(fields[0]) != 0;
  f.lat = atof(fields[1]);
  f.lng = atof(fields[2]);
  f.courseDeg = atof(fields[3]);
  f.speedKmh = atof(fields[4]);
  f.sats = atoi(fields[5]);
  f.updatedMs = millis();
  fix_ = f;
  return true;
}
