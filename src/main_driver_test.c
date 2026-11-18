/*
 * Noccarc STM32H743 low-level driver diagnostic main
 *
 * This test is intended for the CURRENT driver package in system/src and
 * system/inc. It tests driver initialization and safe, non-destructive paths.
 *
 * Important limits:
 * - GPIO test checks PA11/PA12 alternate-function configuration. It does not
 *   electrically loop GPIO pins together.
 * - UART DMA test exercises DMA1 Stream 3 -> USART3. It depends on the
 *   configured DMA buffer address 0x30000000 being mapped as D2 SRAM in the
 *   Renode platform / hardware.
 * - FDCAN test initializes, starts, reads status, then stops FDCAN1. It does
 *   NOT transmit a frame. MCU1<->MCU2 CAN communication still needs a separate
 *   two-node test.
 * - Fault-handler test verifies that handler symbols are linked. It does not
 *   intentionally trigger a fault because that halts the MCU.
 * - Watchdog test checks initialization and refresh only; timeout/reset
 *   behaviour requires a separate deliberate-reset test.
 *
 * The FDCAN timing tuple below assumes an 80 MHz FDCAN kernel clock. This must
 * match the active RCC_Clock_Init() configuration (PLL1Q) before interpreting
 * FDCAN timing results:
 * nominal: 80 MHz / (10 * (1 + 13 + 2)) = 500 kbit/s
 * data:    80 MHz / ( 4 * (1 +  7 + 2)) = 2 Mbit/s
 * No CAN frame is transmitted by this diagnostic main.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "stm32h743xx.h"
#include "system_stm32h7xx.h"

#include "RCC.h"
#include "GPIO.h"
#include "NVIC.h"
#include "DWT.h"
#include "Timer.h"
#include "DMA.h"
#include "UART.h"
#include "Watchdog.h"
#include "FDCAN.h"
#include "Fault_Handler.h"
#include "peripherals_init.h"
#include "FDCAN_Port.h"
#include "peripheral_config.h"

#define DRIVER_TEST_MAGIC              0x44525654UL /* 'DRVT' */
#define DRIVER_TEST_UART_BAUD          115200UL
#define DRIVER_TEST_UART_TIMEOUT       5000000UL
#define DRIVER_TEST_TIMER_WAIT_MS      3U
#define DRIVER_TEST_WAIT_LIMIT_CYCLES  (SystemCoreClock / 2U) /* 500 ms */
#define DRIVER_TEST_SPIN_LIMIT         50000000UL

typedef enum
{
    TEST_NOT_RUN = 0U,
    TEST_PASS,
    TEST_FAIL,
    TEST_SKIP,
    TEST_LINKED_ONLY
} DriverTestState_t;

typedef enum
{
    TEST_RCC = 0U,
    TEST_NVIC,
    TEST_GPIO,
    TEST_DWT,
    TEST_TIMER,
    TEST_DMA_INIT,
    TEST_UART_INIT,
    TEST_UART_BLOCKING_TX,
    TEST_UART_DMA_TX,
    TEST_FDCAN_INIT_START_STOP,
    TEST_FDCAN_PORT_SYMBOL,
    TEST_COMMON_INIT_SYMBOL,
    TEST_FAULT_HANDLER_SYMBOLS,
    TEST_WATCHDOG,
    TEST_COUNT
} DriverTestId_t;

typedef struct
{
    uint32_t state;
    uint32_t detail;
} DriverTestEntry_t;

typedef struct
{
    uint32_t magic;
    uint32_t pass_count;
    uint32_t fail_count;
    uint32_t skip_count;
    uint32_t linked_only_count;
    DriverTestEntry_t test[TEST_COUNT];
} DriverTestReport_t;

/* Inspect this global in GDB if UART output is unavailable. */
volatile DriverTestReport_t g_driver_test_report;

static bool g_uart_ready = false;
static bool g_dwt_ready = false;
static bool g_timer_ready = false;
static bool g_dma_ready = false;
static bool g_watchdog_ready = false;

