# Drop the EP0 Post-Prime Verify Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the EP0 post-prime verification that was built on a theory the RT106x endpoint-conflict errata has superseded, and prove on hardware that nothing depended on it.

**Architecture:** One deletion in `qhd_start_xfer()`, then a rebase onto current master, then an A/B validation whose "with it" arm is already banked (10x 30/30 batteries plus 40 targeted loops on 2026-08-16). No interfaces change: the pre-prime setup-lockout guard keeps `qhd_start_xfer()` returning `bool`, so `dcd_set_address()`'s gating and usbd's failure path stay exactly as they are.

**Tech Stack:** C99, TinyUSB ChipIdea HS DCD (`src/portable/chipidea/ci_hs/`), CMake+Ninja and Make builds, J-Link (JLinkExe V9.66), `test/hil/usbtest.py` driving the Linux testusb battery.

## Global Constraints

- Branch `fix-ci-hs` in worktree `/home/hathach/.herdr/worktrees/tinyusb/fix-ci-hs`. Do NOT push; the user pushes.
- C99, 2-space indent. Commit messages imperative, no `Co-Authored-By:` or `Claude-Session:` trailers (repo rule: hathach is sole author).
- Pre-commit hook (trailing-whitespace, end-of-file-fixer, codespell, unique-PIDs, ceedling) must pass; if it rewrites a file, re-stage and retry the commit once.
- Never edit anything under `hw/mcu/` or `lib/` (vendor code).
- Rig etiquette: hold the board lock for hardware work (`python3 test/hil/hil_lock.py hold <board> --reason "..."`, release after); abort if CI is active (`pgrep -f "hil_test.py [-]-retry"`); NEVER use `uhubctl`, `pci-reset` or `pci-rebind`; never touch the actions-runner.
- JLinkExe on this rig is **V9.66 and has no `verifyfile` command** — use `loadfile` (built-in Program & Verify) plus a mandatory enumeration check.
- Board facts: `mimxrt1064_evk`, serial `BAE96FB95AFA6DBB8F00005002001200`, J-Link probe `000725299165`, device `MIMXRT1064xxx6A`, expected `cafe:4010`.
- Design source of truth: `docs/superpowers/specs/2026-08-16-drop-ep0-prime-verify-design.md`.

## File Structure

| File | Responsibility in this plan |
|---|---|
| `src/portable/chipidea/ci_hs/dcd_ci_hs.c` | The only code change: delete the post-prime block in `qhd_start_xfer()` |

Tasks 2 and 3 change no files; they rebase and validate.

---

### Task 1: Delete the EP0 post-prime verify

**Files:**
- Modify: `src/portable/chipidea/ci_hs/dcd_ci_hs.c` (the tail of `qhd_start_xfer()`)

**Interfaces:**
- Produces: `qhd_start_xfer()` keeps its existing signature `static bool qhd_start_xfer(uint8_t rhport, uint8_t epnum, uint8_t dir)` and still returns `false` from the pre-prime setup-lockout guard. No caller changes.

- [ ] **Step 1: Apply the deletion**

In `qhd_start_xfer()`, replace this (everything from the prime write to the closing `return true;`):

```c
  // start transfer
  const uint32_t prime_bit = TU_BIT(epnum + (dir ? 16 : 0));
  dcd_reg->ENDPTPRIME     = prime_bit;

  if (epnum == 0) {
    // RM (RT1050 RM Executing a Transfer / UM10503 25.10.8): after priming EP0 the DCD must
    // verify the prime completed - ENDPTPRIME bit clear AND the buffer reported ready in
    // ENDPTSTAT - because the controller silently cancels an EP0 prime when a SETUP arrives
    // during the prime operation. An undetected drop NAK-parks the endpoint forever: usbd never
    // re-primes a busy endpoint. A very fast transfer may already have completed and retired the
    // ENDPTSTAT bit, so ENDPTCOMPLETE also counts as the prime having taken.
    uint32_t guard = CI_HS_BUSY_SPIN;
    while (dcd_reg->ENDPTPRIME & prime_bit) {
      if (!guard--) {
        dcd_reg->ENDPTFLUSH = prime_bit; // never leave a wedged prime armed over a freed buffer
        return false;
      }
    }
    // Fail only when the cancel-cause is visibly pending: a completed transfer can have both
    // status bits already retired by the ISR, and a cancel whose SETUP the ISR consumed is
    // re-driven by that queued SETUP event anyway.
    if (!((dcd_reg->ENDPTSTAT | dcd_reg->ENDPTCOMPLETE) & prime_bit) &&
        (dcd_reg->ENDPTSETUPSTAT & TU_BIT(0))) {
      return false; // prime cancelled (setup mid-prime): the pending SETUP re-drives EP0
    }
  }
  return true;
```

with:

