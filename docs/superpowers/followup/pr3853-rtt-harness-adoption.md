# Follow-up: finish RTT-console adoption in the HIL harness

Split out of the `rtt` skill-promotion PR #3853. That PR deliberately ships the skill + CLI and leaves the harness's remaining
VCOM assumptions in place — converting them is separate test-infra scope that
deserves its own review and HIL runs. Scope here is `test/hil/*.py` only; the
src-level `board_putchar` asymmetry this work surfaced has its own handoff
(`pr3853-board-putchar-logger.md`).

## Established (with evidence)

- `hil_util.JlinkRtt` + `open_board_console()` work end-to-end:
  ea4088_quickstart runs its host suite over RTT (16 passed / 0 failed / 3
  skipped, the 'hil: read the host console over RTT when the probe has no VCOM' commit), and the `rtt` skill's boards.md carries the
  validated matrix.
- All three host tests honor `"logger": "rtt"`: `test_host_device_info`,
  `test_host_cdc_msc_hid` and `test_host_msc_file_explorer` open through
  `open_console_reset()` (hil_test.py), which does the per-console reset
  ordering — RTT resets via the flasher BEFORE opening (the console owns the
  probe; Commander delivers the buffered boot burst), VCOM resets after — and
  each read loop fails fast on `JlinkRtt.eof` instead of blaming the board.
  Landed with the ea4088_quickstart roster entry; the interim load-time gate
  that rejected `logger: rtt` + `is_cdc`/`is_msc` is gone.

## Remaining gaps

1. **`hil_pool_check.check_host_serial` carries its own inline RTT branch**
   (reset → `JlinkRtt` → poll through `hil_util.strip_banner`) — RTT boards
   ARE health-checkable today, but the console-opening logic now lives in
   two places (`open_console_reset` in hil_test.py and this branch), each
   with its own reset-ordering. Fix: hoist `open_console_reset()` into
   `hil_util.py` next to `open_board_console()` and collapse pool_check's
   branch onto it; keep the `do_reset` flush semantics for the VCOM path
   intact.
2. **OpenOCD console backend in the harness**: the skill's CLI
   (`tools/rtt.py --backend openocd`, class
   `OpenocdRtt` in the same module) is built, deduplicated behind a shared
   base class next to `JlinkRtt` in `tools/rtt.py`, re-exported by
   `hil_util`, and hardware-validated (all 20 rig boards through the CLI on
   both backends, incl. the 8 native-probe ones). What remains is only the
   `open_board_console` plumbing: choosing `OpenocdRtt` for a
   `"logger": "rtt"` board with an openocd/stlink flasher needs the per-test
   flashed-ELF path (for the control-block address) and, for stlink
   flashers, an openocd target-cfg mapping the roster doesn't carry — until
   then the config-load gate keeps rejecting non-jlink rtt boards.

## Validation for this follow-up

A `hil_pool_check.py` pass on a no-VCOM board (ea4088_quickstart), plus the
ea4088 host suite to show the collapse did not change the reset ordering the
three tests depend on. Delete this doc when the follow-up PR lands.
