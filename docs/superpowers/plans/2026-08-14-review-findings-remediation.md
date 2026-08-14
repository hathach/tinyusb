# Review-Findings Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve all 16 findings from the 2026-08-14 maximum-effort review of `claude/wch-ch569-ch32h417-usb3`, fixing the test instrumentation first so the remaining fixes can be judged honestly.

**Architecture:** Point fixes throughout, except the CH569 USB3 fallback ladder, whose four findings share one root cause and are instead made unreachable by a `fallback_enter()` helper that owns the fallback state, the training timer and the timer's interrupt mask together. Phases A–C land on the existing branch for PR #3779; phase D is two separate branches off `master`.

**Tech Stack:** C99 (TinyUSB, 2-space indent, `TU_` macros), Python 3 (HIL harness), CMake/Ninja, Ceedling+Unity+CMock, WCH RISC-V (`riscv-none-elf-gcc`).

## Global Constraints

- Design source of truth: `docs/superpowers/specs/2026-08-14-review-findings-remediation-design.md`. Finding text: `review_findings_max_2026-08-14.json`.
- Never add `Co-Authored-By:` or `Claude-Session:` trailers to commits — hathach is the sole author.
- Never stage `.idea/`. Always scope `git add` to explicit paths, never `-A` from the repo root.
- Never modify anything under `hw/mcu/` (vendor SDK submodules).
- C style: C99, 2-space indent, no tabs, `snake_case` helpers, `UPPER_CASE` macros, no dynamic allocation.
- `pre-commit run --all-files` must pass before every push (`clang-format`, `codespell`, unit tests).
- Phases A, B, C commit to the current branch `claude/wch-ch569-ch32h417-usb3` in worktree `/home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3`. Phase D uses new worktrees off `master`.
- F4 (Task 9) and F16 (Task 5) cannot be measured without the USB2 sniffer tap. Their commit messages must say built-and-reasoned, never "verified".
- HIL runs need a board lock: `python3 test/hil/hil_lock.py acquire <board>` / `release <board>`. WCH boards: `hydrausb3_v1` (probe index 0), `nanoch32h417` (probe index 1). `wlink` has no default probe — always pass `-d <index>`.

---

## File Structure

| File | Responsibility | Tasks |
| ---- | -------------- | ----- |
| `test/hil/hil_test.py` | Renders the usbtest report cell; must account for skipped cases | 1 |
| `test/hil/hs_drop_rate.py` | Pooled SETUP drop-rate measurement; ACK attribution | 2 |
| `test/hil/test_hs_drop_rate.py` | Unit tests for the parser | 2 |
| `test/hil/test_hil_usbtest_cell.py` | **New.** Unit tests for the usbtest cell rendering | 1 |
| `test/hil/tinyusb-sudoer` | Argument-restricted sudo grants for the rig | 3 |
| `hw/bsp/ch32h417/family.c` | UART flash loader; needs an inactivity escape | 4 |
| `test/hil/hil_flash.py` | Rig reset path; must not equate write success with reset | 4 |
| `examples/device/usbtest/src/usb_descriptors.c` | Runtime quirk nibble in `bcdDevice` | 5 |
| `src/portable/wch/dcd_ch56x_usb30.c` | CH569 USB3 driver + fallback ladder | 6, 7 |
| `src/portable/wch/dcd_ch32h417_usb30.c` | CH32H417 USB3 driver (clear-stall twin) | 7 |
| `src/portable/wch/dcd_ch32h417_usbhs.c` | CH32H417 USBHS driver; OUT completion ordering | 9 |
| `src/class/mtp/mtp_device.c` | MTP ZLP threshold | 8 |
| `examples/device/midi2_device/src/usb_descriptors.c` | Missing HS descriptor callbacks | 10 |
| `src/device/usbd.c` | Per-function remote wake state + SS interface GET_STATUS | 11 |
| `src/class/msc/msc_device.c` | Defer-result propagation (phase D) | 12 |
| `hw/bsp/family_support.cmake` | Board-library warning flag (phase D) | 13 |

---

# Phase A — Make the instruments honest

## Task 1: usbtest skipped cases must not render as a full pass (F2)

`test/hil/usbtest.py` already emits a `skipped` count. `hil_test.py` never reads it, so a CH32H417 high-speed run with five quirked cases renders `✅ 25/25` — indistinguishable from a genuine 30/30. Separately its failing-case list sweeps in the SKIP entries, so one real failure reports six.

**Files:**
- Modify: `test/hil/hil_test.py:1336-1349`
- Test: `test/hil/test_hil_usbtest_cell.py` (create)

**Interfaces:**
- Consumes: the JSON contract from `usbtest.py:571-574` — keys `passed`, `failed`, `skipped`, `cases` (each case a dict with `num` and `status`).
- Produces: `usbtest_cell(data) -> str` and the `TestFail` it raises; Task 14 relies on the cell text.

- [ ] **Step 1: Write the failing test**

Create `test/hil/test_hil_usbtest_cell.py`:

```python
"""The usbtest report cell must never render a quirk-skipped run as a full pass."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hil_test


def test_full_pass_renders_as_pass():
    data = {'passed': 30, 'failed': 0, 'skipped': 0, 'cases': []}
    assert hil_test.usbtest_cell(data) == '✅ 30/30'


def test_skipped_cases_are_counted_in_the_denominator():
    # 25 ran and passed, 5 were quirk-skipped: the cell must not read 25/25
    data = {'passed': 25, 'failed': 0, 'skipped': 5,
            'cases': [{'num': n, 'status': 'SKIP'} for n in (9, 10, 13, 14, 21)]}
    cell = hil_test.usbtest_cell(data)
    assert '25/30' in cell
    assert '5 skipped' in cell
    assert '25/25' not in cell


def test_skipped_cases_are_not_reported_as_failures():
    data = {'passed': 24, 'failed': 1, 'skipped': 5,
            'cases': [{'num': n, 'status': 'SKIP'} for n in (9, 10, 13, 14, 21)]
                     + [{'num': 7, 'status': 'FAIL'}]}
    try:
        hil_test.usbtest_cell(data)
    except hil_test.TestFail as e:
        assert 'cases failed: [7]' in str(e)
    else:
        raise AssertionError('expected TestFail')


def test_missing_skipped_key_defaults_to_zero():
    # older usbtest.py output without the key must still render
    assert hil_test.usbtest_cell({'passed': 30, 'failed': 0, 'cases': []}) == '✅ 30/30'
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
python3 -m pytest test/hil/test_hil_usbtest_cell.py -v
```

Expected: FAIL — `AttributeError: module 'hil_test' has no attribute 'usbtest_cell'`.

- [ ] **Step 3: Extract and fix the cell rendering**

In `test/hil/hil_test.py`, add this function immediately above the function that currently contains lines 1336-1349 (the one that runs `usbtest.py`):

```python
def usbtest_cell(data):
    """Render the usbtest report cell. Skipped cases stay in the denominator: a quirked run
    reports 25/30, never 25/25, so it can never be read as a full pass. The '✅' prefix is
    retained (the battery did pass) rather than a new icon, because render/tally classify
    cells by that prefix."""
    passed, failed = int(data['passed']), int(data['failed'])
    skipped = int(data.get('skipped', 0))
    total = passed + failed + skipped
    if failed == 0 and total > 0:
        if skipped:
            return f'{REPORT_CELL["pass"]} {passed}/{total} ({skipped} skipped)'
        return f'{REPORT_CELL["pass"]} {passed}/{total}'
    bad = [c.get('num') for c in data.get('cases', []) if c.get('status') not in ('PASS', 'SKIP')]
    raise TestFail(f'usbtest {passed}/{total} (cases failed: {bad})',
                   metric=f'{REPORT_CELL["fail"]} {passed}/{total}')
```

