#ifndef WATCHDOG_DRIVER_H
#define WATCHDOG_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { WATCHDOG_OK = 0, WATCHDOG_ERROR, WATCHDOG_TIMEOUT } Watchdog_Status_t;

Watchdog_Status_t Watchdog_Init(void);
void Watchdog_Refresh(void);
uint32_t Watchdog_GetResetFlags(void);
void Watchdog_ClearResetFlags(void);

#endif
