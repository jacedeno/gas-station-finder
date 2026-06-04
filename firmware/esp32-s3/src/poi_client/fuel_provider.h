// Provider abstraction. All POI access goes through IFuelProvider so TomTom,
// Google Places or OSM Overpass are swappable without touching the rest of the
// firmware (see CLAUDE.md §4).
#pragma once

#include <Arduino.h>
#include <vector>

// One fuel station. The human join key for GasBuddy is brand + address + coords
// (provider IDs are provider-internal and do not map across catalogs).
struct Station {
  String brand;        // e.g. "Shell"
  String address;      // full freeform address (required, not just the name)
  double lat = 0;
  double lng = 0;
  String providerId;   // provider-internal id (TomTom id / Google place_id)

  // Filled in locally by the geo layer, not by the provider:
  double distanceM = 0;
  double bearingDeg = 0;
};

class IFuelProvider {
 public:
  virtual ~IFuelProvider() = default;

  // Fetch nearby fuel stations around (lat, lng). Returns true on success and
  // fills brand/address/coords/providerId, plus distanceM when the provider gives
  // it for free (TomTom returns `dist`). The caller computes bearingDeg locally.
  // Returns false on network/parse error.
  virtual bool getNearby(double lat, double lng, std::vector<Station>& out) = 0;

  // Human-readable provider name for logs/UI.
  virtual const char* name() const = 0;
};
