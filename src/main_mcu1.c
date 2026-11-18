#include "common.h"
#include "peripheral_config.h"

FDCAN_Handle_t hfdcan1;

static const FDCAN_Config_t fdcan1_config =
{
    .Instance = FDCAN_1,
    .Mode = FDCAN_MODE_NORMAL,

    .FrameFormat = FD,
    .BitRateSwitch = FDCAN_BRS_ENABLED,
    .AutoRetransmission = false,

    .NominalTiming = NOCARC_FDCAN_NOMINAL_TIMING,
    .DataTiming = NOCARC_FDCAN_DATA_TIMING,

    .StandardFilterCount = 1,
    .ExtendedFilterCount = 0,

    .RxFIFO0ElementCount = 3,
    .RxFIFO1ElementCount = 0,

    .TxBufferCount = 3
};

/*=========================================================
 * INTERRUPT VARIABLES
 *=========================================================*/

volatile uint32_t fdcan_rx_pending = 0U;
volatile uint32_t fdcan_irq_count = 0U;

/*=========================================================
 * TRANSMISSION VARIABLES
 *=========================================================*/

volatile FDCAN_Result_t tx_result;

volatile uint32_t tx_success_count = 0U;
volatile uint32_t tx_error_count = 0U;

/*=========================================================
 * RECEPTION VARIABLES
 *=========================================================*/

volatile FDCAN_Result_t rx_result;

volatile uint32_t response_count = 0U;
volatile uint32_t response_valid_count = 0U;
volatile uint32_t response_invalid_count = 0U;

volatile uint32_t response_identifier = 0U;
volatile uint32_t response_payload_match = 0U;

/*=========================================================
 * APPLICATION VARIABLES
 *=========================================================*/

volatile FDCAN_Result_t init_result;
volatile FDCAN_Result_t start_result;

volatile uint32_t app_error = 0U;

/* TX/RX messages */
FDCAN_TxMessage_t tx_message;
FDCAN_RxMessage_t response_message;

/*=========================================================
 * FDCAN INTERRUPT HANDLER
 *=========================================================*/

void FDCAN1_IT0_IRQHandler(void)
{
    fdcan_irq_count++;

    if (hfdcan1.Instance != NULL)
    {
        /*
         * Check RX FIFO0 fill level.
         * Notify the main application when data is available.
         */
        if ((hfdcan1.Instance->RXF0S & 0x7FU) != 0U)
        {
            fdcan_rx_pending = 1U;
            __DMB();
        }

        /*
         * Record and clear FDCAN interrupt flags.
         */
        FDCAN_IRQHandler(&hfdcan1);
    }
}

/*=========================================================
 * ATOMIC RX EVENT HANDLING
 *=========================================================*/

static uint32_t FDCAN_TakeRxEvent(void)
{
    uint32_t primask;
    uint32_t pending;

    primask = __get_PRIMASK();

    __disable_irq();

    pending = fdcan_rx_pending;

    fdcan_rx_pending = 0U;

    __DMB();

    __set_PRIMASK(primask);

    return pending;
}

/*=========================================================
 * MAIN
 *=========================================================*/

int main(void)
{
    /* 1. Initialize clock */
    if (RCC_Clock_Init() != RCC_OK)
    {
        app_error = 1U;

        while (1)
        {
            __NOP();
        }
    }

    /* 2. Initialize FDCAN */
    GPIO_Init();

    init_result = FDCAN_Init(&hfdcan1, &fdcan1_config);

    if (init_result != FDCAN_OK)
    {
        app_error = 2U;

        while (1)
        {
            __NOP();
        }
    }

    /* 3. Start FDCAN and enable interrupts */
    start_result = FDCAN_Start(&hfdcan1);

    if (start_result != FDCAN_OK)
    {
        app_error = 3U;

        while (1)
        {
            __NOP();
        }
    }

    /*=====================================================
     * CONFIGURE REQUEST MESSAGE
     *=====================================================*/

    tx_message.Identifier = 0x123U;
    tx_message.IDType = STANDARD;
    tx_message.FrameFormat = FD;
    tx_message.BitRateSwitch = FDCAN_BRS_ENABLED;
    tx_message.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_message.DataLength = 64U;

    for (uint32_t i = 0U; i < 64U; i++)
    {
        tx_message.Data[i] = (uint8_t)i;
    }

    /*=====================================================
     * MAIN APPLICATION LOOP
     *=====================================================*/

    uint32_t retry_counter = 0U;

    while (1)
    {
        /*
         * TRANSMISSION:
         *
         * Send requests periodically until a valid
         * response is received from MCU2.
         *
         * This also handles the possibility that MCU2
         * starts slightly later than MCU1.
         */

        if (response_valid_count == 0U)
        {
            if (retry_counter++ >= 1000000U)
            {
                retry_counter = 0U;

                tx_result =
                    FDCAN_Transmit(&hfdcan1, &tx_message);

                if (tx_result == FDCAN_OK)
                {
                    tx_success_count++;
                }
                else
                {
                    tx_error_count++;
                }
            }
        }

        /*
         * RECEPTION:
         *
         * Process received frames only when notified
         * by the FDCAN interrupt.
         */

        if (FDCAN_TakeRxEvent() != 0U)
        {
            /* Drain all available FIFO0 messages. */
            while ((rx_result = FDCAN_Receive(&hfdcan1,&response_message)) == FDCAN_OK)
            {
                uint32_t match = 1U;

                response_count++;

                response_identifier =
                    response_message.Identifier;

                /* Verify message metadata. */
                if ((response_message.Identifier != 0x124U) ||
                    (response_message.IDType != STANDARD) ||
                    (response_message.FrameFormat != FD) ||
                    (response_message.BitRateSwitch !=
                     FDCAN_BRS_ENABLED) ||
                    (response_message.DataLength != 64U))
                {
                    match = 0U;
                }

                /* Verify all 64 payload bytes. */
                for (uint32_t i = 0U; i < 64U; i++)
                {
                    if (response_message.Data[i] !=
                        (uint8_t)(0xA0U + i))
                    {
                        match = 0U;
                        break;
                    }
                }

                response_payload_match = match;

                if (match == 1U)
                {
                    response_valid_count++;
                }
                else
                {
                    response_invalid_count++;
                }
            }

            /*
             * RX queue empty is expected after the FIFO
             * has been completely drained.
             */
            if (rx_result != FDCAN_ERROR_RX_QUEUE_EMPTY)
            {
                response_invalid_count++;
            }
        }
    }
}