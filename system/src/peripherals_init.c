#include "peripherals_init.h"
#include "RCC.h"
#include "GPIO.h"
#include "Timer.h"
#include "NVIC.h"
#include "DWT.h"
#include "DMA.h"
#include "UART.h"
#include "Watchdog.h"
#include "FDCAN_Port.h"
#include "peripheral_config.h"

Peripheral_Status_t Peripherals_Init(MCU_Role_t role)
{
    if ((role != MCU_ROLE_MASTER) && (role != MCU_ROLE_SLAVE))
        return PERIPHERAL_ERROR;

    if (RCC_Init() != RCC_DRIVER_OK) return PERIPHERAL_ERROR;
    NVIC_Driver_Init();
    GPIO_Init();
    if (Timer_Init() != TIMER_OK) return PERIPHERAL_ERROR;
    (void)DWT_Init();

    /* All four controllers participate directly on the same CAN FD bus. */
    if (FDCAN_Port_Init() != FDCAN_OK) return PERIPHERAL_ERROR;

    /* The master alone exposes the telemetry stream to the Python dashboard. */
    if (role == MCU_ROLE_MASTER)
    {
        if (DMA_Init() != DMA_DRIVER_OK) return PERIPHERAL_ERROR;
        if (UART_Init(NOCARC_UART_BAUDRATE) != UART_DRIVER_OK)
            return PERIPHERAL_ERROR;
    }

#if (NOCARC_ENABLE_IWDG != 0U)
    if (Watchdog_Init() != WATCHDOG_OK) return PERIPHERAL_ERROR;
#endif
    return PERIPHERAL_OK;
}
