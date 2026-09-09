// TomTom POI Search implementation of IPlaceProvider (fuel).
// Uses the Nearby Search endpoint filtered to the fuel/petrol category, and
// parses the freeform address (see TOMTOM_SEARCH_API.md).
#pragma once

#include "place_provider.h"

class TomTomProvider : public IPlaceProvider {
 public:
  // apiKey: TomTom Search API key. radiusM/limit bound the query.
  TomTomProvider(const char* apiKey, int radiusM, int limit)
      : apiKey_(apiKey), radiusM_(radiusM), limit_(limit) {}

  bool getNearby(double lat, double lng, std::vector<Place>& out) override;
  const char* name() const override { return "TomTom"; }

 private:
  String apiKey_;
  int radiusM_;
  int limit_;
};
