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
- `test_host_device_info` honors `"logger": "rtt"` (hil_test.py, `test_host_device_info`; the eof fail-fast assert sits in its read loop):
  in RTT mode it resets via the flasher BEFORE opening the console (which
  then owns the probe; Commander delivers the buffered boot burst) and its
  read loop fails fast on `JlinkRtt.eof` instead of blaming the board.

## Remaining gaps

1. **`test_host_cdc_msc_hid` and `test_host_msc_file_explorer` (hil_test.py) still call `hil_util.get_serial_dev(flasher["uid"], ...)`
   directly** — on a `logger: rtt` board with `is_cdc`/`is_msc` fixtures they
   would fail with the same "No serial device found" the console work fixed
   for device_info (an interim load-time gate in `hil_test.py` now rejects
   that combination up front; delete the gate when this lands). Fix: route
   both through `open_board_console(board)` — but design the conversion
   reset-aware rather than hand-copying device_info's dual branch: hoist a
   `reset=` parameter into `open_board_console` that does the per-console
   ordering itself (RTT: reset via flasher BEFORE opening — the console owns
   the probe; VCOM: reset after open to catch the banner), and REMOVE the
   existing post-open `# reset device to catch mount messages` blocks in both
   tests (grep the marker — line numbers churn) — kept as-is on an RTT board they reset
   while the console holds the probe. `JlinkRtt` carries input for their
   menus and implements the `reset_input_buffer()` those tests call.
2. **`hil_pool_check.check_host_serial` carries its own inline RTT branch**
   (reset → `JlinkRtt` → poll through `hil_util.strip_banner`) — RTT boards
   ARE health-checkable today, but the console-opening logic now lives in
   two places (`open_board_console` in hil_test.py and this branch), each
   with its own reset-ordering. Fix: hoist `open_board_console()` into
   `hil_util.py` with the `reset=` parameter from item 1 and collapse
   pool_check's branch onto it; keep the `do_reset` flush semantics for the
   VCOM path intact.
3. **OpenOCD console backend in the harness**: the skill's CLI
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

Run the ea4088 local host suite (a board with a `is_cdc`+`is_msc` capable
device attached to J3, or the rig's frdm_k64f/mimxrt1064 with a temporary
`logger: rtt` entry) so cdc_msc_hid and msc_file_explorer actually execute
over RTT; then a `hil_pool_check.py` pass on a no-VCOM board. Delete this doc
when the follow-up PR lands.
