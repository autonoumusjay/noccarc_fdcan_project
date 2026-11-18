# Assignment and checklist decisions

The official assignment sets the four-node behavior and CAN timing. The downloaded final checklist specifies the next wire formats and recovery behavior. The current firmware remains the Renode baseline while the new protocol is implemented and tested.

## Baseline

Repository root is `/home/jt/projects/noccarc_fdcan`. The checkout was already dirty, with tracked build changes and untracked drivers and test artifacts. Baseline HEAD was `84f9895ea74cfad211f6ee9631633934305d2a9f`. The setup uses four STM32H743 images and one Renode CAN hub. MCU1 UART is connected through a PTY. The repository's host target is `make test`. The reference core ZIP described in the checklist, along with its separate protocol and architecture documents, was absent from the inspected Downloads and project directories. The checklist's historic `make host` result is provenance from that reference, separate from the tests run here.

Existing target APIs are declared in `system/inc/FDCAN.h`, `FDCAN_Port.h`, `UART.h`, `Timer.h`, `Watchdog.h`, and `RCC_Clock.h`. FDCAN already accepts actual byte length and exposes decoded metadata. The driver uses shared Message RAM divided into two 1280-word regions and 18-word elements for 64-byte payloads. The application port configures 16 RX FIFO0 and eight TX elements and uses FDCAN1 interrupt line 0. The port leaves FIFO1 disabled. FDCAN1 ISR signals readiness and the main loop reads Message RAM. A software ring primitive is now host-tested, with hardware integration still pending. The Renode baseline uses 80 MHz PLL1Q from the modeled 25 MHz HSE, 500 kbit/s nominal, 2 Mbit/s data, and BRS. Simulation demonstrates logical message exchange. It does not measure electrical timing or physical bus-off.

## Protocol and timing migration

| Area | Running baseline | Checklist target |
|---|---|---|
| CAN messages | PARAM 24 bytes at 0x110 + node, heartbeat 4 bytes at 0x100 + node | PARAM 32 bytes at 0x100 + node, heartbeat 16 bytes at 0x040 + node, resync 12 bytes at 0x020 + node, emergency 16 bytes at 0x010 + node |
| CAN data integrity | Driver metadata and sequence checks | Explicit little-endian layout and CRC16/CCITT-FALSE, epoch, independent field validity and range checks |
| Timing | 100 ms publish, 250 ms heartbeat, 350 ms freshness, 750 ms timeout, 250 ms dashboard | 100 ms publish, 200 ms heartbeat, 50 ms sweep, 300 ms freshness, 400 ms delayed, 600 ms timeout, 500 ms dashboard |
| Scheduling | TIM2, 10 ms startup offsets, eight receive frames per pass | 1 ms timebase, 25/50 ms publication offsets, bounded receive and control queues. TIM2 is retained until an equivalent SysTick port is demonstrated. |
| Recovery | Sequence-zero restart heuristic and periodic updates | Retained monotonic epoch or documented reset fallback, RECOVERING until heartbeat plus fresh owner block, targeted bounded resync |
| UART | 169 bytes, NC header and CRC-32 over USART3 TX DMA. The earlier baseline used TX-ready polling. | 316 bytes, AA55 header, type 0x10 and CRC-16. The verified DMA transport can carry the revised format once the encoder and decoder migrate together. |
| Watchdog | Main-loop checkpoint-gated watchdog and Renode stall/reset test | Nominal 1 s timeout, service-checkpoint gate, capture reset flags |
| GUI | Three sections and UART link-loss handling | Same three sections fed by the matching 316-byte decoder, verified with live browser evidence |

The CAN protocol helper assigns type bytes 1–4 in order of emergency, resync, heartbeat and parameter update. Heartbeat and resync reserved flag bytes must be zero until their semantics are defined. Switch all four images together when the database and wire codec are integrated. Switch the UART encoder and decoder together.

## Acceptance status at G1

`PASS` signifies test evidence for the stated condition. Baseline passes remain tagged as baseline until rerun after migration.

| Test | Status | Evidence or gap |
|---|---|---|
| T01 | NOT_TESTED | Reference four-node host harness is unavailable locally. Renode baseline coverage is T09. |
| T02 | NOT_TESTED | Running-firmware ownership is verified in Renode for every active owner group. A separate host ownership-boundary test is pending. |
| T03 | PASS | Host tests cover DLC, CRC, exact length, ID/type, source, flags and reserved fields in the new codec. Firmware integration is pending. |
| T04 | PASS | Existing host test covers 65535 to 0, duplicate and older sequence rejection. |
| T05 | FAIL | No boot epoch in running firmware. |
| T06 | FAIL | Running database tracks group validity. |
| T07 | PASS | Host ring test covers full capacity, drop-newest, counter, unchanged tail, wrap and interleaved push/pop. ISR integration is pending. |
| T08 | NOT_TESTED | FIFO0 only in the current app port. No two-producer stress test. |
| T09 | PASS | Four-node simulation checks freshness, validity, ownership and matching-sequence equality in all four databases. |
| T10 | NOT_TESTED | Configuration and register writes were inspected. Target register readback has not been asserted by the harness. |
| T11 | FAIL | Running heartbeat is 250 ms. |
| T12 | PASS | All three healthy nodes invalidate P11–P15 and each retains 15 fresh values during MCU3 isolation. |
| T13 | PASS | Baseline UART captures show healthy peers' update sequences advance during isolation. |
| T14 | FAIL | No same-epoch reconnect distinction. |
| T15 | FAIL | Restart is demonstrated, but no epoch and confirmation contract yet. |
| T16 | PASS | Renode directly checks that all four groups on MCU3 are fresh and valid after reset. |
| T17 | FAIL | Targeted resync is absent from running firmware. |
| T18 | FAIL | Per-peer reported stale-owner mask is absent. |
| T19 | PASS | The integrated Renode test records 14 MCU1 DMA completions and CRC-valid snapshots. The separate live PTY check passed. The 316-byte UART format is pending. |
| T20 | PASS | Source audit confirms exactly three sections and decoder-sourced values. Live screenshot is pending. |
| T21 | BLOCKED | CRC host test passed. Live browser verification is blocked by local socket permissions. |
| T22 | PASS | Existing driver exposes real error state and counters. Renode physical limitations are documented. |
| T23 | PASS | Application gating implemented. Renode records 6 feeds without peers, one watchdog-triggered reset after TIM2 is stopped, and 3 feeds after reboot. |
| T24 | PASS | Bounded steady-state RX, TX and scheduling in source. |
| T25 | NOT_TESTED | Four baseline ELFs, scripts, logs and instructions present. Clean reference and migrated builds remain to verify. |
