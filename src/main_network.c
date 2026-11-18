#include <stddef.h>
#include "FDCAN_Port.h"
#include "GPIO.h"
#include "RCC_Clock.h"
#include "Timer.h"
#include "UART.h"
#include "DMA.h"
#include "Watchdog.h"
#include "peripheral_config.h"
#include "network_state.h"
#include "telemetry.h"

#ifndef NODE_ID
#error NODE_ID is required
#endif
_Static_assert(NODE_ID >= 1 && NODE_ID <= NODE_COUNT, "Invalid node ID");

enum { LOCAL = NODE_ID - 1, HEARTBEAT_ID = 0x100, UPDATE_ID = 0x110,
       PROTOCOL_VERSION = 1, RECEIVE_BUDGET = 8, TX_RETRY_MS = 10,
       HEARTBEAT_ATTEMPTS = 3 };

NetworkState g_network_state;
volatile uint32_t g_network_init_error;
volatile uint32_t g_network_reset_flags;
volatile uint32_t g_network_watchdog_feeds;
volatile uint32_t g_network_rx_invalid;
volatile uint32_t g_network_tx_busy;

static uint16_t heartbeat_sequence;
static uint16_t update_sequence;

static bool Broadcast(unsigned base, uint16_t sequence, const uint32_t *values)
{
    FDCAN_TxMessage_t message = {
        .Identifier = base + NODE_ID, .IDType = STANDARD, .FrameFormat = FD,
        .BitRateSwitch = FDCAN_BRS_ENABLED, .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .DataLength = values == NULL ? 4U : 24U,
        .Data = { PROTOCOL_VERSION, NODE_ID, (uint8_t)sequence, (uint8_t)(sequence >> 8) }
    };
    if (values != NULL)
    {
        for (unsigned i = 0; i < VALUES_PER_NODE; ++i)
            for (unsigned byte = 0; byte < 4; ++byte)
                message.Data[4 + 4 * i + byte] = (uint8_t)(values[i] >> (8 * byte));
    }
    if (FDCAN_Transmit(&g_fdcan1_handle, &message) == FDCAN_OK)
        return true;
    ++g_network_tx_busy;
    return false;
}

/* Periodic snapshots expire. Free a blocked FIFO head after a full
 * heartbeat interval so a detached controller can send new data on rejoin. */
static void ServiceTxBackpressure(uint32_t now)
{
    static bool full;
    static uint32_t full_since;
    FDCAN_GlobalTypeDef *can = g_fdcan1_handle.Instance;
    uint32_t fifo = can->TXFQS;
    if ((fifo & FDCAN_TXFQS_TFQF) == 0U)
    {
        full = false;
        return;
    }

    if (!full)
    {
        full_since = now;
        full = true;
    }
    else if (now - full_since >= HEARTBEAT_MS)
    {
        uint32_t head = (fifo & FDCAN_TXFQS_TFGI_Msk) >> FDCAN_TXFQS_TFGI_Pos;
        can->TXBCR = (1UL << head);
        full_since = now;
    }
}

static void Receive(uint32_t now)
{
    if (!FDCAN_Port_TakeRxReady())
        return;
    FDCAN_RxMessage_t message;
    for (unsigned i = 0; i < RECEIVE_BUDGET; ++i)
    {
        if (FDCAN_Receive(&g_fdcan1_handle, &message) != FDCAN_OK)
            return;

        unsigned source = message.Data[1];
        if (message.IDType != STANDARD || message.FrameFormat != FD ||
            message.BitRateSwitch != FDCAN_BRS_ENABLED ||
            message.Data[0] != PROTOCOL_VERSION ||
            source == 0 || source > NODE_COUNT || source == NODE_ID)
        {
            ++g_network_rx_invalid;
            continue;
        }
        unsigned node = source - 1;
        uint16_t sequence = message.Data[2] | ((uint16_t)message.Data[3] << 8);
        if (message.Identifier == HEARTBEAT_ID + source && message.DataLength == 4)
            Network_Heartbeat(&g_network_state, node, sequence, now);
        else if (message.Identifier == UPDATE_ID + source && message.DataLength == 24)
        {
            uint32_t values[VALUES_PER_NODE] = {0};
            for (unsigned j = 0; j < VALUES_PER_NODE; ++j)
                for (unsigned byte = 0; byte < 4; ++byte)
                    values[j] |= (uint32_t)message.Data[4 + 4 * j + byte] << (8 * byte);
            (void)Network_Parameters(&g_network_state, node, sequence, values, now);
        }
        else
            ++g_network_rx_invalid;
    }
}

