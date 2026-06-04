// LVGL UI — list view + station detail with a geo: QR code (CLAUDE.md §5).
//
// STUB / TODO: the LVGL display + touch backend depends on the Indicator's RGB
// panel driver and pin map, which are still unverified (CLAUDE.md Pending #4/#5).
// Until that is locked, the app logs results to serial instead of drawing them.
// This header fixes the interface so main.cpp can be wired now and the LVGL
// implementation dropped in later without touching the rest of the firmware.
#pragma once

#include <vector>

#include "../poi_client/fuel_provider.h"

namespace ui {

// Initialise the display/LVGL stack. Returns false until implemented.
bool begin();

// Show the ranked list of stations (brand + address + distance + bearing).
void showStations(const std::vector<Station>& stations);

// Show the detail screen for one station, including the geo:lat,lng QR code.
void showDetail(const Station& station);

// Pump LVGL timers; call from loop(). No-op until implemented.
void tick();

}  // namespace ui