Then replace lines 1336-1349 so the caller delegates. The `try` block keeps only the parse:

```python
    try:
        data = json.loads(out[brace:])
    except (ValueError, json.JSONDecodeError):
        raise TestFail(f'usbtest did not run: {compact_output(out) or hil_flash.cmd_stdout_text(r.stderr)}',
                       metric=f'{REPORT_CELL["fail"]} 0/30')
    try:
        return usbtest_cell(data)
    except KeyError:
        raise TestFail(f'usbtest did not run: {compact_output(out) or hil_flash.cmd_stdout_text(r.stderr)}',
                       metric=f'{REPORT_CELL["fail"]} 0/30')
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
python3 -m pytest test/hil/test_hil_usbtest_cell.py -v
python3 -m pytest test/hil/ -v
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add test/hil/hil_test.py test/hil/test_hil_usbtest_cell.py
git commit -m "hil: keep quirk-skipped usbtest cases in the reported total

hil_test.py read only passed/failed, so a run with five quirk-skipped cases
rendered 25/25 - indistinguishable from a genuine 30/30. Skipped cases now stay
in the denominator (25/30, 5 skipped), and the failing-case list no longer
sweeps in the SKIP entries, which previously turned one real failure into six."
```

---

## Task 2: drop-rate parser must not assume wire address 1 (F5)

`hs_drop_rate.py:37` accepts a device ACK only when `usbll.src` starts with `'1.'`. The capture deliberately spans enumeration, where the device answers at address 0, and xHCI hosts routinely assign an address other than 1 — so correctly-ACKed SETUPs are scored as drops.

**Files:**
- Modify: `test/hil/hs_drop_rate.py:24-41`
- Test: `test/hil/test_hs_drop_rate.py`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `parse_setup_outcomes(rows) -> (acked, dropped)`, unchanged signature; rows are `(time, pid, src, dst)` tuples as emitted by `tshark -T fields`.

- [ ] **Step 1: Write the failing tests**

Append to `test/hil/test_hs_drop_rate.py`:

```python
def test_ack_counted_at_enumeration_address_zero():
    # before SET_ADDRESS the device answers at address 0; that ACK is still an ACK
    rows = [('0.000000', '0x2d', 'host', '0.0'),
            ('0.000001', '0xc3', 'host', '0.0'),
            ('0.000002', '0xd2', '0.0', 'host')]
    assert parse_setup_outcomes(rows) == (1, 0)


def test_ack_counted_at_any_assigned_address():
    # xHCI hosts routinely assign an address other than 1
    for addr in ('5', '12'):
        rows = [('0.000000', '0x2d', 'host', f'{addr}.0'),
                ('0.000001', '0xc3', 'host', f'{addr}.0'),
                ('0.000002', '0xd2', f'{addr}.0', 'host')]
        assert parse_setup_outcomes(rows) == (1, 0), f'address {addr}'


def test_host_sourced_ack_is_not_a_device_ack():
    # a host-sourced ACK (the handshake for an OUT data packet) must not count
    rows = [('0.000000', '0x2d', 'host', '1.0'),
            ('0.000001', '0xc3', 'host', '1.0'),
            ('0.000002', '0xd2', 'host', '1.0')]
    assert parse_setup_outcomes(rows) == (0, 1)
```

- [ ] **Step 2: Run them to verify they fail**

```bash
python3 -m pytest test/hil/test_hs_drop_rate.py -v
```

Expected: `test_ack_counted_at_enumeration_address_zero` and `test_ack_counted_at_any_assigned_address` FAIL with `assert (0, 1) == (1, 0)`.

- [ ] **Step 3: Fix the attribution**

In `test/hil/hs_drop_rate.py`, change the docstring and the test in `parse_setup_outcomes`:

```python
def parse_setup_outcomes(rows):
    """Count (acked, dropped) SETUP transactions.

    rows: iterable of (time, pid, src, dst) as emitted by tshark -T fields.
    A SETUP is acked when an ACK sourced BY THE DEVICE appears within the next two frames --
    the DATA0 payload sits between the token and the handshake. Device-sourced means simply
    "not the host": the device answers at address 0 until SET_ADDRESS, and an xHCI host may
    assign any address afterwards, so matching a literal address scores correct ACKs as drops.
    """
    rows = list(rows)
    acked = dropped = 0
    for i, row in enumerate(rows):
        if row[1] != SETUP_PID:
            continue
        window = rows[i + 1:i + 3]
        if any(r[1] == ACK_PID and r[2] != 'host' for r in window):
            acked += 1
        else:
            dropped += 1
    return acked, dropped
```

- [ ] **Step 4: Add the invalidation note to the module docstring**

In the module docstring of `test/hil/hs_drop_rate.py`, append this paragraph:

```
Any drop rate recorded before 2026-08-14 is invalid: the parser matched a literal wire
address 1, so enumeration-phase SETUPs (address 0) and any device on another address were
all scored as drops. Figures in docs/superpowers/notes/h417-ep0-diff.md predate the fix.
```

- [ ] **Step 5: Run the tests to verify they pass**

```bash
python3 -m pytest test/hil/test_hs_drop_rate.py -v
```

Expected: all 8 PASS.

- [ ] **Step 6: Commit**

```bash
git add test/hil/hs_drop_rate.py test/hil/test_hs_drop_rate.py
git commit -m "hil: attribute SETUP ACKs by source, not by a hardcoded address

The parser accepted a device ACK only from wire address 1, while the capture
deliberately spans enumeration, where the device answers at address 0, and an
xHCI host may assign any address afterwards. Correctly-ACKed SETUPs were
therefore scored as drops. Tests now cover addresses 0, 5 and 12, and the
module records that figures taken before this fix are invalid."
```

---

## Task 3: grant the sudo rights the harness actually needs (F9)

`usbtest.py:228` runs `setpci` to read the Renesas uPD720201/2 firmware version and hard-exits at `:235` if it is not permitted. The new sudoers drop-in grants eight binaries, none of them `setpci`.

**Files:**
- Modify: `test/hil/tinyusb-sudoer:66-73`, `test/hil/hs_drop_rate.py:99`

- [ ] **Step 1: Add the grants**

Append after the existing `modprobe` line in `test/hil/tinyusb-sudoer`:

```
# setpci: usbtest.py reads the Renesas uPD720201/2 xHCI firmware version (0x6c.l) and REFUSES
# to run without it; lsusb -v: hs_drop_rate.py provokes control traffic on the tapped device.
#1000 ALL=(root) NOPASSWD: /usr/bin/setpci -s * 0x6c.l
#1000 ALL=(root) NOPASSWD: /usr/bin/lsusb -v -s *
```

- [ ] **Step 2: Make the one un-flagged sudo consistent**

In `test/hil/hs_drop_rate.py`, line 99, add `-n` so a missing grant fails instead of prompting:

```python
        subprocess.run(['sudo', '-n', 'lsusb', '-v', '-s', dev],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
```

- [ ] **Step 3: Verify the file still parses as sudoers**

```bash
sudo visudo -c -f test/hil/tinyusb-sudoer
```

Expected: `test/hil/tinyusb-sudoer: parsed OK`.

- [ ] **Step 4: Verify the grants match the real invocations**

```bash
grep -n "setpci" test/hil/usbtest.py test/hil/tinyusb-sudoer
grep -n "sudo" test/hil/hs_drop_rate.py
```

Expected: `usbtest.py` calls `['setpci', '-s', pci.name, '0x6c.l']`, matching the granted pattern; `hs_drop_rate.py`'s only `sudo` now carries `-n`.

