---
name: usb-kernel-debug
description: Use when USB enumeration fails or misbehaves and packet/URB capture can't explain WHY the Linux kernel acted — port reset storms, repeated re-enumeration, address errors, xHCI ring/command errors, "device descriptor read error", babble — on whichever end of the link runs Linux: the PC host when testing a TinyUSB device, or a Linux gadget peer (e.g. Raspberry Pi) when testing the TinyUSB host stack.
---

# usb-kernel-debug — Linux kernel dynamic debug for USB

Kernel **dynamic debug** shows the Linux side's *reasoning* that packet
capture can't: port resets and their causes, enumeration retries, address
(re)assignment, EP halts, xHCI ring/command errors. It applies wherever Linux
sits in the link — the rig PC when it is the host, or a Linux gadget peer
(dwc2/UDC + gadget modules) when TinyUSB is the host. It cannot see inside
the TinyUSB MCU — that is the `target-debug` skill.

Run this skill's `scripts/usb_dyndbg.sh` with `sudo`. It flips the dynamic-debug
print flag for an allowlisted set of USB modules only:

```bash
# all examples below abbreviate:  sudo .claude/skills/usb-kernel-debug/scripts/usb_dyndbg.sh
sudo usb_dyndbg.sh on  usbcore xhci_hcd   # enable +p; pick modules from `lsusb -t` Driver=
sudo usb_dyndbg.sh status [module]        # list enabled print sites
sudo usb_dyndbg.sh off usbcore xhci_hcd   # ALWAYS turn off when done — very noisy
```

Allowlisted modules: `usbcore xhci_hcd xhci_pci xhci_pci_renesas ehci_hcd
ehci_pci ohci_hcd ohci_pci uhci_hcd dwc2 dwc3 cdc_acm usb_storage uas
libcomposite udc_core` (`dwc2`/`dwc3` + the last two cover a Linux gadget
peer's device side).

## Workflow

1. `sudo usb_dyndbg.sh on usbcore <hcd-module>` — `usbcore` for enumeration/hub
   logic, plus the controller module (`lsusb -t` shows the driver per bus).
   On a gadget peer: `dwc2` (or `dwc3`) + `udc_core` + `libcomposite` instead —
   run on the peer itself (its SSH/serial console); the script is self-contained,
   copy it over or use the raw `dynamic_debug/control` writes from the `usbmon`
   skill.
2. Reproduce (replug / re-enumerate / rerun the failing test) while following
   `sudo dmesg -w` (or grab `sudo dmesg | tail` afterwards).
3. `sudo usb_dyndbg.sh off ...` — leaving it on floods the log and skews timing.

On a Linux-PC-host link, pair with the `usbmon` skill: usbmon for what crossed
the bus, dynamic debug for why the kernel reacted. A gadget peer's UDC has no
usbmon — pair with `usb-sniffer` on the wire instead. For a wedged device/bus
on the rig PC use the `usb-kernel-recover` skill.

Requires `CONFIG_DYNAMIC_DEBUG` and mounted debugfs (standard on distro kernels).
