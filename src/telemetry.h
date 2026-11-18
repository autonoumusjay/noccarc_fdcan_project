#ifndef TELEMETRY_H
#define TELEMETRY_H
#include "network_state.h"

bool Telemetry_Healthy(uint32_t now);
void Telemetry_Service(const NetworkState *state, uint32_t now);
#endif