static FDCAN_Handle_t g_test_fdcan;

static const FDCAN_Config_t g_test_fdcan_config =
{
    .Instance = FDCAN_1,
    .Mode = FDCAN_MODE_NORMAL,
    .FrameFormat = FD,
    .BitRateSwitch = FDCAN_BRS_ENABLED,
    .NominalTiming = NOCARC_FDCAN_NOMINAL_TIMING,
    .DataTiming = NOCARC_FDCAN_DATA_TIMING,
    .AutoRetransmission = true,
    .StandardFilterCount = 0U,
    .ExtendedFilterCount = 0U,
    .RxFIFO0ElementCount = 8U,
    .RxFIFO1ElementCount = 0U,
    .TxBufferCount = 4U
};

static const char * const g_test_names[TEST_COUNT] =
{
    "RCC",
    "NVIC",
    "GPIO_AF_CONFIG",
    "DWT_CYCLE_COUNTER",
    "TIM2_1MS_TICK",
    "DMA_INIT_DMAMUX",
    "UART_INIT",
    "UART_BLOCKING_TX",
    "UART_DMA_TX",
    "FDCAN_INIT_START_STOP",
    "FDCAN_PORT_LINKED",
    "PERIPHERALS_INIT_LINKED",
    "FAULT_HANDLER_SYMBOLS",
    "WATCHDOG_INIT_REFRESH"
};

static void Test_Set(DriverTestId_t id, DriverTestState_t state, uint32_t detail)
{
    if ((uint32_t)id >= (uint32_t)TEST_COUNT)
        return;

    g_driver_test_report.test[id].state = (uint32_t)state;
    g_driver_test_report.test[id].detail = detail;
}

static void Test_InitializeReport(void)
{
    uint32_t i;

    g_driver_test_report.magic = DRIVER_TEST_MAGIC;
    g_driver_test_report.pass_count = 0U;
    g_driver_test_report.fail_count = 0U;
    g_driver_test_report.skip_count = 0U;
    g_driver_test_report.linked_only_count = 0U;

    for (i = 0U; i < (uint32_t)TEST_COUNT; i++)
    {
        g_driver_test_report.test[i].state = TEST_NOT_RUN;
        g_driver_test_report.test[i].detail = 0U;
    }
}

static void Test_Recount(void)
{
    uint32_t i;

    g_driver_test_report.pass_count = 0U;
    g_driver_test_report.fail_count = 0U;
    g_driver_test_report.skip_count = 0U;
    g_driver_test_report.linked_only_count = 0U;

    for (i = 0U; i < (uint32_t)TEST_COUNT; i++)
    {
        switch ((DriverTestState_t)g_driver_test_report.test[i].state)
        {
            case TEST_PASS:
                g_driver_test_report.pass_count++;
                break;
            case TEST_FAIL:
                g_driver_test_report.fail_count++;
                break;
            case TEST_SKIP:
                g_driver_test_report.skip_count++;
                break;
            case TEST_LINKED_ONLY:
                g_driver_test_report.linked_only_count++;
                break;
            default:
                break;
        }
    }
}

static void Test_UartWrite(const char *text)
{
    uint16_t length = 0U;

    if (!g_uart_ready || (text == NULL))
        return;

    while ((text[length] != '\0') && (length < 120U))
        length++;

    if (length > 0U)
    {
        (void)UART_TransmitBlocking((const uint8_t *)text,
                                    length,
                                    DRIVER_TEST_UART_TIMEOUT);
    }
}

static void Test_UartWriteHex32(uint32_t value)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char output[8];
    uint32_t i;

    for (i = 0U; i < 8U; i++)
    {
        uint32_t shift = 28U - (i * 4U);
        output[i] = hex_digits[(value >> shift) & 0x0FU];
    }

    if (g_uart_ready)
    {
        (void)UART_TransmitBlocking((const uint8_t *)output,
                                    (uint16_t)sizeof(output),
                                    DRIVER_TEST_UART_TIMEOUT);
    }
}

