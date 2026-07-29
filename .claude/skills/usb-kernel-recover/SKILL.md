---
name: usb-kernel-recover
description: Use when a USB device or fixture attached to the ci HIL rig's Linux host is stuck, hung, not enumerating, or wedged after a failed flash or test, or when processes touching USB (testusb, JLinkExe, uhubctl, libusb tools) start hanging in D state. Linux-kernel-side only — a bus owned by a TinyUSB host is out of reach (reset the target / cycle its VBUS instead); the rig's probes and serial fixtures always remain in scope.
---

# USB Recovery on the HIL Rig (Linux kernel side)

Run this skill's `scripts/usb_recover.sh` with `sudo`. It wraps the sysfs reset
actions, a uhubctl power-cycle escalator, and a resolver:

```bash
# all examples below abbreviate:  sudo .claude/skills/usb-kernel-recover/scripts/usb_recover.sh
sudo usb_recover.sh resolve    /dev/ttyACM3    # /dev node -> busport (e.g. 3-4.7); also ttyUSB*, sg*
sudo usb_recover.sh authorized <busport>       # deauthorize+reauthorize: re-enumerate, no VBUS cut
sudo usb_recover.sh rebind     <busport>       # usb driver unbind+bind: re-probe
sudo usb_recover.sh hub-cycle  <busport>       # uhubctl VBUS cycle of the feeding port, walking parent hub
                                               # -> root port until the device re-enumerates
sudo usb_recover.sh root-cycle <busport> [serial]  # uhubctl VBUS cut straight at the ROOT port (real ppps), no
                                               # leaf walk, no device-lock touch: the D-state cure.
                                               # [serial] is checked and a mismatch refused.
sudo usb_recover.sh pci-rebind <pciaddr>       # whole HCD controller unbind+bind, e.g. 0000:02:00.0
sudo usb_recover.sh pci-bind   <pciaddr> [drv] # re-bind a DRIVERLESS controller (auto-tries xHCI drivers)
```

`hub-cycle` caveats: leaf hubs that gang (or fake) port power switching bounce
**all siblings** on that hub when cycled; a **self-powered** leaf hub keeps
downstream VBUS up, so cycling it only resets its uplink — that's why the walk
escalates to the root port, where the Renesas cards' per-port power (ppps) is
real. A device that is wedged but bus-powered from a switching hub gets a true
power cycle; one on a self-powered hub may only get a re-enumeration.

## Decide first: is anything stuck in D state?

```bash
ps -eo pid,stat,wchan:30,cmd | awk '$2 ~ /D/'
```

**If yes** (uninterruptible sleep, typically a usbfs ioctl — e.g. testusb inside
`usb_sg_wait`): cut VBUS at the root port, and nothing else.

```bash
sudo usb_recover.sh root-cycle <busport>     # e.g. 11-3.7 -> cycles bus 11 root port 3
```

This drops power to the wedged device, so its in-flight URB fails and the ioctl
returns. It targets the *root hub* — a different USB device from the wedged one —
and never *writes* the wedged device's sysfs. It reads a few attributes from it —
`idVendor`/`idProduct`/`serial`/`product` to report and check the target, and the
directory inode plus `devnum` afterwards — none of which take the device lock, so
it does not join the convoy the way `authorized`/`rebind`/`pci-rebind` do.
Recovery is proven by that inode changing — a real disconnect destroys the
kobject and reconnecting creates a new one, whereas a disconnect blocked on the
device lock leaves it untouched. It exits non-zero if the device does not come
back; a **zero exit only means it re-enumerated**, so still confirm the D-state
process actually let go. Pass the expected serial as a third argument and it
refuses a busport that now names a different device.

It bounces **every fixture under that root port** — on ci that is up to 25
devices. Hold the affected boards' locks first if you can, but note
`hil_lock.py` uses `LOCK_EX | LOCK_NB` and so fails immediately when CI already
holds them; there is no wait-for-lock. When CI is mid-run you are choosing
between bouncing its fixtures and leaving the bus wedged for everything. The
automated path in `usbtest.py` takes no locks at all and accepts that collateral
deliberately: by the time a D-state wedge exists the convoy will take the bus
down anyway.

