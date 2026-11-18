#ifndef NVIC_DRIVER_H
#define NVIC_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32h743xx.h"

void NVIC_Driver_Init(void);
void NVIC_ConfigureIRQ(IRQn_Type irq, uint32_t priority, bool enable);

#endif
