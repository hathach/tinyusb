# HIL fleet-wedge containment

Date: 2026-07-30
Status: implemented, then superseded in part — addendum last checked 2026-08-12
against the shipped code; where they disagree the CODE and the usb-kernel-recover
skill win, never this document.

- **Pool guard.** A single constant, not the flat 4200s below and not a derivation:
  `POOL_TIMEOUT = pos_int_env('HIL_POOL_TIMEOUT', 3600)`. A per-controller model briefly
  lived here and was removed -- it under-modelled the flash phase and could INVERT
  (adding a usbtest board lowered the guard, because the derived value fell below the
  baseline it was meant to raise). The guard's only job is to stop a wedged pool short
  of the job ceiling so the report still gets written; predicting a healthy run's
  duration is a different problem. `pos_int_env` warns only on a non-integer or a value
  <= 0: there is NO upper clamp and no warning above any threshold, so a pin larger than
  a job ceiling silently restores the inversion this work removed.
- **Job ceilings.** 90/90/120 min (build.yml), not 60/60/90 and not the 85/115 below.
  They must clear the 3600s guard plus the pre-pool checkout/artifact merge and the
  post-guard sweep and report upload. No job pins `HIL_POOL_TIMEOUT`.
- **Battery budgets.** `USBTEST_BATTERY_BUDGET` 260s, `USBTEST_RECOVERY_BUDGET` 250s.
  The 200s-with-a-197s-floor derivation recorded here was never shipped; the floor
  assertion was removed with it.
- **HUNG recovery.** Reflash of the DUT through its roster flasher
  (`usbtest.py --recover-board/--recover-fw`), not the root-cycle-first recovery in
  section 1d — replaced after the 2026-08-11 ppps measurement (uhubctl never cuts
  VBUS; root-cycle is probe-only). Since 2026-08-12 the reflash is SKIPPED
  when `hil_flash.convoy_safe(board['flasher'])` is false (usbtest.py:675): the flasher
  would enumerate by opening usbfs nodes, block on the same convoy, and become a second
  stray rather than clear the first. A holder that owns the device lock inside a driver
  ioctl is terminal either way -- a reflash only produces a disconnect, and
  `usb_disconnect()` needs that same lock -- and that state needs a reboot.

Step 0 done — the host was rebooted 2026-07-30 14:11 and the rig
came back clean. The device that triggered this incident was removed from the rig, so
only the containment work remains relevant.
Rig: `ci.lan` (Proxmox guest on `pve.lan`)

## Problem