(The VBUS mechanism is verified on the ci rig — the leaf hubs report
`bmAttributes=e0`, "self-powered", but are physically bus-powered with no adapter,
so a root-port cut really does kill downstream power. Do not re-derive this from
the descriptor; it lies. Not yet confirmed against a live D-state wedge. If
`uhubctl` itself hangs, the convoy has already spread — escalate.)

If `root-cycle` does not free the D-state process, there is no software cure
left: ask the operator for a full PVE **host** power cycle. A VM reboot is NOT
reliable (downstream hubs can latch up across the PCIe reset and need a physical
replug), and a graceful reboot stalls on the D-state process anyway. Do NOT fall
through to `pci-rebind` (see next).

**`pci-rebind` can strand the controller driverless.** Its unbind succeeds but,
with a D-state process still holding a URB, the *re-bind* hangs — leaving the
PCI device with **no driver** (`/sys/bus/pci/devices/<addr>/driver` gone) and the
whole controller's fixtures offline. A second `pci-rebind` then dies with "no
driver bound". Recover with `pci-bind <addr>` (re-attaches the xHCI driver);
if that also hangs because the D-state URB is unkillable, only a full PVE host
power cycle (operator action) recovers. The Renesas binds via `xhci-pci-renesas` (firmware loader), others via
`xhci_hcd` — `pci-bind` auto-tries both, or pass the driver explicitly.

**Ordering is critical.** `authorized`/`rebind`/`pci-rebind` all take the
per-device lock the stuck ioctl holds — they block and join the convoy, and
soon every libusb tool (uhubctl, JLinkExe) hangs too. Worse, a blocked
`pci-rebind` grabs the PCI device lock on its way in and can wedge the whole
function, after which **only a full PVE host power cycle recovers**. `root-cycle`
first, and never `pci-rebind` a D-state wedge.

**If no** (device merely dead or silent), escalate gently:

1. `authorized <busport>` — re-enumerates just that device
2. `rebind <busport>` — re-probe; also worth trying on the parent hub's busport
3. `hub-cycle <busport>` — VBUS cycle of the feeding port, walking up to the
   root port; may bounce sibling fixtures on ganged hubs
4. `pci-rebind <pciaddr>` — last resort: bounces every fixture on that controller

## Finding targets

```bash
grep -l <SERIAL> /sys/bus/usb/devices/*/serial          # serial -> busport (dir name)
readlink -f /sys/bus/usb/devices/usb<N>                  # bus N -> its PCI addr in the path
```

Rig layout (2026-07-15, two Renesas uPD720201 cards; bus numbers renumber every
boot — re-derive with `readlink`): AMD `0000:02:00.0` = the debug-probe tree
(J-Links, ST-Links, WCH-Links), no port power switching; Renesas `0000:01:00.0`
and `0000:03:00.0` = DUT device hubs + serial fixtures, and ALL their root-hub
ports have real per-port power (`ppps`, 4+4 each) — `sudo uhubctl -l <bus> -p
<port> -a cycle` cuts VBUS to the leaf hub on that port. The 1a40:0201 leaf
hubs themselves claim "ganged" switching but do not actually cut power.

## Common mistakes

- `resolve` takes a **/dev node**, not a busport or serial ("no such device node").
- `authorized`/`rebind`/`hub-cycle`/`root-cycle` take a **busport** (`3-4.7`);
  `pci-rebind`/`pci-bind` take a **PCI addr**.
- Command produces no output and doesn't return → it is blocked on the device
  lock: a D-state holder exists; see above.
- Trying `pci-rebind` on a D-state hang — its re-bind hangs and strands the
  controller **driverless**; recover with `pci-bind <addr>`, or a PVE host power
  cycle if the D-state URB is unkillable. Use `root-cycle` for D-state, never
  `pci-rebind`.
- Writing `/sys/bus/pci/devices/<addr>/reset` because the attribute is there. No
  rig controller has FLR, so it becomes a PCIe bus reset that resets the xHCI
  behind its live driver — the write succeeds, the card is halted for good, and
  only a PVE host power cycle brings it back. Use `root-cycle`.
- `root-cycle` bounces **every** fixture under that root port, not just the target
  — hold the sibling boards' locks first.
- A J-Link reset (`r; go`) does not disconnect a wedged DUT from the host: the
  DWC2 soft-connect pullup stays up through a core halt, so stuck URBs stay stuck.
