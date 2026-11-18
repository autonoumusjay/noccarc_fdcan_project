
#include "stm32h7xx.h"
#include "RCC_Clock.h"
#include "RCC.h"
#include "system_stm32h7xx.h"


/* ============================================================
 * Configuration
 * ============================================================ */

#ifndef NOCARC_RENODE
#define NOCARC_RENODE 0
#endif


/* ============================================================
 * RCC Clock Initialization
 * ============================================================ */

RCC_Status_t RCC_Clock_Init(void)
{
    RCC_Status_t status;

    /* 1. Configure voltage scaling */
    status = RCC_ConfigurePower();

    if (status != RCC_OK)
        return status;


    /* 2. Configure Flash latency */
    status = RCC_ConfigureFlash();

    if (status != RCC_OK)
        return status;


    /* 3. Enable external high-speed oscillator */
    status = RCC_EnableHSE();

    if (status != RCC_OK)
        return status;


    /* 4. Configure PLL1 */
    status = RCC_ConfigurePLL1();

    if (status != RCC_OK)
        return status;


    /* 5. Configure bus prescalers */
    status = RCC_ConfigureBusClocks();

    if (status != RCC_OK)
        return status;


    /* 6. Switch system clock to PLL1 */

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL1;


    /* Wait until PLL1 is selected as system clock */

    uint32_t timeout = RCC_TIMEOUT_VALUE;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL1)
    {
        if (timeout-- == 0U)
            return RCC_ERROR_CLOCK_SWITCH;
    }


    /* Update CMSIS clock-frequency variables */

    SystemCoreClockUpdate();


    /* 7. Configure FDCAN kernel clock */

    status = RCC_ConfigureFDCANClock();

    if (status != RCC_OK)
        return status;


    return RCC_OK;
}


/* ============================================================
 * Power Configuration
 * ============================================================ */

RCC_Status_t RCC_ConfigurePower(void)
{
    uint32_t timeout;


    if (NOCARC_RENODE)
    {
        /* Skip power configuration in simulation mode */
        return RCC_OK;
    }


    /* Check that internal LDO supply is selected */

    if ((PWR->CR3 & PWR_CR3_LDOEN) == 0U)
    {
        return RCC_ERROR_POWER;
    }


    /* Wait until supply voltage becomes valid */

    timeout = RCC_TIMEOUT_VALUE;

    while ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) == 0U)
    {
        if (timeout-- == 0U)
        {
            return RCC_ERROR_TIMEOUT;
        }
    }


    /* Select voltage scaling Range 1 */

    PWR->D3CR &= ~PWR_D3CR_VOS;
    PWR->D3CR |= PWR_D3CR_VOS;


    /* Wait until voltage scaling is ready */

    timeout = RCC_TIMEOUT_VALUE;

    while ((PWR->D3CR & PWR_D3CR_VOSRDY) == 0U)
    {
        if (timeout-- == 0U)
        {
            return RCC_ERROR_TIMEOUT;
        }
    }


    return RCC_OK;
}


/* ============================================================
 * Flash Configuration
 * ============================================================ */

RCC_Status_t RCC_ConfigureFlash(void)
{
    uint32_t acr_value;


    /* Read current Flash configuration */

    acr_value = FLASH->ACR;


    /* Configure Flash latency and programming delay */

    acr_value &= ~(FLASH_ACR_LATENCY |
                   FLASH_ACR_WRHIGHFREQ);


    acr_value |= FLASH_ACR_LATENCY_2WS |
                 FLASH_ACR_WRHIGHFREQ_1;


    FLASH->ACR = acr_value;


    /* Verify configuration */

    if ((FLASH->ACR & (FLASH_ACR_LATENCY |
                       FLASH_ACR_WRHIGHFREQ)) !=
        (FLASH_ACR_LATENCY_2WS |
         FLASH_ACR_WRHIGHFREQ_1))
    {
        return RCC_ERROR_TIMEOUT;
    }


    return RCC_OK;
}


/* ============================================================
 * HSE Oscillator Configuration
 * ============================================================ */

