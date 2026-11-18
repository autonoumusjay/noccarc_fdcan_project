#include "FDCAN.h"
#include <stddef.h>

/*
 * STM32H743 FDCAN driver
 *
 * Assumptions / limitations of this version:
 * - Uses PLL1_Q as the FDCAN kernel clock. RCC clock setup must enable
 *   the PLL1_Q output before PLL1 is enabled and calculate its frequency.
 * - Uses 64-byte RX/TX Message RAM elements.
 * - Reserves half of the shared 10-KB Message RAM for each instance.
 * - Routes unmatched standard/extended frames to RX FIFO 0.
 * - RX interrupts notify the application; FDCAN_Receive() copies frames
 *   from Message RAM in main context (deferred processing).
 * - GPIO alternate functions/transceiver configuration are external.
 */

#define FDCAN_TIMEOUT               1000000UL
#define FDCAN_RAM_TOTAL_WORDS       2560U
#define FDCAN_RAM_INSTANCE_WORDS    1280U
#define FDCAN_RAM_FDCAN1_OFFSET     0U
#define FDCAN_RAM_FDCAN2_OFFSET     1280U
#define FDCAN_ELEMENT_WORDS         18U  /* 2 header + 16 data words */
#define FDCAN_DATA_SIZE_64          7U

/* Message RAM element header bit positions (Bosch M_CAN format). */
#define RXTX_R0_ESI                 (1UL << 31)
#define RXTX_R0_XTD                 (1UL << 30)
#define RXTX_R0_ID_STD_SHIFT        18U
#define RXTX_R0_ID_STD_MASK         0x7FFUL
#define RXTX_R0_ID_EXT_MASK         0x1FFFFFFFUL

#define RXTX_R1_ANMF                (1UL << 31)
#define RXTX_R1_FIDX_SHIFT          24U
#define RXTX_R1_FIDX_MASK           0x7FUL
#define RXTX_R1_FDF                 (1UL << 21)
#define RXTX_R1_BRS                 (1UL << 20)
#define RXTX_R1_DLC_SHIFT           16U
#define RXTX_R1_DLC_MASK            0xFUL
#define RXTX_R1_RXTS_MASK           0xFFFFUL

/* RX FIFO status fields. */
#define RXF0S_FILL_MASK             0x7FUL
#define RXF0S_GET_SHIFT             8U
#define RXF1S_FILL_MASK             0x7FUL
#define RXF1S_GET_SHIFT             8U

/* TX FIFO/queue status fields. */
#define TXFQS_FREE_MASK             0x3FUL
#define TXFQS_PUT_SHIFT             16U
#define TXFQS_PUT_MASK              0x1FUL

/* Protocol status fields. */
#define PSR_EP                      (1UL << 5)
#define PSR_BO                      (1UL << 7)
#define ECR_TEC_MASK                0xFFUL
#define ECR_REC_SHIFT               8U
#define ECR_REC_MASK                (0x7FUL << ECR_REC_SHIFT)


