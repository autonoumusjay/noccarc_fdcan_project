#include "Watchdog.h"
#include "peripheral_config.h"
#include "stm32h743xx.h"

#define IWDG_WRITE_ACCESS_KEY 0x5555U
#define IWDG_START_KEY        0xCCCCU
#define IWDG_RELOAD_KEY       0xAAAAU
#define IWDG_UPDATE_TIMEOUT   1000000UL

Watchdog_Status_t Watchdog_Init(void)
{
#if (NOCARC_ENABLE_IWDG == 0U)
    return WATCHDOG_OK;
#else
    uint32_t timeout;
    uint32_t reload;

    /* LSI is nominally 32 kHz; /64 gives 500 Hz. 500 counts ~= 1 s. */
    reload = ((NOCARC_IWDG_TIMEOUT_MS * 500UL) / 1000UL);
    if (reload == 0U) reload = 1U;
    if (reload > 0x0FFFU) reload = 0x0FFFU;

    IWDG1->KR = IWDG_START_KEY;
    IWDG1->KR = IWDG_WRITE_ACCESS_KEY;
    IWDG1->PR = 4U; /* Prescaler /64. */
    IWDG1->RLR = reload - 1U;

    timeout = IWDG_UPDATE_TIMEOUT;
    while ((IWDG1->SR & 0x7U) != 0U)
    {
        if (timeout-- == 0U) return WATCHDOG_TIMEOUT;
    }

    IWDG1->KR = IWDG_RELOAD_KEY;
    return WATCHDOG_OK;
#endif
}

void Watchdog_Refresh(void)
{
#if (NOCARC_ENABLE_IWDG != 0U)
    IWDG1->KR = IWDG_RELOAD_KEY;
#endif
}

uint32_t Watchdog_GetResetFlags(void)
{
    return RCC->RSR;
}

void Watchdog_ClearResetFlags(void)
{
    RCC->RSR |= RCC_RSR_RMVF;
}
