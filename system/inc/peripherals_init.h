#ifndef PERIPHERALS_INIT_H
#define PERIPHERALS_INIT_H

#include <stdint.h>

typedef enum { MCU_ROLE_MASTER = 0, MCU_ROLE_SLAVE = 1 } MCU_Role_t;
typedef enum { PERIPHERAL_OK = 0, PERIPHERAL_ERROR = 1 } Peripheral_Status_t;

Peripheral_Status_t Peripherals_Init(MCU_Role_t role);

#endif
