#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UART_DRIVER_OK = 0,
    UART_DRIVER_BUSY,
    UART_DRIVER_INVALID_PARAM,
    UART_DRIVER_TIMEOUT,
    UART_DRIVER_ERROR
} UART_DriverStatus_t;

typedef struct
{
    uint32_t parity_error;
    uint32_t framing_error;
    uint32_t noise_error;
    uint32_t overrun_error;
    uint32_t dma_error;
} UART_ErrorCounters_t;

UART_DriverStatus_t UART_Init(uint32_t baudrate);
UART_DriverStatus_t UART_TransmitBlocking(const uint8_t *data, uint16_t length,
                                          uint32_t timeout_loops);
UART_DriverStatus_t UART_TransmitDMA(const uint8_t *data, uint16_t length);
bool UART_IsTransmitBusy(void);
bool UART_TryTransmitByte(uint8_t byte);
bool UART_TryReceiveByte(uint8_t *byte);
void UART_ClearErrors(void);
const UART_ErrorCounters_t *UART_GetErrorCounters(void);
void UART_DMA_TransferCompleteFromISR(bool success);

#endif
