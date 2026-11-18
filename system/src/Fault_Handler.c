#include "Fault_Handler.h"
#include "stm32h743xx.h"

volatile Fault_Record_t g_fault_record;

void Fault_Handler_Capture(uint32_t *stacked_registers)
{
    if (stacked_registers != (uint32_t *)0)
    {
        g_fault_record.r0   = stacked_registers[0];
        g_fault_record.r1   = stacked_registers[1];
        g_fault_record.r2   = stacked_registers[2];
        g_fault_record.r3   = stacked_registers[3];
        g_fault_record.r12  = stacked_registers[4];
        g_fault_record.lr   = stacked_registers[5];
        g_fault_record.pc   = stacked_registers[6];
        g_fault_record.xpsr = stacked_registers[7];
    }
    g_fault_record.cfsr  = SCB->CFSR;
    g_fault_record.hfsr  = SCB->HFSR;
    g_fault_record.dfsr  = SCB->DFSR;
    g_fault_record.afsr  = SCB->AFSR;
    g_fault_record.mmfar = SCB->MMFAR;
    g_fault_record.bfar  = SCB->BFAR;
    g_fault_record.valid = 0xFA017EADUL;
    __DSB();
    for (;;) { __NOP(); }
}

#if defined(__GNUC__)
#define FAULT_ENTRY(name) \
    __attribute__((naked)) void name(void) \
    { \
        __asm volatile ("tst lr, #4\n" \
                        "ite eq\n" \
                        "mrseq r0, msp\n" \
                        "mrsne r0, psp\n" \
                        "b Fault_Handler_Capture\n"); \
    }
FAULT_ENTRY(HardFault_Handler)
FAULT_ENTRY(MemManage_Handler)
FAULT_ENTRY(BusFault_Handler)
FAULT_ENTRY(UsageFault_Handler)
#else
void HardFault_Handler(void) { Fault_Handler_Capture((uint32_t *)__get_MSP()); }
void MemManage_Handler(void) { Fault_Handler_Capture((uint32_t *)__get_MSP()); }
void BusFault_Handler(void) { Fault_Handler_Capture((uint32_t *)__get_MSP()); }
void UsageFault_Handler(void) { Fault_Handler_Capture((uint32_t *)__get_MSP()); }
#endif
