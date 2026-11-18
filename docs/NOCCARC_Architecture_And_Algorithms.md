# Architecture and algorithms

```text
MCU1 P1–P5      MCU2 P6–P10      MCU3 P11–P15      MCU4 P16–P20
       All four FDCAN1 peripherals share one logical CAN FD hub.
MCU1 database → USART3 + DMA1 Stream 3 → Renode PTY → Python → browser
```

| Module | Responsibility |
|---|---|
| Startup assembly and linker script | Reset entry, interrupt vectors, Flash and RAM placement, initialized data and zeroed BSS |
| RCC and GPIO | Board clock profile and alternate-function pin routing |
| TIM2 and NVIC | Monotonic 1 ms scheduler tick and interrupt dispatch |
| FDCAN and FDCAN_Port | CAN timing, Message RAM, hardware RX/TX FIFOs and interrupt-ready notification |
| main_network and network_state | Periodic scheduling, protocol validation, five-value group database, freshness and recovery |
| telemetry, UART and DMA | Build and transmit MCU1 snapshots with a static D2 SRAM staging buffer |
| protocol.py, monitor.py and index.html | Validate, serve and render the latest MCU1 snapshot |
| IWDG | Recover if local scheduling and required telemetry progress stop |

## Startup and clocks

The entry point initializes RCC, pin modes, TIM2 and FDCAN. MCU1 also initializes USART3 and DMA. Firmware snapshots RCC reset flags and clears them before starting the watchdog. GPIO uses PA11/PA12 AF9 for FDCAN1 and PB10/PB11 AF7 for MCU1 USART3. Only the hardware profile executes board voltage scaling. The 25 MHz HSE gives 800 MHz PLL1 VCO, 400 MHz SYSCLK and 80 MHz PLL1Q. The M/N/P/Q divisors are 5/160/2/10. HCLK is 200 MHz and PCLK1 is 100 MHz. APB timer doubling gives TIM2 a 200 MHz input. PSC=199 and ARR=999 yield 1 ms. FDCANSEL=01 selects PLL1Q, with CCU bypass and divide-by-one. Nominal 10/2/13/2 and data 4/2/7/2 prescaler/SJW/TSEG1/TSEG2 give 500 kbit/s (87.5%) and 2 Mbit/s (80%). Renode logical delivery verifies no physical bit periods.

## CAN scheduling, validation and storage

Each scheduler pass first updates health, drains at most eight RX frames, handles TX backpressure, schedules due heartbeat and data, services MCU1 UART, and evaluates the watchdog feed gate. Main context owns all database changes. ISR only reports RX readiness and acknowledges hardware interrupts. The driver uses 16 RX and eight TX elements of 18 words each in shared Message RAM (64 payload bytes plus two header words), without programmed acceptance-filter entries. Remote frames are rejected. The application checks sender, known standard ID, FD, BRS, version, exact decoded length and sequence.

| Frame | ID | Byte map |
|---|---|---|
| Heartbeat | 0x100 + node | Byte 0 version 1, byte 1 owner, bytes 2–3 16-bit little-endian heartbeat sequence |
| Parameters | 0x110 + node | Same four header bytes with data sequence, then five little-endian uint32 values |

Every 100 ms the node calculates each owned value as `1000 × parameter_id + uptime_ms / 100`, updates its local group and queues a broadcast. The 10 ms per-node initial offsets spread startup. Heartbeats run every 250 ms. Busy heartbeat enqueue receives up to three total attempts spaced 10 ms apart. A FIFO full for one heartbeat interval triggers asynchronous cancellation of its oldest periodic transmission. The next due group carries the newest values.

Parameter identity is implied by the owner and slot. A received valid owner group replaces its five values with one timestamp and sequence. A modular sequence comparison allows wraparound and rejects duplicates and backwards updates. Data age ≥350 ms is stale. Heartbeat age ≥750 ms invalidates a previously seen owner and marks it offline. A returning heartbeat allows reception. The first accepted owner block establishes a new valid sequence baseline. This passive recovery works with and without reboot. There is no persistent boot epoch in the current wire format.

## UART DMA and watchdog

Every 250 ms MCU1 builds a 169-byte snapshot. Bytes 0–16 carry the NC/version/type header, MCU1 tick, nominal rate, data rate and BRS. Bytes 17–164 contain four 37-byte records with flags, heartbeat age, missed count, recovery count, accepted sequence and five readings. Bytes 165–168 are little-endian IEEE CRC-32. The 115200 baud 8-N-1 link uses about 5.9% of nominal bandwidth at four frames per second. The driver stages the snapshot in DMA-accessible D2 SRAM and cleans cache lines before starting DMA1 Stream 3 through DMAMUX1. A completion interrupt releases the staging buffer. Firmware avoids reusing it while busy.

Python checks the header and CRC before publishing the latest snapshot. The browser shows three sections: overall health and link, four MCUs, and 20 parameters. Loss of valid snapshots for one second freezes the last snapshot and marks its parameters stale. Health score in a valid snapshot is rounded from 100 × (online nodes + fresh valid parameters) / 24.

The watchdog feeds after local heartbeat scheduling and parameter publication have progressed and the timebase advances. MCU1 additionally requires a recent successful DMA completion. Missing peers alone do not stop feeds. In the Renode stall test, stopping TIM2 causes a single reset, after which a reset macro reloads the ELF and feeding resumes.
