// LVGL UI — split screen: nearest fuel (top) + top-rated restaurants ahead
// (bottom), on the real ST7701S 480x480 RGB panel via esp_lcd. The per-station QR
// was removed in favour of the restaurants section (see the design spec).
#pragma once

#include <vector>

#include "../poi_client/place_provider.h"

namespace ui {

// Initialise the display/LVGL stack. Returns false on failure.
bool begin();

// Update the status line under the header (e.g. "Online - waiting for GPS").
// Lets the app reflect real Wi-Fi/GPS state instead of a stale "Connecting...".
void setStatus(const char* msg);

// Render the whole screen: up to 2 nearest-ahead fuel stations on top, up to 2
// nearest-ahead qualifying restaurants on the bottom. Either list may be empty.
void showScreen(const std::vector<Place>& fuel, const std::vector<Place>& food);

// Pump LVGL timers; call from loop().
void tick();

}  // namespace ui
