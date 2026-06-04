#include "ui.h"

#include <Arduino.h>

// STUB implementation: logs to serial so the end-to-end pipeline is testable
// before the LVGL/display backend exists. Replace with real LVGL drawing once the
// Indicator RGB panel driver is confirmed (CLAUDE.md Pending #4/#5).
namespace ui {

bool begin() {
  Serial.println("[ui] STUB: display/LVGL not yet implemented (pending panel driver).");
  return false;
}

void showStations(const std::vector<Station>& stations) {
  Serial.printf("[ui] %u station(s):\n", (unsigned)stations.size());
  for (size_t i = 0; i < stations.size(); ++i) {
    const Station& s = stations[i];
    Serial.printf("  %u. %-12s %6.0f m  brg %3.0f  %s\n", (unsigned)(i + 1),
                  s.brand.c_str(), s.distanceM, s.bearingDeg, s.address.c_str());
  }
}

void showDetail(const Station& s) {
  Serial.printf("[ui] detail: %s | %s | geo:%.6f,%.6f\n", s.brand.c_str(),
                s.address.c_str(), s.lat, s.lng);
}

void tick() { /* no-op until LVGL exists */ }

}  // namespace ui