RCC_Status_t RCC_EnableHSE(void)
{
    uint32_t timeout = RCC_TIMEOUT_VALUE;


    /* Enable HSE oscillator */

    RCC->CR |= RCC_CR_HSEON;


    /* Wait until HSE becomes ready */

    while ((RCC->CR & RCC_CR_HSERDY) == 0U)
    {
        if (timeout == 0U)
        {
            return RCC_ERROR_HSE;
        }

        timeout--;
    }


    return RCC_OK;
}


/* ============================================================
 * PLL1 Configuration
 * ============================================================ */

RCC_Status_t RCC_ConfigurePLL1(void)
{
    uint32_t timeout = RCC_TIMEOUT_VALUE;


    /* Disable PLL1 if already enabled */

    RCC->CR &= ~RCC_CR_PLL1ON;


    /* Wait until PLL1 is disabled */

    while (RCC->CR & RCC_CR_PLL1RDY)
    {
        if (timeout-- == 0U)
            return RCC_ERROR_PLL;
    }


    /*
     * PLL1 Configuration
     *
     * Source = HSE
     * M      = 5
     * N      = 160
     * P      = 2
     * Q      = 10
     */


    /* Configure PLL source and divider M */

    RCC->PLLCKSELR =
        (RCC->PLLCKSELR &
        ~(RCC_PLLCKSELR_PLLSRC |
          RCC_PLLCKSELR_DIVM1)) |

        RCC_PLLCKSELR_PLLSRC_HSE |

        (5U << RCC_PLLCKSELR_DIVM1_Pos);


    /* Configure PLL1 VCO range and outputs */

    RCC->PLLCFGR =
        (RCC->PLLCFGR &
        ~(RCC_PLLCFGR_PLL1RGE |
          RCC_PLLCFGR_PLL1VCOSEL |
          RCC_PLLCFGR_PLL1FRACEN)) |

        RCC_PLLCFGR_PLL1RGE_2 |
        RCC_PLLCFGR_DIVP1EN |
        RCC_PLLCFGR_DIVQ1EN;


    /* Configure PLL1 N, P and Q dividers */

    RCC->PLL1DIVR =
        ((160U - 1U) << RCC_PLL1DIVR_N1_Pos) |
        ((2U   - 1U) << RCC_PLL1DIVR_P1_Pos) |
        ((10U  - 1U) << RCC_PLL1DIVR_Q1_Pos);


    /* Disable fractional mode */

    RCC->PLL1FRACR = 0U;


    /* Enable PLL1 */

    RCC->CR |= RCC_CR_PLL1ON;


    /* Wait until PLL1 becomes ready */

    timeout = RCC_TIMEOUT_VALUE;

    while ((RCC->CR & RCC_CR_PLL1RDY) == 0U)
    {
        if (timeout-- == 0U)
            return RCC_ERROR_PLL;
    }


    return RCC_OK;
}


/* ============================================================
 * Bus Clock Configuration
 * ============================================================ */

RCC_Status_t RCC_ConfigureBusClocks(void)
{

    /*
     * D1 Domain
     *
     * CPU  = /1
     * AHB  = /2
     * APB3 = /2
     */

    RCC->D1CFGR =
        (RCC->D1CFGR &
        ~(RCC_D1CFGR_D1CPRE |
          RCC_D1CFGR_HPRE |
          RCC_D1CFGR_D1PPRE)) |

        (8U << RCC_D1CFGR_HPRE_Pos) |
        (4U << RCC_D1CFGR_D1PPRE_Pos);


    /*
     * D2 Domain
     *
     * APB1 = /2
     * APB2 = /2
     */

    RCC->D2CFGR =
        (RCC->D2CFGR &
        ~(RCC_D2CFGR_D2PPRE1 |
          RCC_D2CFGR_D2PPRE2)) |

        (4U << RCC_D2CFGR_D2PPRE1_Pos) |
        (4U << RCC_D2CFGR_D2PPRE2_Pos);


    /*
     * D3 Domain
     *
     * APB4 = /2
     */

    RCC->D3CFGR =
        (RCC->D3CFGR &
        ~RCC_D3CFGR_D3PPRE) |

        (4U << RCC_D3CFGR_D3PPRE_Pos);


    return RCC_OK;
}


