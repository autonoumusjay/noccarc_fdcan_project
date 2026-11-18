#ifndef FDCAN_H
#define FDCAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "stm32h743xx.h"

/*===========================================================
 * Configuration limits
 *===========================================================*/

#define FDCAN_MAX_DATA_LENGTH       64U
#define FDCAN_STANDARD_ID_MAX      0x7FFU
#define FDCAN_EXTENDED_ID_MAX      0x1FFFFFFFU


/*===========================================================
 * 1. Peripheral selection
 *===========================================================*/

typedef enum
{
    FDCAN_1 = 0U,
    FDCAN_2
} FDCAN_t;


/*===========================================================
 * 2. Operating mode
 *===========================================================*/

typedef enum
{
    FDCAN_MODE_NORMAL = 0U,
    FDCAN_MODE_INTERNAL_LOOPBACK,
    FDCAN_MODE_EXTERNAL_LOOPBACK

} FDCAN_Mode_t;


/*===========================================================
 * 3. Identifier type
 *===========================================================*/

typedef enum
{
    STANDARD = 0U,
    EXTENDED = 1U

} FDCAN_IDType_t;


/*===========================================================
 * 4. Frame format
 *===========================================================*/

typedef enum
{
    CLASSIC = 0U,
    FD = 1U

} FDCAN_FrameFormat_t;


/*===========================================================
 * 5. Bit-rate switching
 *===========================================================*/

typedef enum
{
    FDCAN_BRS_DISABLED = 0U,
    FDCAN_BRS_ENABLED

} FDCAN_BRS_t;


/*===========================================================
 * 6. Error-state indicator
 *===========================================================*/

typedef enum
{
    FDCAN_ESI_ACTIVE = 0U,
    FDCAN_ESI_PASSIVE

} FDCAN_ESI_t;


/*===========================================================
 * 7. Driver return status
 *===========================================================*/

typedef enum
{
    FDCAN_OK = 0U,

    FDCAN_ERROR_INVALID_PARAM,
    FDCAN_ERROR_INVALID_HANDLE,
    FDCAN_ERROR_NOT_INITIALIZED,
    FDCAN_ERROR_NOT_STARTED,
    FDCAN_ERROR_TX_FIFO_FULL,
    FDCAN_ERROR_RX_QUEUE_EMPTY,
    FDCAN_ERROR_TIMEOUT,
    FDCAN_ERROR_BUSY,
    FDCAN_ERROR

}FDCAN_Result_t;


/*===========================================================
 * 8. Bit timing configuration
 *===========================================================*/

/*
 * Values represent actual counts, not register-encoded values.
 *
 * Example:
 * Prescaler = 2 means the register field will be programmed
 * with 2 - 1.
 *
 * TSEG1, TSEG2 and SJW are also expressed as actual counts.
 */

typedef struct
{
    uint16_t Prescaler;
    uint8_t  SJW;
    uint8_t  TimeSeg1;
    uint8_t  TimeSeg2;

} FDCAN_BitTiming_t;


/*===========================================================
 * 9. FDCAN peripheral configuration
 *===========================================================*/

typedef struct
{
    FDCAN_t Instance;

    FDCAN_Mode_t Mode;

    FDCAN_FrameFormat_t FrameFormat;

    FDCAN_BRS_t BitRateSwitch;

    bool AutoRetransmission;

    /*
     * Nominal/arbitration phase timing.
     */
    FDCAN_BitTiming_t NominalTiming;

    /*
     * Data phase timing.
     * Used when CAN FD with BRS is enabled.
     */
    FDCAN_BitTiming_t DataTiming;

    /*
     * Message RAM allocation.
     * These are element counts, not byte sizes.
     */
    uint8_t StandardFilterCount;
    uint8_t ExtendedFilterCount;

    uint8_t RxFIFO0ElementCount;
    uint8_t RxFIFO1ElementCount;

    uint8_t TxBufferCount;

} FDCAN_Config_t;


/*===========================================================
 * 10. Transmit message structure
 *===========================================================*/

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


/*===========================================================
 * 11. Receive message structure
 *===========================================================*/

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


/*===========================================================
 * 12. Hardware status
 *===========================================================*/

typedef enum
{
    FDCAN_BUS_ERROR_ACTIVE = 0U,
    FDCAN_BUS_ERROR_PASSIVE,
    FDCAN_BUS_OFF

} FDCAN_BusState_t;


/*===========================================================
 * 13. FDCAN status information
 *===========================================================*/

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


/*===========================================================
 * 14. FDCAN handle
 *===========================================================*/
/* it is the main hadler for the FDCAN which contains all necessary structures in it.
 instances
 config
 status
 *===========================================================*/

typedef struct
{
    FDCAN_GlobalTypeDef *Instance;

    FDCAN_Config_t Config;

    bool Initialized;
    bool Started;

    FDCAN_Status_t Status;

} FDCAN_Handle_t;


/*===========================================================
 * 15. Driver function prototypes
 *===========================================================*/

/*
 * Initialize the selected FDCAN peripheral.
 */
FDCAN_Result_t FDCAN_Init(FDCAN_Handle_t *pHandle,const FDCAN_Config_t *pConfig);


/*
 * Start the FDCAN peripheral.
 */
FDCAN_Result_t FDCAN_Start(FDCAN_Handle_t *pHandle);


/*
 * Stop the FDCAN peripheral.
 */
FDCAN_Result_t FDCAN_Stop(FDCAN_Handle_t *pHandle);


/*
 * Transmit a CAN/CAN FD message.
 * Non-blocking operation.
 */
FDCAN_Result_t FDCAN_Transmit(FDCAN_Handle_t *pHandle,const FDCAN_TxMessage_t *pMessage);


/*
 * Retrieve a received message.
 * Non-blocking operation.
 */
FDCAN_Result_t FDCAN_Receive(FDCAN_Handle_t *pHandle,FDCAN_RxMessage_t *pMessage);


/*
 * Read current hardware and driver status.
 */
FDCAN_Result_t FDCAN_GetStatus(
    FDCAN_Handle_t *pHandle,
    FDCAN_Status_t *pStatus
);


/*
 * Process FDCAN interrupts.
 * Call this from the corresponding IRQ handler.
 */
void FDCAN_IRQHandler(
    FDCAN_Handle_t *pHandle
);

void FDCAN_CalculateTiming(uint32_t BitRate,uint8_t IsDataPhase,FDCAN_BitTiming_t *Timing);


#endif /* FDCAN_H */