On 2026-07-29/30 every board in the `ci.lan` usbtest fleet failed, `openocd` processes
landed in uninterruptible sleep, and no subsequent HIL run could start. Two GitHub
Actions runs were stranded: `30484641269` sat `in_progress` for over eight hours
(past GitHub's own 360-minute default), and `30485082274` sat `queued` behind it from
2026-07-29 19:35 UTC onward. Both report directories were written empty.

A reboot of the `ci` guest at 10:48 did not clear the condition: the same kernel state
re-formed at 10:52:23.

## Root cause

Five layers, each independently observable.

### 1. A permanently wedged hub worker holds a root-hub device lock

A device that repeatedly re-asserts connect while failing to enumerate keeps
`hub_event()` busy, and `hub_event()` holds `usb_lock_device(hdev)` on its hub for its
whole run (hub.c:5896/5989). The `usb_hub_wq` worker sits in `hub_port_reset`, so that
hub's `device_lock` is effectively never released:

```
kworker/14:6+usb_hub_wq  (state D, 400+ s)
  msleep+0x2b
  hub_port_reset+0x1a4 [usbcore]
  hub_event+0x727 [usbcore]
```

`usb usbN-portM: Cannot enable. Maybe the USB cable is bad?` is logged every four seconds
for as long as it lasts.

Verified against hub.c v6.12.96 rather than inferred: the kernel does **not** retry
without bound, and root and downstream ports are bounded identically —
`hub_port_reset()` tries `PORT_RESET_TRIES` then logs that message (hub.c:3149),
`hub_port_connect()` wraps it in `PORT_INIT_TRIES` = 4 and disables the port on give-up
(hub.c:5455/5619). A count in the thousands is therefore that many separate connect
events, not one runaway loop, and it indicts the device rather than the port.

### 2. A parked board storms the second controller

`ra6m5_ek` (`test/hil/tinyusb.json`, uid `8419032D32363657364EF4622D294B4E`, at
`13-3.3`) runs dfu firmware (`cafe:400b`) and re-enumerates every 1-2 seconds
continuously, wrapping the entire bus-13 devnum space (`...120 -> 127 -> 4 -> 6 -> 10`).
This is standing `hub_event` and Address-Device pressure on controller `03:00.0`,
concurrent with parallel usbtest batteries on the same silicon.

The board is already listed in `boards-skip`, which is precisely why it storms:
`boards-skip` stops testing a board but never parks it, so it keeps running whatever
firmware it last received. Park-flash only runs as teardown of a board that actually
executed tests.

### 3. The kernel `usbtest` control-queue case waits without a timeout

`test_ctrl_queue` blocks on an untimed `wait_for_completion()` while `usbdev_ioctl`
holds the DUT's `device_lock`:

```
wait_for_completion+0x8a          <- no _timeout variant
test_ctrl_queue+0x4ab [usbtest]
usbtest_do_ioctl+0x501 [usbtest]
usbdev_ioctl+0x6b8 [usbcore]
```

`--timeout 60` in `test/hil/usbtest.py` is a subprocess timeout only. `SIGKILL` is not
delivered to a task in uninterruptible sleep. `usbtest.py` already recognises this and
reports `HUNG`, then calls `usb_recover.sh root-cycle`.

### 4. openocd inherits the convoy and the whole fleet dies

Once a device lock is stuck, `port_event()` takes a child device's lock to warm-reset
it and blocks while still holding its hub's lock. Any later
`open("/dev/bus/usb/BBB/DDD")` against such a device blocks uninterruptibly:

```
usbdev_open+0xdc [usbcore]  ->  __mutex_lock
chrdev_open  ->  do_sys_openat2  ->  __x64_sys_openat
```

That is the state of the three `openocd` processes at 04:16:51 (pids 207921, 207987,
208034) — the flasher, unkillable. Because one controller carries two buses, a single
convoy takes out every board on both, which is why the failure presents as the entire
fleet.

The existing `HUNG` recovery cannot help here. A root-port VBUS cycle frees a
*device-lock* holder; it cannot free a lock held by a stuck *hub worker*, and on this
rig the cycle lands on the controller that is already wedged.

### 5. Nothing bounds the damage, so one bad run becomes a CI outage

- `hil-tinyusb` and `hil-tinyusb-esp` in `.github/workflows/build.yml` carry no
  `timeout-minutes`. Only `hil-hfp-iar` does.
- `ci.lan` runs a single runner service, so there is one job slot.
- `test/hil/hil_test.py` bounds the pool with `POOL_TIMEOUT` (4200 s), and that guard
  fires correctly — but the recovery path does not survive a D-state worker:

```python
with Pool(processes=os.cpu_count() or 1, initializer=init_worker, initargs=initargs) as pool:
    async_ret = pool.map_async(test_board, config_boards)
    try:
        mret = async_ret.get(timeout=POOL_TIMEOUT)
    except MpTimeoutError:
        pool.terminate()
        pool.join()      # blocks forever: a D-state worker never reaps
        raise RuntimeError(f'HIL worker pool timed out after {POOL_TIMEOUT}s')
```

`multiprocessing` joins workers unbounded, so both `pool.terminate()` and
`pool.join()` hang, as does the `with Pool(...)` exit on the success path. Normal
`hil-tinyusb (tinyusb.json)` runs take 10-20 minutes; one recent run took 71.3
minutes, which is the 70-minute guard firing and succeeding. The eight-hour run is the
pathological case.

## Design

### Step 0 — recovery (manual prerequisite)

Power-cycle the PVE **host**, not the `ci` guest. A guest reboot is not sufficient;
hubs latch up across the PCIe reset, which the 10:48 reboot demonstrated. Nothing
below can be verified until the rig is clean.

### Section 1 — CI containment

**1a. Two layered timers.** An inner guard inside `hil_test.py` (`POOL_TIMEOUT`, 70 min)
that fails gracefully -- it writes a report naming the timeout and the dispatched boards,
shuts the pool down and exits -- and an outer `timeout-minutes` per rig job (85 for the
hil-tinyusb jobs; 115 for hil-hfp-iar, which also builds four boards with IAR in the same
job) as the backstop for when even exiting cannot free the runner. The ceiling must stay
ABOVE the inner guard, or GitHub kills the job before the report is written.

> **Corrected after measurement.** An earlier revision cut the guard to 30 min on the
> reading that real runs take 9-17 min and everything longer was the old guard firing.
> That was wrong. `hil_lock.py` records 22.2/14.3/12.5/10.8 min at usbtest width 1/2/3/4,
> and raising the per-battery budget to 380s made hung boards cost more again. The 30 min
> guard then fired on 5 of the last 8 HIL job executions across both rigs, and because
> `map_async` is all-or-nothing each of those runs published a banner instead of any
> per-board result. Restored to 4200s, the value whose original rationale -- usbtest
> batteries are serialized fleet-wide, lengthening the tail -- was correct.

**1b. Bound the pool shutdown.** Add a helper to `test/hil/hil_test.py`:

```python
def _shutdown_pool(pool, grace=30):
    """terminate() a Pool without ever blocking forever: multiprocessing joins its
    workers unbounded, and a worker in uninterruptible sleep (wedged usbfs) never
    reaps -- which would hold the runner's only job slot indefinitely."""
    t = threading.Thread(target=pool.terminate, daemon=True)
    t.start()
    t.join(grace)
    return not t.is_alive()
```

On the `MpTimeoutError` path: write the report first, recording the boards that never
reported so the run stops producing an empty report directory; then `_shutdown_pool`;
then `os._exit(1)` if it did not return. The hard exit is the point — it is the only
way past a kernel-side unkillable child. Use the same helper for the `with Pool(...)`
exit path.

**1c. Pre-flight rig health check.** `check_rig_health()` runs before the build and
**never aborts**. It probes `/proc` unprivileged (dmesg is restricted on the rig) for a
wedged `usb_hub_wq` worker, and reports a `/proc` too restricted to trust as its own
distinct cause rather than as a diagnosed fault.

It is deliberately non-fatal: the rig is unattended and every remedy for a real wedge is
manual, so aborting would not fix anything -- it would discard the per-board results the
run can still collect and leave CI red until a human noticed. It emits a GitHub
`::error::` annotation and continues. The automatic containment is 1a and 1b, which bound
a stuck run and explain it without anyone touching the rig.

**1d. Order the recovery correctly.** In `test/hil/usbtest.py`, attempt
`usb_recover.sh root-cycle` FIRST on a `HUNG` case, and only check for a wedged hub worker
*afterwards*.

> **Corrected during implementation.** This section originally said to check for a wedged
> worker *before* the cycle and skip it on a hit. That is backwards. Our own stuck
> `testusb` holds the DUT's device lock, so any port event drives a hub worker into
> `usb_lock_device()` on it -- uninterruptible, so it reads `D` in ~100% of samples and the
> confirmation window makes the wrong verdict *more* confident, not less. Cutting VBUS is
> precisely what completes the in-flight URB, returns the ioctl and frees that worker, so
> gating on that signature would suppress the recovery in the exact ordering it exists for.
> A worker still wedged after the cycle is the genuinely unrecoverable case, and that is
> what the code now reports.

## Verification

- Unit-test `shutdown_pool` and the `hil_health` detectors against a synthetic `/proc`.
  A real wedge cannot be manufactured on demand, so they are tested against fabricated
  inputs rather than live hardware.
- Confirm the detectors flag a genuinely wedged rig, and return clean on a healthy one.
- One clean full-fleet `hil_test.py` run to prove `check_rig_health` does not
  false-abort.

## Out of scope

- **`ra6m5_ek` park and its dfu reset loop.** Dropped by decision. Consequence: the
  layer-2 devnum storm remains as standing pressure on controller `03:00.0`. Unplugging
  the board or flashing `board_test` by hand resolves it without any code change.
- **An unattended PVE watchdog** that detects the wedge and power-cycles the host.
  Declined: more moving parts, and it can cut a running CI job.