static void Test_PrintReport(void)
{
    static const char * const state_names[] =
    {
        "NOT_RUN", "PASS", "FAIL", "SKIP", "LINKED_ONLY"
    };
    uint32_t i;

    Test_Recount();

    if (!g_uart_ready)
        return;

    Test_UartWrite("\r\n========== NOCARC DRIVER TEST =========\r\n");

    for (i = 0U; i < (uint32_t)TEST_COUNT; i++)
    {
        uint32_t state = g_driver_test_report.test[i].state;

        Test_UartWrite(g_test_names[i]);
        Test_UartWrite(": ");
        Test_UartWrite(state_names[(state <= TEST_LINKED_ONLY) ? state : TEST_NOT_RUN]);
        Test_UartWrite(" detail=0x");
        Test_UartWriteHex32(g_driver_test_report.test[i].detail);
        Test_UartWrite("\r\n");
    }

    Test_UartWrite("PASS=");
    Test_UartWriteHex32(g_driver_test_report.pass_count);
    Test_UartWrite(" FAIL=");
    Test_UartWriteHex32(g_driver_test_report.fail_count);
    Test_UartWrite(" SKIP=");
    Test_UartWriteHex32(g_driver_test_report.skip_count);
    Test_UartWrite(" LINKED_ONLY=");
    Test_UartWriteHex32(g_driver_test_report.linked_only_count);
    Test_UartWrite("\r\n========================================\r\n");
}

static bool Test_WaitForTimerTicks(uint32_t milliseconds)
{
    uint32_t start_tick = Timer_GetTick();
    uint32_t start_cycles = DWT_GetCycles();
    uint32_t max_cycles = DRIVER_TEST_WAIT_LIMIT_CYCLES;
    uint32_t spin = DRIVER_TEST_SPIN_LIMIT;

    if (max_cycles == 0U)
        max_cycles = 100000000UL;

    while (!Timer_IsElapsed(start_tick, milliseconds))
    {
        if (g_dwt_ready && (DWT_GetElapsedCycles(start_cycles) > max_cycles))
            return false;

        if (spin-- == 0U)
            return false;

        __NOP();
    }

    return true;
}

#if !NOCARC_RENODE
static bool Test_WaitForUartDma(void)
{
    uint32_t start_cycles = DWT_GetCycles();
    uint32_t max_cycles = DRIVER_TEST_WAIT_LIMIT_CYCLES;
    uint32_t spin = DRIVER_TEST_SPIN_LIMIT;

    if (max_cycles == 0U)
        max_cycles = 100000000UL;

    while (UART_IsTransmitBusy())
    {
        if (g_dwt_ready && (DWT_GetElapsedCycles(start_cycles) > max_cycles))
            return false;

        if (spin-- == 0U)
            return false;

        __NOP();
    }

    return true;
}
#endif

