#include "network_state.h"
#include <assert.h>
#include <stdint.h>

int main(void)
{
    NetworkState state = {0};
    uint32_t values[VALUES_PER_NODE] = {1, 2, 3, 4, 5};
    assert(!Network_Parameters(&state, 2, 1, values, 10));
    Network_Heartbeat(&state, 2, 0, 10);
    assert(Network_Parameters(&state, 2, UINT16_MAX, values, 11));
    assert(Network_Parameters(&state, 2, 0, values, 12));
    assert(!Network_Parameters(&state, 2, UINT16_MAX, values, 13));
    assert(!Network_Parameters(&state, 2, 0, values, 14));
    assert(Network_Fresh(&state, 2, 361));
    assert(!Network_Fresh(&state, 2, 362));
    Network_UpdateHealth(&state, 760);
    assert(!state.health[2].online && !state.parameters[2].valid);
    assert(state.health[2].missed > 0);
    Network_Heartbeat(&state, 2, 0, 761);
    assert(state.health[2].online && state.health[2].recovering);
    assert(state.health[2].recoveries == 1);
    assert(Network_Parameters(&state, 2, 0, values, 762));
    assert(!state.health[2].recovering && Network_Fresh(&state, 2, 762));
    Network_Heartbeat(&state, 2, 1, 1011);
    Network_Heartbeat(&state, 2, 0, 1012);
    assert(state.health[2].recoveries == 2 && !state.parameters[2].valid);
    Network_Heartbeat(&state, 1, 0, UINT32_MAX - 10U);
    assert(Network_Parameters(&state, 1, 1, values, UINT32_MAX - 10U));
    assert(Network_Fresh(&state, 1, 12));
    return 0;
}
