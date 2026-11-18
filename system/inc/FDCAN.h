#ifndef FDCAN_DRIVER_H
#define FDCAN_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32h743xx.h"

#define FDCAN_STANDARD_ID_MAX  0x7FFUL
#define FDCAN_EXTENDED_ID_MAX  0x1FFFFFFFUL
#define FDCAN_MAX_DATA_LENGTH  64U

typedef enum { FDCAN_1 = 0, FDCAN_2 = 1 } FDCAN_t;
typedef enum { FDCAN_MODE_NORMAL = 0 } FDCAN_Mode_t;
typedef enum { CLASSIC = 0, FD = 1 } FDCAN_FrameFormat_t;
typedef enum { FDCAN_BRS_DISABLED = 0, FDCAN_BRS_ENABLED = 1 } FDCAN_BRS_t;
typedef enum { STANDARD = 0, EXTENDED = 1 } FDCAN_IDType_t;
typedef enum { FDCAN_ESI_ACTIVE = 0, FDCAN_ESI_PASSIVE = 1 } FDCAN_ESI_t;
typedef enum { FDCAN_BUS_ERROR_ACTIVE = 0, FDCAN_BUS_ERROR_PASSIVE,
               FDCAN_BUS_OFF } FDCAN_BusState_t;
typedef enum
{
    FDCAN_OK = 0,
    FDCAN_ERROR,
    FDCAN_ERROR_INVALID_PARAM,
    FDCAN_ERROR_NOT_INITIALIZED,
    FDCAN_ERROR_NOT_STARTED,
    FDCAN_ERROR_TX_FIFO_FULL,
    FDCAN_ERROR_RX_QUEUE_EMPTY,
    FDCAN_ERROR_TIMEOUT
} FDCAN_Result_t;

typedef struct
{
    uint16_t Prescaler;
    uint16_t SJW;
    uint16_t TimeSeg1;
    uint16_t TimeSeg2;
} FDCAN_BitTiming_t;

typedef struct
{
    FDCAN_t Instance;
    FDCAN_Mode_t Mode;
    FDCAN_FrameFormat_t FrameFormat;
    FDCAN_BRS_t BitRateSwitch;
    FDCAN_BitTiming_t NominalTiming;
    FDCAN_BitTiming_t DataTiming;
    bool AutoRetransmission;
    uint8_t StandardFilterCount;
    uint8_t ExtendedFilterCount;
    uint8_t RxFIFO0ElementCount;
    uint8_t RxFIFO1ElementCount;
    uint8_t TxBufferCount;
} FDCAN_Config_t;

typedef struct
{
    uint32_t Identifier;
    FDCAN_IDType_t IDType;
    FDCAN_FrameFormat_t FrameFormat;
    FDCAN_BRS_t BitRateSwitch;
    FDCAN_ESI_t ErrorStateIndicator;
    uint8_t DataLength;
    uint8_t Data[FDCAN_MAX_DATA_LENGTH];
} FDCAN_TxMessage_t;

typedef struct
{
    uint32_t Identifier;
    FDCAN_IDType_t IDType;
    FDCAN_FrameFormat_t FrameFormat;
    FDCAN_BRS_t BitRateSwitch;
    FDCAN_ESI_t ErrorStateIndicator;
    uint8_t DataLength;
    uint8_t Data[FDCAN_MAX_DATA_LENGTH];
    uint16_t Timestamp;
    uint8_t FilterIndex;
} FDCAN_RxMessage_t;

typedef struct
{
    bool Initialized;
    bool Started;
    FDCAN_BusState_t BusState;
    uint8_t TxErrorCount;
    uint8_t RxErrorCount;
    uint8_t RxFIFO0FillLevel;
    uint8_t RxFIFO1FillLevel;
    uint8_t TxFreeLevel;
    uint32_t ProtocolStatus;
    uint32_t InterruptFlags;
} FDCAN_Status_t;

typedef struct
{
    FDCAN_GlobalTypeDef *Instance;
    FDCAN_Config_t Config;
    bool Initialized;
    bool Started;
    FDCAN_Status_t Status;
} FDCAN_Handle_t;

FDCAN_Result_t FDCAN_Init(FDCAN_Handle_t *handle, const FDCAN_Config_t *config);
FDCAN_Result_t FDCAN_Start(FDCAN_Handle_t *handle);
FDCAN_Result_t FDCAN_Stop(FDCAN_Handle_t *handle);
FDCAN_Result_t FDCAN_Transmit(FDCAN_Handle_t *handle, const FDCAN_TxMessage_t *message);
FDCAN_Result_t FDCAN_Receive(FDCAN_Handle_t *handle, FDCAN_RxMessage_t *message);
FDCAN_Result_t FDCAN_GetStatus(FDCAN_Handle_t *handle, FDCAN_Status_t *status);
void FDCAN_IRQHandler(FDCAN_Handle_t *handle);

#endif
