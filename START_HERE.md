# NOCCARC Embedded Firmware Assignment — Submission Index

**Start here.** This package contains the original executable project layout, four compiled STM32H743 firmware images, Renode scripts, Python dashboard, archived verification logs, final protocol documents and a dashboard demonstration recording.

## 1. Quick access

| Requested deliverable | Where to find it |
|---|---|
| Firmware sources and drivers | `src/`, `system/`, `include/`, `startup/`, `linker/` |
| Four firmware images | `build/node1/node1.elf` through `build/node4/node4.elf` (BIN and HEX beside them) |
| CAN FD protocol | `docs/submission/02_CAN_FD_Protocol.pdf` |
| CAN identifiers and message structure | `docs/submission/03_CAN_Identifier_Allocation.pdf` |
| Parameter ownership and synchronization | `docs/submission/04_Parameter_Ownership_and_Synchronization.pdf` |
| Heartbeat, timeout and recovery implementation | `src/network_state.c`, `src/main_network.c`, `docs/submission/01_Technical_Report.pdf` |
| Renode platform and launchers | `renode/assignment.repl`, `renode/network_setup.resc`, `renode/network.resc`, `renode/dashboard_demo.resc` |
| Dashboard source | `dashboard/monitor.py`, `dashboard/protocol.py`, `dashboard/index.html` |
| UART protocol | `docs/submission/05_UART_Protocol.pdf` |
| Video demo | `evidence/Dashboard_Demonstration_2026-11-19.webm` |
| Normal/fault simulation logs | `build/submission-verification/`; historical logs in `build/final-verification/` |
| Build and execution instructions | `docs/submission/06_Build_and_Execution_Guide.pdf`, `README.md` |
| Short technical document and architecture rationale | `docs/submission/01_Technical_Report.pdf` |
| Standalone algorithm flowchart | `docs/submission/07_Application_Flowchart.pdf` |

### New setup and demonstration recordings

1. [Setup and demo start to end (1:22)](evidence/01_demo_start_end.webm)
2. [Full dashboard demo (0:40)](evidence/02_dashboard_full_demo.webm)

The original dashboard-only recording is also included.

In the supplied video, the normal dashboard is visible around 4 seconds, MCU3 is offline around 12 seconds, and the dashboard is recovered around 24 seconds.

## 2. Reproduce from the package root

**Requirements:** Linux, GNU Make, host GCC, ARM GNU toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-nm`), Python 3, Renode (developed using 1.17.0). See `README.md` for more detail.

```bash
make -j4 firmware test
```

### Normal four-node simulation

Open two terminals at the project root. First start Renode:

```bash
renode --disable-xwt --console renode/network.resc
```

Then start the dashboard:

```bash
python3 dashboard/monitor.py /tmp/noccarc-uart
```

Visit `http://127.0.0.1:8765`.

### MCU3 disconnection, reset and recovery demonstration

Stop the previous Renode run, leave or restart the dashboard, and run:

```bash
renode --disable-xwt --console renode/dashboard_demo.resc
```

The launcher isolates MCU3, runs with the other nodes connected, resets/reloads MCU3 and reconnects it. `renode/network_setup.resc` provides the common four-machine and CAN-hub setup.

### Automated tests (separate from interactive demo)

```bash
python3 tests/network_sim.py --output build/submission-verification/reset
python3 tests/network_sim.py --reconnect-only --output build/submission-verification/reconnect
python3 tests/can_receive_sim.py
python3 tests/watchdog_sim.py --output build/submission-verification/watchdog
```

**Note:** These tests may overwrite output files in the named directories. Archived results are included so the reviewer can inspect the original evidence before running new tests.

## 3. Evidence summary

The supplied logs (`build/submission-verification/reset-results.txt` and `reconnect-results.txt`) report:

- Normal: 4/4 nodes online, 20/20 parameters fresh.
- MCU3 isolated: 3/4 online, 15/20 fresh; other nodes continue updates.
- MCU3 restored: 4/4 online, 20/20 fresh, including both reset/reconnect and reconnect-without-reset paths.
- Each archived network scenario reports 14 MCU1 UART DMA TX completions.
- `receive-results.txt`: malformed source/length and duplicate frames were rejected, and a valid subsequent frame committed.
- `watchdog-results.txt`: watchdog fed before a TIM2 stall, triggered one modeled reset and resumed feeding after reboot.

The above are **results archived in the supplied ZIP**. Host unit tests were additionally run during packaging. Renode and the ARM cross-compiler are not installed in the packaging environment, so target builds and Renode scenarios were not rerun here. Physical CAN timing and hardware-dependent behavior are outside these Renode results; see the technical report and `docs/NOCCARC_Open_Gaps_And_Limitations.md`.

## 4. Important protocol note

The four provided firmware images use the **running protocol**: 4-byte heartbeat (`0x100 + node`), 24-byte parameter block (`0x110 + node`), 100 ms parameter period, 250 ms heartbeat and 169-byte UART CRC-32 snapshots. The optional `src/network_protocol.c` and `src/network_rx_ring.h` alternatives were host-tested but are not the wire format or receive mechanism of these supplied firmware images. Use the numbered submission PDFs as the primary protocol documentation.

## 5. Package preservation

The original root-relative paths (`src`, `renode`, `dashboard`, `build`) remain unchanged so the Makefile and test scripts work without path edits. Git history, backup drafts, intermediate `.o/.d` files, caches and superseded Word report drafts were excluded for a cleaner email handover. Compiled firmware images and archived test evidence were retained. A rebuild will recreate intermediate objects.