/* Validate timing values expressed as actual counts, not register values. */
static bool FDCAN_ValidateConfig(const FDCAN_Config_t *pConfig)
{
    const FDCAN_BitTiming_t *n;
    const FDCAN_BitTiming_t *d;
    uint32_t words;

    if (pConfig == NULL)
        return false;

    if ((pConfig->Instance != FDCAN_1) &&
        (pConfig->Instance != FDCAN_2))
        return false;

    /* This version implements normal mode only. */
    if (pConfig->Mode != FDCAN_MODE_NORMAL)
        return false;

    if ((pConfig->FrameFormat != CLASSIC) &&
        (pConfig->FrameFormat != FD))
        return false;

    if ((pConfig->BitRateSwitch != FDCAN_BRS_DISABLED) &&
        (pConfig->BitRateSwitch != FDCAN_BRS_ENABLED))
        return false;

    if ((pConfig->FrameFormat == CLASSIC) &&
        (pConfig->BitRateSwitch == FDCAN_BRS_ENABLED))
        return false;

    n = &pConfig->NominalTiming;
    if ((n->Prescaler == 0U) || (n->Prescaler > 512U) ||
        (n->SJW == 0U) || (n->SJW > 128U) ||
        (n->TimeSeg1 < 2U) || (n->TimeSeg1 > 256U) ||
        (n->TimeSeg2 == 0U) || (n->TimeSeg2 > 128U) ||
        (n->SJW > n->TimeSeg2))
        return false;

    if (pConfig->FrameFormat == FD)
    {
        d = &pConfig->DataTiming;
        if ((d->Prescaler == 0U) || (d->Prescaler > 32U) ||
            (d->SJW == 0U) || (d->SJW > 16U) ||
            (d->TimeSeg1 == 0U) || (d->TimeSeg1 > 32U) ||
            (d->TimeSeg2 == 0U) || (d->TimeSeg2 > 16U) ||
            (d->SJW > d->TimeSeg2))
            return false;

        /* With BRS enabled, the data-phase bitrate must not be lower. */
        if (pConfig->BitRateSwitch == FDCAN_BRS_ENABLED)
        {
            uint32_t nominalDiv = (uint32_t)n->Prescaler *
                                  (1U + n->TimeSeg1 + n->TimeSeg2);
            uint32_t dataDiv = (uint32_t)d->Prescaler *
                               (1U + d->TimeSeg1 + d->TimeSeg2);
            if (dataDiv > nominalDiv)
                return false;
        }
    }

    /* Hardware limits for the configured Message RAM sections. */
    if ((pConfig->StandardFilterCount > 128U) ||
        (pConfig->ExtendedFilterCount > 64U) ||
        (pConfig->RxFIFO0ElementCount == 0U) ||
        (pConfig->RxFIFO0ElementCount > 64U) ||
        (pConfig->RxFIFO1ElementCount > 64U) ||
        (pConfig->TxBufferCount == 0U) ||
        (pConfig->TxBufferCount > 32U))
        return false;

    words = (uint32_t)pConfig->StandardFilterCount +
            ((uint32_t)pConfig->ExtendedFilterCount * 2U) +
            (((uint32_t)pConfig->RxFIFO0ElementCount +
              pConfig->RxFIFO1ElementCount +
              pConfig->TxBufferCount) * FDCAN_ELEMENT_WORDS);

    return (words <= FDCAN_RAM_INSTANCE_WORDS);
}


