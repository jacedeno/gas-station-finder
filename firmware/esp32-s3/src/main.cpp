// SenseCAP Indicator — ESP32-S3 main app (Indicator Fuel Finder).
//
// Pipeline: receive GPS fixes from the RP2040 over the inter-proc UART -> on a
// movement threshold, query TomTom (fuel) and Foursquare (top-rated restaurants)
// for nearby places -> compute bearing + heading filter locally -> render the split
// screen (nearest fuel on top, restaurants below). See CLAUDE.md for the full design.

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>
#include <vector>

#if __has_include("config/config.h")
#include "config/config.h"
#else
#include "config/config.example.h"  // builds out-of-the-box with placeholders
#endif

#include "alerts/alerts.h"
#include "geo/geo.h"
#include "gps/gps_link.h"
#include "poi_client/foursquare_provider.h"
#include "poi_client/tomtom_provider.h"
#include "ui/ui.h"

static GpsLink gpsLink;
static TomTomProvider fuelProvider(TOMTOM_API_KEY, TOMTOM_RADIUS_M, TOMTOM_LIMIT);
static FoursquareProvider foodProvider(FSQ_API_KEY, FSQ_API_VERSION, FSQ_HOST,
                                       FSQ_CATEGORIES, FSQ_RADIUS_M, FSQ_LIMIT,
                                       FSQ_MIN_RATING);

// Where we last ran a query, to enforce the movement threshold.
static bool haveQueried = false;
static double lastQueryLat = 0, lastQueryLng = 0;
static unsigned long lastQueryMs = 0;

// Compute bearings, apply the "ahead" heading filter (only when moving fast enough
// that course is trustworthy — below that, course is noisy so we keep radial-nearest),
// then sort nearest-first. Shared by both the fuel and food lists.
static void filterAndRank(std::vector<Place>& v, const GpsFix& fix) {
  for (Place& p : v) {
    p.bearingDeg = geo::bearingDeg(fix.lat, fix.lng, p.lat, p.lng);
  }
  if (fix.speedKmh >= HEADING_FILTER_MIN_KMH) {
    v.erase(std::remove_if(v.begin(), v.end(),
                           [&](const Place& p) {
                             return !geo::isAhead(p.bearingDeg, fix.courseDeg,
                                                  HEADING_TOLERANCE_DEG);
                           }),
            v.end());
  }
  std::sort(v.begin(), v.end(),
            [](const Place& a, const Place& b) { return a.distanceM < b.distanceM; });
}

// HTTPS queries can flake on the first call right after Wi-Fi associates (the
// hotspot association is rocky -> the second back-to-back TLS handshake sometimes
// fails). Retry a few times with a short settle delay before giving up.
static bool queryWithRetry(IPlaceProvider& p, const GpsFix& fix,
                           std::vector<Place>& out) {
  for (int attempt = 1; attempt <= 3; ++attempt) {
    if (p.getNearby(fix.lat, fix.lng, out)) return true;
    Serial.printf("[app] %s query attempt %d failed\n", p.name(), attempt);
    delay(400);
  }
  return false;
}

static void runQuery(const GpsFix& fix) {
  // Distance comes from each provider; bearing + filtering are computed locally.
  std::vector<Place> fuel, food;
  if (!queryWithRetry(fuelProvider, fix, fuel)) {
    Serial.println("[app] fuel provider query failed.");
  }
  // Food is best-effort: a persistent failure (e.g. 429 before billing) just leaves
  // the restaurants section empty — the fuel half still works.
  if (!queryWithRetry(foodProvider, fix, food)) {
    Serial.println("[app] food provider query failed -> empty restaurants section.");
  }

  filterAndRank(fuel, fix);
  filterAndRank(food, fix);

  ui::showScreen(fuel, food);
  if (!fuel.empty() && alerts::shouldAlert(fuel.front(), 500.0)) {
    Serial.println("[app] within range -> would buzz (TODO: send BUZZ to RP2040).");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n# Indicator Fuel Finder (ESP32-S3) starting.");

  ui::begin();

  // The RGB panel streams its framebuffer from PSRAM, which shares the SPI bus with
  // flash. WiFi writing creds to NVS (flash) locks that bus and stalls the panel's
  // bounce-buffer refill -> visible flicker. persistent(false) stops those NVS writes;
  // setSleep(false) keeps the modem steady (no periodic wake bursts). See
  // docs/hardware/display.md "WiFi + RGB panel".
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[app] connecting to Wi-Fi \"%s\"...\n", WIFI_SSID);

  Serial1.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  gpsLink.begin(Serial1);

  Serial.printf("[app] providers: %s (fuel), %s (food)\n", fuelProvider.name(),
                foodProvider.name());
}

// Reflect the real Wi-Fi/GPS state on screen + serial once a second, so the panel
// never sits on a stale "Connecting..." (the station list only appears once we have a
// Wi-Fi link + a GPS fix; the first query then runs immediately, no movement needed).
static void reportState() {
  static unsigned long lastMs = 0;
  if (millis() - lastMs < 1000) return;
  lastMs = millis();

  const bool wifi = WiFi.status() == WL_CONNECTED;
  const GpsFix& f = gpsLink.fix();
  char st[72];
  if (!wifi) {
    snprintf(st, sizeof(st), "Connecting to %s...", WIFI_SSID);
  } else if (!f.valid) {
    snprintf(st, sizeof(st), "Online - waiting for GPS (%d sats)", f.sats);
  } else if (!haveQueried) {
    snprintf(st, sizeof(st), "GPS ok - searching ahead...");
  } else {
    st[0] = '\0';  // a query has run; leave the list's own status alone
  }
  if (st[0]) ui::setStatus(st);
  Serial.printf("[state] wifi=%d ip=%s fix=%d sats=%d%s\n", wifi,
                wifi ? WiFi.localIP().toString().c_str() : "-", f.valid, f.sats,
                haveQueried ? " (queried)" : "");
}

void loop() {
  ui::tick();
  reportState();

#ifdef SELFTEST_TOMTOM
  // One-shot network self-test: once Wi-Fi is up, query a fixed location so the
  // full Wi-Fi + TLS + TomTom + JSON + geo path can be validated without a GPS
  // fix. Build with -DSELFTEST_TOMTOM. Orlando, FL.
  static bool selftestDone = false;
  if (!selftestDone && WiFi.status() == WL_CONNECTED) {
    selftestDone = true;
    Serial.println("[selftest] Wi-Fi up; running a fixed-location TomTom query...");
    GpsFix t;
    t.valid = true;
    t.lat = 28.5384;
    t.lng = -81.3789;
    t.speedKmh = 0;  // stationary -> radial nearest (heading filter skipped)
    runQuery(t);
  }
#endif

  if (!gpsLink.poll()) {
    return;  // no new fix this iteration
  }
  const GpsFix& fix = gpsLink.fix();
  if (!fix.valid) {
    return;  // searching for satellites
  }

  const bool intervalOk = millis() - lastQueryMs >= MIN_QUERY_INTERVAL_MS;
  const bool movedEnough =
      !haveQueried || geo::haversineMeters(lastQueryLat, lastQueryLng, fix.lat,
                                           fix.lng) >= MOVEMENT_THRESHOLD_M;

  if (intervalOk && movedEnough && WiFi.status() == WL_CONNECTED) {
    runQuery(fix);
    haveQueried = true;
    lastQueryLat = fix.lat;
    lastQueryLng = fix.lng;
    lastQueryMs = millis();
  }
}
