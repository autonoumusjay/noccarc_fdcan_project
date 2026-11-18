# Open gaps and limitations

| Area | Classification | Next evidence or change |
|---|---|---|
| Live dashboard screenshot or recording | PDF submission evidence. Live rendering blocked by this session's localhost socket restriction. | Run Renode and the Python monitor on the developer desktop. Capture normal, MCU3 disconnected and recovered browser states. Use the recorded replay as documented captured data. |
| STM32H743 clock, transceivers and electrical CAN behavior | Outside the PDF's simulation scope. Hardware unverified. | Confirm board HSE and mux readback and measure bit periods. Renode logs unhandled FDCANSEL/NBTP/DBTP fields and passes logical frames. |
| USART DMA/cache and watchdog reset flags | Hardware unverified. Renode DMA snapshots and watchdog reset/reboot pass. | Check physical request pacing, cache coherency, abort/error paths, LSI tolerance and the RCC reset cause on the board. |
| Timing and DMA driver hardening | Optional robustness work. | The DMA stream-disable loops have no explicit timeout. Add a model or hardware failure test before changing stream state handling. The application watchdog test exercises a scheduler stall. |
| Never-seen peers and HTTP server loss | Optional UI hardening. | A peer never heard from is offline and waiting, rather than timed out. Loss of HTTP is indicated separately from UART silence. |
| Host-only codec and receive ring | Optional migration, not integrated into target firmware. | Boot epochs, targeted resync, individual per-field validity, added CAN CRC-16, software RX ring, and 316-byte UART format require coordinated adoption. Current 4/24-byte CAN and 169-byte UART formats are the demonstrated configuration. |
| Processor-fault capture | Symbol/link inspection only. | A deliberate fault injection and stacked-context check would be a separate test. |
