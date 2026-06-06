#include "foursquare_provider.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {
// Join up to two category short_names with " . " for the cuisine line, e.g.
// "Steakhouse . American". Skips empties.
String cuisineLabel(JsonArrayConst cats) {
  String out;
  int used = 0;
  for (JsonObjectConst c : cats) {
    const char* s = c["short_name"] | (const char*)nullptr;
    if (!s || !*s) continue;
    if (used) out += " · ";  // middle dot
    out += s;
    if (++used == 2) break;
  }
  return out;
}
}  // namespace

bool FoursquareProvider::getNearby(double lat, double lng, std::vector<Place>& out) {
  out.clear();

  // Build the Search query. The category list is long (~11 hex IDs), so assemble
  // the URL as a String rather than a fixed char buffer.
  String url = "https://" + host_ + "/places/search";
  url += "?ll=";
  url += String(lat, 6) + "," + String(lng, 6);
  url += "&radius=" + String(radiusM_);
  url += "&limit=" + String(limit_);
  url += "&sort=DISTANCE";
  url += "&fsq_category_ids=" + categoryIds_;
  url += "&fields=fsq_place_id,name,location,categories,distance,latitude,longitude,rating";

  WiFiClientSecure client;
  client.setInsecure();  // v1: skip cert validation (matches the TomTom path)

  HTTPClient https;
  https.setTimeout(8000);  // give the TLS handshake + response room (default 5 s)
  if (!https.begin(client, url)) {
    Serial.println("[fsq] begin() failed");
    return false;
  }
  https.addHeader("Authorization", "Bearer " + apiKey_);
  https.addHeader("X-Places-Api-Version", apiVersion_);  // required by the new API
  https.addHeader("accept", "application/json");

  const int code = https.GET();
  if (code != HTTP_CODE_OK) {
    // 429 = no credits / over tier (rating is a Premium field -> needs billing).
    Serial.printf("[fsq] HTTP %d\n", code);
    https.end();
    return false;
  }

  // Filter so ArduinoJson only materialises the fields we use — keeps heap low.
  JsonDocument filter;
  JsonObject f = filter["results"].add<JsonObject>();
  f["fsq_place_id"] = true;
  f["name"] = true;
  f["distance"] = true;
  f["rating"] = true;
  f["latitude"] = true;
  f["longitude"] = true;
  f["location"]["formatted_address"] = true;
  f["location"]["locality"] = true;
  f["location"]["region"] = true;
  f["categories"][0]["short_name"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filter));
  https.end();
  if (err) {
    Serial.printf("[fsq] parse error: %s\n", err.c_str());
    return false;
  }

  for (JsonObject r : doc["results"].as<JsonArray>()) {
    // Rating is Premium; drop anything below the bar (or with no rating at all).
    const float rating = r["rating"] | -1.0f;
    if (rating < minRating_) {
      continue;
    }

    Place p;
    p.name = (const char*)(r["name"] | "");
    p.lat = r["latitude"] | 0.0;
    p.lng = r["longitude"] | 0.0;
    p.providerId = (const char*)(r["fsq_place_id"] | "");
    p.distanceM = r["distance"] | 0.0;  // Foursquare precomputes distance from ll
    p.rating = rating;
    p.category = cuisineLabel(r["categories"].as<JsonArrayConst>());

    // Address: prefer the full formatted line; fall back to "Locality, Region".
    const char* full = r["location"]["formatted_address"] | (const char*)nullptr;
    if (full && *full) {
      p.address = full;
    } else {
      const char* loc = r["location"]["locality"] | "";
      const char* reg = r["location"]["region"] | "";
      p.address = String(loc);
      if (*loc && *reg) p.address += ", ";
      p.address += reg;
    }

    out.push_back(p);
  }
  return true;
}
