# Follow-up: the HIL farm as a commodity, provided by a user-wide `hil` skill

Postponed 2026-09-16 after the design round below; pick up from here. Related:
agentrc issue #1 (move the hil skill out of tinyusb, give chief a rig probe) and
`rtt-full-move.md` (superseded in part, see "rtt.py" below).

## Decided (maintainer)

- The farm (rigs, board inventory, probes, locks, flashing, serial/RTT
  consoles, bounded run, report) is a commodity shared by projects.
- It is provided by a **user-wide `hil` skill in agentrc** with scripts, not a
  separate repository or a pip package (both plans had proposed that; overruled).
- `hil_test.py`'s test cases are **project scope**: they stay in the project.
  The skill is the runner: it gives access (console, probe), runs the project's
  tests under the rig's locks, and reports.

## Agreed by both plans (mine and Codex's, drafted independently)

Generic layer today, in `test/hil/`: roster loading, flashers (`hil_flash.py`:
jlink/stlink/openocd/esptool/lm4flash), board locks + controller permits
(`helper/hil_lock.py`), consoles (pyserial + `tools/rtt.py`), sysfs/usb_scan and
bounded subprocess helpers (`helper/hil_util.py`), pool + health
(`helper/hil_health.py`), pool check (`helper/hil_pool_check.py`), report
(`helper/hil_report.py`), SSH staging (`.claude/skills/hil/scripts/hil_remote.py`).

Project layer: the ~30 `test_*` cases and `_tests_for` map in `hil_test.py`
(~1500 lines), the example list in `hil_util.py:31-52`, `find_firmware` and the
`examples/cmake-build-<board>[-variant]/<example>` layout, `build_board`,
`tools/ci_select.py`, DUT identity (VID `cafe`), "missing binary = skip" and
"metric string = pass" policies.

Seams where TinyUSB leaks into generic files (Codex, verified by path):
`hil_flash.py:308` build-path knowledge; `hil_lock.py:165` controller discovery
through VID `cafe` and firmware serials; `hil_pool_check.py:224` selects and
builds TinyUSB examples, parses `usb_descriptors.c`, recognizes "Hello from
TinyUSB"; `hil_report.py:320` consumes TinyUSB worker tuples and failure
columns; `hil_remote.py` stages a TinyUSB source tree into a reused directory.

Lifecycle the runner owns: validate -> reserve -> flash -> test -> restore
approved state -> release -> report. Test success and cleanup success are
reported apart. A project's adapter keeps flash/reset/console access inside its
reservation (several TinyUSB cases reflash mid-test).

First thin slice (both): a standalone non-TinyUSB RTT echo/heartbeat firmware
on `ea4088_quickstart` (J-Link probe 611000000, RTT console), driven by the
skill with a minimal project module, sharing the board with TinyUSB under the
same lock. Hardware proofs: another holder blocks flashing; the artifact flashes
and prints its build token; a host challenge is answered; a deliberate failure
is reported as such; a timeout releases probe and console and records cleanup;
the next run succeeds; the TinyUSB path still honours the reservation. Then the
same bundle through SSH staging into a unique run directory.

rtt.py: fold into this extraction. Drop the lock-file/loader design in
`rtt-full-move.md`; keep the two byte-identical copies (`tools/rtt.py`,
agentrc `skills/rtt/scripts/rtt.py`) until the farm skill consumes one of them.

Locks: keep `/tmp/tinyusb-hil-locks` and the current lock identity through the
migration (a new namespace would let old and new clients flash one board at
once); an unusable lock directory must fail closed (`hil_lock.py:80` proceeds
unlocked today).

Rigs: ci.lan's runner is user `hathach`, agentrc installed. tusb (hifiphile)
has no agentrc; `hfp.json` has no RTT board. Its rollout goes through its
maintainer.

## Open questions (with both recommendations)

| # | Question | Mine | Codex |
|---|----------|------|-------|
| 1 | Project contract shape | a Python module named in the project's CLAUDE.md recipe line, imported by the runner: `test_*(board, ctx)`, example->test map, firmware locator, DUT identity | a versioned run description (artifacts with hashes, case ids, budgets) plus an explicit Python entry point |
| 2 | Inventory ownership | physical roster moves to agentrc under the hil skill, one file per rig; project keeps a selection file keyed by board name | farm-owned, deployed explicitly by the rig operator, revision logged per run; a project cannot change live wiring |
| 3 | rtt.py home in agentrc | stays `skills/rtt/scripts/rtt.py`, hil scripts import the sibling | with the runtime, rtt skill keeps instructions only (= `skills/hil/scripts/`) |
| 4 | Pinning | float on the installed skill, report logs the agentrc revision (as for rtt/etm-trace/target-debug) | project records an agentrc revision, runner fails closed on mismatch |
| 5 | Lock namespace | keep, alias later | keep (agreed) |
| 6 | First slice | standalone echo firmware (agreed) | same |
| 7 | Second project | throwaway echo firmware first | same |
| 8 | Supersede `rtt-full-move.md` | yes | yes |

## Where things are

- tinyusb branch `migrate-target-debug-skills` (from `agent-refactor`): four
  skill removals; `tools/rtt.py` and `test/hil/test/test_hil_rtt.py` (harness
  contract tests) remain.
- agentrc main 95100de: `rtt`, `etm-trace`, `target-debug`, `esp-target-debug`
  with scripts and tests; `hil` skill still in this repo at
  `.claude/skills/hil/SKILL.md`.
- Codex plans (lane `plan-rtt`, 2026-09-16): rtt full move
  `codex-plan-rtt-20260916-010629-544548`, farm design
  `codex-plan-rtt-20260916-012035-655724`.
