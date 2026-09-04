---
name: usb-kernel-recover
description: Use when a USB device or fixture on a HIL rig's Linux host (ci.lan, hifiphile/tusb, a bench PC) is wedged, not enumerating, or when processes touching USB (testusb, JLinkExe, uhubctl, openocd, libusb tools) hang in D state. Linux-host side only — a bus owned by a TinyUSB host is out of reach.
---

# USB Recovery on the HIL Rig (Linux kernel side)

**The rule:** a wedged usbfs ioctl holds that device's `device_lock`
(`usbdev_do_ioctl` takes `usb_lock_device`, the uninterruptible variant —
v6.12.96 devio.c:2609) and the driver under it waits in a plain
`wait_for_completion()` with no timeout (usbtest.c:1404; `usb_sg_wait`,
message.c:765). Nothing that also takes that lock can help. Only two levers
don't: **failing the URB at the device** (rung 1) and **the port-side data-line
drop** (rung 2).

## 1. Triage: find the holder

```bash
ps -eo pid,stat,etimes,wchan:22,args | awk '$2 ~ /D/'
sudo cat /proc/<pid>/stack          # never opens the node, so it cannot block
```

- **`S` = victim.** Lock-taking sysfs *reads* use `usb_lock_device_interruptible`
  (sysfs.c:124-139, 11 sites), so readers are killable and `timeout` bounds them.
  Ignore them; they unwind by themselves.
- **`D` = the holder, or a writer that took the uninterruptible path.**

| Stack shows | Meaning | Go to |
|---|---|---|
| `usbdev_ioctl` + a driver module (`[usbtest]`) | **owner, holds the lock** | rung 3 — terminal |
| `usbdev_ioctl`, no driver frames | owner waiting on a URB | rung 1 (DUT) / rung 2 (probe) |
| `usbdev_open`, sysfs reads | victim | ignore |
| `tee .../usbtest/new_id`, `bind`, `unbind` | **victim that SPREADS it** | stop issuing them |
| `hub_event` in a kworker | teardown stuck behind an owner | rung 3 |

Driver-bind writes are not passive: `__device_driver_lock` (drivers/base/dd.c)
takes `device_lock()` uninterruptibly **and `device_lock(parent)`**, because
`usb_bus_type` sets `.need_parent_lock = true` (driver.c:2048) — each one holds
the HUB's lock, which is how one wedged port takes a whole bus down.

Map the holder to a busport with **lock-free attrs only** (`devnum`, `idVendor`,
`idProduct` are `usb_descriptor_attr*`, plain `sysfs_emit`, sysfs.c:688-705):

```bash
for d in /sys/bus/usb/devices/<bus>-*/; do
  [ "$(cat $d/devnum)" = "<devnum>" ] && echo "$d $(cat $d/idVendor):$(cat $d/idProduct)"
done
grep -l <SERIAL> /sys/bus/usb/devices/*/serial    # only on a HEALTHY device
```

## 2. Shield first (prerequisite for anything using libusb)

A wedged device blocks every enumerator that reads its locking attributes —
JLinkExe, uhubctl, openocd's HID fallback. `chmod 000` makes the VFS reject the
read before `->show()` runs, so they skip it and keep enumerating:

```bash
for f in bNumInterfaces bmAttributes bMaxPower configuration bConfigurationValue \
         product manufacturer serial avoid_reset_quirk; do
  sudo chmod 000 /sys/bus/usb/devices/<busport>/$f
done
```

- Shield the **leaf, its parent hub, and the root hub** (`usb<N>`) — a stuck
  uhubctl locks the root hub too.
- **Run the recovery tool as NON-root**: root has `CAP_DAC_OVERRIDE`, ignores the
  `000`, and blocks anyway.
- **Only those nine.** `descriptors`, `busnum`, `devnum`, `speed`, `idVendor`,
  `idProduct` are lock-free and libusb needs them; a blanket `chmod` breaks
  enumeration instead of fixing it.
- `chmod` never blocks (inode setattr, no `show()`), so it works on a fully
  wedged device.
- **Not needed for openocd pinned with `vid_pid`** — it matches the cached
  descriptor and skips a foreign device before `libusb_open`
  (cmsis_dap_usb_bulk.c:107, bulk backend; the HID fallback ignores the pin).
- Leaf shields vanish on re-enumeration; **the root hub's must be restored**:
  `sudo chmod "$(stat -c %a /sys/bus/usb/devices/usb<healthy>/$f)" …/usb<N>/$f`

