#ifndef RCC_DRIVER_H
#define RCC_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    RCC_DRIVER_OK = 0,
    RCC_DRIVER_ERROR,
    RCC_DRIVER_TIMEOUT
} RCC_DriverStatus_t;

RCC_DriverStatus_t RCC_Init(void);
void RCC_EnableGPIOClock(uint8_t port_index);
void RCC_EnableFDCANClock(void);
void RCC_EnableUSART3Clock(void);
void RCC_EnableDMA1Clock(void);
void RCC_EnableD2SRAMClock(void);
void RCC_EnableTIM2Clock(void);
uint32_t RCC_GetPCLK1Hz(void);
uint32_t RCC_GetTIM2ClockHz(void);

#endif
