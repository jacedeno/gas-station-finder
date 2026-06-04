// TomTom POI Search implementation of IFuelProvider.
// Uses the Category Search endpoint filtered to the fuel/petrol category, and
// parses the freeform address (see TOMTOM_SEARCH_API.md).
#pragma once

#include "fuel_provider.h"

class TomTomProvider : public IFuelProvider {
 public:
  // apiKey: TomTom Search API key. radiusM/limit bound the query.
  TomTomProvider(const char* apiKey, int radiusM, int limit)
      : apiKey_(apiKey), radiusM_(radiusM), limit_(limit) {}

  bool getNearby(double lat, double lng, std::vector<Station>& out) override;
  const char* name() const override { return "TomTom"; }

 private:
  String apiKey_;
  int radiusM_;
  int limit_;
};
