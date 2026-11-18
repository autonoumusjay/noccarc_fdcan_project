#include "DMA.h"
#include "RCC.h"
#include "NVIC.h"
#include "peripheral_config.h"
#include "stm32h743xx.h"
#include <stddef.h>

#define DMA1_STREAM3_ALL_FLAGS ((1UL << 22) | (1UL << 24) | (1UL << 25) | \
                                (1UL << 26) | (1UL << 27))
#define DMA1_STREAM3_TCIF      (1UL << 27)
#define DMA1_STREAM3_TEIF      (1UL << 25)
#define DMA1_STREAM3_DMEIF     (1UL << 24)
#define DMA1_STREAM3_FEIF      (1UL << 22)

static volatile bool g_uart3_tx_dma_busy = false;

DMA_DriverStatus_t DMA_Init(void)
{
    RCC_EnableDMA1Clock();

    /* DMA1 Stream 3 is connected to DMAMUX1 Channel 3. */
    DMA1_Stream3->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream3->CR & DMA_SxCR_EN) != 0U) { }
    DMA1->LIFCR = DMA1_STREAM3_ALL_FLAGS;
    DMAMUX1_Channel3->CCR = NOCARC_UART3_DMA_REQUEST_ID;

    NVIC_ConfigureIRQ(DMA1_Stream3_IRQn, 5U, true);
    g_uart3_tx_dma_busy = false;
    return DMA_DRIVER_OK;
}

DMA_DriverStatus_t DMA_USART3_TX_Start(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U)) return DMA_DRIVER_INVALID_PARAM;
    if (g_uart3_tx_dma_busy) return DMA_DRIVER_BUSY;

    DMA1_Stream3->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream3->CR & DMA_SxCR_EN) != 0U) { }
    DMA1->LIFCR = DMA1_STREAM3_ALL_FLAGS;

    DMA1_Stream3->PAR = (uint32_t)(uintptr_t)&USART3->TDR;
    DMA1_Stream3->M0AR = (uint32_t)(uintptr_t)data;
    DMA1_Stream3->NDTR = length;
    DMA1_Stream3->FCR = 0U; /* Direct mode, single transfers. */

    DMA1_Stream3->CR = DMA_SxCR_DIR_0 | DMA_SxCR_MINC |
                       (2UL << DMA_SxCR_PL_Pos) |
                       DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;

    USART3->CR3 |= USART_CR3_DMAT;
    g_uart3_tx_dma_busy = true;
    __DMB();
    DMA1_Stream3->CR |= DMA_SxCR_EN;
    return DMA_DRIVER_OK;
}

bool DMA_USART3_TX_IsBusy(void)
{
    return g_uart3_tx_dma_busy;
}

void DMA_USART3_TX_Stop(void)
{
    DMA1_Stream3->CR &= ~DMA_SxCR_EN;
    USART3->CR3 &= ~USART_CR3_DMAT;
    bool aborted = g_uart3_tx_dma_busy;
    g_uart3_tx_dma_busy = false;
    DMA1->LIFCR = DMA1_STREAM3_ALL_FLAGS;
    NVIC_ClearPendingIRQ(DMA1_Stream3_IRQn);
    if (aborted)
        UART_DMA_TransferCompleteFromISR(false);
}

void DMA1_Stream3_IRQHandler(void)
{
    uint32_t status = DMA1->LISR;
    bool success = ((status & DMA1_STREAM3_TCIF) != 0U);
    bool error = ((status & (DMA1_STREAM3_TEIF | DMA1_STREAM3_DMEIF |
                             DMA1_STREAM3_FEIF)) != 0U);

    DMA1->LIFCR = DMA1_STREAM3_ALL_FLAGS;
    DMA1_Stream3->CR &= ~DMA_SxCR_EN;
    USART3->CR3 &= ~USART_CR3_DMAT;
    g_uart3_tx_dma_busy = false;

    UART_DMA_TransferCompleteFromISR(success && !error);
}
