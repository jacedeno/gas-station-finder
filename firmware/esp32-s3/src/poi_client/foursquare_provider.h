// Foursquare Places implementation of IPlaceProvider (restaurants).
//
// Calls the new Places platform (places-api.foursquare.com) Search endpoint,
// constrained to a set of cuisine category IDs, sorted by distance. Requests the
// `rating` field (Premium -> billable) and keeps only places rated >= minRating.
// Auth + field shape confirmed live 2026-06-05 (see TOMTOM_SEARCH_API.md sibling
// notes and docs/superpowers/specs/2026-06-05-restaurants-section-design.md §3).
#pragma once

#include "place_provider.h"

class FoursquareProvider : public IPlaceProvider {
 public:
  // apiKey:       Foursquare Service API key (Bearer).
  // apiVersion:   value for the required X-Places-Api-Version header (e.g. 2025-06-17).
  // host:         API host (places-api.foursquare.com).
  // categoryIds:  comma-separated fsq_category_id list to include (cuisines).
  // radiusM/limit bound the query; minRating drops anything rated below it.
  FoursquareProvider(const char* apiKey, const char* apiVersion, const char* host,
                     const char* categoryIds, int radiusM, int limit, float minRating)
      : apiKey_(apiKey),
        apiVersion_(apiVersion),
        host_(host),
        categoryIds_(categoryIds),
        radiusM_(radiusM),
        limit_(limit),
        minRating_(minRating) {}

  bool getNearby(double lat, double lng, std::vector<Place>& out) override;
  const char* name() const override { return "Foursquare"; }

 private:
  String apiKey_;
  String apiVersion_;
  String host_;
  String categoryIds_;
  int radiusM_;
  int limit_;
  float minRating_;
};
