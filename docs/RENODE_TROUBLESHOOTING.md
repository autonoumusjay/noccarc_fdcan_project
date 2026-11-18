# Renode troubleshooting and UART transport

This record covers issues observed while building and testing the four-node STM32H743 project with Renode 1.17.0 (6549dbc07). It records the observed symptom, the change made, and the evidence supporting it.

| Symptom | Cause or diagnosis | Fix and verification |
|---|---|---|
| Renode stopped with `Arithmetic operation resulted in an overflow`. | The stack trace pointed to virtual-time scheduling during a multi-CPU run. | Enabled global serial execution and used a 100 µs global quantum. The four nodes still run together in simulated time. The fault-and-recovery tests ran with these settings. See [network.resc](../renode/network.resc). |
| Heartbeat and timeout behavior was slower than expected. | The stock platform modeled an 8 MHz HSE. The firmware clock tree assumes 25 MHz, affecting TIM2. | Set the HSE to 25 MHz in [assignment.repl](../renode/assignment.repl). Used the overlay for both network scenarios. |
| Renode reported `Could not find file 'assignment.repl'`. | The launcher resolved paths from its working directory. | Changed the [launcher](../renode/network.resc) to use project-root-relative paths and verified that all four images loaded when launched from the project root. |
| After `machine Reset`, MCU3 started with PC and SP both at zero. | The test scenario had not reloaded the ELF and its startup vector after reset. | Reloaded MCU3's ELF and reconnected it. The [watchdog test](../tests/watchdog_sim.py) registers a reset macro to restore the vector table location and reload the ELF when the watchdog resets the simulated MCU. |
| MCU3 remained offline after reconnecting without reboot. | Transmissions queued during disconnection filled its TX FIFO. The reconnect did not free the pending slots. | The firmware now requests cancellation of the oldest periodic frame if the FIFO stays full for a heartbeat interval. Cancellation is asynchronous and rate-limited. After the fix, MCU3's sequence advanced from 9 to 27 and all four databases returned to 20 valid parameters. See the [firmware](../src/main_network.c) and [reconnect log](../build/final-verification/reconnect/renode.log). |
| Watchdog expired earlier than expected. | The Renode platform used 56 kHz and the firmware calculated reload for a 32 kHz LSI. | Set the simulated watchdog clock to 32 kHz. The [stall test](../build/final-verification/watchdog/results.txt) verified that MCU2 serviced the watchdog without peers, reset once after TIM2 stalled, and resumed servicing after reboot. |
| The test occasionally reported a missing database result even though the value was printed. | Renode inserted asynchronous log lines between the marker and the register-read result. | The harness removes timestamped simulator log lines before parsing the database reads. The [reset-and-reconnect test](../build/final-verification/network/results.txt) then confirmed all 32 expected validity checks. |
| Renode reported unhandled writes for FDCAN timing and RCC fields. | The model only implements a subset of the hardware behavior. | Preserved the firmware's configuration for 500 kbit/s nominal, 2 Mbit/s data, and BRS. Verified logical frame exchange and recovery. Physical bit periods and bus-off behavior require hardware evidence. |
| The dashboard's HTTP server failed with `Operation not permitted`. | The execution environment denied the local socket. | Verified the real Renode USART3 → PTY → Python decoder path with zero CRC errors and generated a [browser replay](../build/final-verification/network/dashboard-replay.html) from captured snapshots. Live browser rendering remains unverified in this environment. |

During deliberate node isolation, `FrameSent is not initialized` is expected for the disconnected node. A fault run passes when the other three nodes continue exchanging data and the isolated node later recovers.

## Is the dashboard UART using DMA?

**MCU1's dashboard UART now uses DMA1 Stream 3 through DMAMUX1.** [Telemetry_Service()](../src/telemetry.c) submits each 169-byte snapshot with `UART_TransmitDMA()`. The UART driver copies it into a static DMA-accessible D2 SRAM buffer. The completion interrupt releases that buffer, and main-loop telemetry counts successful completions. The CAN loop keeps running while DMA is busy.

The default peripheral diagnostic still skips TX DMA under its Renode profile. A standalone retest overrode that skip, reported `UART_DMA_TX: PASS`, and captured the exact test bytes. The four-node scenario now additionally checks the DMA completion counter on MCU1 and validates the captured dashboard frames. The integrated run observed 14 successful completions and CRC-valid snapshots throughout recovery. A separate live run decoded DMA-backed snapshots from the PTY with zero CRC errors. [DMA run results](../build/final-verification/dashboard-dma/results.txt). Physical USART DMA request timing and cache coherency remain hardware checks.

## Reproduce the final checks

Run from the project root.

```bash
make -j4 firmware test
python3 tests/network_sim.py --output build/final-verification/network
python3 tests/network_sim.py --reconnect-only --output build/final-verification/reconnect
python3 tests/watchdog_sim.py
```

## UART link-loss banner while MCU1 is still sending

The firmware publishes every 250 ms of simulated time. Python uses wall time for the link-loss banner. On this host the old one-second rule falsely reported loss five times in the healthy run and six times in the MCU3 fault run. The longest measured gap was 4.20 wall-clock seconds, with valid UART CRCs.

The monitor now defaults to five wall-clock seconds without a valid frame. Both Renode launchers completed with zero false link-loss transitions after the change. A separate PTY test verified that five seconds of silence still triggers link loss and a new valid frame restores the link. Use `--link-timeout 1` with a physical UART if a one-second threshold is appropriate.

- [Before the fix](../build/submission-verification/uart-link/before.txt)
- [After the fix](../build/submission-verification/uart-link/after.txt)
- [Link-loss and restoration result](../build/submission-verification/uart-link/disconnect-reconnect.txt)