## 3. The rungs — go straight to the one triage names

**Rung 1 — wedged DUT: reset it through its own probe.**

```bash
printf "r\ng\nq\n" > /tmp/rec.jlink
JLinkExe -device <DEV> -if SWD -speed 4000 -SelectEmuBySN <probe-sn> \
         -autoconnect 1 -nogui 1 -CommandFile /tmp/rec.jlink
```

Reset **before** park-flash: non-destructive (the firmware under test survives
for autopsy), no flash wear, and no bad park image — a `wfe`/`wfi` park has
bricked SWD on mimxrt1064_evk and max32666fthr through a power cycle.
`ResetTarget` measures 128-129 ms; cleared 57 → 0, 26 → 0 and 5 → 0 D-state
processes, single shot each. Mechanism: chip reset drops the pull-up →
`usb_hcd_flush_endpoint` unlinks the URB `-ESHUTDOWN` (hcd.c:1783) → the
completion fires → the ioctl returns → the lock releases.

Works on i.MX RT (`USBCMD.RS` = 0 detaches, RT1050 RM Rev 3 p.2453) **and on
DWC2** — measured 2026-08-16 on stm32f407disco: `r; g` gave
`usb 13-2.2: USB disconnect, device number 107`, re-enumerating 325 ms later.
(A bare **halt** does not: the core keeps running with the pull-up asserted.)

**Park-flash** (`--recover-board`/`--recover-fw`, what `usbtest.py` automates) is
the fallback where the reset cannot reach the peripheral. Delivery must be
convoy-safe: **openocd pinned with `vid_pid`**, or esptool (`-p <ttyACM>`).
JLinkExe selects by serial, which needs `libusb_open`, so it needs the shield.

**Rung 2 — wedged PROBE: `root-cycle`.** A probe has no probe to reset it, so the
port-side drop is the only lever left that avoids the KERNEL device lock. It
commands the ROOT hub and never touches the wedged device's `device_lock` — which
is exactly why rungs 1 and 3 are dangerous and this one is not.

That is a different lock from the rig's **board flocks**, and this rung still needs
those: it bounces every fixture under the root port, including boards another job is
mid-flash on. Take them first, and release after:

```bash
python3 test/hil/helper/hil_lock.py hold --all --config <this host's config> --reason "root-cycle <busport>"
sudo .claude/skills/usb-kernel-recover/scripts/usb_recover.sh root-cycle <busport> [expected-serial]
python3 test/hil/helper/hil_lock.py release --all      # no --config: it walks the lock dir
```

`--all` is coarse for one root port, but nothing maps a sysfs busport to a board name,
so it is the only reservation that actually covers the blast radius; `hold` accepts any
string, so a hand-listed "just the siblings" hold reserves nothing while reporting
success. A refusal naming `hil_test.py` means CI is mid-test — wait, do not force. Give
the script the wedged probe's own busport (e.g. `13-1.6`), not the `13-1` hub path: it
derives the root port itself, and the expected-serial guard and the success check both
read the path you pass.

Bounces **every fixture under that root port** (up to 25 here). The Renesas cards
advertise `ppps` but do not implement it: VBUS stays up and only D+/D− drop, so
this is a forced re-enumeration, never a power cycle — a device whose firmware is
wedged can ride it out. Success is the sysfs inode changing, not uhubctl's exit code.

**Rung 3 — terminal case: a driver ioctl that OWNS the lock.** No software cure:
the task is uninterruptible and SIGKILL is queued, not delivered. Reboot with
**sysrq**, never `reboot(2)` — a graceful reboot runs `device_shutdown()`, which
takes every device lock and stalls on the wedged one.

```bash
echo b | sudo tee /proc/sysrq-trigger     # after: sync; sudo umount -a
```

**Rung 4 — hypervisor.** ci.lan only, and never needed in eight recorded wedges:
`qm stop <vmid> && qm start <vmid>` from the PVE host. A VM *reboot* is not
reliable — hubs can latch across the PCIe reset.

## 3b. If the CONTROLLER is dead, not a device

Signature: `xhci-pci-renesas <addr>: Timeout while waiting for setup device
command`, devices on that controller failing to enumerate, or its buses gone —
as opposed to ONE device wedged. The rungs above cannot help; the controller
itself needs re-initialising.

