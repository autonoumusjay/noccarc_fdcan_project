#include "common.h"
#include "peripheral_config.h"
#include "FDCAN.h"

/* FDCAN1 handle */
FDCAN_Handle_t hfdcan1;

/* FDCAN1 configuration */
static const FDCAN_Config_t fdcan1_config =
{
    .Instance = FDCAN_1,

    .Mode = FDCAN_MODE_NORMAL,

    .FrameFormat = FD,

    .BitRateSwitch = FDCAN_BRS_ENABLED,

    .AutoRetransmission = false,

    /* 500 kbps nominal bitrate @ 80 MHz */
    .NominalTiming = NOCARC_FDCAN_NOMINAL_TIMING,

    /* 2 Mbps data bitrate @ 80 MHz */
    .DataTiming = NOCARC_FDCAN_DATA_TIMING,

    .StandardFilterCount = 1,
    .ExtendedFilterCount = 0,

    .RxFIFO0ElementCount = 3,
    .RxFIFO1ElementCount = 0,

    .TxBufferCount = 3
};
/* Loopback test variables - inspect these in the debugger */
volatile FDCAN_Result_t tx_result;
volatile FDCAN_Result_t rx_result;

volatile uint32_t loopback_test_done = 0U;
volatile uint32_t loopback_test_pass = 0U;

FDCAN_RxMessage_t rx_message;
int main(void)
{
    FDCAN_Result_t fdcan_result;

    /* 1. Initialize system clocks */
    if (RCC_Clock_Init() != RCC_OK)
    {
        while (1)
        {
            __NOP();
        }
    }

    /* 2. Initialize GPIO */
    const GPIO_PinConfig_t led = {
        GPIO_MODE_OUTPUT, GPIO_NOPULL, GPIO_SPEED_LOW, GPIO_PUSH_PULL, 0U
    };
    (void)GPIO_ConfigPin(GPIOA, 5U, &led);

    /* 3. Initialize FDCAN1 */
    fdcan_result = FDCAN_Init(&hfdcan1, &fdcan1_config);

    if (fdcan_result != FDCAN_OK)
    {
        while (1)
        {
            __NOP();
        }
    }
    hfdcan1.Instance->CCCR |= (1U << 7);  // TEST = 1
    hfdcan1.Instance->TEST |= (1U << 4);  // LBCK = 1
    hfdcan1.Instance->CCCR |= (1U << 5);  // MON = 1 
    /* 4. Start FDCAN1 */
    hfdcan1.Instance->IE = 0U; /* This diagnostic polls RX. */
    fdcan_result = FDCAN_Start(&hfdcan1);

    /* =========================================================
 * 6. CAN FD Internal Loopback Test
 * ========================================================= */

FDCAN_TxMessage_t tx_message = {0};

/* Configure message */
tx_message.Identifier = 0x123U;
tx_message.IDType = STANDARD;
tx_message.FrameFormat = FD;
tx_message.BitRateSwitch = FDCAN_BRS_ENABLED;
tx_message.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
tx_message.DataLength = 64U;

/* Fill payload with 0x00 to 0x3F */
for (uint32_t i = 0U; i < 64U; i++)
{
    tx_message.Data[i] = (uint8_t)i;
}

/* Transmit */
tx_result = FDCAN_Transmit(&hfdcan1, &tx_message);

if (tx_result == FDCAN_OK)
{
    /* Wait for loopback reception */
    uint32_t timeout = 1000000U;

    while (timeout--)
    {
        rx_result = FDCAN_Receive(&hfdcan1, &rx_message);

        if (rx_result == FDCAN_OK)
        {
            /* Verify received message */
            if ((rx_message.Identifier == tx_message.Identifier) &&
                (rx_message.IDType == tx_message.IDType) &&
                (rx_message.FrameFormat == tx_message.FrameFormat) &&
                (rx_message.BitRateSwitch == tx_message.BitRateSwitch) &&
                (rx_message.DataLength == tx_message.DataLength))
            {
                uint32_t match = 1U;

                for (uint32_t i = 0U; i < 64U; i++)
                {
                    if (rx_message.Data[i] != tx_message.Data[i])
                    {
                        match = 0U;
                        break;
                    }
                }

                loopback_test_pass = match;
            }

            break;
        }
    }
}

loopback_test_done = 1U;

    if (fdcan_result != FDCAN_OK)
    {
        while (1)
        {
            __NOP();
        }
    }

    /* 5. Main application loop */
    while (1)
    {
        GPIO_TogglePin(GPIOA, 5U);

        for (volatile int i = 0; i < 1000000; i++)
        {
            __NOP();
        }
    }
}   
