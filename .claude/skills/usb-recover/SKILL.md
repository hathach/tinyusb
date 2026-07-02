---
name: usb-recover
description: Use when a USB device or fixture on the ci HIL rig is stuck, hung, not enumerating, or wedged after a failed flash or test, or when processes touching USB (testusb, JLinkExe, uhubctl, libusb tools) start hanging in D state.
---

# USB Recovery on the HIL Rig

`/usr/local/sbin/usb_recover.sh` (passwordless sudo on ci) wraps three sysfs reset
actions plus a resolver. Its built-in usage output is broken (prints shell code
instead of help); the real interface:

```bash
sudo usb_recover.sh resolve    /dev/ttyACM3    # /dev node -> busport (e.g. 3-4.7); also ttyUSB*, sg*
sudo usb_recover.sh authorized <busport>       # deauthorize+reauthorize: re-enumerate, no VBUS cut
sudo usb_recover.sh rebind     <busport>       # usb driver unbind+bind: re-probe
sudo usb_recover.sh pci-rebind <pciaddr>       # whole HCD controller unbind+bind, e.g. 0000:02:00.0
```

## Decide first: is anything stuck in D state?

```bash
ps -eo pid,stat,wchan:30,cmd | awk '$2 ~ /D/'
```

**If yes** (uninterruptible sleep, typically a usbfs ioctl — e.g. testusb inside
`usb_sg_wait`): **do not run this script.** All three write modes go through
orderly kernel paths that take the same per-device lock the stuck ioctl holds;
they block, join the lock convoy, and soon every libusb tool (uhubctl, JLinkExe)
hangs too. Only hardware-level URB death breaks the convoy:

```bash
# PCI function-level reset (real root; NOT covered by sudoers) — or reboot the rig
echo 1 | sudo tee /sys/bus/pci/devices/<pciaddr>/reset
```

Once the URBs die, the ioctl returns and any convoyed unbind/rebind writes
complete on their own.

**If no** (device merely dead or silent), escalate gently:

1. `authorized <busport>` — re-enumerates just that device
2. `rebind <busport>` — re-probe; also worth trying on the parent hub's busport
3. `pci-rebind <pciaddr>` — last resort: bounces every fixture on that controller

## Finding targets

```bash
grep -l <SERIAL> /sys/bus/usb/devices/*/serial          # serial -> busport (dir name)
readlink -f /sys/bus/usb/devices/usb<N>                  # bus N -> its PCI addr in the path
```

Rig layout: buses 3+4 = `0000:02:00.0` (main fixture tree: J-Links, ST-Links,
WCH-Links, DUTs); buses 9+12 = `0000:01:00.0`, the only ones with uhubctl port
power (ganged VBUS: `sudo uhubctl -l 9 -a cycle`). Hubs on buses 1-4 have no
port power switching — uhubctl reports "No compatible devices" there.

## Common mistakes

- `resolve` takes a **/dev node**, not a busport or serial ("no such device node").
- `authorized`/`rebind` take a **busport** (`3-4.7`); `pci-rebind` takes a **PCI addr**.
- Command produces no output and doesn't return → it is blocked on the device
  lock: a D-state holder exists; see above.
- A J-Link reset (`r; go`) does not disconnect a wedged DUT from the host: the
  DWC2 soft-connect pullup stays up through a core halt, so stuck URBs stay stuck.
