#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    NODE_COUNT = 4, VALUES_PER_NODE = 5,
    UPDATE_MS = 100, HEARTBEAT_MS = 250, TIMEOUT_MS = 750, STALE_MS = 350
};

typedef struct {
    uint32_t value[VALUES_PER_NODE];
    uint32_t updated_at;
    uint16_t sequence;
    bool valid;
} ParameterGroup;

typedef struct {
    uint32_t heartbeat_at;
    uint32_t missed;
    uint32_t recoveries;
    uint16_t sequence;
    uint8_t missed_this_gap;
    bool seen;
    bool online;
    bool recovering;
} NodeHealth;

typedef struct {
    ParameterGroup parameters[NODE_COUNT];
    NodeHealth health[NODE_COUNT];
} NetworkState;

void Network_UpdateHealth(NetworkState *state, uint32_t now);
void Network_Heartbeat(NetworkState *state, unsigned node, uint16_t sequence,
                       uint32_t now);
bool Network_Parameters(NetworkState *state, unsigned node, uint16_t sequence,
                        const uint32_t *values, uint32_t now);
bool Network_Fresh(const NetworkState *state, unsigned node, uint32_t now);

#endif
