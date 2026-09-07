# `flasher_recover` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the 15 HIL boards whose flasher cannot reach its probe past a poisoned usbfs
node a second, convoy-safe flasher used only for recovery.

**Architecture:** An optional roster key `flasher_recover` beside `flasher`.
`hil_flash.recover_flasher(board)` picks it when present; `hil_test` substitutes it into the
`--recover-board` JSON so `usbtest.py` never learns a second entry exists. Delivery over
openocd's jlink driver is convoy-safe by construction, but the flash command form must
differ from the one `flash_openocd` uses, so the recovery gets its own flasher name.

**Tech Stack:** Python 3.13 stdlib, openocd 0.12.0+dev (build 0ce743125 on ci.lan),
libjaylink, J-Link probes.

## Global Constraints

- Roster JSON: `test/hil/tinyusb.json`. `flasher_recover` is OPTIONAL; absent means today's
  behaviour (`recover_flasher` returns the primary).
- Never change the shape of `board['flasher']` — it is read as a dict in `hil_flash`,
  `hil_test`, `usbtest`, `hil_pool_check`, `ci_select` and the roster lint, and is shipped
  as JSON to a subprocess.
- Flasher dispatch is by name: `getattr(hil_flash, f'flash_{name}')` / `reset_{name}`.
- `RECOVER_FLASH_TIMEOUT = 90`, `RECOVER_RESET_TIMEOUT = 30` (`usbtest.py`). Any board whose
  flash cannot finish inside 90 s is not a candidate.
- Tests run offline: `cd test/hil && python3 test/test_ci_select.py`.

## What is already established

**Landed on PR #3803 and inert without roster entries:** `hil_flash.recover_flasher()`,
`convoy_safe()` accepting openocd-over-jlink, `hil_test` substituting the recovery flasher
into `--recover-board`, and `test_ci_select.FlasherRecoverEntry` (4 tests).

**Verified in source:**
- openocd's jlink driver ignores `adapter usb vid_pid` — `jlink.c` never reads
  `adapter_usb_get_vids/pids`; selection is `adapter serial` / USB address / usb location.
  Do NOT lint a jlink recovery entry for `vid_pid`.
- It is convoy-safe anyway: libjaylink `discovery_usb.c` returns early unless
  `idVendor == 0x1366` and the PID is in its table, and only THEN calls `libusb_open`. A
  wedged `cafe:4010` DUT is never opened.
- CMSIS-DAP stays pin-gated: `cmsis_dap_usb_bulk.c:107` skips before `libusb_open`, and
  `id_filter` is only `vids[0] || pids[0]`.

**Measured on ci.lan 2026-08-17**, base args
`-f interface/jlink.cfg -c "transport select swd" -c "adapter speed 4000" -f target/<cfg>`:

| Board | target cfg | flash | reset |
|--------------------------|--------------|-------|-------|
| stm32f407disco           | stm32f4x     | OK    | OK    |
| stm32f072disco           | stm32f0x     | OK    | OK    |
| stm32f723disco           | stm32f7x     | OK    | OK    |
| stm32l476disco           | stm32l4x     | OK    | OK    |
| feather_nrf52840_express | nrf52        | OK    | OK    |
| metro_m4_express         | atsame5x     | OK    | OK    |
| frdm_k64f                | k60          | OK    | OK    |

`frdm_k64f` is host-only (`tests.device == false`) — verify its reset over UART
(`/dev/serial/by-id/usb-SEGGER_J-Link_000621000000-if00`), never by USB disconnect.

**Excluded, with reasons:** `lpcxpresso11u37` — 118 s for 24 KB at 1 MHz with a verify
mismatch, versus 0.277 s via JLinkExe; cannot fit `RECOVER_FLASH_TIMEOUT`.
`mimxrt1064_evk`, `ra4m1_ek`, `nrf54lm20dk` — no target config exists in this openocd
build, so they cannot be covered at all. **The board that wedges most (mimxrt1064_evk) is
therefore still uncovered by this work.**

**The blocker this plan solves:** `flash_openocd` issues `program <fw> verify reset exit`,
which fails over the jlink transport on BOTH families tried (`stm32f4x`, `stm32f0x`) with
`Examination failed` → `auto_probe failed`, with or without a preceding `init; reset halt`.
Every successful flash above used the explicit sequence in Task 1.