- [ ] **Step 5: Commit**

```bash
git add test/hil/tinyusb-sudoer test/hil/hs_drop_rate.py
git commit -m "hil: grant setpci and lsusb in the sudoers drop-in

usbtest.py reads the Renesas uPD720201/2 firmware version via setpci and
refuses to run without it, so a rig provisioned from this file could not run
usbtest on the controllers it actually uses. hs_drop_rate.py's lone sudo gains
-n to match every other call in test/hil."
```

---

## Task 4: the UART loader must never strand the board (F13)

`uart_flash_loader()` is `noreturn` and its first act disables the RXNE interrupt and `USART1_IRQn` — the only other way in or out. An interrupted flash leaves the board in the banner loop, off the USB bus, while `reset_wch_uart_loader()` reports success because the park *write* succeeded.

**Files:**
- Modify: `hw/bsp/ch32h417/family.c:166-175`, `test/hil/hil_flash.py:236-246`

- [ ] **Step 1: Give the banner loop an inactivity escape**

In `hw/bsp/ch32h417/family.c`, replace the banner `do { ... } while (c != 'H');` block:

```c
    // Banner until the host opens a session. Bounded: an interrupted flash (Ctrl-C, a harness
    // timeout, a VCP hiccup) must not strand the board here with USB down and the RXNE vector
    // disabled - after ~30 s with no session, reset back into the application.
    int c;
    uint32_t blink = 0;
    uint32_t idle = 0;
    do {
      loader_putc('L');
      board_led_write((++blink) & 1);
      c = loader_getc(5000000u); // ~0.5 s
      if (c == -1 && ++idle >= 60u) {
        NVIC_SystemReset(); // ~30 s idle: back to the application
      }
    } while (c != 'H');
```

- [ ] **Step 2: Reset the idle counter once a session starts**

No extra code is needed — `idle` is declared inside the `for (;;)` body, so every new session attempt starts from zero.

Verify by reading the loop: `idle` must be declared **after** `for (;;) {` and before the `do`.

- [ ] **Step 3: Stop the rig equating a write with a reset**

In `test/hil/hil_flash.py`, replace the comment and return of `reset_wch_uart_loader()`:

```python
def reset_wch_uart_loader(board):
    # A park request reboots the firmware (~10 s park window, then a normal boot). The WCH-LinkE
    # VCP is always present (it is the probe, not the board), so a failure here means the probe is
    # gone/busy. Note the park bytes only take effect in the application's boot window: a board
    # already sitting in the UART loader consumes them as noise, and recovers instead via the
    # loader's own ~30 s inactivity reset (hw/bsp/ch32h417/family.c). Sit out the park window
    # before returning: callers start waiting for the board to re-enumerate the moment reset_*
    # returns, on a budget shorter than the window (hil_pool_check.ENUM_WAIT_RETRY = 8 s).
    ok = _wch_uart_park(board['flasher'])
    if ok:
        time.sleep(WCH_PARK_WINDOW)
    return subprocess.CompletedProcess(args='park', returncode=0 if ok else 1)
```

- [ ] **Step 4: Build the board to verify the change compiles**

```bash
cd examples
cmake -B cmake-build-nanoch32h417 -DBOARD=nanoch32h417 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-nanoch32h417 --target board_test
```

Expected: build succeeds, no warnings about `idle` or `NVIC_SystemReset`.

- [ ] **Step 5: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
git add hw/bsp/ch32h417/family.c test/hil/hil_flash.py
git commit -m "bsp: bound the CH32H417 UART loader with an inactivity reset

The loader is noreturn and disables the RXNE interrupt it was entered through,
so an interrupted flash left the board in the banner loop with USB down and no
way back. It now resets into the application after ~30 s with no session, which
makes stranding structurally impossible rather than dependent on the rig
rescuing it. The rig's reset path documents that park bytes cannot reach a
board already sitting in the loader."
```

---

## Task 5: quirk advertisement must follow the live link, not the compiled dcd (F16)

`USBTEST_QUIRKS` is chosen by which dcd was compiled in. An SS build has `CFG_TUD_WCH_USBIP_USBHS == 0` and so advertises `0x20`, even though that same image drives the USBHS block at 480 Mbps when `CFG_TUD_WCH_USB30_FALLBACK=1`.

**Files:**
- Modify: `examples/device/usbtest/src/usb_descriptors.c`

**Interfaces:**
- Consumes: `USBTEST_QUIRKS` / `USBTEST_TIER` from `usb_descriptors.h`, `tud_speed_get()`.
- Produces: no new external symbol; `tud_descriptor_device_cb()` keeps its signature.

- [ ] **Step 1: Add the runtime quirk helper**

In `examples/device/usbtest/src/usb_descriptors.c`, add above `tud_descriptor_device_cb`:

```c
// The compile-time USBTEST_QUIRKS is picked by which dcd was built in, which cannot see a
// runtime USB2 fallback: a CH32H417 SuperSpeed image drives the USBHS block at 480 Mbps when
// CFG_TUD_WCH_USB30_FALLBACK=1, and must then advertise the USBHS erratum, not the USB3 one.
// Quirk bits are bcdDevice 4-7; the tier (bits 0-3) is speed-independent.
static uint16_t usbtest_quirks_for_link(void) {
#if TU_CHECK_MCU(OPT_MCU_CH32H417)
  return (tud_speed_get() == TUSB_SPEED_SUPER) ? 0x20 : 0x40;
#elif TU_CHECK_MCU(OPT_MCU_CH569)
  return (tud_speed_get() == TUSB_SPEED_SUPER) ? 0x30 : 0x00; // CH569 USBHS passes 30/30
#else
  return USBTEST_QUIRKS;
#endif
}
```

- [ ] **Step 2: Patch bcdDevice in the device-descriptor callback**

Replace the body of `tud_descriptor_device_cb()` so it returns a patched RAM copy. Keep whichever descriptor the existing body selects; only the return is wrapped:

```c
uint8_t const *tud_descriptor_device_cb(void) {
  // Copy so the quirk nibble can follow the live link speed; the source descriptors are const.
  static tusb_desc_device_t desc;
#if TUD_OPT_SUPER_SPEED
  desc = (tud_speed_get() == TUSB_SPEED_SUPER) ? desc_device_ss : desc_device;
#else
  desc = desc_device;
#endif
  desc.bcdDevice = (uint16_t) (0x0100 | usbtest_quirks_for_link() | USBTEST_TIER);
  return (uint8_t const *) &desc;
}
```

If the existing body names the SuperSpeed descriptor something other than `desc_device_ss`, use that name — check with `grep -n "desc_device" examples/device/usbtest/src/usb_descriptors.c` before editing.

- [ ] **Step 3: Build both speeds for both WCH boards**

```bash
cd examples
cmake -B cmake-build-nanoch32h417 -DBOARD=nanoch32h417 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-nanoch32h417 --target usbtest
cmake -B cmake-build-hydrausb3_v1 -DBOARD=hydrausb3_v1 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-hydrausb3_v1 --target usbtest
```

Expected: both build clean.

- [ ] **Step 4: Verify the descriptor is still 18 bytes and well-formed**

```bash
riscv-none-elf-size examples/cmake-build-hydrausb3_v1/device/usbtest/usbtest.elf
```

Expected: builds and links; `.data` grows by 18 bytes (the RAM copy).

- [ ] **Step 5: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
git add examples/device/usbtest/src/usb_descriptors.c
git commit -m "usbtest: advertise the quirk for the live link, not the compiled dcd

USBTEST_QUIRKS is selected by which dcd was built in, so a CH32H417 SuperSpeed
image advertised the USB3 erratum even while driving the USBHS block at 480
Mbps under CFG_TUD_WCH_USB30_FALLBACK. bcdDevice's quirk nibble is now patched
from tud_speed_get() at descriptor time.

Built and reasoned only: the fallback link needs the USB2 tap inline to run, so
this is not measured on hardware."
```

