#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32h743xx.h"

typedef enum { GPIO_MODE_INPUT = 0U, GPIO_MODE_OUTPUT = 1U,
               GPIO_MODE_ALTERNATE = 2U, GPIO_MODE_ANALOG = 3U } GPIO_Mode_t;
typedef enum { GPIO_NOPULL = 0U, GPIO_PULLUP = 1U,
               GPIO_PULLDOWN = 2U } GPIO_Pull_t;
typedef enum { GPIO_SPEED_LOW = 0U, GPIO_SPEED_MEDIUM = 1U,
               GPIO_SPEED_HIGH = 2U, GPIO_SPEED_VERY_HIGH = 3U } GPIO_Speed_t;
typedef enum { GPIO_PUSH_PULL = 0U, GPIO_OPEN_DRAIN = 1U } GPIO_OutputType_t;

typedef struct
{
    GPIO_Mode_t mode;
    GPIO_Pull_t pull;
    GPIO_Speed_t speed;
    GPIO_OutputType_t output_type;
    uint8_t alternate_function;
} GPIO_PinConfig_t;

void GPIO_Init(void);
bool GPIO_ConfigPin(GPIO_TypeDef *port, uint8_t pin, const GPIO_PinConfig_t *config);
void GPIO_WritePin(GPIO_TypeDef *port, uint8_t pin, bool high);
void GPIO_TogglePin(GPIO_TypeDef *port, uint8_t pin);
bool GPIO_ReadPin(GPIO_TypeDef *port, uint8_t pin);

#endif
