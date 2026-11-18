#ifndef DMA_DRIVER_H
#define DMA_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DMA_DRIVER_OK = 0,
    DMA_DRIVER_BUSY,
    DMA_DRIVER_INVALID_PARAM,
    DMA_DRIVER_ERROR
} DMA_DriverStatus_t;

/* USART3_TX is routed to DMA1 Stream 3 through DMAMUX1 request 46. */
DMA_DriverStatus_t DMA_Init(void);
DMA_DriverStatus_t DMA_USART3_TX_Start(const uint8_t *data, uint16_t length);
bool DMA_USART3_TX_IsBusy(void);
void DMA_USART3_TX_Stop(void);
void DMA1_Stream3_IRQHandler(void);

/* Implemented by UART.c and called from the DMA interrupt. */
void UART_DMA_TransferCompleteFromISR(bool success);

#endif
