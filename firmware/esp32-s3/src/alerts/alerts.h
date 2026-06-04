// Proximity / low-fuel buzzer alerts via the MLT-8530 (CLAUDE.md §5.7).
//
// NOTE: on the Indicator the buzzer is driven by the RP2040, not the ESP32-S3.
// So "alerting" here means deciding WHEN to alert; the actual tone request must be
// sent to the RP2040 over the inter-processor link (a future BUZZ command in
// firmware/PROTOCOL.md). For now this is a stub that logs the decision.
#pragma once

#include "../poi_client/fuel_provider.h"

namespace alerts {

// Returns true if the nearest target is within `triggerM` metres — i.e. the
// buzzer should sound. (Wiring the actual tone is a TODO; see note above.)
bool shouldAlert(const Station& nearest, double triggerM);

}  // namespace alerts
