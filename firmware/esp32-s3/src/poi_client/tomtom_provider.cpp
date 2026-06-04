#include "tomtom_provider.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool TomTomProvider::getNearby(double lat, double lng, std::vector<Station>& out) {
  out.clear();

  // Build the Nearby Search query (see TOMTOM_SEARCH_API.md §2-3).
  char url[256];
  snprintf(url, sizeof(url),
           "https://api.tomtom.com/search/2/nearbySearch/.json"
           "?key=%s&lat=%.6f&lon=%.6f&radius=%d&categorySet=7311&limit=%d&countrySet=US",
           apiKey_.c_str(), lat, lng, radiusM_, limit_);

  WiFiClientSecure client;
  client.setInsecure();  // v1: skip cert validation (see checklist in the API doc)

  HTTPClient https;
  if (!https.begin(client, url)) {
    return false;
  }
  const int code = https.GET();
  if (code != HTTP_CODE_OK) {
    https.end();
    return false;
  }

  // Filter so ArduinoJson only materialises the fields we use — keeps heap low.
  JsonDocument filter;
  JsonObject f = filter["results"].add<JsonObject>();
  f["id"] = true;
  f["dist"] = true;
  f["poi"]["name"] = true;
  f["poi"]["brands"][0]["name"] = true;
  f["address"]["freeformAddress"] = true;
  f["position"]["lat"] = true;
  f["position"]["lon"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filter));
  https.end();
  if (err) {
    return false;
  }

  for (JsonObject r : doc["results"].as<JsonArray>()) {
    Station s;
    // Prefer the brand name; fall back to the POI display name.
    const char* brand = r["poi"]["brands"][0]["name"] | r["poi"]["name"] | "";
    s.brand = brand;
    s.address = (const char*)(r["address"]["freeformAddress"] | "");
    s.lat = r["position"]["lat"] | 0.0;
    s.lng = r["position"]["lon"] | 0.0;
    s.providerId = (const char*)(r["id"] | "");
    s.distanceM = r["dist"] | 0.0;  // TomTom precomputes radial distance
    out.push_back(s);
  }
  return true;
}
