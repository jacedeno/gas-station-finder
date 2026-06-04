#include "geo.h"

#include <math.h>

namespace {
constexpr double kEarthRadiusM = 6371000.0;
constexpr double kDeg2Rad = M_PI / 180.0;
constexpr double kRad2Deg = 180.0 / M_PI;
}  // namespace

namespace geo {

double haversineMeters(double lat1, double lng1, double lat2, double lng2) {
  const double dLat = (lat2 - lat1) * kDeg2Rad;
  const double dLng = (lng2 - lng1) * kDeg2Rad;
  const double a = sin(dLat / 2) * sin(dLat / 2) +
                   cos(lat1 * kDeg2Rad) * cos(lat2 * kDeg2Rad) *
                       sin(dLng / 2) * sin(dLng / 2);
  return 2 * kEarthRadiusM * atan2(sqrt(a), sqrt(1 - a));
}

double bearingDeg(double lat1, double lng1, double lat2, double lng2) {
  const double phi1 = lat1 * kDeg2Rad;
  const double phi2 = lat2 * kDeg2Rad;
  const double dLng = (lng2 - lng1) * kDeg2Rad;
  const double y = sin(dLng) * cos(phi2);
  const double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dLng);
  double brng = atan2(y, x) * kRad2Deg;
  return fmod(brng + 360.0, 360.0);
}

double angularDiffDeg(double a, double b) {
  double d = fmod(fabs(a - b), 360.0);
  return d > 180.0 ? 360.0 - d : d;
}

bool isAhead(double targetBearing, double courseDeg, double toleranceDeg) {
  return angularDiffDeg(targetBearing, courseDeg) <= toleranceDeg;
}

}  // namespace geo
