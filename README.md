# Four-node CAN FD network and UART monitor

The firmware implements the Noccarc embedded firmware assignment. Four independent STM32H743 instances share a Renode CAN hub. Each owns five parameters and keeps a local copy of all 20. MCU1 also emits its database and node health over UART.

## Build and run

On Linux, install Make, ARM GNU GCC, a host C compiler, Python 3 and Renode. The simulation was developed with Renode **1.17.0 (6549dbc07)**.

```sh
make -j4 firmware
make test
make sim-test
```

The four firmware targets produce ELF, HEX and BIN images under `build/node1` through `build/node4`. The fault scenario writes `build/simulation/uart.bin` and `renode.log`. To select a Renode executable explicitly, run `python3 tests/network_sim.py --renode /path/to/renode` after building.

For live monitoring, run the following in separate terminals from the repository root.

```sh
renode --disable-xwt --console renode/network.resc
python3 dashboard/monitor.py /tmp/noccarc-uart
```

Open http://127.0.0.1:8765. To replay a captured fault demonstration, run `python3 dashboard/monitor.py --replay build/simulation/uart.bin`.

The Renode monitor allows fault injection. Select node3 with `mach set "node3"`, disconnect it with `connector Disconnect sysbus.fdcan1 canHub`, and leave the other nodes running. To restart it, issue `machine Reset`, reload `sysbus LoadELF @/absolute/path/to/build/node3/node3.elf`, and reconnect with `connector Connect sysbus.fdcan1 canHub`. The automated test does this and checks a normal, offline and recovered snapshot from MCU1.

`make mcu1 mcu2 loopback driver_test` builds the original bring-up programs. The project defaults to the Renode clock profile, which bypasses the board-specific power sequence. For a hardware build, run `make RENODE=0 firmware`. Hardware images are written to `build/hardware`. The 25 MHz HSE, supply and I/O assumptions require confirmation on the actual board.

## CAN FD and parameter database

- MCU1 owns P1–P5, MCU2 owns P6–P10, MCU3 owns P11–P15 and MCU4 owns P16–P20. Each owner generates deterministic unsigned 32-bit sample values and sends its whole group. Received groups are read-only replicas.
- Standard IDs `0x101–0x104` are heartbeats and `0x111–0x114` are parameter updates. Both are CAN FD with BRS. Byte 0 is version 1, byte 1 is source MCU 1–4, and bytes 2–3 are a little-endian sequence number. Heartbeats are four bytes. Updates append five little-endian 32-bit values in parameter-ID order, for 24 bytes total. The driver supports up to 64 bytes.
- RX validates the ID, source, version, frame format, BRS and exact length before changing the database. A per-owner sequence uses modular half-range comparison to reject duplicate and older updates. Each group stores an arrival tick, sequence and validity shared by its five values because each update replaces the entire group.
- Parameters publish every 100 ms to make changes visible without saturating the bus. Freshness expires after 350 ms, allowing three missed update periods plus margin. Heartbeats run every 250 ms. A 750 ms timeout tolerates two missed intervals and invalidates the owner’s group. Missed-heartbeat accounting allows half an interval of jitter and increments from sequence gaps when a peer resumes.
- An offline peer becomes online on its next valid heartbeat and remains in recovery until a fresh group arrives. Sequence zero after an established nonzero heartbeat also signals a restart, except at natural counter wrap. A heartbeat gets up to three enqueue attempts, 10 ms apart. Its sequence advances after a successful enqueue or after the final failed attempt; a congested parameter update is superseded by the next 100 ms publication. Initial deadlines stagger by 10 ms per node.
- PLL1Q supplies 80 MHz from the 25 MHz HSE. The nominal timing counts are prescaler/SJW/TSEG1/TSEG2 of 10/2/13/2, yielding 500 kbit/s at an 87.5% sample point. The data counts are 4/2/7/2, yielding 2 Mbit/s at 80%. Renode validates logical frame exchange. Its FDCAN model does not validate electrical signaling, arbitration timing, physical ACK or bus-off behavior.

## Execution and UART protocol

The startup path configures RCC, GPIO, a TIM2 monotonic tick and FDCAN. FDCAN RX IRQ records readiness. The main loop drains at most eight frames per pass, updates heartbeat state and runs due application tasks. The parameter and health databases are written in main context. The firmware allocates its state and telemetry buffers statically. The master UART service sends at most eight ready bytes per pass and never waits for UART TX.

USART3 uses 115200 baud, 8 data bits, no parity and one stop bit. The 169-byte snapshot frame uses little-endian fields.

| Offset | Contents |
|---|---|
| 0 | ASCII `NC`, version 1, snapshot type 1 |
| 4 | MCU1 monotonic tick, arbitration bitrate, data bitrate (three 32-bit integers), BRS byte |
| 17 | Four 37-byte records in MCU order. Each record has flags followed by heartbeat age, missed count, recovery count, parameter sequence and five values, all 32-bit. Flags bits 0–4 indicate online, heartbeat seen, recovering, valid and fresh. Heartbeat age is `0xffffffff` before the first heartbeat. |
| 165 | CRC-32 (IEEE) over bytes 0–164 |

MCU1 produces one snapshot every 250 ms. The Python parser handles partial reads, resynchronizes on the header after corruption and rejects CRC mismatches. The dashboard shows UART loss after one second and marks retained data stale. Network health weighs each online controller and each fresh valid parameter equally, using `100 × (online + synchronized) / 24`. All values displayed come from MCU1.

## Verification evidence

[Scenario results](build/simulation/results.txt) cover normal operation, disconnection of MCU3 and recovery after reloading its firmware. The test inspects all four local databases before and after the outage, and uses MCU1 UART snapshots to verify 4/4 → 3/4 → 4/4 online and 20/20 → 15/20 → 20/20 fresh parameters. [Dashboard replay](build/simulation/dashboard-replay.html) opens directly in a browser and cycles through the captured UART snapshots. The simulation test regenerates it, the binary UART capture and the Renode log. Live Renode USART3 output was also decoded through the UART PTY.

The [peripheral diagnostic](build/simulation/driver-diagnostic.log) reported 10 runtime passes, zero failures, one simulator skip and three symbol checks. The STM32H7 platform uses an STM32F7 USART model with RX DMA requests only, so the diagnostic skips TX DMA in the Renode profile. MCU1's assignment telemetry uses non-blocking TX-ready polling. Hardware-only checks include physical CAN timing, board power setup and USART3 TX DMA.

## Engineering report and final checks

[Engineering report](docs/ENGINEERING_REPORT.md) explains the algorithm, timing and protocol choices, watchdog behavior, verification procedure, and limitations. [Checklist audit](docs/DECISIONS.md) tracks the stricter downloaded specification separately.

```sh
python3 tests/network_sim.py --output build/final-verification/network
python3 tests/network_sim.py --reconnect-only --output build/final-verification/reconnect
python3 tests/watchdog_sim.py
```

The watchdog test stops TIM2 in a single isolated simulated MCU. It checks that IWDG expires and the firmware resumes feeding after reboot.