/* Allocate and configure all Message RAM sections used by this driver. */
static FDCAN_Result_t FDCAN_ConfigureMessageRAM(FDCAN_GlobalTypeDef *pFDCAN,const FDCAN_Config_t *pConfig)
{
    uint32_t start;
    uint32_t used;
    volatile uint32_t *ram = (volatile uint32_t *)SRAMCAN_BASE;

    start = (pConfig->Instance == FDCAN_1) ?
            FDCAN_RAM_FDCAN1_OFFSET : FDCAN_RAM_FDCAN2_OFFSET;

    used = (uint32_t)pConfig->StandardFilterCount +
           ((uint32_t)pConfig->ExtendedFilterCount * 2U) +
           (((uint32_t)pConfig->RxFIFO0ElementCount +
             pConfig->RxFIFO1ElementCount +
             pConfig->TxBufferCount) * FDCAN_ELEMENT_WORDS);

    if ((used > FDCAN_RAM_INSTANCE_WORDS) ||
        ((start + used) > FDCAN_RAM_TOTAL_WORDS))
        return FDCAN_ERROR_INVALID_PARAM;

    /* Clear only this controller's allocated Message RAM region. */
    for (uint32_t i = 0U; i < used; i++)
        ram[start + i] = 0U;

    /* Standard filters: one 32-bit word per filter. */
    pFDCAN->SIDFC = ((start << FDCAN_SIDFC_FLSSA_Pos) &
                     FDCAN_SIDFC_FLSSA_Msk) |
                    ((uint32_t)pConfig->StandardFilterCount <<
                     FDCAN_SIDFC_LSS_Pos);
    start += pConfig->StandardFilterCount;

    /* Extended filters: two 32-bit words per filter. */
    pFDCAN->XIDFC = ((start << FDCAN_XIDFC_FLESA_Pos) &
                     FDCAN_XIDFC_FLESA_Msk) |
                    ((uint32_t)pConfig->ExtendedFilterCount <<
                     FDCAN_XIDFC_LSE_Pos);
    start += (uint32_t)pConfig->ExtendedFilterCount * 2U;

    /* RX FIFO 0. Blocking mode, watermark disabled. */
    pFDCAN->RXF0C = ((start << FDCAN_RXF0C_F0SA_Pos) &
                     FDCAN_RXF0C_F0SA_Msk) |
                    ((uint32_t)pConfig->RxFIFO0ElementCount <<
                     FDCAN_RXF0C_F0S_Pos);
    start += (uint32_t)pConfig->RxFIFO0ElementCount * FDCAN_ELEMENT_WORDS;

    /* RX FIFO 1. Blocking mode, watermark disabled. */
    pFDCAN->RXF1C = ((start << FDCAN_RXF1C_F1SA_Pos) &
                     FDCAN_RXF1C_F1SA_Msk) |
                    ((uint32_t)pConfig->RxFIFO1ElementCount <<
                     FDCAN_RXF1C_F1S_Pos);
    start += (uint32_t)pConfig->RxFIFO1ElementCount * FDCAN_ELEMENT_WORDS;

    /* Both RX FIFOs use 64-byte payload capacity. */
    pFDCAN->RXESC = (FDCAN_DATA_SIZE_64 << FDCAN_RXESC_F0DS_Pos) |
                    (FDCAN_DATA_SIZE_64 << FDCAN_RXESC_F1DS_Pos);

    /* Dedicated RX buffers and TX event FIFO are unused. */
    pFDCAN->RXBC = 0U;
    pFDCAN->TXEFC = 0U;

    /* TX FIFO (NDTB=0, TFQM=0). */
    pFDCAN->TXBC = ((start << FDCAN_TXBC_TBSA_Pos) &
                    FDCAN_TXBC_TBSA_Msk) |
                   ((uint32_t)pConfig->TxBufferCount <<
                    FDCAN_TXBC_TFQS_Pos);
    pFDCAN->TXESC = (FDCAN_DATA_SIZE_64 << FDCAN_TXESC_TBDS_Pos);

    /* Reject remote frames; unmatched standard/extended frames go to FIFO 0. */
    pFDCAN->GFC = FDCAN_GFC_RRFS | FDCAN_GFC_RRFE;

    return FDCAN_OK;
}


/* Convert payload byte count to the CAN FD DLC encoding. */
static uint8_t FDCAN_LengthToDLC(uint8_t length)
{
    if (length <= 8U)  return length;
    if (length == 12U) return 9U;
    if (length == 16U) return 10U;
    if (length == 20U) return 11U;
    if (length == 24U) return 12U;
    if (length == 32U) return 13U;
    if (length == 48U) return 14U;
    if (length == 64U) return 15U;
    return 0xFFU;
}


/* Convert received DLC encoding to payload byte count. */
static uint8_t FDCAN_DLCToLength(uint8_t dlc, bool fdFrame)
{
    static const uint8_t fdLength[16] =
        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
          8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U };

    if (dlc > 15U)
        return 0U;
    if (!fdFrame && (dlc > 8U))
        return 8U;
    return fdLength[dlc];
}

FDCAN_Result_t Init_FDCAN(
    FDCAN_Handle_t *pHandle,
    const FDCAN_Config_t *pConfig)
    {
        if ((pHandle == NULL) || (pConfig == NULL))
            return FDCAN_ERROR_INVALID_PARAM;

        //enable kernal and peripheral clock
       // if()

    }
