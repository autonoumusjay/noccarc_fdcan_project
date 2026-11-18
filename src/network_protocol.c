#include "network_protocol.h"

static const struct {
    uint16_t base;
    uint8_t length;
} layout[] = {
    [NETWORK_EMERGENCY] = {0x010, NETWORK_EMERGENCY_SIZE},
    [NETWORK_RESYNC_REQ] = {0x020, NETWORK_RESYNC_SIZE},
    [NETWORK_HEARTBEAT] = {0x040, NETWORK_HEARTBEAT_SIZE},
    [NETWORK_PARAM_DATA] = {0x100, NETWORK_PARAM_SIZE}
};

uint16_t Network_Crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffffU;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
    }
    return crc;
}

uint16_t Network_Read16(const uint8_t *data)
{
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

uint32_t Network_Read32(const uint8_t *data)
{
    return Network_Read16(data) | ((uint32_t)Network_Read16(data + 2) << 16);
}

void Network_Write16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

void Network_Write32(uint8_t *data, uint32_t value)
{
    Network_Write16(data, (uint16_t)value);
    Network_Write16(data + 2, (uint16_t)(value >> 16));
}

uint8_t Network_DlcLength(uint8_t dlc)
{
    static const uint8_t lengths[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64
    };
    return dlc < sizeof(lengths) ? lengths[dlc] : UINT8_MAX;
}

uint16_t Network_FrameId(NetworkFrameType type, uint8_t source)
{
    if (type < NETWORK_EMERGENCY || type > NETWORK_PARAM_DATA ||
        source == 0 || source > NETWORK_MAX_NODE_ID)
        return 0;
    return layout[type].base + source;
}

bool Network_ValidateFrame(uint32_t id, bool standard, bool fd, bool brs,
                           bool remote, const uint8_t *data, size_t length)
{
    if (!standard || !fd || !brs || remote || data == NULL ||
        length < NETWORK_RESYNC_SIZE || data[0] != NETWORK_VERSION)
        return false;

    NetworkFrameType type = (NetworkFrameType)data[1];
    if (type < NETWORK_EMERGENCY || type > NETWORK_PARAM_DATA ||
        length != layout[type].length)
        return false;

    uint16_t expected_id = Network_FrameId(type, data[2]);
    if (expected_id == 0 || id != expected_id ||
        Network_Read16(data + length - 2) != Network_Crc16(data, length - 2))
        return false;

    switch (type)
    {
    case NETWORK_PARAM_DATA:
        return (data[3] & 0xe0U) == 0 && Network_Read16(data + 28) == 0;
    case NETWORK_HEARTBEAT:
        return data[3] <= 3 && (data[10] & 0xf0U) == 0 &&
               data[11] <= 20 && data[13] == 0;
    case NETWORK_RESYNC_REQ:
        return data[3] >= 1 && data[3] <= NETWORK_MAX_NODE_ID &&
               data[3] != data[2] && data[5] == 0;
    case NETWORK_EMERGENCY:
        return data[6] <= 20;
    }
    return false;
}