---

# Phase B — The CH569 fallback ladder

## Task 6: one owner for fallback state, timer and interrupt mask (F1, F7, F10, F14)

`_fb_state`, the TMR0 interrupt mask and the timer registers are three pieces of one logical state, mutated independently at six sites. That produces four findings: a terminal `FB_USB3_UP` after disconnect (F1), a re-init task that checks state before a 30 ms wait and never re-checks (F7), a timer restarted with its vector still masked (F10), and a warm-reset path that blocks the ISR for tens of milliseconds while omitting the bus-reset event (F14).

**Files:**
- Modify: `src/portable/wch/dcd_ch56x_usb30.c` — lines 116-135 (timer helpers), 198-210 (`usb30_hw_reinit_task`), 553-554, 574-586, 596-616 (link IRQ), 628-636 (warm reset), 736-750 (ladder), 796-800 (`dcd_init`), 917-920 (`dcd_connect`)

**Interfaces:**
- Produces: `static void fallback_enter(uint8_t next)` — the only writer of `_fb_state`. Accepts `FB_USB3_TRAINING`, `FB_USB3_OFF`, `FB_USB3_UP`, `FB_USB2_ACTIVE`.

- [ ] **Step 1: Re-enable the timer vector inside the start helper**

In `fallback_timer_start()`, add the enable as the last statement:

```c
static void fallback_timer_start(void) {
  _fb_train_ticks = 0;
  _fb_saw_terms = false;
  R8_TMR0_CTRL_MOD = RB_TMR_ALL_CLEAR;
  // TMR0 counts 26 bits: CNT_END must stay below 2^26 (67108864). 0.55 s per expiry at
  // 120 MHz; training gets several expiries (with re-attempts) before USB2 comes up
  R32_TMR0_CNT_END = 66000000;
  R8_TMR0_INT_FLAG = RB_TMR_IF_CYC_END;
  R8_TMR0_INTER_EN = RB_TMR_IE_CYC_END;
  R8_TMR0_CTRL_MOD = RB_TMR_COUNT_EN;
  PFIC_EnableIRQ(TMR0_IRQn); // fallback_timer_stop() masked it; a timer with a masked vector
                             // never advances the ladder and latches a flag that the shared
                             // ISR then consumes in place of the next USBSS/LINK interrupt
}
```

- [ ] **Step 2: Add the state-transition helper**

Immediately after `fallback_timer_start()`:

```c
// Single owner of the fallback state, the training timer and the timer's interrupt mask: the
// three are one logical state, and mutating them independently is what allowed a terminal
// FB_USB3_UP and a timer armed behind a masked vector.
static void fallback_enter(uint8_t next) {
  _fb_state = next;
  if (next == FB_USB3_TRAINING) {
    fallback_timer_start();
  } else if (next != FB_USB3_OFF) {
    fallback_timer_stop(); // UP and USB2_ACTIVE: the ladder is finished
  }
  // FB_USB3_OFF deliberately leaves the timer running: its second expiry brings USB2 up
}
```

- [ ] **Step 3: Route all six existing sites through the helper**

Apply each of these edits:

| Site | Before | After |
| ---- | ------ | ----- |
| `:553-554` | `_fb_state = FB_USB3_UP;` + `fallback_timer_stop();` | `fallback_enter(FB_USB3_UP);` |
| `:579-580` | `fallback_timer_stop();` + `_fb_state = FB_USB2_ACTIVE;` | `fallback_enter(FB_USB2_ACTIVE);` |
| `:742` | `_fb_state = FB_USB3_OFF;` | `fallback_enter(FB_USB3_OFF);` |
| `:745-746` | `_fb_state = FB_USB2_ACTIVE;` + `fallback_timer_stop();` | `fallback_enter(FB_USB2_ACTIVE);` |
| `:797-799` | `_fb_state = FB_USB3_TRAINING;` … `fallback_timer_start();` | keep `ch56x_usb2_deinit();` between them, then `fallback_enter(FB_USB3_TRAINING);` replacing both |
| `:918-919` | `_fb_state = FB_USB3_TRAINING;` + `fallback_timer_start();` | `fallback_enter(FB_USB3_TRAINING);` |

For `dcd_init` the resulting block is:

```c
#if CFG_TUD_WCH_USB30_FALLBACK
  ch56x_usb2_deinit(); // keep the USB2 controller quiescent while SS trains
  fallback_enter(FB_USB3_TRAINING);
#endif
```

- [ ] **Step 4: Re-arm the ladder when the partner disappears (F1)**

In `handle_link_irq()`, the `USBSS_LINK_IF_TERM_PRESENT` branch's `else` (partner gone), add the transition before the early return:

```c
    } else {
      // partner disappeared: teardown now, settle + re-init deferred to task context
      USBSS->LINK_INT_CTRL = 0;
#if CFG_TUD_WCH_USB30_FALLBACK
      // Re-arm the ladder: FB_USB3_UP is otherwise terminal, and a replug into a USB2-only
      // host would find no ladder running and no USB2 controller (dcd_init deinit'd it)
      fallback_enter(FB_USB3_TRAINING);
#endif
      dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_UNPLUGGED};
      dcd_event_handler(&event, true);
      usb30_bus_reset_from_isr();
      return; // the link is torn down: the remaining flags are stale
    }
```

- [ ] **Step 5: Re-check the state after the settle (F7)**

In `usb30_hw_reinit_task()`, add a second guard after the busy-wait:

```c
  link_delay_us(30000);
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB3_OFF || _fb_state == FB_USB2_ACTIVE) {
    _hw_reinit_deferred = false;
    return; // the ladder claimed the port during the settle: USB3 must stay down
  }
#endif
  USBSS->LINK_INT_FLAG = 0xFFFFFFFFu;
  usb30_hw_init();
```

- [ ] **Step 6: Defer the warm-reset settle and emit the bus reset (F14)**

Replace the `usb30_bus_reset();` call in the `LINK_IF_WARM_RESET` branch:

```c
    // A warm reset has a spec'd ~100 ms window, which comfortably covers one task-loop turn,
    // so the 30 ms settle and re-init are deferred exactly as the TERM_PRESENT teardown does
    // rather than blocking every other vector (SysTick included) inside the ISR.
    ep_state_reset();
    dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
    usb30_bus_reset_from_isr();
```

- [ ] **Step 7: Verify no direct state writes remain**

```bash
grep -n "_fb_state = " src/portable/wch/dcd_ch56x_usb30.c
```

Expected: exactly one hit, inside `fallback_enter()`.

- [ ] **Step 8: Build both speeds**

```bash
cd examples
cmake -B cmake-build-hydrausb3_v1 -DBOARD=hydrausb3_v1 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-hydrausb3_v1
```

Expected: all examples build; no unused-function warning for `fallback_timer_start`/`fallback_timer_stop`.

- [ ] **Step 9: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
git add src/portable/wch/dcd_ch56x_usb30.c
git commit -m "wch: give the CH569 fallback ladder a single state owner

_fb_state, the TMR0 interrupt mask and the timer registers are one logical
state that was mutated independently at six sites, which produced four
distinct defects. fallback_enter() now owns all three.

That makes two of them unreachable: fallback_timer_start() re-enables the
vector its stop() masked, so a tud_disconnect()/tud_connect() cycle can no
longer leave the ladder counting behind a masked interrupt; and the
partner-disappeared branch re-arms training, so a board that trained
SuperSpeed once is no longer dead after a replug into a USB2-only host.

