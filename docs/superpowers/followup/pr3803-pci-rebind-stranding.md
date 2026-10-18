# `pci-rebind` Stranding Investigation Plan

**Goal:** Settle when a PCI unbind/rebind of an xHCI controller strands it driverless, so
the `usb-kernel-recover` skill can state a rule instead of a hypothesis.

**Architecture:** No product code. This is a controlled reproduction against the rig's
kernel, ending in a documentation change and — if the boundary turns out to be
detectable — a guard in `usb_recover.sh`.

**Tech Stack:** Linux 6.12.96 on ci.lan when filed (check `uname -r`), Renesas uPD720201 xHCI, `usb_recover.sh`.

## Global Constraints

- **Blocked:** the HIL contract (`.claude/skills/hil-contract/SKILL.md`, Reservation) refuses
  controller operations (`pci-rebind`, `pci-bind`) until it defines a reservation procedure
  that covers them. Do not run Task 1 or Task 2 before that procedure exists.
- ci.lan is a live CI rig. Take the controller-operation reservation that procedure defines,
  across every roster on the host, and confirm no `hil_test.py` is running, with an `if`,
  not an `&&` chain.
- A stranded controller takes every fixture on it offline; recovery is
  `usb_recover.sh pci-bind <addr>` or, failing that, a PVE **host** power cycle — an
  operator action. Do not start this without being able to reach the host.
- The rig has eight Renesas controllers; pick the controller with the fewest
  fixtures for the experiment.

## What is already established

**The skill claimed, unconditionally, that `pci-rebind`'s re-bind hangs on the D-state URB
and leaves the controller with no driver.** That claim was generalised from ONE observation
and was used to delete `pci-rebind` and `pci-bind` from `usb_recover.sh` entirely.

**It was refuted in the field on 2026-08-17.** After `hub-cycle 17-2.7` failed to clear a
wedge, `pci-rebind 0000:05:00.0` recovered the controller in about one second:

```
02:34:41  remove, state 4 / USB bus 18 deregistered
02:34:41  remove, state 1 / USB bus 17 deregistered
02:34:42  xHCI Host Controller / new USB bus registered, assigned bus number 1
02:34:42  new USB bus registered, assigned bus number 2
```

Both actions were restored, with the guidance scoped to failure mode: **dead controller →
use it; device-lock convoy → do not**. Buses renumbered 17/18 → 1/2, which is why rig-wide
operations need every board's lock.

**What is NOT known:** why the earlier attempt stranded and this one did not. The leading
hypothesis is that it turns on whether a live D-state URB exists **on that controller** at
the moment of the re-bind — but in the 02:34 incident the wedged board (17-2.7) was on that
very controller, which weakens it. An alternative is that `hub-cycle` had already cleared
the holder, leaving only a dead controller.

**Why this is a separate PR:** it is an experiment that risks taking the rig offline, and
its output is a documentation change plus possibly a guard — a different scope from any
code change.

## File Structure

- `lib/agentrc/skills/usb-kernel-recover/SKILL.md` — replace the hypothesis in section 3b and
  the Common-mistakes entry with whatever the experiment establishes.
- `lib/agentrc/skills/usb-kernel-recover/scripts/usb_recover.sh` — only if the boundary is
  detectable from userspace.

---

### Task 1: Reproduce a controller-scoped D-state wedge

**Files:** none.

- [ ] **Step 1: Establish the safety net**

```bash
ssh hathach@ci.lan 'if pgrep -f "[h]il_test.py" >/dev/null; then echo BUSY; exit 1; fi'
# take the HIL contract's controller-operation reservation (Reservation); none exists yet,
# so stop here until it does
```

Confirm host access to pve.lan before continuing.

- [ ] **Step 2: Create a wedge deliberately**

Run `usbtest.py` against a board known to hang (`mimxrt1064_evk` has wedged eight times,
TEST 9/10/24/27), or drive `testusb` directly until a case does not return.

- [ ] **Step 3: Confirm the holder and its controller**

```bash
ps -eo pid,stat,etimes,wchan:22,args | awk '$2 ~ /D/'
sudo cat /proc/<pid>/stack        # usbdev_ioctl + [usbtest] = the owner
readlink -f /sys/bus/usb/devices/usb<N>   # bus -> PCI addr
```

Record whether the holder is on the SAME controller you will rebind.

---

### Task 2: Rebind and record the outcome

**Files:** none.

Runs only under the controller-operation reservation from Task 1 Step 1; the HIL contract
refuses these commands until that procedure exists.

- [ ] **Step 1: Rebind, with a bounded observer**

```bash
timeout 120 sudo lib/agentrc/skills/usb-kernel-recover/scripts/usb_recover.sh pci-rebind <addr>; echo "rc=$?"
```

- [ ] **Step 2: Record which of the three outcomes occurred**

1. Re-bind completes, controller recovers (as on 2026-08-17).
2. Re-bind hangs; `/sys/bus/pci/devices/<addr>/driver` is gone → **stranded**.
3. Re-bind completes but the wedge persists.

Capture `sudo journalctl -k --since ...` around the attempt either way.

- [ ] **Step 3: If stranded, recover**

```bash
sudo lib/agentrc/skills/usb-kernel-recover/scripts/usb_recover.sh pci-bind <addr>
```

If that hangs too, the only remaining step is a PVE host power cycle — an operator action.

- [ ] **Step 4: Repeat at least three times**

One observation is what produced the wrong rule in the first place. Vary whether a D-state
holder is live on that controller at rebind time; that is the hypothesis under test.

---

### Task 3: Write down what was learned

**Files:**
- Modify: `lib/agentrc/skills/usb-kernel-recover/SKILL.md`

- [ ] **Step 1: Replace section 3b's scoping with the measured rule**

State the condition under which stranding occurs, with the journal lines. If the experiment
does NOT reproduce stranding, say that too, with the attempt count — "not reproduced in N
attempts" is a better record than an unexplained warning.

- [ ] **Step 2: If the boundary is detectable, guard the script**

For example, refuse `pci-rebind` when a D-state holder exists on that controller, since the
holder is enumerable from `/proc` and the controller from `readlink`. Only add this if the
experiment shows it predicts the outcome.

- [ ] **Step 3: Commit in agentrc, then re-pin**

The skill lives in agentrc now; `lib/agentrc` here is a pinned, ignored fetch. Commit the
change in an agentrc checkout and open its PR there, then bump the agentrc pin in
`tools/get_deps.py` from this repository once it lands.

```bash
# in the agentrc checkout
git add -- skills/usb-kernel-recover/
git commit -m "usb-kernel-recover: replace the pci-rebind stranding hypothesis with measurement"
```

---

## Abort criteria

Stop and hand back to the operator if: a rebind strands the controller and `pci-bind` does
not recover it; `uhubctl` starts hanging (the convoy has spread to the hub locks); or a CI
run starts while the rig is in a broken state.