```c
  // start transfer
  dcd_reg->ENDPTPRIME = TU_BIT(epnum + (dir ? 16 : 0));
  return true;
```

Leave the `if (epnum == 0)` setup-lockout block ABOVE the prime write completely untouched —
that one spins on `ENDPTSETUPSTAT` before priming and is required by UM10503 25.10.8.1.1
step 4.

- [ ] **Step 2: Confirm nothing else referenced the removed code**

Run:

```bash
grep -n "ENDPTSTAT\|ENDPTCOMPLETE\|prime_bit" src/portable/chipidea/ci_hs/dcd_ci_hs.c
```

Expected: no `prime_bit` hits at all; `ENDPTCOMPLETE` hits only in `bus_reset_begin()` and the
`INTR_USB` branch of `dcd_int_handler()`; `ENDPTSTAT` hits only in `ci_hs_type.h`-style register
declarations if any appear — none inside `qhd_start_xfer()`.

- [ ] **Step 3: Build both ci_hs board families**

Run:

```bash
cmake --build examples/cmake-build-mimxrt1064_evk && cmake --build examples/cmake-build-lpcxpresso18s37
```

Expected: both succeed, no new warnings (in particular no "unused variable" for anything the
deletion orphaned).

- [ ] **Step 4: Commit**

```bash
git add src/portable/chipidea/ci_hs/dcd_ci_hs.c
git commit -m "dcd(ci_hs): drop the EP0 post-prime verify

The verify came from a theory that a setup arriving mid-prime silently
cancels an EP0 prime, which was how the recurring wedge on the test rig
looked at the time. The wedge turned out to be Errata i.MX RT1064_A
ERR050101: with an isochronous IN endpoint active, an IN token to that
endpoint number on another device sharing the host unprimes one of our OUT
endpoints, undetectably and with no interrupt. Moving the usbtest iso IN
endpoint clear of the conflict fixed it - 340 runs where the board used to
wedge within hours.

The capture that motivated the verify (EP0 status stage armed but unprimed,
device a control transfer ahead of the host) is explained by that errata
just as well, because it covers control OUT endpoints and a control status
stage is one. So the verify has no independent evidence behind it, while it
does cost two register spins on every EP0 transfer and can misread a
transfer the interrupt handler already completed as a cancelled prime.

The setup-lockout check before priming stays - that one is in the manual."
```

---

### Task 2: Rebase onto current master and re-run the software gates

**Files:** none modified by hand.

**Interfaces:** none.

- [ ] **Step 1: Rebase**

Master has advanced (midi2/usbtmc/video changes) since this branch last rebased. Validating a
tree that is not the one being merged would be a false pass.

```bash
git fetch origin master
git rebase origin/master
```

Expected: clean rebase. If a conflict appears in `src/portable/chipidea/ci_hs/dcd_ci_hs.c` or
`src/device/usbd.c`, resolve it hunk-by-hunk keeping BOTH sides' intent (never `git checkout
--theirs/--ours` on a whole file), then `git rebase --continue`.

- [ ] **Step 2: Rebuild everything from scratch**

```bash
cd examples
for b in mimxrt1064_evk lpcxpresso18s37 lpcxpresso11u37 lpcxpresso55s28; do
  rm -rf cmake-build-$b
  cmake -B cmake-build-$b -DBOARD=$b -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . && cmake --build cmake-build-$b || echo "FAILED $b"
done
cd ..
```

Expected: all four boards build every example, no "FAILED" line.

- [ ] **Step 3: Make link canaries**

```bash
cd examples/host/cdc_msc_hid && make -j8 BOARD=lpcxpresso55s28 all && cd ../../..
cd examples/device/cdc_msc_throughput && make -j8 BOARD=lpcxpresso11u37 all && cd ../../..
```

Expected: both link. These two were broken earlier in the branch's life and are the regression
canaries for the BSP changes.

- [ ] **Step 4: Unit tests and pre-commit**

```bash
cd test/unit-test && ceedling test:all && cd ../..
pre-commit run --all-files
```

Expected: all unit tests pass; every pre-commit hook passes.

- [ ] **Step 5: No commit**

This task produces no commit of its own — the rebase rewrites existing commits and the builds
are throwaway. Record the resulting HEAD hash in the report for Task 3 to reference.

---

### Task 3: Hardware A/B on mimxrt1064_evk

**Files:** none modified — this task produces the evidence.

**Interfaces:** consumes the firmware built in Task 2 at
`examples/cmake-build-mimxrt1064_evk/device/usbtest/usbtest.elf`.

Only this board is tested: it is the sole ci_hs board on the rig. The lpcxpresso55s28 and
lpcxpresso11u37 run the ip3511 driver, which this change does not touch.

- [ ] **Step 1: Preconditions**

```bash
pgrep -f "hil_test.py [-]-retry" && echo "CI ACTIVE - wait" || echo "CI idle"
ps -eo stat,pid,etimes,comm | awk '$1 ~ /^D/'
python3 test/hil/hil_lock.py hold mimxrt1064_evk --reason "prime-verify removal A/B"
```

Expected: CI idle, no pre-existing D-state processes, lock acquired. If CI is active, wait for
it to drain rather than running concurrently.

- [ ] **Step 2: Flash with verification**

```bash
cat > /tmp/pv.jlink <<'EOF'
r
h
loadfile examples/cmake-build-mimxrt1064_evk/device/usbtest/usbtest.elf
r
g
qc
EOF
JLinkExe -device MIMXRT1064xxx6A -if SWD -speed 4000 -SelectEmuBySN 000725299165 \
         -autoconnect 1 -nogui 1 -CommandFile /tmp/pv.jlink