/* ============================================================
 * FDCAN Kernel Clock Configuration
 * ============================================================ */

RCC_Status_t RCC_ConfigureFDCANClock(void)
{

    /* Select PLL1Q as FDCAN kernel clock */

    RCC->D2CCIP1R &= ~RCC_D2CCIP1R_FDCANSEL;

    RCC->D2CCIP1R |= RCC_D2CCIP1R_FDCANSEL_0;


    /* Enable FDCAN peripheral clock */

    RCC->APB1HENR |= RCC_APB1HENR_FDCANEN;


    /* Read back register */

    (void)RCC->APB1HENR;


    return RCC_OK;
}


/* ============================================================
 * GPIO Peripheral Clock
 * ============================================================ */

/*
 * GPIO port index:
 *
 * A = 0
 * B = 1
 * C = 2
 * D = 3
 * E = 4
 * F = 5
 * G = 6
 * H = 7
 * I = 8
 * J = 9
 * K = 10
 */

void RCC_EnableGPIOClock(uint8_t port_index)
{
    if (port_index <= 10U)
    {
        RCC->AHB4ENR |= (1UL << port_index);

        (void)RCC->AHB4ENR;
    }
}


/* ============================================================
 * FDCAN Peripheral Clock
 * ============================================================ */

void RCC_EnableFDCANClock(void)
{
    RCC->APB1HENR |= RCC_APB1HENR_FDCANEN;

    (void)RCC->APB1HENR;
}


/* ============================================================
 * USART3 Peripheral Clock
 * ============================================================ */

void RCC_EnableUSART3Clock(void)
{
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;

    (void)RCC->APB1LENR;
}


/* ============================================================
 * DMA1 Peripheral Clock
 * ============================================================ */

void RCC_EnableDMA1Clock(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    (void)RCC->AHB1ENR;
}


/* ============================================================
 * D2 SRAM Clock
 * ============================================================ */

void RCC_EnableD2SRAMClock(void)
{
    RCC->AHB2ENR |=
        RCC_AHB2ENR_D2SRAM1EN |
        RCC_AHB2ENR_D2SRAM2EN |
        RCC_AHB2ENR_D2SRAM3EN;

    (void)RCC->AHB2ENR;
}


/* ============================================================
 * TIM2 Peripheral Clock
 * ============================================================ */

void RCC_EnableTIM2Clock(void)
{
    RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;

    (void)RCC->APB1LENR;
}


/* ============================================================
 * APB1 Clock Frequency Calculation
 * ============================================================ */

uint32_t RCC_GetPCLK1Hz(void)
{
    static const uint8_t apb_divider[8] =
    {
        1U,
        1U,
        1U,
        1U,
        2U,
        4U,
        8U,
        16U
    };


    uint32_t hclk = SystemD2Clock;


    uint32_t ppre =
        (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) >>
        RCC_D2CFGR_D2PPRE1_Pos;


    if (hclk == 0U)
    {
        hclk = 64000000UL;
    }


    return hclk / apb_divider[ppre & 0x7U];
}


/* ============================================================
 * TIM2 Input Clock Frequency Calculation
 * ============================================================ */

uint32_t RCC_GetTIM2ClockHz(void)
{
    uint32_t pclk = RCC_GetPCLK1Hz();


    uint32_t ppre =
        (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) >>
        RCC_D2CFGR_D2PPRE1_Pos;


    /*
     * When APB1 prescaler is greater than 1,
     * TIM2 clock = 2 x PCLK1.
     */

    if (ppre >= 4U)
    {
        return pclk * 2U;
    }

    return pclk;
}


/* ============================================================
 * RCC Driver Initialization Wrapper
 * ============================================================ */

/*
 * This function is required by peripherals_init.c.
 *
 * It calls the existing RCC_Clock_Init().
 *
 * IMPORTANT:
 * Do not configure PLL1 again here.
 */

RCC_DriverStatus_t RCC_Init(void)
{
    RCC_Status_t status;


    status = RCC_Clock_Init();


    if (status == RCC_OK)
    {
        return RCC_DRIVER_OK;
    }


    return RCC_DRIVER_ERROR;
}