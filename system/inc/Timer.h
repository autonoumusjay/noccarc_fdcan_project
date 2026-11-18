#ifndef TIMER_DRIVER_H
#define TIMER_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { TIMER_OK = 0, TIMER_ERROR } Timer_Status_t;

/* TIM2 generates a 1 ms update interrupt used as the local timebase. */
Timer_Status_t Timer_Init(void);
uint32_t Timer_GetTick(void);
bool Timer_IsElapsed(uint32_t start_tick, uint32_t period_ms);

#endif
