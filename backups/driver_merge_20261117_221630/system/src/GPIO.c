#include "GPIO.h"
#include <stddef.h>



void Clk_Enable(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        GPIOA_CLK_ENABLE();
    }
    else if (port == GPIOB)
    {
        GPIOB_CLK_ENABLE();
    }
    else if (port == GPIOC)
    {
        GPIOC_CLK_ENABLE();
    }
    else if (port == GPIOD)
    {
        GPIOD_CLK_ENABLE();
    }
    else if (port == GPIOE)
    {
        GPIOE_CLK_ENABLE();
    }
    else if (port == GPIOF)
    {
        GPIOF_CLK_ENABLE();
    }
    else if (port == GPIOG)
    {
        GPIOG_CLK_ENABLE();
    }
    else if (port == GPIOH)
    {
        GPIOH_CLK_ENABLE();
    }
    else if (port == GPIOI)
    {
        GPIOI_CLK_ENABLE();
    }
    else if (port == GPIOJ)
    {
        GPIOJ_CLK_ENABLE();
    }
    else if (port == GPIOK)
    {
        GPIOK_CLK_ENABLE();
    }
}


void GPIO_Init(GPIO_Handle_t *port_handle)
{
    Clk_Enable(port_handle->port);

    GPIO_TypeDef *port;
    GPIO_Config_t config;

    uint32_t pin;
    uint32_t position;

    /* Validate pointers */
    if ((port_handle == NULL) || (port_handle->port == NULL))    
    {
        return;
    }

    port   = port_handle->port;
    config = port_handle->config;

    pin = (uint32_t)config.pin;

    /* Validate pin number */
    if (pin > 15U)
    {
        return;
    }

    /* Validate configuration values */
    if ((config.mode > GPIO_MODE_ANALOG) ||
        (config.op_type > GPIO_OUTPUT_OD) ||
        (config.speed > GPIO_SPEED_VERY_HIGH) ||
        (config.pupdctrl > GPIO_PULL_DOWN) ||
        (config.alternate_function > 15U) ||
        (config.state > HIGH))
    {
        return;
    }

    position = pin * 2U;

    /* 1. Configure initial output state */
    if (config.mode == GPIO_MODE_OUTPUT)
    {
        if (config.state == HIGH)
        {
            port->BSRR = (1U << pin);
        }
        else
        {
            port->BSRR = (1U << (pin + 16U));
        }
    }

    /* 2. Configure GPIO Mode */
    if(config.mode >= GPIO_MODE_IT_RISING)
    {
        /* Configure the pin as input for interrupt mode */
        port->MODER &= ~(0x3U << position);
        port->MODER |= (GPIO_MODE_INPUT << position);

        /* Interrupt configuration */



    }
    else
    {
        /* Configure the pin as per the specified mode */
        port->MODER &= ~(0x3U << position);
        port->MODER |= ((uint32_t)config.mode << position);
    }

    /* 3. Configure Output Type */
    port->OTYPER &= ~(1U << pin);
    port->OTYPER |= ((uint32_t)config.op_type << pin);

    /* 4. Configure Output Speed */
    port->OSPEEDR &= ~(0x3U << position);
    port->OSPEEDR |= ((uint32_t)config.speed << position);

    /* 5. Configure Pull-up / Pull-down */
    port->PUPDR &= ~(0x3U << position);
    port->PUPDR |= ((uint32_t)config.pupdctrl << position);

    /* 6. Configure Alternate Function */
    if (config.mode == GPIO_MODE_AF)
    {
        uint32_t afr_index;
        uint32_t afr_position;

        afr_index = pin / 8U;
        afr_position = (pin % 8U) * 4U;

        port->AFR[afr_index] &= ~(0xFU << afr_position);

        port->AFR[afr_index] |=
            ((uint32_t)config.alternate_function << afr_position);
    }
}

void GPIO_WritePin(GPIO_TypeDef *port, GPIO_Pin pin, GPIO_OP state)
{
    if ((port == NULL) || ((uint32_t)pin > 15U))
    {
        return;
    }

    if (state == HIGH)
    {
        /* Set pin */
        port->BSRR = (1U << (uint32_t)pin);
    }
    else if (state == LOW)
    {
        /* Reset pin */
        port->BSRR = (1U << ((uint32_t)pin + 16U));
    }
}

GPIO_OP GPIO_ReadPin(GPIO_TypeDef *port, GPIO_Pin pin)
{
    if ((port == NULL) || ((uint32_t)pin > 15U))
    {
        return LOW;
    }

    if (port->IDR & (1U << (uint32_t)pin))
    {
        return HIGH;
    }

    return LOW;
}


void GPIO_TogglePin(GPIO_TypeDef *port, GPIO_Pin pin)
{
    if ((port == NULL) || ((uint32_t)pin > 15U))
    {
        return;
    }

    port->ODR ^= (1U << (uint32_t)pin);
}