int main(void)
{
    if (RCC_Clock_Init() != RCC_OK)
        g_network_init_error = 1;
    else
    {
        GPIO_Init();
        if (Timer_Init() != TIMER_OK)
            g_network_init_error = 2;
        else if (FDCAN_Port_Init() != FDCAN_OK)
            g_network_init_error = 3;
        else if (NODE_ID == 1 && (UART_Init(NOCARC_UART_BAUDRATE) != UART_DRIVER_OK ||
                                  DMA_Init() != DMA_DRIVER_OK))
            g_network_init_error = 4;
    }
    if (!g_network_init_error)
    {
        g_network_reset_flags = Watchdog_GetResetFlags();
        Watchdog_ClearResetFlags();
        if (Watchdog_Init() != WATCHDOG_OK)
            g_network_init_error = 5;
    }
    while (g_network_init_error)
        __NOP();

    uint32_t now = Timer_GetTick();
    uint32_t heartbeat_due = now + 10U * LOCAL;
    uint32_t update_due = heartbeat_due;
    unsigned heartbeat_attempts = 0;
    uint16_t local_heartbeat = 0;
    uint32_t local_health_due = heartbeat_due;
    uint32_t fed_at = now;
    bool heartbeat_serviced = false;
    bool update_serviced = false;
    for (;;)
    {
        now = Timer_GetTick();
        Network_UpdateHealth(&g_network_state, now);
        Receive(now);
        ServiceTxBackpressure(now);
        if ((int32_t)(now - local_health_due) >= 0)
        {
            Network_Heartbeat(&g_network_state, LOCAL, local_heartbeat++, now);
            local_health_due = now + HEARTBEAT_MS;
        }
        if ((int32_t)(now - heartbeat_due) >= 0)
        {
            heartbeat_serviced = true;
            if (Broadcast(HEARTBEAT_ID, heartbeat_sequence, NULL))
            {
                ++heartbeat_sequence;
                heartbeat_attempts = 0;
                heartbeat_due = now + HEARTBEAT_MS;
            }
            else if (++heartbeat_attempts < HEARTBEAT_ATTEMPTS)
                heartbeat_due = now + TX_RETRY_MS;
            else
            {
                ++heartbeat_sequence;
                heartbeat_attempts = 0;
                heartbeat_due = now + HEARTBEAT_MS;
            }
        }
        if (g_network_state.health[LOCAL].online &&
            (int32_t)(now - update_due) >= 0)
        {
            uint32_t values[VALUES_PER_NODE];
            for (unsigned i = 0; i < VALUES_PER_NODE; ++i)
                values[i] = 1000U * (LOCAL * VALUES_PER_NODE + i + 1U) + now / UPDATE_MS;
            (void)Network_Parameters(&g_network_state, LOCAL, update_sequence, values, now);
            /* Keep generating local values even while transmission is congested. */
            (void)Broadcast(UPDATE_ID, update_sequence++, values);
            update_due = now + UPDATE_MS;
            update_serviced = true;
        }
        if (NODE_ID == 1)
            Telemetry_Service(&g_network_state, now);

        /* RX and health sweep ran this pass, even if no remote traffic arrived. */
        if (now - fed_at >= 100U && heartbeat_serviced && update_serviced &&
            (NODE_ID != 1 || Telemetry_Healthy(now)))
        {
            Watchdog_Refresh();
            ++g_network_watchdog_feeds;
            fed_at = now;
            heartbeat_serviced = false;
            update_serviced = false;
        }
    }
}
