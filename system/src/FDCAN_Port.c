#include "FDCAN_Port.h"
#include "peripheral_config.h"

FDCAN_Handle_t g_fdcan1_handle;
static volatile bool rx_ready;

FDCAN_Result_t FDCAN_Port_Init(void)
{
    static const FDCAN_Config_t config = {
        .Instance = FDCAN_1,
        .Mode = FDCAN_MODE_NORMAL,
        .FrameFormat = FD,
        .BitRateSwitch = FDCAN_BRS_ENABLED,
        .NominalTiming = NOCARC_FDCAN_NOMINAL_TIMING,
        .DataTiming = NOCARC_FDCAN_DATA_TIMING,
        .AutoRetransmission = true,
        .StandardFilterCount = 0U,
        .ExtendedFilterCount = 0U,
        .RxFIFO0ElementCount = 16U,
        .RxFIFO1ElementCount = 0U,
        .TxBufferCount = 8U
    };

    FDCAN_Result_t result = FDCAN_Init(&g_fdcan1_handle, &config);
    if (result != FDCAN_OK) return result;
    return FDCAN_Start(&g_fdcan1_handle);
}

void FDCAN1_IT0_IRQHandler(void)
{
    if (g_fdcan1_handle.Initialized &&
        (g_fdcan1_handle.Instance->IR & FDCAN_IR_RF0N) != 0U)
        rx_ready = true;
    FDCAN_IRQHandler(&g_fdcan1_handle);
}

bool FDCAN_Port_TakeRxReady(void)
{
    static bool draining;
    uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    bool notified = rx_ready;
    rx_ready = false;
    __set_PRIMASK(interrupt_mask);
    draining = (notified || draining) &&
               (g_fdcan1_handle.Instance->RXF0S & FDCAN_RXF0S_F0FL_Msk) != 0U;
    return draining;
}
