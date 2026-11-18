#include "network_protocol.h"
#include <assert.h>

static void Seal(uint8_t *data, size_t length)
{
    Network_Write16(data + length - 2, Network_Crc16(data, length - 2));
}

int main(void)
{
    assert(Network_Crc16((const uint8_t *)"123456789", 9) == 0x29b1);
    const uint8_t lengths[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    for (uint8_t i = 0; i < sizeof(lengths); ++i)
        assert(Network_DlcLength(i) == lengths[i]);
    assert(Network_DlcLength(16) == UINT8_MAX);

    uint8_t param[32] = {1, NETWORK_PARAM_DATA, 3, 0x1f};
    Network_Write32(param + 8, UINT32_C(0x89abcdef));
    assert(param[8] == 0xef && param[11] == 0x89);
    assert(Network_Read32(param + 8) == UINT32_C(0x89abcdef));
    Seal(param, sizeof(param));
    assert(Network_ValidateFrame(0x103, true, true, true, false, param, 32));
    assert(!Network_ValidateFrame(0x104, true, true, true, false, param, 32));
    assert(!Network_ValidateFrame(0x103, true, true, true, false, param, 24));
    assert(!Network_ValidateFrame(0x103, false, true, true, false, param, 32));
    assert(!Network_ValidateFrame(0x103, true, false, true, false, param, 32));
    assert(!Network_ValidateFrame(0x103, true, true, false, false, param, 32));
    assert(!Network_ValidateFrame(0x103, true, true, true, true, param, 32));
    param[8] ^= 1;
    assert(!Network_ValidateFrame(0x103, true, true, true, false, param, 32));
    param[3] = 0x9f;
    Seal(param, sizeof(param));
    assert(!Network_ValidateFrame(0x103, true, true, true, false, param, 32));

    uint8_t heartbeat[16] = {1, NETWORK_HEARTBEAT, 2, 1};
    Seal(heartbeat, sizeof(heartbeat));
    assert(Network_ValidateFrame(0x042, true, true, true, false, heartbeat, 16));
    uint8_t resync[12] = {1, NETWORK_RESYNC_REQ, 4, 2};
    Seal(resync, sizeof(resync));
    assert(Network_ValidateFrame(0x024, true, true, true, false, resync, 12));
    uint8_t emergency[16] = {1, NETWORK_EMERGENCY, 1};
    Seal(emergency, sizeof(emergency));
    assert(Network_ValidateFrame(0x011, true, true, true, false, emergency, 16));
    emergency[1] = 255;
    Seal(emergency, sizeof(emergency));
    assert(!Network_ValidateFrame(0x011, true, true, true, false, emergency, 16));
}