FDCAN_Result_t FDCAN_Init(
    FDCAN_Handle_t *pHandle,
    const FDCAN_Config_t *pConfig)
{
    FDCAN_GlobalTypeDef *pFDCAN;
    FDCAN_Result_t result;
    uint32_t timeout;
    uint32_t cccr;
    uint32_t ie;

    if ((pHandle == NULL) || !FDCAN_ValidateConfig(pConfig))
        return FDCAN_ERROR_INVALID_PARAM;

    pFDCAN = (pConfig->Instance == FDCAN_1) ? FDCAN1 : FDCAN2;
    pHandle->Initialized = false;
    pHandle->Started = false;

    /* Select PLL1_Q as FDCAN kernel clock (FDCANSEL = 01). */
    RCC->D2CCIP1R = (RCC->D2CCIP1R & ~RCC_D2CCIP1R_FDCANSEL) |
                    RCC_D2CCIP1R_FDCANSEL_0;

    /* Enable FDCAN peripheral bus clock. */
    RCC->APB1HENR |= RCC_APB1HENR_FDCANEN;
   // (void)RCC->APB1HENR;

    /* Entering in the init mode */
    pFDCAN->CCCR |= FDCAN_CCCR_INIT;
    timeout = FDCAN_TIMEOUT;
    while ((pFDCAN->CCCR & FDCAN_CCCR_INIT) == 0U)
    {
        if (timeout-- == 0U)
            return FDCAN_ERROR_TIMEOUT;
    }

    /* Permit protected configuration-register writes. */
    pFDCAN->CCCR |= FDCAN_CCCR_CCE;

    /* Configure FD operation, BRS and automatic retransmission. */
    cccr = pFDCAN->CCCR & ~(FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE | FDCAN_CCCR_DAR);

    if (pConfig->FrameFormat == FD)
    {
        cccr |= FDCAN_CCCR_FDOE;
        if (pConfig->BitRateSwitch == FDCAN_BRS_ENABLED)
            cccr |= FDCAN_CCCR_BRSE;
    }

    if (!pConfig->AutoRetransmission)
        cccr |= FDCAN_CCCR_DAR;

    pFDCAN->CCCR = cccr;

    pFDCAN->NBTP =
        (((uint32_t)(pConfig->NominalTiming.Prescaler - 1U) <<
          FDCAN_NBTP_NBRP_Pos) & FDCAN_NBTP_NBRP_Msk) |
        (((uint32_t)(pConfig->NominalTiming.SJW - 1U) <<
          FDCAN_NBTP_NSJW_Pos) & FDCAN_NBTP_NSJW_Msk) |
        (((uint32_t)(pConfig->NominalTiming.TimeSeg1 - 1U) <<
          FDCAN_NBTP_NTSEG1_Pos) & FDCAN_NBTP_NTSEG1_Msk) |
        (((uint32_t)(pConfig->NominalTiming.TimeSeg2 - 1U) <<
          FDCAN_NBTP_NTSEG2_Pos) & FDCAN_NBTP_NTSEG2_Msk);

    if (pConfig->FrameFormat == FD)
    {
        pFDCAN->DBTP =
            (((uint32_t)(pConfig->DataTiming.SJW - 1U) <<
              FDCAN_DBTP_DSJW_Pos) & FDCAN_DBTP_DSJW_Msk) |
            (((uint32_t)(pConfig->DataTiming.TimeSeg2 - 1U) <<
              FDCAN_DBTP_DTSEG2_Pos) & FDCAN_DBTP_DTSEG2_Msk) |
            (((uint32_t)(pConfig->DataTiming.TimeSeg1 - 1U) <<
              FDCAN_DBTP_DTSEG1_Pos) & FDCAN_DBTP_DTSEG1_Msk) |
            (((uint32_t)(pConfig->DataTiming.Prescaler - 1U) <<
              FDCAN_DBTP_DBRP_Pos) & FDCAN_DBTP_DBRP_Msk);
    }
    volatile uint32_t nbtp_readback = pFDCAN->NBTP;
    volatile uint32_t dbtp_readback = pFDCAN->DBTP;     
    /* Transmitter Delay Compensation is deliberately disabled. */
    pFDCAN->TDCR = 0U;

    result = FDCAN_ConfigureMessageRAM(pFDCAN, pConfig);
    if (result != FDCAN_OK)
        return result;

    /* Use the internal timestamp counter only if configured elsewhere;
       leave it disabled in this initial driver. */
    pFDCAN->TSCC = 0U;

    /* Clear stale interrupt flags (IR is write-one-to-clear). */
    pFDCAN->IR = 0xFFFFFFFFU;
    pFDCAN->ILS = 0U;  /* Route enabled interrupt sources to line 0. */
    pFDCAN->ILE = 0U;  /* Enable line 0 when FDCAN_Start() is called. */

    ie = FDCAN_IE_RF0NE | FDCAN_IE_RF0LE |
         FDCAN_IE_BOE | FDCAN_IE_EPE | FDCAN_IE_EWE | FDCAN_IE_MRAFE;
    if (pConfig->RxFIFO1ElementCount > 0U)
        ie |= FDCAN_IE_RF1NE | FDCAN_IE_RF1LE;
    pFDCAN->IE = ie;

    pHandle->Instance = pFDCAN;
    pHandle->Config = *pConfig;
    pHandle->Initialized = true;
    pHandle->Started = false;

    pHandle->Status.Initialized = true;
    pHandle->Status.Started = false;
    pHandle->Status.BusState = FDCAN_BUS_ERROR_ACTIVE;
    pHandle->Status.TxErrorCount = 0U;
    pHandle->Status.RxErrorCount = 0U;
    pHandle->Status.RxFIFO0FillLevel = 0U;
    pHandle->Status.RxFIFO1FillLevel = 0U;
    pHandle->Status.TxFreeLevel = pConfig->TxBufferCount;
    pHandle->Status.ProtocolStatus = 0U;
    pHandle->Status.InterruptFlags = 0U;

    return FDCAN_OK;
}


