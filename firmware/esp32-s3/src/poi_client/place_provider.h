// Provider abstraction. All POI access goes through IPlaceProvider so TomTom
// (fuel), Foursquare (restaurants), Google Places or OSM Overpass are swappable
// without touching the rest of the firmware (see CLAUDE.md §4).
#pragma once

#include <Arduino.h>
#include <vector>

// One place (fuel station or restaurant). The human join key is name + address +
// coords (provider IDs are provider-internal and do not map across catalogs).
struct Place {
  String name;         // display label: brand for fuel, venue name for food
  String address;      // full freeform address (may be partial if the provider lacks one)
  double lat = 0;
  double lng = 0;
  String providerId;   // provider-internal id (TomTom id / Foursquare fsq_place_id)

  // Filled in locally by the geo layer, not by the provider:
  double distanceM = 0;
  double bearingDeg = 0;

  // Food-only extras (left at defaults for fuel):
  float rating = -1;   // Foursquare rating 0-10; -1 = none / not applicable
  String category;     // cuisine label(s), e.g. "Steakhouse · American"
};

class IPlaceProvider {
 public:
  virtual ~IPlaceProvider() = default;

  // Fetch nearby places around (lat, lng). Returns true on success and fills
  // name/address/coords/providerId, plus distanceM when the provider gives it for
  // free (both TomTom and Foursquare do). The caller computes bearingDeg locally.
  // Returns false on network/parse error.
  virtual bool getNearby(double lat, double lng, std::vector<Place>& out) = 0;

  // Human-readable provider name for logs/UI.
  virtual const char* name() const = 0;
};