```

Expected: `Program & Verify` reports O.K.

- [ ] **Step 3: Confirm the right image is actually running**

```bash
sleep 5
grep -l BAE96FB95AFA6DBB8F00005002001200 /sys/bus/usb/devices/*/serial
sudo lsusb -v -d cafe:4010 2>/dev/null | grep -A3 "Isochronous" | grep bEndpointAddress
```

Expected: the board is present, and the iso IN endpoint reads **0x87**. If it reads 0x83 the
flash did not take (this board has silently no-op'd a flash twice) — reflash and re-check
before running anything.

- [ ] **Step 4: 5x full battery**

```bash
for i in $(seq 1 5); do
  timeout 700 python3 test/hil/usbtest.py --serial BAE96FB95AFA6DBB8F00005002001200 \
    --json --keep-binding --timeout 60 2>/dev/null | python3 -c "
import json,sys
d=json.load(sys.stdin)
bad=[str(c['num']) for c in d['cases'] if c['status']!='PASS']
print(f\"run: {d['passed']}/30 speed={d['speed']}\" + (' FAILED:'+','.join(bad) if bad else ''))
"
  ps -eo stat,pid,etimes,comm | awk '$1 ~ /^D/ && $4=="testusb"'
done
```

Expected: five lines each reading `30/30 speed=480`, and no testusb D-state line between runs.

- [ ] **Step 5: 15x control-focused loop**

These are the paths the removed verify actually protected — queued control, the ch9 subset, and
both ctrl_out cases. A full battery samples each only once per run.

```bash
PASS=0
for i in $(seq 1 15); do
  timeout 300 python3 test/hil/usbtest.py --serial BAE96FB95AFA6DBB8F00005002001200 \
    --tests 9,10,14,21 --json --keep-binding --timeout 60 >/dev/null 2>&1 && PASS=$((PASS+1)) || { echo "FAILED at iteration $i"; break; }
  D=$(ps -eo stat,comm | awk '$1 ~ /^D/ && $2=="testusb"' | wc -l)
  [ "$D" != "0" ] && { echo "D-STATE at iteration $i"; break; }
done
echo "control loops: $PASS/15"
```

Expected: `control loops: 15/15`, no FAILED or D-STATE line.

- [ ] **Step 6: Release the lock and record**

```bash
python3 test/hil/hil_lock.py release mimxrt1064_evk
ps -eo stat,pid,etimes,comm | awk '$1 ~ /^D/'
```

Expected: lock released, no leftover D-state.

**Acceptance:** 5/5 batteries at 30/30, 15/15 control loops, no `testusb` D-state outliving its
case runtime.

**Rollback trigger:** any control-case failure (errno 110 or 71 on cases 9, 10, 14, 21) or a
lingering D-state means the verify was load-bearing after all. In that case: `git revert` the
Task 1 commit, re-run Steps 4-5 to confirm the failure disappears, and record the result — that
is a finding worth keeping, not a setback to hide.

---

## Self-Review

**Spec coverage:** the spec's change section → Task 1; "rebase first, then rebuild" → Task 2
Steps 1-2; software gates → Task 2 Steps 3-4; hardware preconditions, verified flash and the
0x87 descriptor check → Task 3 Steps 1-3; 5x battery and 15x control loop → Task 3 Steps 4-5;
acceptance and rollback trigger → Task 3's closing block. The spec's "deliberately kept" list is
enforced negatively by Task 1 Step 1's instruction to leave the setup-lockout block untouched
and by Task 1 Step 2's grep. No gaps.

**Placeholder scan:** no TBD/TODO/"handle edge cases"; every step carries its exact command or
code and its expected result.

**Type consistency:** `qhd_start_xfer(uint8_t rhport, uint8_t epnum, uint8_t dir) -> bool` is
unchanged by this plan and no caller is touched, so there are no cross-task signatures to
reconcile. The only removed identifier, `prime_bit`, is local to the deleted block and Task 1
Step 2 greps to confirm it has no remaining references.
