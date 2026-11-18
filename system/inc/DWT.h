#ifndef DWT_DRIVER_H
#define DWT_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

bool DWT_Init(void);
uint32_t DWT_GetCycles(void);
uint32_t DWT_GetElapsedCycles(uint32_t start_cycle);
uint32_t DWT_GetMicroseconds(void);

#endif