**Why this is a separate PR:** it adds a roster capability and a new flasher backend, which
is a different scope from containing a wedge; and it needs bench time on seven boards.

## File Structure

- `test/hil/hil_flash.py` — add `flash_openocd_seq` / `reset_openocd_seq`; extend
  `convoy_safe` to accept the new name. This is the only file that learns the command form.
- `test/hil/tinyusb.json` — seven `flasher_recover` entries.
- `test/hil/test/test_ci_select.py` — extend `FlasherRecoverEntry`; add a roster lint.

---

### Task 1: `openocd_seq` flasher backend

**Files:**
- Modify: `test/hil/hil_flash.py` (beside `flash_openocd`, ~line 100)
- Test: `test/hil/test/test_ci_select.py`

**Interfaces:**
- Consumes: `_openocd_cmd_base(flasher)`, `hil_util.run_cmd`.
- Produces: `flash_openocd_seq(board, firmware, timeout=None)`,
  `reset_openocd_seq(board, timeout=None)`, both returning
  `subprocess.CompletedProcess`; `convoy_safe()` returns True for
  `{'name': 'openocd_seq', 'args': '...interface/jlink.cfg...'}`.

- [ ] **Step 1: Write the failing test**

```python
    def test_openocd_seq_is_convoy_safe_over_jlink(self):
        self.assertTrue(hil_flash.convoy_safe(
            {'name': 'openocd_seq', 'args': '-f interface/jlink.cfg -f target/stm32f4x.cfg'}))

    def test_openocd_seq_uses_explicit_flash_commands_not_program(self):
        """`program` fails over the jlink transport: Examination failed -> auto_probe
        failed, measured on stm32f4x and stm32f0x."""
        seen = {}
        real = hil_util.run_cmd
        hil_util.run_cmd = lambda cmd, **k: seen.setdefault('cmd', cmd) or real('true')
        try:
            hil_flash.flash_openocd_seq(
                {'flasher': {'name': 'openocd_seq', 'uid': 'X', 'args': '-f interface/jlink.cfg'}},
                '/tmp/fw.elf', timeout=5)
        finally:
            hil_util.run_cmd = real
        self.assertIn('flash write_image erase /tmp/fw.elf', seen['cmd'])
        self.assertIn('verify_image /tmp/fw.elf', seen['cmd'])
        self.assertNotIn('program ', seen['cmd'])
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test/hil && python3 test/test_ci_select.py FlasherRecoverEntry -v`
Expected: FAIL — `module 'hil_flash' has no attribute 'flash_openocd_seq'`

- [ ] **Step 3: Write minimal implementation**

```python
def flash_openocd_seq(board, firmware, timeout=None):
    # Explicit commands, NOT `program`: over the jlink transport `program` fails at the
    # flash bank probe ("Examination failed" -> "auto_probe failed"), measured on
    # stm32f4x and stm32f0x, with or without a preceding reset halt. This sequence
    # succeeded on all seven candidate boards.
    flasher = board['flasher']
    verify = f' -c "verify_image {firmware}"' if flasher.get('verify', True) else ''
    return hil_util.run_cmd(
        f'{_openocd_cmd_base(flasher)} -c "init" -c "reset halt" '
        f'-c "flash write_image erase {firmware}"{verify} -c "reset run" -c "shutdown"',
        timeout=timeout)


def reset_openocd_seq(board, timeout=None):
    flasher = board['flasher']
    return hil_util.run_cmd(
        f'{_openocd_cmd_base(flasher)} -c "init" -c "reset run" -c "shutdown"',
        timeout=timeout)
```

In `convoy_safe`, replace `if name != 'openocd':` with:

```python
    if name not in ('openocd', 'openocd_seq'):
        return False
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd test/hil && python3 test/test_ci_select.py FlasherRecoverEntry -v`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add test/hil/hil_flash.py test/hil/test/test_ci_select.py
git commit -m "hil: add openocd_seq flasher for convoy-safe recovery delivery"
```

---

### Task 2: Roster entries for the seven validated boards

**Files:**
- Modify: `test/hil/tinyusb.json`
- Test: `test/hil/test/test_ci_select.py`

**Interfaces:**
- Consumes: `flash_openocd_seq` / `reset_openocd_seq` from Task 1.
- Produces: seven boards for which `hil_flash.convoy_safe(hil_flash.recover_flasher(b))`
  is True.

- [ ] **Step 1: Write the failing test**

```python
    def test_roster_recover_entries_are_convoy_safe_and_named_openocd_seq(self):
        import json, pathlib
        roster = json.loads((pathlib.Path(__file__).parent.parent / 'tinyusb.json').read_text())
        recover = [b for b in roster['boards'] if 'flasher_recover' in b]
        self.assertGreaterEqual(len(recover), 7)
        for b in recover:
            f = b['flasher_recover']
            self.assertEqual(f['name'], 'openocd_seq', b['name'])
            self.assertIn('interface/jlink.cfg', f['args'], b['name'])
            self.assertIn('adapter speed', f['args'], b['name'])   # required; see below
            self.assertTrue(hil_flash.convoy_safe(f), b['name'])
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test/hil && python3 test/test_ci_select.py FlasherRecoverEntry -v`
Expected: FAIL — `0 >= 7`

- [ ] **Step 3: Add the entries**

`adapter speed` is REQUIRED: without it examination fails outright on the jlink driver.
Add to each board below, using the SAME `uid` as its primary jlink entry:

```json
"flasher_recover": {
  "name": "openocd_seq",
  "uid": "<same probe serial as flasher.uid>",
  "args": "-f interface/jlink.cfg -c \"transport select swd\" -c \"adapter speed 4000\" -f target/<cfg>.cfg"
}
```

| Board | `uid` | `<cfg>` |
|--------------------------|----------------|-----------|
| stm32f407disco           | 000773661813   | stm32f4x  |
| stm32f072disco           | 779541626      | stm32f0x  |
| stm32f723disco           | 000776606156   | stm32f7x  |
| stm32l476disco           | 777632258      | stm32l4x  |
| feather_nrf52840_express | 681295394      | nrf52     |
| metro_m4_express         | 123456         | atsame5x  |
| frdm_k64f                | 000621000000   | k60       |

- [ ] **Step 4: Run test to verify it passes**

Run: `cd test/hil && python3 test/test_ci_select.py -v`
Expected: PASS, and no other selector test regresses.

- [ ] **Step 5: Commit**

```bash
git add test/hil/tinyusb.json test/hil/test/test_ci_select.py
git commit -m "hil: give seven J-Link boards a convoy-safe recovery flasher"
```

---

### Task 3: Bench validation on the rig

**Files:** none — this task produces evidence, not code.

- [ ] **Step 1: Confirm the rig is idle and take the locks**

```bash
ssh hathach@ci.lan 'if pgrep -f "[h]il_test.py" >/dev/null; then echo BUSY; exit 1; fi'
ssh hathach@ci.lan 'cd ~/actions-runner/_work/tinyusb/tinyusb && \
  nohup timeout 900 python3 test/hil/helper/hil_lock.py hold <boards...> --reason "flasher_recover validation" &'
```

Guard with `if`, never `cmd && echo || echo` — that form only gates the echo and will take
locks during a live CI run.

- [ ] **Step 2: For each board, flash then reset through the recovery entry**

```bash
python3 test/hil/hil_test.py -b <board> test/hil/tinyusb.json   # normal path still works
```

Then force the recovery path by running usbtest with the recovery flags and a firmware that
hangs a case, or drive `hil_flash.flash_openocd_seq` / `reset_openocd_seq` directly.

- [ ] **Step 3: Verify**

Device boards: `sudo dmesg` shows `USB disconnect` then a fresh enumeration.
`frdm_k64f`: UART shows the boot banner (see above).
Every flash must finish well inside `RECOVER_FLASH_TIMEOUT` (90 s).

- [ ] **Step 4: Release locks and record the results in the PR body**

---

## Out of scope, and why

- **`mimxrt1064_evk`** needs an i.MX RT target config that this openocd build does not
  have. Sourcing or writing one is its own investigation; until then the board with the
  most wedges has no automated recovery.
- **Changing `flash_openocd`** to the explicit form would cover these boards without a new
  name, but `program` is what nine pinned CMSIS-DAP boards use in CI daily and no CMSIS-DAP
  image could be built in the originating worktree (no pico-sdk) to re-validate it.