static void Test_Rcc(void)
{
    RCC_DriverStatus_t status;
    uint32_t pclk1;
    uint32_t tim2_clock;
    uint32_t passed = 1U;

    status = RCC_Init();
    if (status != RCC_DRIVER_OK)
    {
        Test_Set(TEST_RCC, TEST_FAIL, (uint32_t)status);
        return;
    }

    /* Exercise the RCC helper APIs; no PLL factors or bus prescalers changed here. */
    RCC_EnableGPIOClock(0U); /* GPIOA */
    RCC_EnableGPIOClock(1U); /* GPIOB */
    RCC_EnableFDCANClock();
    RCC_EnableUSART3Clock();
    RCC_EnableDMA1Clock();
    RCC_EnableD2SRAMClock();
    RCC_EnableTIM2Clock();

    pclk1 = RCC_GetPCLK1Hz();
    tim2_clock = RCC_GetTIM2ClockHz();

    /* Renode's STM32H7 RCC model reports FDCANSEL as unhandled. Do not use
     * its readback as an RCC pass/fail condition in this Renode diagnostic.
     * Validate the FDCAN kernel-clock mux on physical hardware separately. */
    if (pclk1 == 0U) passed = 0U;
    if (tim2_clock == 0U) passed = 0U;
    if ((RCC->AHB4ENR & (RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN)) !=
        (RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN)) passed = 0U;
#if !NOCARC_RENODE
    if ((RCC->APB1HENR & RCC_APB1HENR_FDCANEN) == 0U) passed = 0U;
#endif
    if ((RCC->APB1LENR & RCC_APB1LENR_USART3EN) == 0U) passed = 0U;
    if ((RCC->APB1LENR & RCC_APB1LENR_TIM2EN) == 0U) passed = 0U;
    if ((RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN) == 0U) passed = 0U;
#if !NOCARC_RENODE
    if ((RCC->AHB2ENR & (RCC_AHB2ENR_D2SRAM1EN |
                         RCC_AHB2ENR_D2SRAM2EN |
                         RCC_AHB2ENR_D2SRAM3EN)) !=
        (RCC_AHB2ENR_D2SRAM1EN |
         RCC_AHB2ENR_D2SRAM2EN |
         RCC_AHB2ENR_D2SRAM3EN)) passed = 0U;
#endif

    Test_Set(TEST_RCC, passed ? TEST_PASS : TEST_FAIL, pclk1);
}

static void Test_Nvic(void)
{
    uint32_t irq = (uint32_t)DMA1_Stream3_IRQn;
    uint32_t mask = 1UL << (irq & 31U);
    uint32_t grouping;
    bool disabled_ok;
    bool enabled_ok;

    NVIC_Driver_Init();
    NVIC_ConfigureIRQ(DMA1_Stream3_IRQn, 5U, false);
    disabled_ok = ((NVIC->ISER[irq >> 5U] & mask) == 0U);

    NVIC_ConfigureIRQ(DMA1_Stream3_IRQn, 5U, true);
    enabled_ok = ((NVIC->ISER[irq >> 5U] & mask) != 0U);

    grouping = (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) >> SCB_AIRCR_PRIGROUP_Pos;

    Test_Set(TEST_NVIC,
             (disabled_ok && enabled_ok && (grouping == 3U)) ? TEST_PASS : TEST_FAIL,
             grouping);
}

static void Test_Gpio(void)
{
    GPIO_PinConfig_t fdcan_pin =
    {
        GPIO_MODE_ALTERNATE,
        GPIO_NOPULL,
        GPIO_SPEED_VERY_HIGH,
        GPIO_PUSH_PULL,
        NOCARC_FDCAN1_AF
    };
    uint32_t rx_mode;
    uint32_t tx_mode;
    uint32_t rx_af;
    uint32_t tx_af;
    bool rx_configured;
    bool tx_configured;
    bool invalid_pin_rejected;

    GPIO_Init();

    rx_configured = GPIO_ConfigPin(GPIOA, NOCARC_FDCAN1_RX_PIN, &fdcan_pin);
    tx_configured = GPIO_ConfigPin(GPIOA, NOCARC_FDCAN1_TX_PIN, &fdcan_pin);
    invalid_pin_rejected = !GPIO_ConfigPin(GPIOA, 16U, &fdcan_pin);

    rx_mode = (GPIOA->MODER >> (NOCARC_FDCAN1_RX_PIN * 2U)) & 3UL;
    tx_mode = (GPIOA->MODER >> (NOCARC_FDCAN1_TX_PIN * 2U)) & 3UL;
    rx_af = (GPIOA->AFR[1] >> ((NOCARC_FDCAN1_RX_PIN - 8U) * 4U)) & 0xFUL;
    tx_af = (GPIOA->AFR[1] >> ((NOCARC_FDCAN1_TX_PIN - 8U) * 4U)) & 0xFUL;

    if (rx_configured && tx_configured && invalid_pin_rejected &&
        (rx_mode == GPIO_MODE_ALTERNATE) &&
        (tx_mode == GPIO_MODE_ALTERNATE) &&
        (rx_af == NOCARC_FDCAN1_AF) &&
        (tx_af == NOCARC_FDCAN1_AF))
    {
        Test_Set(TEST_GPIO, TEST_PASS, (rx_af << 4U) | tx_af);
    }
    else
    {
        Test_Set(TEST_GPIO, TEST_FAIL, (rx_mode << 24U) | (tx_mode << 16U) |
                                      (rx_af << 8U) | tx_af);
    }
}

