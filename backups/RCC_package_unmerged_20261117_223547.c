#include "RCC.h"
#include "stm32h743xx.h"
#include "system_stm32h7xx.h"

#define RCC_WAIT_TIMEOUT 1000000UL

static bool RCC_WaitForSet(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t timeout = RCC_WAIT_TIMEOUT;
    while (((*reg) & mask) == 0U)
    {
        if (timeout-- == 0U) return false;
    }
    return true;
}

static bool RCC_WaitForClear(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t timeout = RCC_WAIT_TIMEOUT;
    while (((*reg) & mask) != 0U)
    {
        if (timeout-- == 0U) return false;
    }
    return true;
}

RCC_DriverStatus_t RCC_Init(void)
{
    /*
     * Keep SYSCLK on the existing system clock. Configure PLL1_Q separately
     * at 64 MHz for FDCAN: HSI 64 MHz / M=4 * N=16 / Q=4 = 64 MHz.
     * This matches the timing values used by FDCAN_Port.c.
     */
    RCC->CR |= RCC_CR_HSION;
    if (!RCC_WaitForSet(&RCC->CR, RCC_CR_HSIRDY))
        return RCC_DRIVER_TIMEOUT;

    if ((RCC->CR & RCC_CR_PLL1ON) != 0U)
    {
        RCC->CR &= ~RCC_CR_PLL1ON;
        if (!RCC_WaitForClear(&RCC->CR, RCC_CR_PLL1RDY))
            return RCC_DRIVER_TIMEOUT;
    }

    /* PLL source HSI (00), PLLM1=4. */
    RCC->PLLCKSELR = (RCC->PLLCKSELR &
                      ~(RCC_PLLCKSELR_PLLSRC_Msk | RCC_PLLCKSELR_DIVM1_Msk)) |
                     (4UL << RCC_PLLCKSELR_DIVM1_Pos);

    /* Disable fractional mode, select wide VCO and 8-16 MHz input range. */
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLL1FRACEN |
                      RCC_PLLCFGR_PLL1VCOSEL |
                      RCC_PLLCFGR_PLL1RGE);
    RCC->PLLCFGR |= RCC_PLLCFGR_PLL1RGE_3 | RCC_PLLCFGR_DIVQ1EN;
    RCC->PLL1FRACR = 0U;

    /* N=16, P=2, Q=4, R=2 (divider fields encode divider minus one). */
    RCC->PLL1DIVR = ((15UL << RCC_PLL1DIVR_N1_Pos) & RCC_PLL1DIVR_N1_Msk) |
                    (1UL << RCC_PLL1DIVR_P1_Pos) |
                    (3UL << RCC_PLL1DIVR_Q1_Pos) |
                    (1UL << RCC_PLL1DIVR_R1_Pos);

    RCC->CR |= RCC_CR_PLL1ON;
    if (!RCC_WaitForSet(&RCC->CR, RCC_CR_PLL1RDY))
        return RCC_DRIVER_TIMEOUT;

    /* FDCANSEL=01 selects PLL1_Q, as expected by the FDCAN driver. */
    RCC->D2CCIP1R = (RCC->D2CCIP1R & ~RCC_D2CCIP1R_FDCANSEL) |
                    RCC_D2CCIP1R_FDCANSEL_0;

    /* Enable the FDCAN peripheral bus clock. */
    RCC_EnableFDCANClock();
    return RCC_DRIVER_OK;
}

void RCC_EnableGPIOClock(uint8_t port_index)
{
    if (port_index <= 10U)
    {
        RCC->AHB4ENR |= (1UL << port_index);
        (void)RCC->AHB4ENR;
    }
}

void RCC_EnableFDCANClock(void)
{
    RCC->APB1HENR |= RCC_APB1HENR_FDCANEN;
    (void)RCC->APB1HENR;
}

void RCC_EnableUSART3Clock(void)
{
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;
    (void)RCC->APB1LENR;
}

void RCC_EnableDMA1Clock(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    (void)RCC->AHB1ENR;
}

void RCC_EnableD2SRAMClock(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_D2SRAM1EN |
                    RCC_AHB2ENR_D2SRAM2EN |
                    RCC_AHB2ENR_D2SRAM3EN;
    (void)RCC->AHB2ENR;
}

void RCC_EnableTIM2Clock(void)
{
    RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;
    (void)RCC->APB1LENR;
}

uint32_t RCC_GetPCLK1Hz(void)
{
    static const uint8_t apb_divider[8] = {1U, 1U, 1U, 1U, 2U, 4U, 8U, 16U};
    uint32_t hclk = SystemD2Clock;
    uint32_t ppre = (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) >> RCC_D2CFGR_D2PPRE1_Pos;

    if (hclk == 0U) hclk = 64000000UL;
    return hclk / apb_divider[ppre & 0x7U];
}

uint32_t RCC_GetTIM2ClockHz(void)
{
    uint32_t pclk = RCC_GetPCLK1Hz();
    uint32_t ppre = (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) >> RCC_D2CFGR_D2PPRE1_Pos;
    return (ppre >= 4U) ? (pclk * 2U) : pclk;
}
