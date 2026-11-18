# Four-node CAN FD network and monitoring gateway

## Scope and architecture

The assignment calls for four independent STM32H743 nodes sharing one CAN FD network, each with five owned values and a replica of the full 20-value database. MCU1 additionally reports its view over UART. The same bare-metal C application builds into four ELF images with distinct `NODE_ID` values. Renode loads each image into an independent MCU and connects each FDCAN1 to one logical CAN hub. Every node publishes directly to the other three. MCU1 carries no forwarding responsibility.

MCU1 owns P1–P5, MCU2 owns P6–P10, MCU3 owns P11–P15 and MCU4 owns P16–P20. Indexing the four groups by source ID keeps ownership and the five-slot mapping deterministic. The current firmware generates repeatable unsigned demonstration values from parameter ID and uptime. This makes a board reset visible and keeps the verification independent of external sensor hardware.

## Algorithm and design choices

1. **Initialize clocks and services.** Startup sets up the core, data and interrupt vectors. The application configures RCC, GPIO, the TIM2 timebase and FDCAN1. MCU1 also starts USART3 and DMA1 Stream 3. After peripheral initialization, each application captures RCC reset flags and starts IWDG.
2. **Publish owned state every 100 ms.** A complete five-value group is sent at each deadline and inserted into the local database. A group format gives all five values one sequence and timestamp. Periodic publication makes late joining and recovery automatic. If the TX FIFO stays full for one heartbeat interval, the scheduler requests cancellation of its oldest periodic frame. This frees space for current updates and prevents a disconnected transmitter from remaining stuck after rejoin. Cancellation is asynchronous and rate-limited to one request per heartbeat interval while full. Initial deadlines are staggered by 10 ms per node to reduce synchronized bursts in the simulation.
3. **Receive with bounded work.** The FDCAN interrupt acknowledges the event and signals readiness. Main context drains up to eight messages per pass. It validates the CAN identifier, sender, exact payload length, version, FD flag and BRS before updating a group. One main context owns database writes. Within an established group, a modular 16-bit sequence comparison discards duplicates and older updates while allowing wraparound.
4. **Check link health separately from readings.** Heartbeats are sent every 250 ms. The deadline retries a busy heartbeat up to three times with 10 ms spacing. A group becomes stale after 350 ms without a parameter update. Its peer becomes offline after 750 ms without a heartbeat, and its group is invalidated. These separate thresholds distinguish missing readings from a missing controller.
5. **Rejoin using periodic traffic.** A valid heartbeat from an offline peer reopens its receive path and sets a recovery indicator. That heartbeat alone leaves the parameter group invalid. A subsequent valid owner update replaces the invalid group and completes recovery. Healthy nodes continue publishing throughout the outage. The algorithm accepts the rejoined sender's first valid sequence as a new baseline after the group was invalidated, so both reconnect without reset and reconnect following reset converge.
6. **Export MCU1's real view.** MCU1 emits a bounded UART snapshot every 250 ms. Python finds the header, validates the CRC, and exposes the latest decoded snapshot. The browser renders overall health, individual controller health, and the 20 readings. It labels link loss when validated snapshots stop for one second. The captured replay uses the same display and reports actual UART data from the run.
7. **Refresh the watchdog only on application progress.** Main runs the health sweep and receive service, then requires both a heartbeat scheduling attempt and a local publication since the prior feed. The feed also requires an advancing timer and, on MCU1, recent UART transmit progress. This allows a disconnected slave to keep running and causes a stopped scheduler clock to stop feeding the watchdog.

## CAN configuration and protocol

A modeled 25 MHz HSE is divided by 5 and multiplied by 160 for an 800 MHz VCO. PLL1P divides that by 2 and PLL1Q by 10, giving 400 MHz SYSCLK and 80 MHz to FDCAN. The CAN clock calibration path is configured for bypass and divide-by-one. Using timing counts of prescaler/SJW/TSEG1/TSEG2 of 10/2/13/2 gives 500 kbit/s and an 87.5% nominal sample point. Counts 4/2/7/2 give 2 Mbit/s and an 80% data sample point. CAN FD and BRS are enabled. The driver uses 64-byte Message RAM elements so it supports all legal FD payload lengths.

| Frame | Standard CAN ID | Payload |
|---|---|---|
| Heartbeat | `0x100 + node` | 4 bytes with version, source, and little-endian 16-bit sequence |
| Parameters | `0x110 + node` | 24 bytes with the same header and five little-endian 32-bit values |

Four nodes produce about 40 parameter frames and 16 heartbeats per second. As a conservative planning estimate, budget 64 nominal-rate bit-times per frame, 320 data-rate bit-times per parameter frame and 160 per heartbeat, then add 25% for stuffing margin. This gives about 1.9% bus occupancy before retries or additional traffic. It is an analytic planning estimate. Renode's hub delivers logical frames and does not measure physical wire occupancy or timing.

## UART protocol and monitoring

