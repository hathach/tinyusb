---
name: usb-debug
description: Use when USB enumeration fails or misbehaves and usbmon alone can't explain WHY the host acted — port reset storms, repeated re-enumeration, address errors, xHCI ring/command errors, "device descriptor read error", babble, or when you need the host driver's own reasoning from dmesg on the ci HIL rig.
---

# usb-debug — host-side kernel dynamic debug for USB

usbmon shows the URBs; kernel **dynamic debug** shows the host driver's
*reasoning* usbmon can't: port resets and their causes, enumeration retries,
address (re)assignment, EP halts, xHCI ring/command errors.

`/usr/local/sbin/usb_dyndbg.sh` (passwordless sudo on ci) flips the dynamic-debug
print flag for an allowlisted set of USB host modules only:

```bash
sudo usb_dyndbg.sh on  usbcore xhci_hcd   # enable +p; pick modules from `lsusb -t` Driver=
sudo usb_dyndbg.sh status [module]        # list enabled print sites
sudo usb_dyndbg.sh off usbcore xhci_hcd   # ALWAYS turn off when done — very noisy
```

Allowlisted modules: `usbcore xhci_hcd xhci_pci xhci_pci_renesas ehci_hcd
ehci_pci ohci_hcd ohci_pci uhci_hcd dwc2 cdc_acm usb_storage uas`.

## Workflow

1. `sudo usb_dyndbg.sh on usbcore <hcd-module>` — `usbcore` for enumeration/hub
   logic, plus the controller module (`lsusb -t` shows the driver per bus).
2. Reproduce (replug / re-enumerate / rerun the failing test) while following
   `sudo dmesg -w` (or grab `sudo dmesg | tail` afterwards).
3. `sudo usb_dyndbg.sh off ...` — leaving it on floods the log and skews timing.

Pair with the `usbmon` skill: usbmon for what crossed the bus, dynamic debug for
why the host reacted. For a wedged device/bus use the `usb-recover` skill.

## Deploy (new rig / after reimage)

The script is versioned here; install + sudoers (needs a password once):

```bash
sudo install -m0755 -oroot -groot .claude/skills/usb-debug/scripts/usb_dyndbg.sh /usr/local/sbin/
sudo install -m0440 -oroot -groot test/hil/tinyusb-sudoer /etc/sudoers.d/tinyusb-sudoer
sudo visudo -c
```

Requires `CONFIG_DYNAMIC_DEBUG` and mounted debugfs (standard on distro kernels).
