#ifndef PERIPHERAL_CONFIG_H
#define PERIPHERAL_CONFIG_H

/* STM32H743VIT6 / Noccarc assignment default configuration. */
#define NOCARC_FDCAN_KERNEL_HZ          80000000UL
#define NOCARC_FDCAN_NOMINAL_BAUD       500000UL
#define NOCARC_FDCAN_DATA_BAUD          2000000UL
/* Timing counts for PLL1Q at 80 MHz with CCU bypass and divide-by-one. */
#define NOCARC_FDCAN_NOMINAL_TIMING     { 10U, 2U, 13U, 2U }
#define NOCARC_FDCAN_DATA_TIMING        { 4U, 2U, 7U, 2U }

/* USART3: PB10=TX (AF7), PB11=RX (AF7), 115200-8-N-1. */
#define NOCARC_UART_BAUDRATE            115200UL
#define NOCARC_UART_TX_BUFFER_SIZE      512U
#define NOCARC_UART3_DMA_REQUEST_ID     46U
#define NOCARC_UART_DMA_BUFFER_ADDRESS  0x30000000UL /* Reserved D2 SRAM1 block. */
#define NOCARC_UART3_DMA_STREAM         3U

/* Watchdog period is approximate because the internal LSI oscillator varies. */
#define NOCARC_IWDG_TIMEOUT_MS          1000UL
#define NOCARC_ENABLE_IWDG              1U

/* Default board pin mapping; adjust if your Renode .repl or board differs. */
#define NOCARC_FDCAN1_GPIO_PORT         GPIOA
#define NOCARC_FDCAN1_RX_PIN            11U
#define NOCARC_FDCAN1_TX_PIN            12U
#define NOCARC_FDCAN1_AF                9U

#define NOCARC_UART3_GPIO_PORT          GPIOB
#define NOCARC_UART3_TX_PIN             10U
#define NOCARC_UART3_RX_PIN             11U
#define NOCARC_UART3_AF                 7U

#endif