static void Test_Dwt(void)
{
    volatile uint32_t i;
    uint32_t start;
    uint32_t elapsed;

    g_dwt_ready = DWT_Init();
    if (!g_dwt_ready)
    {
        Test_Set(TEST_DWT, TEST_FAIL, 0U);
        return;
    }

    start = DWT_GetCycles();
    for (i = 0U; i < 5000U; i++)
        __NOP();
    elapsed = DWT_GetElapsedCycles(start);

    Test_Set(TEST_DWT,
             ((elapsed != 0U) && (SystemCoreClock != 0U)) ? TEST_PASS : TEST_FAIL,
             elapsed);
}

static void Test_Timer(void)
{
    Timer_Status_t status;
    uint32_t start_tick;

    status = Timer_Init();
    if (status != TIMER_OK)
    {
        g_timer_ready = false;
        Test_Set(TEST_TIMER, TEST_FAIL, (uint32_t)status);
        return;
    }

    g_timer_ready = true;
    start_tick = Timer_GetTick();

    if (Test_WaitForTimerTicks(DRIVER_TEST_TIMER_WAIT_MS) &&
        Timer_IsElapsed(start_tick, DRIVER_TEST_TIMER_WAIT_MS))
    {
        Test_Set(TEST_TIMER, TEST_PASS, Timer_GetTick() - start_tick);
    }
    else
    {
        Test_Set(TEST_TIMER, TEST_FAIL, Timer_GetTick() - start_tick);
    }
}

static void Test_Dma(void)
{
    DMA_DriverStatus_t status;
    uint32_t request;
    uint32_t irq = (uint32_t)DMA1_Stream3_IRQn;
    uint32_t mask = 1UL << (irq & 31U);

    status = DMA_Init();
    g_dma_ready = (status == DMA_DRIVER_OK);
    request = DMAMUX1_Channel3->CCR & 0x7FUL;

    if (g_dma_ready &&
        (request == NOCARC_UART3_DMA_REQUEST_ID) &&
        ((RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN) != 0U) &&
        ((NVIC->ISER[irq >> 5U] & mask) != 0U))
    {
        Test_Set(TEST_DMA_INIT, TEST_PASS, request);
    }
    else
    {
        Test_Set(TEST_DMA_INIT, TEST_FAIL, (uint32_t)status);
    }
}

static void Test_Uart(void)
{
    UART_DriverStatus_t status;
    static const uint8_t message[] =
        "[Noccarc driver test] USART3 blocking TX is active.\r\n";

    status = UART_Init(DRIVER_TEST_UART_BAUD);
    g_uart_ready = (status == UART_DRIVER_OK);

    if (!g_uart_ready ||
        ((USART3->CR1 & (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE)) !=
         (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE)) ||
        (USART3->BRR == 0U))
    {
        Test_Set(TEST_UART_INIT, TEST_FAIL, (uint32_t)status);
        Test_Set(TEST_UART_BLOCKING_TX, TEST_SKIP, 0U);
        Test_Set(TEST_UART_DMA_TX, TEST_SKIP, 0U);
        return;
    }

    Test_Set(TEST_UART_INIT, TEST_PASS, USART3->BRR);

    status = UART_TransmitBlocking(message,
                                   (uint16_t)(sizeof(message) - 1U),
                                   DRIVER_TEST_UART_TIMEOUT);

    Test_Set(TEST_UART_BLOCKING_TX,
             (status == UART_DRIVER_OK) ? TEST_PASS : TEST_FAIL,
             (uint32_t)status);
}