FDCAN_Result_t FDCAN_Start(FDCAN_Handle_t *pHandle)
{
    uint32_t timeout;

    if ((pHandle == NULL) || !pHandle->Initialized ||
        (pHandle->Instance == NULL))
        return FDCAN_ERROR_NOT_INITIALIZED;

    pHandle->Instance->CCCR &= ~FDCAN_CCCR_INIT;

    timeout = FDCAN_TIMEOUT;
    while ((pHandle->Instance->CCCR & FDCAN_CCCR_INIT) != 0U)
    {
        if (timeout-- == 0U)
            return FDCAN_ERROR_TIMEOUT;
    }

    pHandle->Instance->ILE = 0U; /* Enable interrupt line 0. */

    if (pHandle->Config.Instance == FDCAN_1)
    {
        NVIC_ClearPendingIRQ(FDCAN1_IT0_IRQn);
        NVIC_SetPriority(FDCAN1_IT0_IRQn, 5U);
        NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    }
    else
    {
        NVIC_ClearPendingIRQ(FDCAN2_IT0_IRQn);
        NVIC_SetPriority(FDCAN2_IT0_IRQn, 5U);
        NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
    }

    pHandle->Started = true;
    pHandle->Status.Started = true;
    return FDCAN_OK;
}


FDCAN_Result_t FDCAN_Stop(FDCAN_Handle_t *pHandle)
{
    uint32_t timeout;

    if ((pHandle == NULL) || !pHandle->Initialized ||
        (pHandle->Instance == NULL))
        return FDCAN_ERROR_NOT_INITIALIZED;

    pHandle->Instance->ILE = 0U;

    if (pHandle->Config.Instance == FDCAN_1)
        NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
    else
        NVIC_DisableIRQ(FDCAN2_IT0_IRQn);

    pHandle->Instance->CCCR |= FDCAN_CCCR_INIT;

    timeout = FDCAN_TIMEOUT;
    while ((pHandle->Instance->CCCR & FDCAN_CCCR_INIT) == 0U)
    {
        if (timeout-- == 0U)
            return FDCAN_ERROR_TIMEOUT;
    }

    pHandle->Started = false;
    pHandle->Status.Started = false;
    return FDCAN_OK;
}