Two need their own fix: the deferred re-init re-checks the state after its
30 ms settle instead of trusting a check made before it, and the warm-reset
branch defers the settle the way the teardown path already does while emitting
the ep_state_reset()/dcd_event_bus_reset() pair it previously omitted."
```

---

# Phase C — Correctness

## Task 7: clear-stall must not resurrect a retired transfer (F3)

`dcd_edpt_clear_stall()` re-arms on the assumption that the class driver still considers the transfer submitted. But `usbd.c:1846-1850` clears `BUSY` **and** `CLAIMED` on a was-stalled clear, and `vendord_abort_ep()` is `stall(); clear_stall();` used to abort on every `SET_INTERFACE`.

**Files:**
- Modify: `src/portable/wch/dcd_ch56x_usb30.c:1149-1160`, `src/portable/wch/dcd_ch32h417_usb30.c:1079-1100`

- [ ] **Step 1: Drop the re-arm in the CH569 driver**

```c
  // Do NOT re-arm here. usbd clears BUSY and CLAIMED on a was-stalled clear (usbd.c
  // clear_mask), so the transfer is retired and the class owns resubmission -- and
  // vendord_abort_ep() uses stall+clear_stall precisely to abort. Re-arming resurrected the
  // old chain, which then sent stale data and had its completion accounted against whatever
  // the class armed next.
  xfer_ctl_t *xfer = &xfer_status[ep_num][dir];
  xfer->stalled = false;
  xfer->active = false;
  if (dir == TUSB_DIR_IN) {
    USBSS_TX_CTRL(ep_num) = 0;
  } else {
    USBSS_RX_CTRL(ep_num) = 0;
  }
}
```

- [ ] **Step 2: Drop the re-arm in the CH32H417 twin**

In `dcd_ch32h417_usb30.c`, remove both `if (xfer->valid) { ... }` blocks and clear the flag instead:

```c
  if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);
    tx->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    tx->UEP_TX_CR = TX_CHAIN_MAX_PKTS;
    XFER_CTL_BASE(ep_num, TUSB_DIR_IN)->valid = false;
  } else {
    volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
    rx->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    rx->UEP_RX_CR = CFG_TUD_WCH_USB30_MAX_BURST;
    XFER_CTL_BASE(ep_num, TUSB_DIR_OUT)->valid = false;
  }
}
```

- [ ] **Step 3: Build both boards**

```bash
cd examples
cmake --build cmake-build-hydrausb3_v1
cmake --build cmake-build-nanoch32h417
```

Expected: clean.

- [ ] **Step 4: Note the hardware gate**

This is the one Class C change the hydra can adjudicate. Task 14 runs usbtest cases 13 and 29 — the cases the original re-arm was added for. **If either regresses, stop and report rather than reinstating the re-arm**, because the re-arm conflicts with usbd's documented ownership rules and the correct fix would then be in the class driver.

- [ ] **Step 5: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
git add src/portable/wch/dcd_ch56x_usb30.c src/portable/wch/dcd_ch32h417_usb30.c
git commit -m "wch: do not re-arm a retired transfer on clear-stall

Both USB3 drivers re-armed the in-flight transfer on clear-stall, assuming the
class driver still considered it submitted. usbd clears BUSY and CLAIMED on a
was-stalled clear, so it is retired and the class owns resubmission - and
vendord_abort_ep() is literally stall+clear_stall used to abort on every
SET_INTERFACE. The re-armed chain sent stale data and had its completion
accounted against the transfer the class armed next."
```

---

## Task 8: MTP's ZLP threshold must be the real max packet size (F6)

`ep_sz_fs` is written only at full speed, so the threshold falls back to a hardcoded 512 while a SuperSpeed bulk endpoint runs at 1024. A data phase ending in exactly 512 bytes then queues a ZLP where the response container belongs.

**Files:**
- Modify: `src/class/mtp/mtp_device.c:79, 293-295, 426`

- [ ] **Step 1: Widen and rename the field**

At `mtp_device.c:79`, replace `uint8_t ep_sz_fs;`:

```c
  uint16_t ep_sz_bulk; // bulk max packet size for the live link (64 FS / 512 HS / 1024 SS)
```

- [ ] **Step 2: Record it for every speed**

Replace the full-speed-only assignment near `:293`:

```c
  p_mtp->ep_sz_bulk = tu_edpt_packet_size(ep_desc_bulk);
```

- [ ] **Step 3: Use it as the threshold**

At `:426`:

```c
        threshold = p_mtp->ep_sz_bulk;
```

- [ ] **Step 4: Confirm no stale references remain**

```bash
grep -n "ep_sz_fs" src/class/mtp/mtp_device.c
```

Expected: no output.

- [ ] **Step 5: Build an MTP-capable board**

```bash
cd examples
cmake --build cmake-build-hydrausb3_v1 --target mtp
```

Expected: clean build.

- [ ] **Step 6: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
git add src/class/mtp/mtp_device.c
git commit -m "mtp: derive the ZLP threshold from the live bulk max packet size

ep_sz_fs was written only at full speed, so the threshold fell back to a
hardcoded 512 while a SuperSpeed bulk endpoint runs at 1024. A data phase
ending in exactly 512 bytes then queued a ZLP the host reads where the response
container belongs. The field is now uint16_t, set for every speed, and renamed
to match what it holds."
```

---

## Task 9: clear the OUT completion flag after re-arming (F4)

`dcd_ch32h417_usbhs.c:553` clears `RB_UEP_R_DONE` before `EP_RX_LEN` is sampled and before `queue_out_packet()` reprograms DMA, leaving the endpoint ACK-armed at the finished packet's address. The CH56x twin is immune by hardware (`RB_USB_INT_BUSY`); CH32H417 RM 25.2.1.1 lists no equivalent.

**Files:**
- Modify: `src/portable/wch/dcd_ch32h417_usbhs.c:552-556`

- [ ] **Step 1: Move the clear after `update_out()`**

```c
          // Clear DONE only after update_out() has sampled the length and re-armed DMA: it is
          // the sole interlock on this part (RM 25.2.1.1 lists no RB_USB_INT_BUSY equivalent),
          // and clearing it first leaves the endpoint ACK-armed at the finished packet's
          // address. Mirrors where dcd_ch56x_usbhs.c clears R8_USB_INT_FG.
          const uint8_t rx_ctrl = EP_RX_CTRL(ep_num);
          if ((ep_num == 0) || xfer_status[ep_num][TUSB_DIR_OUT].is_iso || (rx_ctrl & USBHS_UEP_R_TOG_MATCH)) {
            update_out(rhport, ep_num, EP_RX_LEN(ep_num));
          }
          EP_RX_CTRL(ep_num) = (uint8_t)(EP_RX_CTRL(ep_num) & ~USBHS_UEP_R_DONE);
```

Note the re-read of `EP_RX_CTRL(ep_num)` in the final line: `update_out()` re-arms the endpoint and rewrites `EP_RX_CTRL`, so clearing DONE from the stale `rx_ctrl` snapshot would undo that arm.

- [ ] **Step 2: Build both speeds for the H417**

```bash
cd examples
cmake -B cmake-build-nanoch32h417 -DBOARD=nanoch32h417 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DRHPORT_DEVICE_SPEED=OPT_MODE_HIGH_SPEED . && cmake --build cmake-build-nanoch32h417
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
git add src/portable/wch/dcd_ch32h417_usbhs.c
git commit -m "wch: clear the CH32H417 OUT completion flag after re-arming