static void Test_UartDma(void)
{
#if NOCARC_RENODE
    /* The Renode STM32F7_USART model exposes RX DMA requests only. */
    Test_Set(TEST_UART_DMA_TX, TEST_SKIP, 0U);
#else
    static const uint8_t dma_message[] =
        "[Noccarc driver test] USART3 DMA TX complete.\r\n";
    UART_DriverStatus_t status;
    const UART_ErrorCounters_t *errors;

    if (!g_uart_ready || !g_dma_ready)
    {
        Test_Set(TEST_UART_DMA_TX, TEST_SKIP, 0U);
        return;
    }

    status = UART_TransmitDMA(dma_message,
                              (uint16_t)(sizeof(dma_message) - 1U));
    if (status != UART_DRIVER_OK)
    {
        Test_Set(TEST_UART_DMA_TX, TEST_FAIL, (uint32_t)status);
        return;
    }

    if (!Test_WaitForUartDma())
    {
        DMA_USART3_TX_Stop();
        Test_Set(TEST_UART_DMA_TX, TEST_FAIL, 1U); /* Completion timeout. */
        return;
    }

    errors = UART_GetErrorCounters();
    if ((errors != NULL) && (errors->dma_error == 0U))
    {
        Test_Set(TEST_UART_DMA_TX, TEST_PASS, 0U);
    }
    else
    {
        Test_Set(TEST_UART_DMA_TX, TEST_FAIL,
                 (errors != NULL) ? errors->dma_error : 2U);
    }
#endif
}

static void Test_Fdcan(void)
{
    FDCAN_Result_t result;
    FDCAN_Status_t status;

    result = FDCAN_Init(&g_test_fdcan, &g_test_fdcan_config);
    if (result != FDCAN_OK)
    {
        Test_Set(TEST_FDCAN_INIT_START_STOP, TEST_FAIL, (uint32_t)result);
        return;
    }

    if (!g_test_fdcan.Initialized || (g_test_fdcan.Instance != FDCAN1))
    {
        Test_Set(TEST_FDCAN_INIT_START_STOP, TEST_FAIL, 1U);
        return;
    }

    /* No frames are transmitted by this test. Disable FDCAN event sources so
     * the package port IRQ wrapper cannot receive unrelated test interrupts. */
    g_test_fdcan.Instance->IE = 0U;
    g_test_fdcan.Instance->IR = 0xFFFFFFFFUL;

    result = FDCAN_Start(&g_test_fdcan);
    if (result != FDCAN_OK)
    {
        Test_Set(TEST_FDCAN_INIT_START_STOP, TEST_FAIL, (uint32_t)result);
        return;
    }

    result = FDCAN_GetStatus(&g_test_fdcan, &status);
    if ((result != FDCAN_OK) || !status.Started)
    {
        (void)FDCAN_Stop(&g_test_fdcan);
        Test_Set(TEST_FDCAN_INIT_START_STOP, TEST_FAIL, 2U);
        return;
    }

    result = FDCAN_Stop(&g_test_fdcan);
    if ((result == FDCAN_OK) && !g_test_fdcan.Started)
    {
        Test_Set(TEST_FDCAN_INIT_START_STOP, TEST_PASS,
                 (uint32_t)status.TxFreeLevel);
    }
    else
    {
        Test_Set(TEST_FDCAN_INIT_START_STOP, TEST_FAIL, (uint32_t)result);
    }
}

static void Test_IntegrationSymbols(void)
{
    /* Check integration symbols without reinitializing the peripherals. */
    volatile uintptr_t fdcan_port_address = (uintptr_t)FDCAN_Port_Init;
    volatile uintptr_t peripherals_init_address = (uintptr_t)Peripherals_Init;

    if ((fdcan_port_address != 0U) && (peripherals_init_address != 0U))
    {
        Test_Set(TEST_FDCAN_PORT_SYMBOL, TEST_LINKED_ONLY,
                 (uint32_t)fdcan_port_address);
        Test_Set(TEST_COMMON_INIT_SYMBOL, TEST_LINKED_ONLY,
                 (uint32_t)peripherals_init_address);
    }
    else
    {
        Test_Set(TEST_FDCAN_PORT_SYMBOL, TEST_FAIL, 0U);
        Test_Set(TEST_COMMON_INIT_SYMBOL, TEST_FAIL, 0U);
    }
}

