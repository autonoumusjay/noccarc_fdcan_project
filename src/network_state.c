#include "network_state.h"
#include <string.h>

static bool SequenceIsNew(uint16_t sequence, uint16_t previous)
{
    uint16_t distance = (uint16_t)(sequence - previous);
    return distance != 0U && distance < 0x8000U;
}

void Network_UpdateHealth(NetworkState *state, uint32_t now)
{
    for (unsigned i = 0; i < NODE_COUNT; ++i)
    {
        NodeHealth *health = &state->health[i];
        if (!health->seen)
            continue;
        uint32_t age = now - health->heartbeat_at;
        uint32_t missed = age > HEARTBEAT_MS / 2U ?
                          (age - HEARTBEAT_MS / 2U) / HEARTBEAT_MS : 0U;
        /* Accounting is saturating per outage. */
        if (missed > UINT8_MAX)
            missed = UINT8_MAX;
        if (missed > health->missed_this_gap)
        {
            health->missed += missed - health->missed_this_gap;
            health->missed_this_gap = (uint8_t)missed;
        }
        if (health->online && age >= TIMEOUT_MS)
        {
            health->online = false;
            health->recovering = false;
            state->parameters[i].valid = false;
        }
    }
}

void Network_Heartbeat(NetworkState *state, unsigned node, uint16_t sequence,
                       uint32_t now)
{
    NodeHealth *health = &state->health[node];
    bool restart = sequence == 0U && health->sequence != 0U &&
                   health->sequence != UINT16_MAX;
    if (health->seen && health->online &&
        !SequenceIsNew(sequence, health->sequence) && !restart)
        return;

    if (health->seen && (!health->online || restart))
    {
        ++health->recoveries;
        health->recovering = true;
        state->parameters[node].valid = false;
    }
    else if (health->seen)
    {
        uint16_t gap = (uint16_t)(sequence - health->sequence - 1U);
        if (gap > health->missed_this_gap)
            health->missed += gap - health->missed_this_gap;
    }
    health->seen = true;
    health->online = true;
    health->heartbeat_at = now;
    health->sequence = sequence;
    health->missed_this_gap = 0U;
}

bool Network_Parameters(NetworkState *state, unsigned node, uint16_t sequence,
                        const uint32_t *values, uint32_t now)
{
    ParameterGroup *group = &state->parameters[node];
    if (!state->health[node].online ||
        (group->valid && !SequenceIsNew(sequence, group->sequence)))
        return false;
    memcpy(group->value, values, sizeof(group->value));
    group->sequence = sequence;
    group->updated_at = now;
    group->valid = true;
    state->health[node].recovering = false;
    return true;
}

bool Network_Fresh(const NetworkState *state, unsigned node, uint32_t now)
{
    const ParameterGroup *group = &state->parameters[node];
    return group->valid && now - group->updated_at < STALE_MS;
}
