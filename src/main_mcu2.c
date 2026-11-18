#include "common.h"
#include "peripheral_config.h"

/*=========================================================
 * MCU2 - SLAVE CONTROLLER
 *=========================================================*/

/* FDCAN handle */
FDCAN_Handle_t hfdcan1;

/*=========================================================
 * FDCAN CONFIGURATION
 *=========================================================*/

static const FDCAN_Config_t fdcan1_config =
{
    .Instance = FDCAN_1,
    .Mode = FDCAN_MODE_NORMAL,

    .FrameFormat = FD,
    .BitRateSwitch = FDCAN_BRS_ENABLED,
    .AutoRetransmission = false,

    /* 500 kbps nominal bitrate */
    .NominalTiming = NOCARC_FDCAN_NOMINAL_TIMING,

    /* 2 Mbps data bitrate */
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
 * RECEPTION VARIABLES
 *=========================================================*/

volatile FDCAN_Result_t rx_result;

volatile uint32_t rx_count = 0U;
volatile uint32_t rx_valid_count = 0U;
volatile uint32_t rx_invalid_count = 0U;
volatile uint32_t rx_error_count = 0U;

volatile uint32_t last_rx_identifier = 0U;
volatile uint32_t last_payload_match = 0U;

/*=========================================================
 * RESPONSE TRANSMISSION VARIABLES
 *=========================================================*/

volatile FDCAN_Result_t response_tx_result;

volatile uint32_t response_tx_count = 0U;
volatile uint32_t response_tx_error_count = 0U;

/*=========================================================
 * APPLICATION VARIABLES
 *=========================================================*/

volatile FDCAN_Result_t init_result;
volatile FDCAN_Result_t start_result;

volatile uint32_t app_error = 0U;

/* Messages */
FDCAN_RxMessage_t rx_message;
FDCAN_TxMessage_t response_message;

/*=========================================================
 * FDCAN INTERRUPT HANDLER
 *=========================================================*/

void FDCAN1_IT0_IRQHandler(void)
{
    fdcan_irq_count++;

    if (hfdcan1.Instance != NULL)
    {
        if ((hfdcan1.Instance->RXF0S & 0x7FU) != 0U)
        {
            fdcan_rx_pending = 1U;
            __DMB();
        }

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
 * PREPARE RESPONSE MESSAGE
 *=========================================================*/

static void FDCAN_PrepareResponse(void)
{
    response_message.Identifier = 0x124U;
    response_message.IDType = STANDARD;
    response_message.FrameFormat = FD;
    response_message.BitRateSwitch = FDCAN_BRS_ENABLED;
    response_message.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    response_message.DataLength = 64U;

    for (uint32_t i = 0U; i < 64U; i++)
    {
        response_message.Data[i] =
            (uint8_t)(0xA0U + i);
    }
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

    GPIO_Init();

    /* 2. Initialize FDCAN */
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

    FDCAN_PrepareResponse();

    /*=====================================================
     * MAIN APPLICATION LOOP
     *=====================================================*/

    while (1)
    {
        /*
         * Wait for notification from FDCAN ISR.
         */
        if (FDCAN_TakeRxEvent() != 0U)
        {
            /*
             * Process all available messages from RX FIFO0.
             */
            while ((rx_result =
                    FDCAN_Receive(&hfdcan1, &rx_message)) == FDCAN_OK)
            {
                uint32_t match = 1U;

                rx_count++;

                last_rx_identifier =
                    rx_message.Identifier;

                /* Verify message metadata. */
                if ((rx_message.Identifier != 0x123U) ||
                    (rx_message.IDType != STANDARD) ||
                    (rx_message.FrameFormat != FD) ||
                    (rx_message.BitRateSwitch !=
                     FDCAN_BRS_ENABLED) ||
                    (rx_message.DataLength != 64U))
                {
                    match = 0U;
                }

                /* Verify all 64 payload bytes. */
                for (uint32_t i = 0U; i < 64U; i++)
                {
                    if (rx_message.Data[i] != (uint8_t)i)
                    {
                        match = 0U;
                        break;
                    }
                }

                last_payload_match = match;

                if (match == 1U)
                {
                    rx_valid_count++;

                    response_tx_result =
                        FDCAN_Transmit(&hfdcan1, &response_message);
                    if (response_tx_result == FDCAN_OK)
                        response_tx_count++;
                    else
                        response_tx_error_count++;
                }
                else
                {
                    rx_invalid_count++;
                }
            }

            /*
             * RX queue empty is expected after draining.
             */
            if (rx_result != FDCAN_ERROR_RX_QUEUE_EMPTY)
            {
                rx_error_count++;
            }
        }
    }
}