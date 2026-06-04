// Copy this file to `config.h` (which is git-ignored) and fill in real values.
// Never commit secrets — see CLAUDE.md §10.
#pragma once

// ---- Wi-Fi (phone hotspot, 2.4 GHz only — the device has no 5 GHz radio) ----
#define WIFI_SSID "your-hotspot-ssid"
#define WIFI_PASS "your-hotspot-password"

// ---- TomTom Search API ----
// Scope the key to Search API only and set a spend cap (the key ships in flash).
#define TOMTOM_API_KEY "your-tomtom-search-api-key"
#define TOMTOM_RADIUS_M 10000  // search radius in metres
#define TOMTOM_LIMIT 10        // max results (keep small for memory)

// ---- Behaviour thresholds (CLAUDE.md §5) ----
#define MOVEMENT_THRESHOLD_M 3000.0   // re-query after moving this far
#define HEADING_TOLERANCE_DEG 70.0    // "ahead" = within +/- this of course
#define HEADING_FILTER_MIN_KMH 15.0   // below this, CoG is noisy -> radial-nearest
#define MIN_QUERY_INTERVAL_MS 15000   // never hit the API faster than this

// ---- Inter-processor UART to the RP2040 (see firmware/PROTOCOL.md) ----
// ESPHome places the link on ESP32-S3 GPIO19/20; exact TX/RX split unconfirmed.
// If no FIX lines arrive, swap these two.
#define PIN_LINK_RX 20
#define PIN_LINK_TX 19
#define LINK_BAUD 115200

// ---- Optional brand filter (empty = all brands) ----
// e.g. "Shell,Chevron" — passed to TomTom as brandSet (URL-encode if used).
#define BRAND_FILTER ""