static void Test_FaultHandlerSymbols(void)
{
    /* Taking these addresses forces the linker to resolve the handler symbols. */
    volatile uintptr_t handler_addresses[4];

    handler_addresses[0] = (uintptr_t)HardFault_Handler;
    handler_addresses[1] = (uintptr_t)MemManage_Handler;
    handler_addresses[2] = (uintptr_t)BusFault_Handler;
    handler_addresses[3] = (uintptr_t)UsageFault_Handler;

    if ((handler_addresses[0] != 0U) &&
        (handler_addresses[1] != 0U) &&
        (handler_addresses[2] != 0U) &&
        (handler_addresses[3] != 0U) &&
        ((uintptr_t)&g_fault_record != 0U))
    {
        /* LINKED_ONLY is intentionally not reported as a runtime PASS. */
        Test_Set(TEST_FAULT_HANDLER_SYMBOLS, TEST_LINKED_ONLY,
                 (uint32_t)(g_fault_record.valid));
    }
    else
    {
        Test_Set(TEST_FAULT_HANDLER_SYMBOLS, TEST_FAIL, 0U);
    }
}

static void Test_Watchdog(void)
{
    Watchdog_Status_t status;

    status = Watchdog_Init();
    if (status == WATCHDOG_OK)
    {
        Watchdog_Refresh();
        g_watchdog_ready = true;
        Test_Set(TEST_WATCHDOG, TEST_PASS, Watchdog_GetResetFlags());
    }
    else
    {
        g_watchdog_ready = false;
        Test_Set(TEST_WATCHDOG, TEST_FAIL, (uint32_t)status);
    }
}

int main(void)
{
    __enable_irq();
    Test_InitializeReport();

    Test_Rcc();
    if (g_driver_test_report.test[TEST_RCC].state != TEST_PASS)
    {
        Test_Set(TEST_NVIC, TEST_SKIP, 0U);
        Test_Set(TEST_GPIO, TEST_SKIP, 0U);
        Test_Set(TEST_DWT, TEST_SKIP, 0U);
        Test_Set(TEST_TIMER, TEST_SKIP, 0U);
        Test_Set(TEST_DMA_INIT, TEST_SKIP, 0U);
        Test_Set(TEST_UART_INIT, TEST_SKIP, 0U);
        Test_Set(TEST_UART_BLOCKING_TX, TEST_SKIP, 0U);
        Test_Set(TEST_UART_DMA_TX, TEST_SKIP, 0U);
        Test_Set(TEST_FDCAN_INIT_START_STOP, TEST_SKIP, 0U);
        Test_Set(TEST_FDCAN_PORT_SYMBOL, TEST_SKIP, 0U);
        Test_Set(TEST_COMMON_INIT_SYMBOL, TEST_SKIP, 0U);
        Test_Set(TEST_FAULT_HANDLER_SYMBOLS, TEST_SKIP, 0U);
        Test_Set(TEST_WATCHDOG, TEST_SKIP, 0U);
        Test_PrintReport();
        for (;;) { __NOP(); }
    }

    Test_Nvic();
    Test_Gpio();
    Test_Dwt();
    Test_Timer();
    Test_Dma();
    Test_Uart();
    Test_UartDma();
    Test_Fdcan();
    Test_IntegrationSymbols();
    Test_FaultHandlerSymbols();
    Test_Watchdog();

    Test_PrintReport();

    /* Keep the board running and periodically service IWDG if it initialized. */
    for (;;)
    {
        if (g_watchdog_ready)
            Watchdog_Refresh();
        __NOP();
    }
}