RB_UEP_R_DONE was cleared before EP_RX_LEN was sampled and before
queue_out_packet() reprogrammed the DMA address, leaving the endpoint ACK-armed
at the finished packet's address. On the CH56x that window is covered by
RB_USB_INT_BUSY; RM 25.2.1.1 lists no equivalent for this part, so the flag is
the only interlock and must be cleared last.

Built and reasoned only: this path does not enumerate without the USB2 tap
inline, so it is not measured on hardware."
```

---

## Task 10: midi2_device must answer the descriptors its HS config implies (F8)

This branch gave `midi2_device` a high-speed configuration (master had none) without `tud_descriptor_device_qualifier_cb` or `tud_descriptor_other_speed_configuration_cb`, so `usbd.c` stalls EP0 on `GET_DESCRIPTOR(DEVICE_QUALIFIER)` — which USB 2.0 §9.6.2 forbids for a high-speed-capable device.

**Files:**
- Modify: `examples/device/midi2_device/src/usb_descriptors.c`

- [ ] **Step 1: Read the reference pattern**

```bash
sed -n '195,235p' examples/device/cdc_msc/src/usb_descriptors.c
```

Use that file's `desc_device_qualifier`, `tud_descriptor_device_qualifier_cb` and `tud_descriptor_other_speed_configuration_cb` as the template, including its `desc_other_speed_config` buffer and the `memcpy` + descriptor-type overwrite.

- [ ] **Step 2: Add the qualifier descriptor and both callbacks**

Inside the existing `#if TUD_OPT_HIGH_SPEED` region of `examples/device/midi2_device/src/usb_descriptors.c`, add:

```c
tusb_desc_device_qualifier_t const desc_device_qualifier = {
  .bLength            = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB             = USB_BCD,
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .bNumConfigurations = 0x01,
  .bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
uint8_t const *tud_descriptor_device_qualifier_cb(void) {
  return (uint8_t const *) &desc_device_qualifier;
}

// Invoked when received GET OTHER SPEED CONFIGURATION DESCRIPTOR request
uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index) {
  (void) index; // for multiple configurations

  // if link speed is high return fullspeed config, and vice versa
  // Note: the descriptor type is OTHER_SPEED_CONFIG instead of CONFIG
  memcpy(desc_other_speed_config,
         (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration : desc_hs_configuration,
         CONFIG_TOTAL_LEN);
  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;
  return desc_other_speed_config;
}
```

Add the backing buffer next to the other descriptor arrays in the same `#if` region:

```c
static uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];
```

If `CONFIG_TOTAL_LEN` is not the name used in this file, use whatever the file's `desc_fs_configuration` length macro is — check with `grep -n "TOTAL_LEN" examples/device/midi2_device/src/usb_descriptors.c`.

- [ ] **Step 3: Ensure `string.h` is included**

```bash
grep -n "include <string.h>\|include \"tusb.h\"" examples/device/midi2_device/src/usb_descriptors.c
```

If `string.h` is absent, add `#include <string.h>` next to the existing includes (`memcpy` needs it).

- [ ] **Step 4: Build for a high-speed board**

```bash
cd examples
cmake -B cmake-build-stm32f407disco -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-stm32f407disco --target midi2_device
```

Expected: clean build. (If `midi2_device` is skipped for this board, build it for `hydrausb3_v1` instead.)

- [ ] **Step 5: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
git add examples/device/midi2_device/src/usb_descriptors.c
git commit -m "examples/midi2: add the descriptor callbacks its HS config requires

This branch gave midi2_device a high-speed configuration without
tud_descriptor_device_qualifier_cb or
tud_descriptor_other_speed_configuration_cb, leaving the weak stubs returning
NULL - so usbd stalls EP0 on GET_DESCRIPTOR(DEVICE_QUALIFIER), which USB 2.0
9.6.2 forbids for a high-speed-capable device."
```

---

## Task 11: per-function remote wake must be readable and resettable (F12)

`func_wakeup_bm` is write-only: interface `GET_STATUS` still returns a hardcoded `0x0000`, so a wake-capable function reports itself non-capable (USB 3.2 §9.4.5). It also survives `SET_INTERFACE`, a same-value `SET_CONFIGURATION` and `SET_ADDRESS(0)`, all of which USB 3.2 Table 9-10 marks as resetting FUNCTION REMOTE WAKEUP.

**Files:**
- Modify: `src/device/usbd.c:983-991, 1278-1284, 1286-1289`
- Test: `test/unit-test/test/device/usbd/test_usbd.c`

**Interfaces:**
- Consumes: `_usbd_dev.func_wakeup_bm` written at `:1305/1307`.
- Produces: no new external symbol.

- [ ] **Step 1: Write the failing unit tests**

Append to `test/unit-test/test/device/usbd/test_usbd.c`:

```c
// USB 3.2 9.4.5 Figure 9-5: the first interface of a function reports D0 Function Remote Wake
// Capable / D1 Function Remote Wakeup, which is how the host discovers the capability (9.2.5.4).
void test_usbd_get_status_interface_reports_function_wakeup(void) {
  usbd_driver_open_expect();
  tusb_init(0, &dev_init);
  dcd_event_bus_reset(0, TUSB_SPEED_SUPER, false);
  tud_task();

  // enable function remote wake on interface 0 (wIndex high byte bit 1)
  tusb_control_request_t const set_feat = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_STANDARD,
                          .direction = TUSB_DIR_OUT},
    .bRequest = TUSB_REQ_SET_FEATURE, .wValue = 0, .wIndex = 0x0200, .wLength = 0};
  dcd_event_setup_received(0, (uint8_t const *) &set_feat, false);
  tud_task();

  tusb_control_request_t const get_status = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_STANDARD,
                          .direction = TUSB_DIR_IN},
    .bRequest = TUSB_REQ_GET_STATUS, .wValue = 0, .wIndex = 0, .wLength = 2};
  dcd_event_setup_received(0, (uint8_t const *) &get_status, false);
  tud_task();

  TEST_ASSERT_EQUAL(0x0002, tu_le16toh(tu_unaligned_read16(_usbd_ctrl_buf)));
}

// USB 3.2 Table 9-10: SetAddress(0) resets FUNCTION REMOTE WAKEUP
void test_usbd_set_address_zero_clears_function_wakeup(void) {
  usbd_driver_open_expect();
  tusb_init(0, &dev_init);
  dcd_event_bus_reset(0, TUSB_SPEED_SUPER, false);
  tud_task();

  tusb_control_request_t const set_feat = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_STANDARD,
                          .direction = TUSB_DIR_OUT},
    .bRequest = TUSB_REQ_SET_FEATURE, .wValue = 0, .wIndex = 0x0200, .wLength = 0};
  dcd_event_setup_received(0, (uint8_t const *) &set_feat, false);
  tud_task();
  TEST_ASSERT_EQUAL(1, tud_suspended() ? 1 : _usbd_dev.remote_wakeup_en);

  tusb_control_request_t const set_addr0 = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_DEVICE, .type = TUSB_REQ_TYPE_STANDARD,
                          .direction = TUSB_DIR_OUT},
    .bRequest = TUSB_REQ_SET_ADDRESS, .wValue = 0, .wIndex = 0, .wLength = 0};
  dcd_event_setup_received(0, (uint8_t const *) &set_addr0, false);
  tud_task();

  TEST_ASSERT_EQUAL(0, _usbd_dev.remote_wakeup_en);
}
```

- [ ] **Step 2: Run them to verify they fail**

```bash
cd test/unit-test && ceedling test:test_usbd
```

Expected: `test_usbd_get_status_interface_reports_function_wakeup` FAILS (expected 2, got 0); `test_usbd_set_address_zero_clears_function_wakeup` FAILS (expected 0, got 1).

- [ ] **Step 3: Add a helper that recomputes the aggregate**

In `src/device/usbd.c`, above the control-request handler, add:

```c
#if TUD_OPT_SUPER_SPEED
// USB 3.2 Table 9-10 resets FUNCTION REMOTE WAKEUP on SetAddress(0), SetConfiguration and
// SetInterface. Clearing the per-function bits must also drop the aggregate authorization.
static void func_wakeup_clear_all(void) {
  tu_memclr(_usbd_dev.func_wakeup_bm, sizeof(_usbd_dev.func_wakeup_bm));
  _usbd_dev.remote_wakeup_en = 0;
}
#endif
```

- [ ] **Step 4: Report the status bits at SuperSpeed**

Replace the interface `GET_STATUS` case at `:1286-1289`:

```c
          case TUSB_REQ_GET_STATUS: {
            // USB 2.0 9.4.5: interface GET_STATUS returns 2 reserved (zero) bytes.
            // USB 3.2 9.4.5 Figure 9-5: D0 Function Remote Wake Capable, D1 Function Remote
            // Wakeup -- 9.2.5.4 makes this the host's discovery mechanism, so a wake-capable
            // function must not report itself incapable.
            uint16_t status = 0x0000;
            #if TUD_OPT_SUPER_SPEED
            if (link_is_superspeed()) {
              const uint8_t itf_mask = (uint8_t) (1u << (itf % 8));
              status = 0x0001; // capable
              if (_usbd_dev.func_wakeup_bm[itf / 8] & itf_mask) {
                status |= 0x0002; // currently enabled
              }
            }
            #endif
            TU_VERIFY(process_get_status(rhport, p_request, status));
            break;
          }