```bash
sudo usb_recover.sh pci-rebind <pciaddr>     # unbind + bind the whole xHCI
sudo usb_recover.sh pci-bind   <pciaddr>     # only if it ends up driverless
```

Measured on ci.lan 2026-08-17 02:34:41 after a `hub-cycle` failed to take: unbind
deregistered buses 17 and 18, the re-bind registered new buses **1 and 2** one
second later, and every fixture re-enumerated. **It renumbers every bus that
controller owns**, so hold all affected boards' locks first (`hil_lock.py hold
--all`) and re-derive busports afterwards.

Do NOT reach for it while a device-lock convoy is live — see Common mistakes.

## 4. If nothing is in D state

The device is dead or silent, not wedged. `sudo usb_recover.sh authorized
<busport>` unconfigures and reconfigures it (`usb_set_configuration(dev, -1)`
then re-choose, hub.c) — it fixes stale driver/interface state, does **not**
replug: the `usb_device` survives, so most probes keep their sysfs node. If that
does not take, the device is wedged rather than silent — go to rung 1 or 2.
`resolve <dev-node>` maps `/dev/ttyACM3` → busport.

**It takes `usb_lock_device` uninterruptibly** (hub.c `usb_deauthorize_device`),
so it is safe only while nothing is in D state.

## 5. Before declaring the rig healthy

```bash
ps -eo stat,args | awk '$1 ~ /D/' | wc -l    # must be 0
timeout 15 lsusb                             # rc 0 and a sane device count
sudo uhubctl -l <bus> -p <port>              # "0000 off" = never came back
sudo uhubctl -l <bus> -p <port> -a on
```

Observed: 5 boards missing with a completely clean D-state list, because
`usb17-port2` sat at `disable=1`.

## Common mistakes

- **`uhubctl -a cycle` on a root port without `-S`.** It writes sysfs
  `disable`, and `disable_store` takes `usb_lock_device(hdev)` uninterruptibly
  then calls `usb_disconnect(child)` inside it (port.c) — against a wedged child
  that blocks while holding the root hub's lock, poisoning the bus.
  `usb_recover.sh` passes `-S`.
- **`echo 1 > .../remove`** to make a wedged device "go away": `remove_store` is
  the one attribute in sysfs.c taking the uninterruptible `usb_lock_device`
  (sysfs.c:765). It joins the convoy instead of clearing it.
- **`authorized` on anything wedged** — same uninterruptible lock. Driver
  unbind/bind (`/sys/bus/usb/drivers/usb/{unbind,bind}`) does the same
  unconfigure/reconfigure via `usb_generic_driver_disconnect` (generic.c) but ALSO
  takes the parent hub's lock (`need_parent_lock`), so it is strictly worse; it was
  removed from `usb_recover.sh` for that reason.
- **`pci-rebind` for a wedged DEVICE.** It is the cure for a dead CONTROLLER (see
  below), not for a device-lock convoy: with a live D-state URB the re-bind can
  hang and leave the controller with **no driver** and every fixture offline
  (observed once). Recover that with `pci-bind <addr>`.
- **Writing `/sys/bus/pci/devices/<addr>/reset`** — no rig controller has FLR, so
  it becomes a bus reset behind a live driver: card halted, host power cycle.
- **Resetting a victim's board.** Two boards were reset innocently before anyone
  found the holder. Map by `devnum`, not by which board "should" be running.
- **Assuming one controller.** Observed: 26 D-state processes across three xHCI
  controllers, all cleared by one probe reset on one device.

## Rig layout (ci.lan, bus numbers renumber every boot)

`readlink -f /sys/bus/usb/devices/usb<N>` → its PCI address; `sudo uhubctl` lists
the root hubs it can drive, against their PCI address. Five Renesas uPD720201 cards
— `0000:01:00.0`, `03`, `04`, `05`, `06:00.0` — advertise per-port `ppps` on both
their USB2 and USB3 root hubs, **but do not implement it**: the silicon never drops
VBUS, so a cycle re-enumerates the port and nothing more (above). Do not read
`uhubctl`'s `ppps` as power control on this rig. AMD `0000:02:00.0` does not appear
in `uhubctl` at all — no switching of any kind. Which tree holds which probes moves
with re-cabling, so derive it (`lsusb -s <bus>:`) rather than trusting a stored map.
Verified 2026-08-18; `sudo` is passwordless for `hathach` here, so every rung above
runs without a prompt.
