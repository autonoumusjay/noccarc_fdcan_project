#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#include <stdint.h>

typedef struct
{
    uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;
    uint32_t cfsr, hfsr, dfsr, afsr, mmfar, bfar;
    uint32_t valid;
} Fault_Record_t;

extern volatile Fault_Record_t g_fault_record;
void Fault_Handler_Capture(uint32_t *stacked_registers);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);

#endif