```

- [ ] **Step 5: Reset on SET_INTERFACE and SET_ADDRESS(0)**

In the `TUSB_REQ_SET_INTERFACE` case at `:1278-1284`, before `tud_control_status`:

```c
            TU_VERIFY(tu_u16_low(p_request->wValue) == 0);
            #if TUD_OPT_SUPER_SPEED
            func_wakeup_clear_all(); // USB 3.2 Table 9-10
            #endif
            tud_control_status(rhport, p_request);
```

In the `SET_ADDRESS` handler at `:986-990`, extend the existing `wValue == 0` block:

```c
      #if TUD_OPT_SUPER_SPEED
      if (0 == p_request->wValue) {
        _usbd_dev.u1_enable = 0; // USB 3.2 Table 9-10: SetAddress(0) resets U1/U2 Enable
        _usbd_dev.u2_enable = 0;
        func_wakeup_clear_all(); // ... and FUNCTION REMOTE WAKEUP
      }
      #endif
```

- [ ] **Step 6: Reset on a same-value SET_CONFIGURATION**

In `process_set_config()` (the handler containing the `if (_usbd_dev.cfg_num != cfg_num)` guard near `:1004`), add before that guard:

```c
  #if TUD_OPT_SUPER_SPEED
  // USB 3.2 Table 9-10: SetConfiguration resets FUNCTION REMOTE WAKEUP even when the value is
  // unchanged -- Linux's usb_reset_configuration() sends exactly that and would otherwise leave
  // the authorization armed after the host revoked it.
  func_wakeup_clear_all();
  #endif
```

- [ ] **Step 7: Run the tests to verify they pass**

```bash
cd test/unit-test && ceedling test:all
```

Expected: 74/74 pass (the two new tests plus the existing 72).

- [ ] **Step 8: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
git add src/device/usbd.c test/unit-test/test/device/usbd/test_usbd.c
git commit -m "device: make per-function remote wake readable and resettable

func_wakeup_bm was write-only: interface GET_STATUS returned a hardcoded zero,
so a wake-capable function reported itself incapable even though USB 3.2 9.2.5.4
makes that the host's discovery mechanism. It also survived SET_INTERFACE, a
same-value SET_CONFIGURATION (which Linux's usb_reset_configuration sends) and
SetAddress(0), all of which Table 9-10 marks as resetting FUNCTION REMOTE
WAKEUP, leaving tud_remote_wakeup() armed after the host revoked it."
```

---

# Phase D — Separate branches off master

## Task 12: MSC must propagate the deferral result (F11)

**Files:**
- Create worktree + branch off `master`
- Modify: `src/class/msc/msc_device.c:331`

- [ ] **Step 1: Create an isolated worktree off master**

```bash
cd /home/hathach/code/tinyusb
git worktree add .worktrees/claude-msc-defer -b claude/msc-defer-result master
cd .worktrees/claude-msc-defer
for d in $(python3 -c "import sys;sys.path.insert(0,'tools');\
from get_deps import deps_all;print(' '.join(deps_all.keys()))"); do
  [ -e "$d" ] || ln -s /home/hathach/code/tinyusb/"$d" "$d"
done
```

- [ ] **Step 2: Propagate the result**

In `src/class/msc/msc_device.c`, replace the discard at `:331`:

```c
  // usbd_defer_func returns false when the event queue is full. proc_async_io_done is the only
  // place pending_io is cleared on this path, so a dropped deferral hangs the transfer forever
  // -- report it instead of telling the application the completion was accepted.
  TU_VERIFY(usbd_defer_func(proc_async_io_done, (void *) (intptr_t) bytes_io, in_isr));
  return true;
```

- [ ] **Step 3: Build and unit-test**

```bash
cd test/unit-test && ceedling test:all && cd ../..
cd examples && cmake -B cmake-build-stm32f407disco -DBOARD=stm32f407disco -G Ninja \
  -DCMAKE_BUILD_TYPE=MinSizeRel . && cmake --build cmake-build-stm32f407disco --target msc_dual_lun
```

Expected: 72/72 unit tests, clean build.

- [ ] **Step 4: Commit and push**

```bash
cd /home/hathach/code/tinyusb/.worktrees/claude-msc-defer
git add src/class/msc/msc_device.c
git commit -m "msc: propagate the usbd_defer_func result

usbd_defer_func returns bool so a dropped deferral is visible, but MSC
discarded it and returned unconditional true. proc_async_io_done is the only
place pending_io is cleared on the async path, so a full event queue leaves no
CSW and no next block while the application has been told the completion was
accepted. The identical exposure in dcd_nrf5x.c is noted as follow-up."
git push -u origin claude/msc-defer-result
```

---

## Task 13: stop the board library inheriting family compile options (F15)

`family_support.cmake:451` changed from a property assignment to `APPEND`, so every family's `BOARD_TARGET` compile options now reach the BSP library's own sources — changing codegen for ~19 non-WCH families, none of which were re-validated.

**Files:**
- Create worktree + branch off `master`
- Modify: `hw/bsp/family_support.cmake:439-452`

- [ ] **Step 1: Create the worktree**

```bash
cd /home/hathach/code/tinyusb
git worktree add .worktrees/claude-bsp-warning-flag -b claude/bsp-warning-flag master
cd .worktrees/claude-bsp-warning-flag
for d in $(python3 -c "import sys;sys.path.insert(0,'tools');\
from get_deps import deps_all;print(' '.join(deps_all.keys()))"); do
  [ -e "$d" ] || ln -s /home/hathach/code/tinyusb/"$d" "$d"
done
```

- [ ] **Step 2: Capture the current (master) codegen for a control family**

```bash
cd examples
cmake -B /tmp/bsp-before -DBOARD=raspberrypi_cm4 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . 2>/dev/null \
  || cmake -B /tmp/bsp-before -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel .
grep -m1 "FLAGS = " /tmp/bsp-before/build.ninja > /tmp/bsp-before-flags.txt
cat /tmp/bsp-before-flags.txt
```

Record this output — Step 5 compares against it.

