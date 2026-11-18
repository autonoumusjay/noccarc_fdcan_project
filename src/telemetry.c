#include "telemetry.h"
#include "UART.h"
#include "peripheral_config.h"

enum { SNAPSHOT_MS = 250, FRAME_SIZE = 169 };
static uint8_t frame[FRAME_SIZE];
_Static_assert(FRAME_SIZE <= NOCARC_UART_TX_BUFFER_SIZE, "Snapshot exceeds DMA buffer");
static bool transfer_pending;
static uint32_t errors_at_start;
volatile uint32_t g_telemetry_dma_completed;
static uint32_t last_snapshot;
static uint32_t last_progress;

static void Write32(unsigned *offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        frame[(*offset)++] = (uint8_t)value;
        value >>= 8;
    }
}

static uint32_t Crc32(unsigned length)
{
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < length; ++i)
    {
        crc ^= frame[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0U);
    }
    return ~crc;
}

void Telemetry_Service(const NetworkState *state, uint32_t now)
{
    if (UART_IsTransmitBusy())
        return;

    if (transfer_pending)
    {
        transfer_pending = false;
        if (UART_GetErrorCounters()->dma_error == errors_at_start)
        {
            ++g_telemetry_dma_completed;
            last_progress = now;
        }
    }

    if (now - last_snapshot >= SNAPSHOT_MS)
    {
        unsigned offset = 4;
        frame[0] = 'N';
        frame[1] = 'C';
        frame[2] = 1U; /* Protocol version */
        frame[3] = 1U; /* Snapshot type */
        Write32(&offset, now);
        Write32(&offset, NOCARC_FDCAN_NOMINAL_BAUD);
        Write32(&offset, NOCARC_FDCAN_DATA_BAUD);
        frame[offset++] = 1U; /* BRS */
        for (unsigned i = 0; i < NODE_COUNT; ++i)
        {
            const NodeHealth *health = &state->health[i];
            const ParameterGroup *group = &state->parameters[i];
            frame[offset++] = (health->online ? 1U : 0U) |
                              (health->seen ? 2U : 0U) |
                              (health->recovering ? 4U : 0U) |
                              (group->valid ? 8U : 0U) |
                              (Network_Fresh(state, i, now) ? 16U : 0U);
            Write32(&offset, health->seen ? now - health->heartbeat_at : UINT32_MAX);
            Write32(&offset, health->missed);
            Write32(&offset, health->recoveries);
            Write32(&offset, group->sequence);
            for (unsigned j = 0; j < VALUES_PER_NODE; ++j)
                Write32(&offset, group->value[j]);
        }
        uint32_t crc = Crc32(offset);
        Write32(&offset, crc);
        /* Capture the error count before submitting: the IRQ can run immediately. */
        errors_at_start = UART_GetErrorCounters()->dma_error;
        transfer_pending = UART_TransmitDMA(frame, FRAME_SIZE) == UART_DRIVER_OK;
        last_snapshot = now;
    }
}

bool Telemetry_Healthy(uint32_t now)
{
    return now - last_progress < 600U;
}
