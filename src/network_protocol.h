#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    NETWORK_EMERGENCY = 1,
    NETWORK_RESYNC_REQ = 2,
    NETWORK_HEARTBEAT = 3,
    NETWORK_PARAM_DATA = 4
} NetworkFrameType;

enum {
    NETWORK_VERSION = 1,
    NETWORK_MAX_NODE_ID = 4,
    NETWORK_EMERGENCY_SIZE = 16,
    NETWORK_RESYNC_SIZE = 12,
    NETWORK_HEARTBEAT_SIZE = 16,
    NETWORK_PARAM_SIZE = 32
};

uint16_t Network_Crc16(const uint8_t *data, size_t length);
uint16_t Network_Read16(const uint8_t *data);
uint32_t Network_Read32(const uint8_t *data);
void Network_Write16(uint8_t *data, uint16_t value);
void Network_Write32(uint8_t *data, uint32_t value);
uint8_t Network_DlcLength(uint8_t dlc);
uint16_t Network_FrameId(NetworkFrameType type, uint8_t source);
bool Network_ValidateFrame(uint32_t id, bool standard, bool fd, bool brs,
                           bool remote, const uint8_t *data, size_t length);

#endif