USART3 runs at 115200 baud, 8-N-1. Each 169-byte `NC` version-1 snapshot contains a 17-byte header, four 37-byte group records and a 4-byte IEEE CRC-32. The header carries time, nominal rate, data rate and BRS. Each record carries health flags, heartbeat age, missed count, recovery count, parameter sequence and five readings. Exact offsets are in the [project README](../README.md).

MCU1 submits one snapshot to DMA1 Stream 3 through DMAMUX1. The UART driver copies it to a static DMA-accessible D2 SRAM buffer and cleans the cache before starting the transfer. The CPU continues servicing CAN while DMA transfers the bytes. It does not overwrite an active snapshot, and watchdog telemetry progress requires a successful DMA completion. At four snapshots per second, 169 bytes × 10 bits per byte uses about 5.9% of the configured UART bandwidth. The dashboard health score weighs each online MCU and each fresh valid parameter equally. It uses `100 × (online + synchronized) / 24`.

## Reproduce the verification

Run the following from the repository root on Linux with ARM GNU GCC, Make, Python 3 and Renode available.

```sh
make -j4 firmware test
python3 tests/network_sim.py --output build/final-verification/network
python3 tests/network_sim.py --reconnect-only --output build/final-verification/reconnect
python3 tests/watchdog_sim.py
```

The fault harness checks all four database groups on all four boards before and after the fault. It checks that MCU3's group becomes invalid and the other three groups continue updating. It verifies the recovered group has a newer sequence and value after a reconnect, or a reset-era sequence and value after a reboot. It rejects any unplanned watchdog expiry in these runs. The watchdog harness runs MCU2 without other peers, checks the feed counter, disables TIM2, and checks for one watchdog reset followed by resumed feeding. The Renode reset hook reloads the image after the watchdog model requests a machine reset.

Firmware derives the watchdog reload from a nominal 32 kHz LSI clock and a divide-by-64 prescaler. The Renode overlay explicitly uses 32 kHz because the stock platform uses 56 kHz. The test checks no expiry before 0.7 seconds after the induced stall and expiry with a restart during the following 0.9-second window. Physical LSI tolerance and the physical RCC watchdog reset-cause flag require a board check. Renode's watchdog model requests a reset without setting the physical RCC watchdog cause flag.

## Acceptance and limitations

The watchdog exercise covers legitimate absence of peers, a stalled scheduler timebase and recovery after actual model-triggered reset. The network tests cover the assignment's passive resynchronization after a disconnected controller returns. Hardware checks that remain include oscillator and power bring-up, measured physical CAN bit timing, electrical errors and bus-off, physical watchdog reset flags, and USART3 TX DMA. The integrated DMA test recorded 14 successful completions with CRC-valid snapshots during the outage and recovery. A separate live test passed the DMA-backed USART3 stream through the PTY to Python with zero CRC errors. See the [DMA run results](../build/final-verification/dashboard-dma/results.txt) and [replay](../build/final-verification/dashboard-dma/dashboard-replay.html). The environment blocks the browser's HTTP server, so live browser verification remains outstanding. Open the generated `dashboard-replay.html` from either network run to review the captured normal, offline and recovery states.

The downloaded checklist adds requirements beyond the current assignment implementation, including a boot epoch, exact 32/16/12/16-byte CRC-16 CAN formats, per-parameter validity and ranges, targeted resync, and a 316-byte UART frame. The G1 codec and software receive ring pass host tests. The production images still use the running protocol described above. The current recovery demonstration establishes passive resynchronization and does not establish stale-epoch replay rejection or targeted resync. A short restart that loses the initial sequence-zero heartbeat may remain undetected until timeout or subsequent accepted traffic. See [DECISIONS.md](DECISIONS.md) for the migration and the itemized checklist audit.

## Submission verification update

The source checkout is based on `c3a8592`. The strengthened Renode tests inspect each board's own clock and database. Normal and recovery phases verify freshness, validity, value ownership and matching-sequence equality. During isolation, each healthy node must invalidate P11–P15 and keep the other 15 parameters fresh. The reset run observed 12 matching-sequence payload comparisons in normal and recovered phases and six during the outage.

The live firmware receiver was separately tested with a wrong sender ID, wrong application length and a duplicate with altered data. None changed the stored values, sequence, timestamp or validity. A subsequent valid update was accepted. Both four-node runs delivered 14 USART3 TX DMA transfers each with CRC-valid snapshots. The watchdog test verified one reset and subsequent feeds.

| Submission appendix | Content |
|---|---|
| [Requirement traceability](NOCCARC_Requirement_Traceability.md) | PDF objective, implementation and status |
| [Architecture and algorithms](NOCCARC_Architecture_And_Algorithms.md) | MCU resources, data flow and timing decisions |
| [Verification matrix](NOCCARC_Verification_Matrix.md) | V01–V10 with commands and evidence paths |
| [Open gaps](NOCCARC_Open_Gaps_And_Limitations.md) | Live browser evidence and hardware limits |

The current code is suitable for the core Renode demonstration. A live three-state browser capture remains outstanding. The recorded replay uses real MCU1 output. The alternate CAN and UART protocol proposals remain outside the current running implementation.
