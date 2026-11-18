#include "UART.h"
#include "GPIO.h"
#include "RCC.h"
#include "DMA.h"
#include "peripheral_config.h"
#include "stm32h743xx.h"
#include <string.h>

#define UART_DMA_BUFFER ((uint8_t *)(uintptr_t)NOCARC_UART_DMA_BUFFER_ADDRESS)
static volatile bool g_uart_tx_busy = false;
static UART_ErrorCounters_t g_uart_errors;

static void UART_CleanDCache(const void *address, uint32_t length)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    SCB_CleanDCache_by_Addr((uint32_t *)(uintptr_t)address, (int32_t)length);
#else
    (void)address;
    (void)length;
#endif
    __DMB();
}

UART_DriverStatus_t UART_Init(uint32_t baudrate)
{
    GPIO_PinConfig_t pin_cfg = {
        GPIO_MODE_ALTERNATE, GPIO_PULLUP, GPIO_SPEED_VERY_HIGH,
        GPIO_PUSH_PULL, NOCARC_UART3_AF
    };
    uint32_t pclk;
    uint32_t brr;

    if (baudrate == 0U) return UART_DRIVER_INVALID_PARAM;

    RCC_EnableD2SRAMClock();
    RCC_EnableGPIOClock(1U); /* GPIOB */
    RCC_EnableUSART3Clock();
    (void)GPIO_ConfigPin(NOCARC_UART3_GPIO_PORT, NOCARC_UART3_TX_PIN, &pin_cfg);
    (void)GPIO_ConfigPin(NOCARC_UART3_GPIO_PORT, NOCARC_UART3_RX_PIN, &pin_cfg);

    pclk = RCC_GetPCLK1Hz();
    brr = (pclk + (baudrate / 2U)) / baudrate;
    if ((brr < 16U) || (brr > 0xFFFFU)) return UART_DRIVER_ERROR;

    USART3->CR1 = 0U;
    USART3->CR2 = 0U;       /* 1 stop bit. */
    USART3->CR3 = 0U;
    USART3->PRESC = 0U;     /* PRESC encoding 0 = divide by 1. */
    USART3->BRR = brr;
    USART3->ICR = USART_ICR_PECF | USART_ICR_FECF |
                  USART_ICR_NECF | USART_ICR_ORECF | USART_ICR_TCCF;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    g_uart_tx_busy = false;
    memset(&g_uart_errors, 0, sizeof(g_uart_errors));
    return UART_DRIVER_OK;
}

UART_DriverStatus_t UART_TransmitBlocking(const uint8_t *data,
                                          uint16_t length,
                                          uint32_t timeout_loops)
{
    uint16_t i;
    if ((data == NULL) || (length == 0U)) return UART_DRIVER_INVALID_PARAM;

    for (i = 0U; i < length; i++)
    {
        uint32_t timeout = timeout_loops;
        while ((USART3->ISR & USART_ISR_TXE_TXFNF) == 0U)
        {
            if (timeout-- == 0U) return UART_DRIVER_TIMEOUT;
        }
        USART3->TDR = data[i];
    }

    {
        uint32_t timeout = timeout_loops;
        while ((USART3->ISR & USART_ISR_TC) == 0U)
        {
            if (timeout-- == 0U) return UART_DRIVER_TIMEOUT;
        }
    }
    return UART_DRIVER_OK;
}

UART_DriverStatus_t UART_TransmitDMA(const uint8_t *data, uint16_t length)
{
    DMA_DriverStatus_t status;
    if ((data == NULL) || (length == 0U) ||
        (length > NOCARC_UART_TX_BUFFER_SIZE))
        return UART_DRIVER_INVALID_PARAM;
    if (g_uart_tx_busy) return UART_DRIVER_BUSY;

    /* D2 SRAM is DMA1-accessible; first 512 bytes are reserved for this TX buffer. */
    memcpy(UART_DMA_BUFFER, data, length);
    UART_CleanDCache(UART_DMA_BUFFER, length);
    g_uart_tx_busy = true;

    status = DMA_USART3_TX_Start(UART_DMA_BUFFER, length);
    if (status != DMA_DRIVER_OK)
    {
        g_uart_tx_busy = false;
        return (status == DMA_DRIVER_BUSY) ? UART_DRIVER_BUSY : UART_DRIVER_ERROR;
    }
    return UART_DRIVER_OK;
}

bool UART_IsTransmitBusy(void)
{
    return g_uart_tx_busy;
}

bool UART_TryReceiveByte(uint8_t *byte)
{
    uint32_t status;
    if (byte == NULL) return false;

    status = USART3->ISR;
    if ((status & USART_ISR_PE) != 0U) g_uart_errors.parity_error++;
    if ((status & USART_ISR_FE) != 0U) g_uart_errors.framing_error++;
    if ((status & USART_ISR_NE) != 0U) g_uart_errors.noise_error++;
    if ((status & USART_ISR_ORE) != 0U) g_uart_errors.overrun_error++;
    if ((status & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) != 0U)
        UART_ClearErrors();

    if ((status & USART_ISR_RXNE_RXFNE) == 0U) return false;
    *byte = (uint8_t)USART3->RDR;
    return true;
}

void UART_ClearErrors(void)
{
    USART3->ICR = USART_ICR_PECF | USART_ICR_FECF |
                  USART_ICR_NECF | USART_ICR_ORECF;
}

const UART_ErrorCounters_t *UART_GetErrorCounters(void)
{
    return &g_uart_errors;
}

void UART_DMA_TransferCompleteFromISR(bool success)
{
    if (!success) g_uart_errors.dma_error++;
    g_uart_tx_busy = false;
}

bool UART_TryTransmitByte(uint8_t byte)
{
    if (g_uart_tx_busy || (USART3->ISR & USART_ISR_TXE_TXFNF) == 0U)
        return false;
    USART3->TDR = byte;
    return true;
}
