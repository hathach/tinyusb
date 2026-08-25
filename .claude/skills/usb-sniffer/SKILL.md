---
name: usb-sniffer
description: Use when you need wire-level USB evidence that host-side capture can't provide — a device that never enumerates (usbmon shows nothing or only Submits), suspected NAK storms/STALL/babble/bad handshakes, bus-reset or enumeration timing, split-transaction issues, a usbmon-vs-device-log disagreement the wire must arbitrate, or any link where TinyUSB is the host (no Linux PC host to run usbmon on). Captures LS/FS/HS packets (PIDs, tokens, handshakes, SE0/line states) with the ataradov usb-sniffer hardware into Wireshark pcapng.
---

# usb-sniffer — wire-level capture with the ataradov hardware analyzer

Extends the debugging trio with the layer below URBs:

| Skill              | Answers                                                            |
|--------------------|--------------------------------------------------------------------|
| `usbmon`           | what a Linux PC host exchanged (URBs)                              |
| `usb-kernel-debug` | why the Linux kernel acted (dmesg / dynamic debug)                 |
| `target-debug`     | what the TinyUSB target did (device or host role)                  |
| **`usb-sniffer`**  | **what actually crossed D+/D-** (PIDs, handshakes, resets, timing) |

Reach for it when usbmon can't see (device never binds, pre-enumeration
failures), can't be trusted (URB completed but did the wire really ACK?), or
doesn't exist — a link where TinyUSB is the host has no usbmon on either end
(an MCU host runs no kernel; a Linux gadget peer's UDC bypasses usbmon).
Where a Linux PC is the host, usbmon is cheaper — no hardware, no locks.

## Rig inventory — find the sniffer and what it taps

```bash
lsusb -d 6666:6620          # sniffer present? (github.com/ataradov/usb-sniffer)
```

The sniffer is a passive tap: host-side and device-side connectors pass
through, the capture port is a separate USB device. What it taps is a cabling
fact you must confirm every session, not assume: start a capture (below),
provoke known control traffic to a candidate (`lsusb -v -s <bus>:<dev>
>/dev/null`), and see whether those requests appear on the wire. The DUT's
link speed (`cat /sys/bus/usb/devices/<port>/speed`) picks `--speed`.

The tapped board is rig hardware: hold its board lock for any session that
resets or reflashes it (`hil` skill). The sniffer itself is not lockable and
capture alone perturbs nothing.

## Capture

The tool is `usb_sniffer` (installed in `~/.local/bin`, extcap-symlinked so
Wireshark's GUI also shows a "USB Sniffer" interface). Headless recipe:

```bash
timeout 15s usb_sniffer --capture --fifo /tmp/cap.pcapng --speed hs   # or fs / ls
```

- `--speed` MUST match the DUT's link speed (default is fs!). Wrong speed =
  no USB packets, only Syslog pseudo-packets ("Line state: SE0", "VBUS ON").
  If you see only those, fix `--speed` before doubting the hardware.
- ALWAYS bound the capture: `timeout` and/or `--limit N` (packets). HS runs
  15–20 MB/s even with `--fold` when any device on the bus is busy (`--fold`
  only collapses truly empty frames). Unbounded HS captures reach GB fast.
- The output is valid pcapng the moment the process dies; a plain file path
  works (no FIFO needed). `--trigger low|high|falling|rising` arms capture
  on the external trigger pin instead of starting immediately.
- Tool diagnostics: `USB_SNIFFER_LOG=/tmp/sniffer.log usb_sniffer ...`

Start the capture FIRST, then trigger the event you care about. The proven
one-pass enumeration recipe (`--limit` makes the tool exit by itself; on a
busy HS bus ~470k packets/s ≈ 20 MB/s, so 3M packets ≈ 6–7 s ≈ 120 MB — do
NOT capture for 20+ s "to be safe", the raw balloons and every later tshark
pass pays for it; but do NOT go below ~3M either: J-Link connect latency
varies run-to-run (0.5–4 s) and a 3 s window has provably missed the ladder):

```bash
usb_sniffer --capture --fifo raw.pcapng --speed hs --fold --limit 3000000 &
sleep 1
# trigger: full ladder incl. SET_ADDRESS (needs board lock; J-Link resets the MCU):
printf 'r\ng\nqc\n' | JLinkExe -device $JLINK_DEVICE -SelectEmuBySN <probe-uid> \
  -if swd -speed 4000 -autoconnect 1 -nogui 1
wait   # tool prints "Capture limit reached" and exits
```

No-probe trigger alternative — kernel-side re-enumeration (may reuse the
xHCI address and skip parts of the ladder; fine for descriptor reads, weak
for reset timing):
`echo 0 | sudo tee /sys/bus/usb/devices/<port>/authorized; sleep 1; echo 1 | sudo tee ...`

## Reading the capture

```bash
tshark -r cap.pcapng -Y 'usb.bmRequestType'                 # the control ladder
tshark -r cap.pcapng -Y 'usb.bDescriptorType == 1' \
       -T fields -e usb.idVendor -e usb.idProduct           # VID:PID off the wire
tshark -r cap.pcapng -Y 'usbll.pid'                         # raw token/handshake level
editcap -r cap.pcapng slice.pcapng <first>-<last>           # trim huge captures
```

On a capture >100 MB, make exactly ONE filtered pass (the ladder filter
above) to find the frame numbers of your event window, `editcap -r` to that
window, and do all further analysis on the slice — repeated broad tshark
passes over a 300 MB raw are what turn a 5-minute job into 15.

Find the DUT's wire address from the capture, not from lsusb: the
SET ADDRESS request payload carries it (`00 05 <addr> 00 ...`), and all
subsequent traffic goes to `<addr>.<ep>` (`usbll.addr`). **On xHCI hosts the
lsusb device number is NOT the wire address** — they diverge routinely.
Filter analysis to the DUT: `-Y 'usbll.addr contains "4."'`.

## What the wire really shows (read before concluding anything)

- **Downstream is broadcast.** Tokens, SETUP and OUT data addressed to EVERY
  device on the tapped bus segment appear in the capture; upstream (DATA in
  response to IN) appears only from devices on the tapped branch. Lone
  IN→ACK pairs without DATA to some other address are normal, not corruption.
- **The sniffer can capture its own upload.** If its capture port shares the
  host controller bus with the tap, its bulk-IN polling floods the capture
  (easily >90% of packets) — filter it out by address; for surgically clean
  captures move the capture cable to a different host controller.
- **Port-reset visibility depends on the tap point.** Tapping the DUT's own
  cable: a reset reaches the sniffer PHY and you get explicit
  `--- Bus Reset ---` / `Detected speed:` Syslog records. Tapping a hub
  upstream: the hub isolates downstream port resets — no marker appears. Anchor reset timing on the hub choreography instead:
  SetPortFeature(PORT_RESET) to the hub's address = reset start,
  ClearPortFeature(C_PORT_RESET) = reset end (start the capture before
  triggering, or the initiating SetPortFeature is missing from the file).
  The DUT's silence gap corroborates, but do not read every gap as a
  reset — idle captures contain benign multi-ms gaps.
- **FS device behind an HS hub**: the upstream tap shows SPLIT transactions,
  not native FS packets. Tap the DUT's own cable and capture at `fs` for
  clean full-speed traffic.

## Setup (one-time)

```bash
# udev: tools/88-tinyusb.rules covers 6666:6620 + blank FX2LP 04b4:8613
sudo cp tools/88-tinyusb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger -s usb

# binary (ataradov repo bin/usb_sniffer_linux) + Wireshark extcap symlink (needs Wireshark >= 4.x)
cp usb_sniffer_linux ~/.local/bin/usb_sniffer && chmod +x ~/.local/bin/usb_sniffer
mkdir -p ~/.local/lib/wireshark/extcap
ln -sf ~/.local/bin/usb_sniffer ~/.local/lib/wireshark/extcap/usb_sniffer
```

Never run `--mcu-eeprom` / `--fpga-flash` / `--fpga-erase` against a working sniffer — those program NEW hardware.

## Warnings

- **Bound every capture** (`timeout` / `--limit`) and delete or `editcap`-trim
  multi-hundred-MB raws before handing off; a forgotten capture process fills
  the disk at HS rates.
- The tap is passive — capturing, or unplugging the capture port, does not
  disturb the DUT's link. Unplugging the pass-through DOES.
- Answers must come from packet payloads (SETUP/DATA hex), not from host-side
  logs — that is the whole point of being on the wire; if an answer isn't in
  the capture, say so rather than approximating from sysfs/dmesg.
- Release the board lock and leave no capture processes running at session
  end (`pgrep -a usb_sniffer`).
