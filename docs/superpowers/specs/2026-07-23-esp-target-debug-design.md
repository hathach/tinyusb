# esp-target-debug Skill Design

Backend skill for debugging TinyUSB firmware on Espressif targets (rig:
`espressif_p4_function_ev`, `espressif_s3_devkitm`) via the chips' **built-in
USB-Serial-JTAG**, with external JTAG documented as a TODO until the rig has
an adapter. Companion to `target-debug`, which keeps the architecture-neutral
methodology (intrusiveness ladder, board locks, dual-side capture, diagnosis
standards) — this skill is the Espressif toolchain/probe backend, the same
boundary that makes `usb-kernel-debug` its own skill.

## Goals

- An agent can attach, halt, backtrace, set breakpoints/watchpoints, list
  FreeRTOS threads, and capture logs on the rig's P4 **while TinyUSB device
  traffic is live** — every recipe hardware-verified before landing unmarked
  (the `target-debug` ethos).
- The S3's USB-SJ/OTG PHY conflict is mapped precisely, not hand-waved:
  verified working via `board_test` (TinyUSB off — PHY free), verified failure
  mode with a USB device example, external-JTAG escape hatch documented as
  TODO.

## Non-goals (deferred)

- External JTAG bring-up (no adapter on the rig) — TODO section with S3 JTAG
  pin notes (GPIO39-42) and openocd-esp32 adapter support pointers.
- Xtensa/S3 full parity under live USB traffic (needs external JTAG).
- ETM-class instruction trace; SystemView tooling beyond an apptrace spike.

## Architecture

New skill `.claude/skills/esp-target-debug/SKILL.md`; two integration edits:

- `target-debug` gains a 2-line pointer under the probe-mapping bullets:
  Espressif boards use a different toolchain, probe model, and trace story —
  read `esp-target-debug`.
- `target-debugger` agent table gains an `esp-target-debug` row (name-only,
  aligned columns, per the established conventions).

Skill content (order):

1. **Role + defer line** — methodology lives in `target-debug`; this file is
   the Espressif backend. Built-in USB-SJ now; external JTAG TODO.
2. **PHY-conflict map** —
   - S3: USB-SJ and OTG share one PHY (GPIO19/20). TinyUSB claiming the PHY
     drops JTAG-over-USB mid-session: JTAG works for non-USB examples
     (`board_test`), dies for USB device examples (verified boundary, exact
     symptom recorded). External JTAG = the future escape hatch (TODO).
   - P4: OTG-HS has a dedicated HS PHY; USB-SJ is separate — JTAG and the
     TinyUSB DUT port coexist (verified). USB-SJ doubles as a live log
     console during device traffic — the TU_LOG-equivalent channel.
3. **Toolchain & attach** — `. $HOME/code/esp-idf/export.sh` provides
   `openocd-esp32` + `riscv32-esp-elf-gdb` (P4) / `xtensa-esp32s3-elf-gdb`
   (S3). Rig path is raw openocd (HIL firmware isn't an idf project on disk):
   `openocd -f board/esp32p4-builtin.cfg` with `adapter serial <uid>` (USB-SJ
   is VID 303A:1001; uid = the `flasher.uid` already in `tinyusb.json`), gdb
   on :3333. `idf.py openocd` / `idf.py gdb` noted for idf-project work.
4. **Technique mapping table** (aligned) — ARM technique → Espressif
   equivalent:

   | target-debug technique | Espressif backend |
   |---|---|
   | GDB autopsy, bp/wp    | same flow; RISC-V trigger module (P4) / Xtensa 2 bp + 2 wp (S3); budget read verified on P4 |
   | Vector catch          | none — breakpoint the panic handler; decode `mcause`/`mepc`/`mtval` (P4) |
   | SWO / DWT data trace  | none — apptrace over JTAG is the analog (gated spike; lands `(untested)` if it fails) |
   | RTT / TU_LOG          | USB-SJ console — on P4 it coexists with DUT traffic |
   | FreeRTOS threads      | native in openocd-esp32 — `info threads` out of the box |
   | verifybin             | `esptool.py verify_flash` |

5. **Rig discipline deltas** — same `board_lock.py` protocol; flasher is
   esptool (serial-port-by-uid); reflash pristine before release; one client
   per USB-SJ device.
6. **External JTAG — TODO** — S3 JTAG pins, adapter classes openocd-esp32
   supports, and the efuse caveat (JTAG pin selection), unverified.

## Verification gates (execution order)

All under board locks, serial, evidence in commit messages:

1. **P4 coexistence (headline)**: flash a device example, confirm enumeration
   + traffic on the DUT port, then attach openocd+gdb over USB-SJ →
   halt, `bt`, resume — device stays functional after resume.
2. **P4 budget**: read trigger/watchpoint counts via openocd/gdb; set a
   hardware watchpoint on a TinyUSB variable, confirm hit.
3. **P4 threads**: `info threads` lists ESP-IDF tasks (usbd task visible).
4. **P4 console**: capture USB-SJ console log output during device traffic.
5. **P4 apptrace spike (gated)**: bounded attempt; verified recipe or
   `(untested)` tag.
6. **S3 boundary**: `board_test` flashed → attach works (halt+bt); then a USB
   device example → record the exact JTAG failure symptom when the PHY
   switches. No further S3 work (external JTAG TODO).

## Constraints

- Worktree `claude/improve-debug-skill-agent`; commit per gate; pre-commit
  before each; no Co-Authored-By trailers.
- Formatting conventions already established: aligned table columns,
  skill-name-only cross references, bullets over run-on paragraphs.
- Espressif builds need `export.sh` first (CLAUDE.md); P4/S3 examples build
  via idf.py — reuse existing HIL-built firmware where possible instead of
  rebuilding.
- Hardware-verify-before-landing: unverified content ships tagged
  `(untested)` or not at all.
