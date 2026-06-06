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

// ---- Foursquare Places API (restaurants) ----
// Service API Key (Bearer). Premium tier needed for the `rating` field -> enable
// billing on Foursquare, set a spend cap, never commit, rotate. (Auth/endpoint
// confirmed live; see docs/superpowers/specs/2026-06-05-restaurants-section-design.md.)
#define FSQ_API_KEY "your-foursquare-service-api-key"
#define FSQ_API_VERSION "2025-06-17"  // required X-Places-Api-Version header
#define FSQ_HOST "places-api.foursquare.com"
#define FSQ_RADIUS_M 10000        // search radius in metres
#define FSQ_LIMIT 20              // raw results fetched before rating filter
#define FSQ_MIN_RATING 8.6        // keep restaurants rated >= this (0-10, ~4.3/5)
// Cuisine category IDs to include (only these come back, so fast-food/Mexican are
// excluded by omission). Order: Greek, Mediterranean, Peruvian, Steakhouse,
// American, New American, Turkish, Meze, Italian, Salad, BBQ.
#define FSQ_CATEGORIES \
  "4bf58dd8d48988d10e941735,4bf58dd8d48988d1c0941735,4eb1bfa43b7b52c0e1adc2e8," \
  "4bf58dd8d48988d1cc941735,4bf58dd8d48988d14e941735,4bf58dd8d48988d157941735," \
  "4f04af1f2fb6e1c99f3db0bb,53d6c1b0e4b02351e88a83da,4bf58dd8d48988d110941735," \
  "4bf58dd8d48988d1bd941735,4bf58dd8d48988d1df931735"

// ---- Behaviour thresholds (CLAUDE.md §5) ----
#define MOVEMENT_THRESHOLD_M 3000.0   // re-query after moving this far
#define HEADING_TOLERANCE_DEG 70.0    // "ahead" = within +/- this of course
#define HEADING_FILTER_MIN_KMH 15.0   // below this, CoG is noisy -> radial-nearest
#define MIN_QUERY_INTERVAL_MS 15000   // never hit the API faster than this

// ---- Inter-processor UART to the RP2040 (see firmware/PROTOCOL.md) ----
// Confirmed on-device 2026-06-03 (tools/esp32-link-probe): RX = GPIO20, TX = GPIO19.
#define PIN_LINK_RX 20
#define PIN_LINK_TX 19
#define LINK_BAUD 115200

// ---- Optional brand filter (empty = all brands) ----
// e.g. "Shell,Chevron" — passed to TomTom as brandSet (URL-encode if used).
#define BRAND_FILTER ""
