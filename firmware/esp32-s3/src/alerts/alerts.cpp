#include "alerts.h"

namespace alerts {

bool shouldAlert(const Station& nearest, double triggerM) {
  return nearest.distanceM > 0 && nearest.distanceM <= triggerM;
}

}  // namespace alerts