- [ ] **Step 3: Restore the overwriting assignment, scoped to the WCH need**

In `hw/bsp/family_support.cmake`, replace the `set_property(... APPEND ...)` line and its blast-radius comment:

```cmake
        # -w silences the vendor SDK sources the board library compiles. Assign rather than
        # append: APPEND lets every family's BOARD_TARGET PUBLIC/PRIVATE options reach the board
        # library's own sources, which changed BSP codegen for ~19 unrelated families (e.g.
        # broadcom_32bit/64bit's -O0 -ffreestanding beating MinSizeRel's -Os, ch583 gaining
        # -flto -fsigned-char, several lpc families passing link-only -nostdlib as a compile
        # option). Those flags still reach the example through INTERFACE_COMPILE_OPTIONS.
        set_target_properties(${BOARD_TARGET} PROPERTIES COMPILE_OPTIONS -w)
```

- [ ] **Step 4: Verify the WCH boards still build warning-free**

The WCH families must keep their SDK warnings silenced. Build both:

```bash
cd examples
cmake -B cmake-build-hydrausb3_v1 -DBOARD=hydrausb3_v1 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-hydrausb3_v1 --target board_test 2>&1 | grep -i "warning" | head
```

Expected: no warnings from `hw/mcu/wch/**`. If any appear, add `-w` to that family's own `BOARD_TARGET` options in `hw/bsp/ch569/family.cmake` and `hw/bsp/ch32h417/family.cmake` rather than reinstating the global APPEND.

- [ ] **Step 5: Verify the control families are back to master codegen**

```bash
cd examples
cmake -B /tmp/bsp-after -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel .
grep -m1 "FLAGS = " /tmp/bsp-after/build.ninja > /tmp/bsp-after-flags.txt
diff /tmp/bsp-before-flags.txt /tmp/bsp-after-flags.txt && echo "IDENTICAL to master codegen"
```

Expected: `IDENTICAL to master codegen`.

- [ ] **Step 6: Build the two families the APPEND provably changed**

```bash
cd examples
cmake -B cmake-build-ch583 -DBOARD=ch583 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-ch583 --target board_test
```

Expected: clean build. (`broadcom_32bit` needs an aarch64 toolchain; if it is not installed, note that in the commit message rather than skipping silently.)

- [ ] **Step 7: Commit and push**

```bash
cd /home/hathach/code/tinyusb/.worktrees/claude-bsp-warning-flag
git add hw/bsp/family_support.cmake
git commit -m "bsp: stop the board library inheriting family compile options

Appending -w to COMPILE_OPTIONS instead of assigning it let every family's
BOARD_TARGET PUBLIC/PRIVATE options apply to the board library's own sources,
changing BSP codegen for ~19 unrelated families - broadcom's -O0 beating
MinSizeRel's -Os, ch583 gaining -flto -fsigned-char (ABI-visible between BSP
and SDK objects), several lpc families passing link-only -nostdlib as a compile
option. Those flags still reach the example through INTERFACE_COMPILE_OPTIONS."
git push -u origin claude/bsp-warning-flag
```

---

# Phase E — Verification

## Task 14: full verification sweep and hardware sign-off

**Files:** none modified; this task only runs and reports.

- [ ] **Step 1: Format, spell and unit tests**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
pre-commit run --all-files
```

Expected: all hooks pass.

- [ ] **Step 2: Python harness tests**

```bash
python3 -m pytest test/hil/ -v
```

Expected: all pass, including the new parser and cell tests.

- [ ] **Step 3: Build both WCH boards at both speeds**

```bash
cd examples
cmake -B cmake-build-hydrausb3_v1 -DBOARD=hydrausb3_v1 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-hydrausb3_v1
cmake -B cmake-build-nanoch32h417 -DBOARD=nanoch32h417 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-nanoch32h417
```

Expected: all examples build for both boards.

- [ ] **Step 4: Build an unrelated port as a regression check**

```bash
cd examples
cmake -B cmake-build-stm32f407disco -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-stm32f407disco
```

Expected: 44 ELFs, no errors — the shared `usbd.c` and class changes must not regress a non-WCH port.

- [ ] **Step 5: HIL sweep on the CH569 hydra**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/ch569-hydrausb3
python3 test/hil/hil_lock.py acquire hydrausb3_v1
python3 test/hil/hil_test.py test/hil/tinyusb.json -b hydrausb3_v1
```

Expected: 12/12 examples pass, usbtest cell reads `✅ 30/30` (no skips at SuperSpeed on the CH569).

- [ ] **Step 6: usbtest cases 13 and 29 specifically (Task 7 gate)**

```bash
python3 test/hil/usbtest.py --serial <hydra-serial> --tests 13,29
```

Expected: both PASS. **If either fails, stop and report** — see Task 7 Step 4; do not reinstate the clear-stall re-arm.

- [ ] **Step 7: The replug test (Task 6 gate, F1)**

**[ACTION]** This step needs you at the bench:

1. With the hydra enumerated at SuperSpeed (confirm: `cat /sys/bus/usb/devices/<port>/speed` reads `5000`), unplug it from the SuperSpeed port.
2. Plug it into a **USB 2.0-only** port (a port with no SuperSpeed pairs, or via a USB 2.0-only hub).
3. Confirm it enumerates at 480 Mbps: `dmesg | tail` should show `new high-speed USB device`.

Expected: the board comes up on the USB2 controller. Before this fix it stayed dead until a power cycle.

- [ ] **Step 8: Release the lock**

```bash
python3 test/hil/hil_lock.py release hydrausb3_v1
```

- [ ] **Step 9: Update the deferred list with anything that did not get measured**

Edit `docs/superpowers/todo/pr3779-wch-superspeed.md` and record the outcome of Steps 6 and 7, plus confirmation that F4 (Task 9) and F16 (Task 5) shipped unmeasured.

- [ ] **Step 10: Commit and push**

```bash
git add docs/superpowers/todo/pr3779-wch-superspeed.md
git commit -m "docs: record the remediation verification results"
git push origin claude/wch-ch569-ch32h417-usb3
```

---

## Self-Review

**Spec coverage:** every finding maps to a task — F2→1, F5→2, F9→3, F13→4, F16→5, F1/F7/F10/F14→6, F3→7, F6→8, F4→9, F8→10, F12→11, F11→12, F15→13; verification →14. No spec section is unimplemented.

**Deviation from the spec, recorded deliberately:** the design proposed rendering a skipped usbtest run with a distinct `⚠️` icon. Task 1 keeps the `✅` prefix and fixes the denominator instead (`✅ 25/30 (5 skipped)`), because `hil_test.py` classifies report cells by that icon prefix and introducing a fourth icon risks miscounting the run tally. The design's intent — that a quirked run can never be read as a full pass — is met by the honest denominator.

**Placeholder scan:** no "TBD", no "handle edge cases", no "similar to Task N". Three steps intentionally direct the implementer to `grep` for a local symbol name before editing (Task 5 Step 2, Task 10 Steps 2-3); each states the expected name and the fallback, so no guessing is required.

**Type consistency:** `usbtest_cell(data) -> str` (Task 1) is used only in Task 1. `parse_setup_outcomes(rows) -> (int, int)` (Task 2) keeps its existing signature. `fallback_enter(uint8_t next)` (Task 6) is the sole `_fb_state` writer and is verified by grep in Step 7. `ep_sz_bulk` is `uint16_t` everywhere (Task 8) — the rename from `uint8_t ep_sz_fs` is required, since 512 and 1024 do not fit the old type. `func_wakeup_clear_all(void)` (Task 11) is defined once and called from three sites, all inside `#if TUD_OPT_SUPER_SPEED`.
