#ifndef GPIO_H
#define GPIO_H

#include "stm32h743xx.h"


#define GPIOA_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN)
#define GPIOB_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN)
#define GPIOC_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN)
#define GPIOD_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN)
#define GPIOE_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN)
#define GPIOF_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOFEN)
#define GPIOG_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOGEN)
#define GPIOH_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOHEN)
#define GPIOI_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOIEN)
#define GPIOJ_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOJEN)
#define GPIOK_CLK_ENABLE()    (RCC->AHB4ENR |= RCC_AHB4ENR_GPIOKEN)

typedef enum
{
    GPIO_PIN_0 = 0,
    GPIO_PIN_1,
    GPIO_PIN_2,
    GPIO_PIN_3,
    GPIO_PIN_4,
    GPIO_PIN_5,
    GPIO_PIN_6,
    GPIO_PIN_7,
    GPIO_PIN_8,
    GPIO_PIN_9,
    GPIO_PIN_10,
    GPIO_PIN_11,
    GPIO_PIN_12,
    GPIO_PIN_13,
    GPIO_PIN_14,
    GPIO_PIN_15
} GPIO_Pin;


/* GPIO Modes */
typedef enum
{
    GPIO_MODE_INPUT  = 0x00,
    GPIO_MODE_OUTPUT = 0x01,
    GPIO_MODE_AF     = 0x02,
    GPIO_MODE_ANALOG = 0x03,
    GPIO_MODE_IT_RISING = 0x04,
    GPIO_MODE_IT_FALLING = 0x05,
    GPIO_MODE_IT_RISING_FALLING = 0x06
} GPIO_Mode;


/* Output Type */
typedef enum
{
    GPIO_OUTPUT_PP = 0x00,     // Push-Pull
    GPIO_OUTPUT_OD = 0x01      // Open-Drain

} GPIO_OutputType;


/* GPIO Output Speed */
typedef enum
{
    GPIO_SPEED_LOW       = 0x00,
    GPIO_SPEED_MEDIUM    = 0x01,
    GPIO_SPEED_HIGH      = 0x02,
    GPIO_SPEED_VERY_HIGH = 0x03

} GPIO_Speed;


/* GPIO Pull-up / Pull-down */
typedef enum
{
    GPIO_NO_PULL   = 0x00,
    GPIO_PULL_UP   = 0x01,
    GPIO_PULL_DOWN = 0x02

} GPIO_PUPD;


/* GPIO Output State */
typedef enum
{
    LOW  = 0x00,
    HIGH = 0x01

} GPIO_OP;


/* GPIO Configuration Structure */
typedef struct
{
    GPIO_Pin            pin;
    GPIO_Mode           mode;
    GPIO_OutputType     op_type;
    GPIO_Speed          speed;
    GPIO_PUPD           pupdctrl;
    uint8_t             alternate_function;
    GPIO_OP             state;
} GPIO_Config_t;

typedef struct
{
    GPIO_TypeDef *port;
    GPIO_Config_t config;
} GPIO_Handle_t;


/* GPIO Functions  Prototype */
void GPIO_Init(GPIO_Handle_t *port_handle);

void GPIO_WritePin(GPIO_TypeDef *port, GPIO_Pin pin, GPIO_OP state);

GPIO_OP GPIO_ReadPin(GPIO_TypeDef *port, GPIO_Pin pin);

void GPIO_TogglePin(GPIO_TypeDef *port, GPIO_Pin pin);

#endif