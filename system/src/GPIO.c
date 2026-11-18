#include "GPIO.h"
#include "RCC.h"
#include "peripheral_config.h"
#include <stddef.h>

static bool GPIO_GetPortIndex(GPIO_TypeDef *port, uint8_t *index)
{
    if ((port == NULL) || (index == NULL)) return false;
    if (port == GPIOA) *index = 0U;
    else if (port == GPIOB) *index = 1U;
    else if (port == GPIOC) *index = 2U;
    else if (port == GPIOD) *index = 3U;
    else if (port == GPIOE) *index = 4U;
    else if (port == GPIOF) *index = 5U;
    else if (port == GPIOG) *index = 6U;
    else if (port == GPIOH) *index = 7U;
    else if (port == GPIOI) *index = 8U;
    else if (port == GPIOJ) *index = 9U;
    else if (port == GPIOK) *index = 10U;
    else return false;
    return true;
}

bool GPIO_ConfigPin(GPIO_TypeDef *port, uint8_t pin, const GPIO_PinConfig_t *config)
{
    uint8_t port_index;
    uint32_t shift;
    uint32_t afr_index;
    uint32_t afr_shift;

    if ((pin > 15U) || (config == NULL) || !GPIO_GetPortIndex(port, &port_index))
        return false;
    if ((uint32_t)config->mode > 3U || (uint32_t)config->pull > 2U ||
        (uint32_t)config->speed > 3U || (uint32_t)config->output_type > 1U ||
        config->alternate_function > 15U)
        return false;

    RCC_EnableGPIOClock(port_index);
    shift = (uint32_t)pin * 2U;
    port->MODER = (port->MODER & ~(3UL << shift)) |
                  ((uint32_t)config->mode << shift);
    port->OTYPER = (port->OTYPER & ~(1UL << pin)) |
                   ((uint32_t)config->output_type << pin);
    port->OSPEEDR = (port->OSPEEDR & ~(3UL << shift)) |
                    ((uint32_t)config->speed << shift);
    port->PUPDR = (port->PUPDR & ~(3UL << shift)) |
                  ((uint32_t)config->pull << shift);

    if (config->mode == GPIO_MODE_ALTERNATE)
    {
        afr_index = (uint32_t)pin / 8U;
        afr_shift = ((uint32_t)pin % 8U) * 4U;
        port->AFR[afr_index] = (port->AFR[afr_index] & ~(0xFUL << afr_shift)) |
                               ((uint32_t)config->alternate_function << afr_shift);
    }
    return true;
}

void GPIO_Init(void)
{
    const GPIO_PinConfig_t fdcan_pin = {
        GPIO_MODE_ALTERNATE, GPIO_NOPULL, GPIO_SPEED_VERY_HIGH,
        GPIO_PUSH_PULL, NOCARC_FDCAN1_AF
    };

    (void)GPIO_ConfigPin(NOCARC_FDCAN1_GPIO_PORT, NOCARC_FDCAN1_RX_PIN, &fdcan_pin);
    (void)GPIO_ConfigPin(NOCARC_FDCAN1_GPIO_PORT, NOCARC_FDCAN1_TX_PIN, &fdcan_pin);
}

void GPIO_WritePin(GPIO_TypeDef *port, uint8_t pin, bool high)
{
    if ((port == NULL) || (pin > 15U)) return;
    port->BSRR = high ? (1UL << pin) : (1UL << (pin + 16U));
}

void GPIO_TogglePin(GPIO_TypeDef *port, uint8_t pin)
{
    if ((port == NULL) || (pin > 15U)) return;
    GPIO_WritePin(port, pin, ((port->ODR & (1UL << pin)) == 0U));
}

bool GPIO_ReadPin(GPIO_TypeDef *port, uint8_t pin)
{
    if ((port == NULL) || (pin > 15U)) return false;
    return ((port->IDR & (1UL << pin)) != 0U);
}