FDCAN_Result_t FDCAN_Transmit(
    FDCAN_Handle_t *pHandle,
    const FDCAN_TxMessage_t *pMessage)
{
    uint32_t txStatus;
    uint32_t putIndex;
    uint32_t txStart;
    uint32_t header0;
    uint32_t header1;
    uint8_t dlc;
    volatile uint32_t *ram;
    volatile uint32_t *element;

    if ((pHandle == NULL) || (pMessage == NULL))
        return FDCAN_ERROR_INVALID_PARAM;
    if (!pHandle->Initialized || (pHandle->Instance == NULL))
        return FDCAN_ERROR_NOT_INITIALIZED;
    if (!pHandle->Started)
        return FDCAN_ERROR_NOT_STARTED;

    if ((pMessage->IDType == STANDARD &&
         pMessage->Identifier > FDCAN_STANDARD_ID_MAX) ||
        (pMessage->IDType == EXTENDED &&
         pMessage->Identifier > FDCAN_EXTENDED_ID_MAX) ||
        ((pMessage->IDType != STANDARD) &&
         (pMessage->IDType != EXTENDED)))
        return FDCAN_ERROR_INVALID_PARAM;

    if ((pMessage->FrameFormat != CLASSIC) &&
        (pMessage->FrameFormat != FD))
        return FDCAN_ERROR_INVALID_PARAM;
    if ((pHandle->Config.FrameFormat == CLASSIC) &&
        (pMessage->FrameFormat == FD))
        return FDCAN_ERROR_INVALID_PARAM;
    if ((pMessage->FrameFormat == CLASSIC) &&
        (pMessage->BitRateSwitch == FDCAN_BRS_ENABLED))
        return FDCAN_ERROR_INVALID_PARAM;
    if ((pMessage->BitRateSwitch == FDCAN_BRS_ENABLED) &&
        (pHandle->Config.BitRateSwitch != FDCAN_BRS_ENABLED))
        return FDCAN_ERROR_INVALID_PARAM;

    if (pMessage->DataLength > FDCAN_MAX_DATA_LENGTH)
        return FDCAN_ERROR_INVALID_PARAM;
    if ((pMessage->FrameFormat == CLASSIC) &&
        (pMessage->DataLength > 8U))
        return FDCAN_ERROR_INVALID_PARAM;

    dlc = FDCAN_LengthToDLC(pMessage->DataLength);
    if (dlc == 0xFFU)
        return FDCAN_ERROR_INVALID_PARAM;

    txStatus = pHandle->Instance->TXFQS;
    if ((txStatus & TXFQS_FREE_MASK) == 0U)
        return FDCAN_ERROR_TX_FIFO_FULL;

    putIndex = (txStatus >> TXFQS_PUT_SHIFT) & TXFQS_PUT_MASK;
    if (putIndex >= pHandle->Config.TxBufferCount)
        return FDCAN_ERROR;

    txStart = (pHandle->Config.Instance == FDCAN_1) ?
              FDCAN_RAM_FDCAN1_OFFSET : FDCAN_RAM_FDCAN2_OFFSET;
    txStart += pHandle->Config.StandardFilterCount;
    txStart += (uint32_t)pHandle->Config.ExtendedFilterCount * 2U;
    txStart += (uint32_t)pHandle->Config.RxFIFO0ElementCount * FDCAN_ELEMENT_WORDS;
    txStart += (uint32_t)pHandle->Config.RxFIFO1ElementCount * FDCAN_ELEMENT_WORDS;

    ram = (volatile uint32_t *)SRAMCAN_BASE;
    element = &ram[txStart + (putIndex * FDCAN_ELEMENT_WORDS)];

    header0 = 0U;
    if (pMessage->ErrorStateIndicator == FDCAN_ESI_PASSIVE)
        header0 |= RXTX_R0_ESI;

    if (pMessage->IDType == STANDARD)
        header0 |= (pMessage->Identifier & RXTX_R0_ID_STD_MASK) <<
                   RXTX_R0_ID_STD_SHIFT;
    else
        header0 |= RXTX_R0_XTD |
                   (pMessage->Identifier & RXTX_R0_ID_EXT_MASK);

    header1 = ((uint32_t)dlc << RXTX_R1_DLC_SHIFT);
    if (pMessage->FrameFormat == FD)
        header1 |= RXTX_R1_FDF;
    if (pMessage->BitRateSwitch == FDCAN_BRS_ENABLED)
        header1 |= RXTX_R1_BRS;

    element[0] = header0;
    element[1] = header1;

    /* Clear all 64 payload bytes, then copy the requested payload. */
    for (uint32_t i = 0U; i < 16U; i++)
        element[2U + i] = 0U;

    for (uint32_t i = 0U; i < pMessage->DataLength; i++)
        element[2U + (i / 4U)] |=
            ((uint32_t)pMessage->Data[i] << ((i % 4U) * 8U));

    __DMB();
    pHandle->Instance->TXBAR = (1UL << putIndex);
    return FDCAN_OK;
}


