// On-device geometry: haversine distance, bearing, and the "ahead" heading filter.
// Pure C++/math — no Arduino or hardware dependencies, so it is unit-testable.
#pragma once

namespace geo {

// Great-circle distance between two WGS84 points, in metres.
double haversineMeters(double lat1, double lng1, double lat2, double lng2);

// Initial bearing (forward azimuth) from point 1 to point 2, in degrees [0, 360).
double bearingDeg(double lat1, double lng1, double lat2, double lng2);

// Smallest absolute difference between two compass headings, in degrees [0, 180].
double angularDiffDeg(double a, double b);

// True if `targetBearing` is within +/- toleranceDeg of `courseDeg` (i.e. "ahead").
bool isAhead(double targetBearing, double courseDeg, double toleranceDeg);

}  // namespace geo
