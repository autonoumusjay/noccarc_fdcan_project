# Verification matrix

Tests used Linux, Renode 1.17.0 (6549dbc07), ARM GNU GCC 10.3.1 and Python 3.10.12. Source and original working images came from Git commit `c3a8592`. This submission revision adds stronger test coverage and documentation. It preserves the running firmware and wire formats.

Run from the repository root.

```sh
make -j4 firmware test
python3 tests/network_sim.py --output build/submission-verification/reset
python3 tests/network_sim.py --reconnect-only --output build/submission-verification/reconnect
python3 tests/can_receive_sim.py
python3 tests/watchdog_sim.py --output build/submission-verification/watchdog
```

| ID | Verification | Result and evidence |
|---|---|---|
| V01 | Four firmware images and host tests | **PASS.** Four `nodeN.elf` files and host C and Python tests. [Build log](../build/submission-verification/host.log) |
| V02 | All four databases fresh, valid and correctly assigned | **PASS.** Direct per-node memory and tick comparison. All 20 values fresh and valid. All matching owner sequences compared for exact five-value equality. [Reset results](../build/submission-verification/reset-results.txt) |
| V03–V04 | Isolate only MCU3, invalidate P11–P15 everywhere relevant, keep healthy peers running | **PASS.** All three healthy databases invalidated the MCU3 group and retained 15 fresh values. UART showed 3/4 online and sequences for healthy nodes advanced. [Reset log](../build/submission-verification/reset/renode.log) |
| V05 | Reconnect without reboot | **PASS.** MCU3 sequence 9 → 27, the four local databases returned to 20 fresh values, and MCU1 received 14 TX DMA completions. [Reconnect results](../build/submission-verification/reconnect-results.txt) |
| V06 | Reset and reconnect | **PASS.** MCU3 sequence 9 → 2, confirmed as a new post-reset value set. All four databases contained fresh valid groups. [Reset results](../build/submission-verification/reset-results.txt) |
| V07 | Stop TIM2 and verify watchdog reset | **PASS.** On an isolated MCU2, 6 feeds before stall, one reset and 3 feeds after reboot. [Watchdog results](../build/submission-verification/watchdog-results.txt) |
| V08 | UART DMA, CRC and link loss | **PASS for transport and decoder.** Both latest four-node tests verified 14 completed DMA transfers and zero decoder CRC errors. Previous live PTY and link-loss checks validated Python behavior. Browser verification remains blocked. [UART capture](../build/submission-verification/reset/uart.bin) · [Live UART record](../build/final-verification/dma-regression/live-uart-results.txt) |
| V09 | Malformed and duplicate frame rejection in running firmware | **PASS.** Injected through Renode FDCAN1 into the unmodified receiver. Mismatched ID and invalid application length increased reject count by two. A duplicate with altered bytes left the full group, sequence and update tick unchanged. A valid follow-up was accepted. [Result](../build/submission-verification/receive-results.txt) · [Injection test](../tests/can_receive_sim.py) |
| V10 | Three panels and live visual evidence | **BLOCKED for live browser rendering.** Browser file shows exactly three sections in source. This environment denies a localhost socket. [Replay from the captured DMA run](../build/submission-verification/reset/dashboard-replay.html) provides the recorded visual flow. |

Renode validates logical behavior. The report does not infer physical CAN timing, electrical bus-off, or physical DMA and cache behavior from these tests.
