---
name: usb-recover
description: Use when a USB device or fixture on the ci HIL rig is stuck, hung, not enumerating, or wedged after a failed flash or test, or when processes touching USB (testusb, JLinkExe, uhubctl, libusb tools) start hanging in D state.
---

# USB Recovery on the HIL Rig

`/usr/local/sbin/usb_recover.sh` (passwordless sudo on ci; versioned copy in this
skill's `scripts/`) wraps four sysfs reset actions plus a resolver:

```bash
sudo usb_recover.sh resolve    /dev/ttyACM3    # /dev node -> busport (e.g. 3-4.7); also ttyUSB*, sg*
sudo usb_recover.sh authorized <busport>       # deauthorize+reauthorize: re-enumerate, no VBUS cut
sudo usb_recover.sh rebind     <busport>       # usb driver unbind+bind: re-probe
sudo usb_recover.sh pci-rebind <pciaddr>       # whole HCD controller unbind+bind, e.g. 0000:02:00.0
sudo usb_recover.sh pci-reset  <pciaddr>       # PCI function-level reset: kills URBs at HW level, no device lock
sudo usb_recover.sh pci-bind   <pciaddr> [drv] # re-bind a DRIVERLESS controller (auto-tries xHCI drivers)
```

## Decide first: is anything stuck in D state?

```bash
ps -eo pid,stat,wchan:30,cmd | awk '$2 ~ /D/'
```

**If yes** (uninterruptible sleep, typically a usbfs ioctl — e.g. testusb inside
`usb_sg_wait`): run `pci-reset` and NOTHING ELSE first:

```bash
sudo usb_recover.sh pci-reset <pciaddr>
```

FLR kills the URBs at the hardware level without taking the per-device lock;
the ioctl then returns and the convoy unwinds on its own.

**Not every controller supports FLR.** The Renesas uPD720201 (`0000:01:00.0`)
has no reset method — `pci-reset` fails with `Inappropriate ioctl for device`
(ENOTTY). On those, there is no clean D-state cure short of a **reboot**; do NOT
fall through to `pci-rebind` (see next).

**`pci-rebind` can strand the controller driverless.** Its unbind succeeds but,
with a D-state process still holding a URB, the *re-bind* hangs — leaving the
PCI device with **no driver** (`/sys/bus/pci/devices/<addr>/driver` gone) and the
whole controller's fixtures offline. A second `pci-rebind` then dies with "no
driver bound". Recover with `pci-bind <addr>` (re-attaches the xHCI driver);
if that also hangs because the D-state URB is unkillable, **reboot** is the only
cure. The Renesas binds via `xhci-pci-renesas` (firmware loader), others via
`xhci_hcd` — `pci-bind` auto-tries both, or pass the driver explicitly.

**Ordering is critical.** `authorized`/`rebind`/`pci-rebind` all take the
per-device lock the stuck ioctl holds — they block and join the convoy, and
soon every libusb tool (uhubctl, JLinkExe) hangs too. Worse, a blocked
`pci-rebind` grabs the PCI device lock on its way in, which `pci-reset` also
needs: once a rebind has been attempted and is stuck, even FLR deadlocks and
**only a rig reboot recovers**. pci-reset first (if supported), and never
`pci-rebind` a D-state wedge.

### Last resort: reboot (self-service, auto-resumes the session)

When an unkillable D-state URB has stranded a controller and `pci-reset`/`pci-bind`
can't revive it, reboot the rig. Use **`--force`**: a graceful reboot blocks in
shutdown waiting for the wedged D-state process to terminate (it never does), so
plain `systemctl reboot` stalls indefinitely — `--force` terminates services and
reboots immediately. It's sudoers-granted, so do it without waiting on the operator:

```bash
sudo -n systemctl reboot --force
```

**Before rebooting, arm a one-shot auto-resume** so your session picks the work
back up after boot instead of stalling:

```bash
cat > "$HOME/.claude-resume-after-reboot.sh" <<'EOS'
#!/usr/bin/env bash
exec >> "$HOME/.claude-resume-after-reboot.log" 2>&1; date
crontab -l 2>/dev/null | grep -v claude-resume-after-reboot | crontab - 2>/dev/null || true  # one-shot
sleep 25
export PATH="$HOME/.local/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
cd "$HOME/code/tinyusb/.claude/worktrees/usbtest" || exit 1
tmux new-session -d -s claude-resume \
  "bash -lc \"claude --resume <SESSION_ID> --dangerously-skip-permissions 'just rebooted to recover usb, continue your work'\""
EOS
chmod +x "$HOME/.claude-resume-after-reboot.sh"
( crontab -l 2>/dev/null; echo "@reboot $HOME/.claude-resume-after-reboot.sh" ) | crontab -
```

Fill `<SESSION_ID>` from the newest transcript
(`ls -t ~/.claude/projects/-*/*.jsonl | head -1`). The script deletes its own
`@reboot` entry on first run (fires once). After boot: `tmux attach -t claude-resume`.
Note `/tmp` (build scratchpad) is wiped by the reboot — the resumed session rebuilds.
The `bash -lc "..."` wrapper is required: tmux runs its command through `/bin/sh`
(dash on Debian), so without it `claude` starts under dash with a different
PATH/profile than every Bash tool call — always force a bash login shell here.

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
- `authorized`/`rebind` take a **busport** (`3-4.7`); `pci-rebind`/`pci-reset`
  take a **PCI addr**.
- Command produces no output and doesn't return → it is blocked on the device
  lock: a D-state holder exists; see above.
- Trying `pci-rebind` on a D-state hang — its re-bind hangs and strands the
  controller **driverless**; recover with `pci-bind <addr>`, or reboot if the
  D-state URB is unkillable. Use `pci-reset` (if supported) for D-state, never
  `pci-rebind`.
- Running `pci-reset` on a controller without FLR support (Renesas) → ENOTTY;
  no recovery but reboot.
- A J-Link reset (`r; go`) does not disconnect a wedged DUT from the host: the
  DWC2 soft-connect pullup stays up through a core halt, so stuck URBs stay stuck.

## Deploy (new rig / after reimage)

The script is versioned in this skill's `scripts/`; install + sudoers (needs a
password once):

```bash
sudo install -m0755 -oroot -groot .claude/skills/usb-recover/scripts/usb_recover.sh /usr/local/sbin/
sudo install -m0440 -oroot -groot test/hil/tinyusb-sudoer /etc/sudoers.d/tinyusb-sudoer  # grants target #1000
sudo visudo -c
```