FDCAN_Result_t FDCAN_Receive(
    FDCAN_Handle_t *pHandle,
    FDCAN_RxMessage_t *pMessage)
{
    FDCAN_GlobalTypeDef *pFDCAN;
    volatile uint32_t *ram;
    volatile uint32_t *element;
    uint32_t fifoStatus;
    uint32_t getIndex;
    uint32_t startWord;
    uint32_t r0;
    uint32_t r1;
    uint32_t fill;
    uint32_t dlc;
    uint8_t fifo;
    bool fdFrame;

    if ((pHandle == NULL) || (pMessage == NULL))
        return FDCAN_ERROR_INVALID_PARAM;
    if (!pHandle->Initialized || (pHandle->Instance == NULL))
        return FDCAN_ERROR_NOT_INITIALIZED;
    if (!pHandle->Started)
        return FDCAN_ERROR_NOT_STARTED;

    pFDCAN = pHandle->Instance;
    fifoStatus = pFDCAN->RXF0S;
    fill = fifoStatus & RXF0S_FILL_MASK;

    /* Prefer FIFO 0; use FIFO 1 when FIFO 0 is empty. */
    if (fill > 0U)
    {
        fifo = 0U;
        getIndex = (fifoStatus >> RXF0S_GET_SHIFT) & 0x3FU;
    }
    else
    {
        fifoStatus = pFDCAN->RXF1S;
        fill = fifoStatus & RXF1S_FILL_MASK;
        if (fill == 0U)
            return FDCAN_ERROR_RX_QUEUE_EMPTY;

        fifo = 1U;
        getIndex = (fifoStatus >> RXF1S_GET_SHIFT) & 0x3FU;
    }

    startWord = (pHandle->Config.Instance == FDCAN_1) ?
                FDCAN_RAM_FDCAN1_OFFSET : FDCAN_RAM_FDCAN2_OFFSET;
    startWord += pHandle->Config.StandardFilterCount;
    startWord += (uint32_t)pHandle->Config.ExtendedFilterCount * 2U;

    if (fifo == 1U)
        startWord += (uint32_t)pHandle->Config.RxFIFO0ElementCount * FDCAN_ELEMENT_WORDS;

    ram = (volatile uint32_t *)SRAMCAN_BASE;
    element = &ram[startWord + (getIndex * FDCAN_ELEMENT_WORDS)];

    r0 = element[0];
    r1 = element[1];

    pMessage->ErrorStateIndicator =
        ((r0 & RXTX_R0_ESI) != 0U) ? FDCAN_ESI_PASSIVE : FDCAN_ESI_ACTIVE;

    if ((r0 & RXTX_R0_XTD) != 0U)
    {
        pMessage->IDType = EXTENDED;
        pMessage->Identifier = r0 & RXTX_R0_ID_EXT_MASK;
    }
    else
    {
        pMessage->IDType = STANDARD;
        pMessage->Identifier =
            (r0 >> RXTX_R0_ID_STD_SHIFT) & RXTX_R0_ID_STD_MASK;
    }

    fdFrame = ((r1 & RXTX_R1_FDF) != 0U);
    pMessage->FrameFormat = fdFrame ? FD : CLASSIC;
    pMessage->BitRateSwitch =
        ((r1 & RXTX_R1_BRS) != 0U) ? FDCAN_BRS_ENABLED : FDCAN_BRS_DISABLED;

    dlc = (r1 >> RXTX_R1_DLC_SHIFT) & RXTX_R1_DLC_MASK;
    pMessage->DataLength = FDCAN_DLCToLength((uint8_t)dlc, fdFrame);
    pMessage->Timestamp = (uint16_t)(r1 & RXTX_R1_RXTS_MASK);
    pMessage->FilterIndex = ((r1 & RXTX_R1_ANMF) != 0U) ?
                            0xFFU :
                            (uint8_t)((r1 >> RXTX_R1_FIDX_SHIFT) &
                                      RXTX_R1_FIDX_MASK);

    for (uint32_t i = 0U; i < FDCAN_MAX_DATA_LENGTH; i++)
        pMessage->Data[i] = 0U;

    for (uint32_t i = 0U; i < pMessage->DataLength; i++)
    {
        uint32_t dataWord = element[2U + (i / 4U)];
        pMessage->Data[i] = (uint8_t)(dataWord >> ((i % 4U) * 8U));
    }

    /* Release the element after all header/data words have been copied. */
    __DMB();
    if (fifo == 0U)
        pFDCAN->RXF0A = getIndex;
    else
        pFDCAN->RXF1A = getIndex;

    return FDCAN_OK;
}


