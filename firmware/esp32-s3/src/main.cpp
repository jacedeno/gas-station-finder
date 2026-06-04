// SenseCAP Indicator — ESP32-S3 main app (Indicator Fuel Finder).
//
// Pipeline: receive GPS fixes from the RP2040 over the inter-proc UART -> on a
// movement threshold, query TomTom for nearby fuel -> compute bearing + heading
// filter locally -> render (UI is a serial-log stub until the panel driver is
// confirmed). See CLAUDE.md for the full design.

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
#include "poi_client/tomtom_provider.h"
#include "ui/ui.h"

static GpsLink gpsLink;
static TomTomProvider provider(TOMTOM_API_KEY, TOMTOM_RADIUS_M, TOMTOM_LIMIT);

// Where we last ran a query, to enforce the movement threshold.
static bool haveQueried = false;
static double lastQueryLat = 0, lastQueryLng = 0;
static unsigned long lastQueryMs = 0;

static void runQuery(const GpsFix& fix) {
  std::vector<Station> stations;
  if (!provider.getNearby(fix.lat, fix.lng, stations)) {
    Serial.println("[app] provider query failed.");
    return;
  }

  // Bearing is computed locally; distance comes from the provider (`dist`).
  for (Station& s : stations) {
    s.bearingDeg = geo::bearingDeg(fix.lat, fix.lng, s.lat, s.lng);
  }

  // Heading filter only when moving fast enough that course is trustworthy.
  if (fix.speedKmh >= HEADING_FILTER_MIN_KMH) {
    stations.erase(
        std::remove_if(stations.begin(), stations.end(),
                       [&](const Station& s) {
                         return !geo::isAhead(s.bearingDeg, fix.courseDeg,
                                              HEADING_TOLERANCE_DEG);
                       }),
        stations.end());
  }

  std::sort(stations.begin(), stations.end(),
            [](const Station& a, const Station& b) {
              return a.distanceM < b.distanceM;
            });

  ui::showStations(stations);
  if (!stations.empty() && alerts::shouldAlert(stations.front(), 500.0)) {
    Serial.println("[app] within range -> would buzz (TODO: send BUZZ to RP2040).");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n# Indicator Fuel Finder (ESP32-S3) starting.");

  ui::begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[app] connecting to Wi-Fi \"%s\"...\n", WIFI_SSID);

  Serial1.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  gpsLink.begin(Serial1);

  Serial.printf("[app] provider: %s\n", provider.name());
}

void loop() {
  ui::tick();

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
