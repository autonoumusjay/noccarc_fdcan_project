#ifndef RCC_CLOCK_H
#define RCC_CLOCK_H

#include <stdint.h>

#define RCC_TIMEOUT_VALUE    1000000U

typedef enum
{
    RCC_OK = 0,
    RCC_ERROR_TIMEOUT,
    RCC_ERROR_HSE,
    RCC_ERROR_PLL,
    RCC_ERROR_CLOCK_SWITCH,
    RCC_ERROR_POWER
} RCC_Status_t;

RCC_Status_t RCC_Clock_Init(void);

RCC_Status_t RCC_ConfigurePower(void);
RCC_Status_t RCC_ConfigureFlash(void);
RCC_Status_t RCC_EnableHSE(void);
RCC_Status_t RCC_ConfigurePLL1(void);
RCC_Status_t RCC_ConfigureBusClocks(void);
RCC_Status_t RCC_ConfigureFDCANClock(void);

#endif