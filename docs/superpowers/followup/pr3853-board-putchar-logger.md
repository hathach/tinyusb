# `board_putchar` is not LOGGER-aware

**Origin:** surfaced while validating the RTT console in PR #3853 (the `rtt` skill
promotion), which is harness-only scope. This is a src-level fix to `hw/bsp/board.c`
that touches every board/logger combination, so it needs its own build sweep rather
than a drive-by. Delete this file when its own PR lands.

## Established (with evidence)

`hw/bsp/board.c` retargets stdio through `sys_write`/`sys_read`, which are compiled
per logger: `SEGGER_RTT_Write`/`SEGGER_RTT_Read` under `LOGGER_RTT`, ITM under
`LOGGER_SWO`, `board_uart_write`/`board_uart_read` by default. The two board-level
character helpers do not agree:

```c
168: int board_getchar(void) {
169:   char c;
170:   return (sys_read(0, &c, 1) > 0) ? (int) c : (-1);
171: }
172:
173: int board_putchar(int c) {
174:   if (board_uart_write((const char *)&c, 1) > 0) {
```

`board_getchar` follows the logger; `board_putchar` always goes to the UART. So with
`LOGGER=rtt` console input arrives over RTT while the echo goes out the UART.

Measured on ea4088_quickstart (`LOGGER=rtt`, `board_uart_write` is a `-1` stub on
lpc40): the `board_test` echo vanishes entirely while a `printf` echo — same console,
same keystroke — comes back byte-for-byte. `LOGGER=swo` has the same asymmetry by
construction (ITM out of `sys_write`, UART out of `board_putchar`), unverified on
hardware.

## What remains

Candidate fix: route `board_putchar` through `sys_write(0, ...)` for symmetry with
`board_getchar`. Two things to settle while doing it:

- `board_putchar` currently passes `&c` of an `int` to a `const char*` — it writes
  the low byte only on little-endian. Narrow to a `char` local as part of the change.
- The default (UART) path must keep its current return contract: `board_uart_write`
  returns negative when the UART is a stub, and the default `sys_write` breaks out of
  its retry loop on that, returning a short count — so `board_putchar` still has to
  map "wrote nothing" to `-1`.

## Validation

Build sweep across loggers and families — at minimum one UART board, one
`LOGGER=rtt` board and one `LOGGER=swo` board — plus a hardware check that the
`board_test` echo comes back on an RTT board (ea4088_quickstart reproduces the bug
today) and that a plain UART board's echo is unchanged.

## Why it was split out

PR #3853 promotes a debug-tooling skill and touches `test/hil/*.py` and
`tools/rtt.py`. A `hw/bsp/board.c` change lands in every example on every board and
belongs in a review that carries the build evidence for it.