FDCAN_Result_t FDCAN_GetStatus(
    FDCAN_Handle_t *pHandle,
    FDCAN_Status_t *pStatus)
{
    uint32_t ecr;
    uint32_t psr;
    uint32_t txfqs;

    if ((pHandle == NULL) || (pStatus == NULL))
        return FDCAN_ERROR_INVALID_PARAM;
    if (!pHandle->Initialized || (pHandle->Instance == NULL))
        return FDCAN_ERROR_NOT_INITIALIZED;

    ecr = pHandle->Instance->ECR;
    psr = pHandle->Instance->PSR;
    txfqs = pHandle->Instance->TXFQS;

    pHandle->Status.Initialized = pHandle->Initialized;
    pHandle->Status.Started = pHandle->Started;
    pHandle->Status.TxErrorCount = (uint8_t)(ecr & ECR_TEC_MASK);
    pHandle->Status.RxErrorCount = (uint8_t)((ecr & ECR_REC_MASK) >> ECR_REC_SHIFT);
    pHandle->Status.RxFIFO0FillLevel =
        (uint8_t)(pHandle->Instance->RXF0S & RXF0S_FILL_MASK);
    pHandle->Status.RxFIFO1FillLevel =
        (uint8_t)(pHandle->Instance->RXF1S & RXF1S_FILL_MASK);
    pHandle->Status.TxFreeLevel = (uint8_t)(txfqs & TXFQS_FREE_MASK);
    pHandle->Status.ProtocolStatus = psr;

    if ((psr & PSR_BO) != 0U)
        pHandle->Status.BusState = FDCAN_BUS_OFF;
    else if ((psr & PSR_EP) != 0U)
        pHandle->Status.BusState = FDCAN_BUS_ERROR_PASSIVE;
    else
        pHandle->Status.BusState = FDCAN_BUS_ERROR_ACTIVE;

    *pStatus = pHandle->Status;
    return FDCAN_OK;
}


void FDCAN_IRQHandler(FDCAN_Handle_t *pHandle)
{
    uint32_t pending;

    if ((pHandle == NULL) || !pHandle->Initialized ||
        (pHandle->Instance == NULL))
        return;

    pending = pHandle->Instance->IR;
    if (pending != 0U)
    {
        /* Preserve event bits for the application/status monitor. */
        pHandle->Status.InterruptFlags |= pending;

        /* IR flags are write-one-to-clear. */
        pHandle->Instance->IR = pending;
    }
}