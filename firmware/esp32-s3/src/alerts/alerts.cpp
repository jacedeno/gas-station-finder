#include "alerts.h"

namespace alerts {

bool shouldAlert(const Place& nearest, double triggerM) {
  return nearest.distanceM > 0 && nearest.distanceM <= triggerM;
}

}  // namespace alerts
