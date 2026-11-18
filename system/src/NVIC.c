#include "NVIC.h"

void NVIC_Driver_Init(void)
{
    /* 4 pre-emption priority bits, no subpriority bits on this configuration. */
    NVIC_SetPriorityGrouping(3U);
}

void NVIC_ConfigureIRQ(IRQn_Type irq, uint32_t priority, bool enable)
{
    NVIC_DisableIRQ(irq);
    NVIC_ClearPendingIRQ(irq);
    NVIC_SetPriority(irq, priority);
    if (enable) NVIC_EnableIRQ(irq);
}
