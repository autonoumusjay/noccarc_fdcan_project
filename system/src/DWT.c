#include "DWT.h"
#include "stm32h743xx.h"
#include "system_stm32h7xx.h"

bool DWT_Init(void)
{
#if defined(DWT) && defined(CoreDebug)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    return ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U);
#else
    return false;
#endif
}

uint32_t DWT_GetCycles(void)
{
    return DWT->CYCCNT;
}

uint32_t DWT_GetElapsedCycles(uint32_t start_cycle)
{
    return (uint32_t)(DWT_GetCycles() - start_cycle);
}

uint32_t DWT_GetMicroseconds(void)
{
    uint32_t frequency = SystemCoreClock;
    if (frequency == 0U) return 0U;
    return (uint32_t)(((uint64_t)DWT_GetCycles() * 1000000ULL) / frequency);
}
