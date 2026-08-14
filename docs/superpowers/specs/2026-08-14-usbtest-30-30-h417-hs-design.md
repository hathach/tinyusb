# usbtest 30/30 on the CH32H417 USB2 high-speed fallback

Date: 2026-08-14
Status: **RESOLVED AS A HARDWARE LIMITATION** -- high speed is now 12/12 with usbtest
25/25 and five documented quirk skips. Literal 30/30 was not reached and does not look
reachable on this controller; see the outcome below.
Branch: `claude/wch-ch569-ch32h417-usb3`

## Goal

`usbtest` 30/30 on the nanoCH32H417 USB2 high-speed fallback (`-DSPEED=high`), with every
case genuinely passing on the wire and no quirk skips.

If the vendor oracle (Phase B) shows the silicon cannot do it, that verdict — with its
evidence — is the deliverable instead.

## Outcome (2026-08-14)

**The second was delivered: a silicon verdict with evidence.** High speed is now **12/12 with
usbtest 25/25**, the five control-stress cases (9, 10, 13, 14, 21) skipped under a new quirk
`0x40`. Literal 30/30 was not reached.

The mechanism: the CH32H417 USBHS drops a SETUP that arrives while a transfer completion is
still unserviced. The CH569 USBHS has `RB_USB_INT_BUSY` -- WCH's own header calls it
"automatic responding busy for device mode ... during interrupt flag UIF_TRANSFER valid" --
so there the same window merely NAKs and the host retries successfully; it passes 30/30 at
high speed on identical usbd, host and rig. RM 25.2.1.1 lists no equivalent bit for the H417
USBHS, though the part's USBFS block has `USBFS_UC_INT_BUSY`. This is a strong inference from
WCH's documentation, **not confirmed with WCH**, and it explains why no EP0 register change
ever moved the rate: the drop precedes anything software can do.

Ordinary class traffic is unaffected because host retries absorb it -- all twelve HIL device
examples pass at CDC 5.8/6.5 MB/s and MSC 25.2/10 MB/s. Only usbtest's control stress exposes
it.

What the campaign did establish:

- **The CH569 oracle is real**, re-measured today at 12/12 + usbtest 30/30 including 13, 14
  and 21, under identical usbd, host and rig. usbd's deferred status-stage model is
  exonerated and the defect is H417-specific.
- **No portable difference from that oracle moves the drop rate**: baseline 34.5%, status
  pre-arm removed 32.8%, correctness fixes 37.7% — all inside the per-run band.
- **The one difference that could explain a SIE-level drop is not portable**: CH569 has a
  dedicated `RB_USB_IE_SETUPACT` interrupt; the H417 silicon has none.
- **Phase B was attempted and aborted** — the vendor demo bricked the board (details in
  `docs/superpowers/notes/h417-ep0-diff.md`), needing a physical cable pull to recover.

Silicon-vs-driver therefore remains **undecided**, and that is the honest state. The Phase B
retry recipe in the notes is the only remaining way to settle it.

## Current state

| Item                     | Value                                                  |
| ------------------------ | ------------------------------------------------------ |
| HIL device suite (HS)    | 11/12                                                  |
| usbtest (HS, fresh flash)| 25/30                                                  |
| Failing cases            | 9 ch9-subset, 10 queued-control, 13 ep-halt, 14/21 ctrl_out |
| Throughput               | CDC 5.8/6.5 MB/s, MSC 25.2/10.1 MB/s                   |
| SuperSpeed path          | untouched; the HS file compiles to nothing at `SPEED=super` + `FALLBACK=0` |

Quirk flags are gated on `CFG_TUD_WCH_USBIP_USB30`, so at high speed none are advertised:
all 30 cases run. Both existing quirks describe SuperSpeed errata (`0x10` → cases 14/21,
`0x20` → case 13), and the HS failing set is exactly those plus 9 and 10.

## The blocker

The SIE intermittently gives **no handshake at all** to a SETUP. A SETUP may not be NAKed or
STALLed, so the host retries three times ~124 µs apart and fails the transfer with
`-EPROTO`/`-EIO`. Wire-confirmed; the device is completely silent on a dropped attempt, with
no malformed or bad-CRC packets.

A dropped SETUP is harmless alone — the host retries. A case fails only when **all three**
attempts drop. At a ~37% drop rate that is ~5% per transfer, and case 9 alone issues 1000
control transfers. **The target is therefore a drop rate near zero, not merely lower.**

Related mechanism already measured: after a control-IN data stage completes, the host sends
the status ZLP in the very next microframe, but usbd submits it a `tud_task` round-trip
later, leaving EP0 RX at `RES=NAK` across the gap. On the wire the host retried the status
OUT for ~750 µs, PINGing in between, while the SIE answered PING with ACK from the RES field
and still refused the data.

## Measurement rules

Non-negotiable. Violating these produced a phantom bug, two harmful "fixes", and one
unproven headline number across three review rounds.

| Rule                                                    | Why                                                                   |
| ------------------------------------------------------- | --------------------------------------------------------------------- |
| Capture wire and UART in the **same run**                | Separate runs produced the phantom "duplicate SETUP" finding           |
| Always start from a **fresh flash**                      | Drop rate drifts 44% → 71% over ~3 min of uptime                       |
| Pool **≥300 SETUPs** per config; interleave A/B/A         | A single ~90-SETUP capture spans 44–71% on one unchanged build         |
| Gate flashing on **build success**                       | A failed build silently flashes the stale binary                       |
| Judge by **wire drop rate**, not usbtest pass count      | usbtest is noisy at 24–25/30                                           |
| Revert any change that does not move the pooled metric   | The failure mode of the last session was accumulating unproven edits   |

Primary metric: pooled SETUP drop rate from a fresh flash.
Acceptance metric: usbtest 30/30 on three consecutive runs, **each from its own fresh flash**
(the drift makes a single flash followed by three runs a weaker claim).

## Phase 0 — re-establish ground truth

No code changes. Two facts this design leans on are memory claims, not present-day
measurements:

1. **CH569 at HS is 30/30.** Flash the hydra with the HS `usbtest` build and run the
   battery. If it is not 30/30 today, Phase A loses its oracle and we go straight to Phase B.
2. **The H417 failing set is stable.** Three fresh-flash runs to confirm 9/10/13/14/21 is
   the real set rather than a noise band.

## Phase A — differential against the CH569 driver

Assuming Phase 0 confirms it, `dcd_ch56x_usbhs.c` reaches 30/30 under *identical* usbd — so
the deferred status-stage model is survivable and something in the H417 EP0 path differs.
Compare the two state machines on four axes:

- **SETUP detection and acknowledgement** — how each learns a SETUP arrived, and what it writes back
- **Status-stage arming** — when EP0 RX becomes ACK-armed relative to the data stage completing
- **Toggle management** — CH569 uses hardware auto-toggle for data endpoints; H417 drives it manually per arm
- **Response/DONE bookkeeping** — order of clearing, and what runs in ISR versus task context

Each difference becomes one candidate change, applied **one at a time**, judged by pooled
drop rate. Changes that do not move the metric are reverted, not accumulated.

usbd core changes are authorised. One candidate is explicitly in scope: letting the DCD
complete the EP0 status stage without waiting for a `tud_task` round-trip.

## Phase B — vendor oracle (only if Phase A falls short)

Build `hw/mcu/wch/ch32h417/EVT/EXAM/USBHS/DEVICE/SimulateCDC-HID` unmodified — the only SDK
family that handles a multi-packet EP0 OUT, which is what cases 14/21 exercise. Build
out-of-tree with the riscv toolchain plus `hw/bsp/ch32h417/linker/ch32h417_v3f.ld`.
**Nothing under `hw/mcu/` is edited.** Run the same control hammer and measure its pooled
drop rate.

- Vendor **also drops** → silicon. Literal 30/30 is unreachable. Deliver the verdict with
  evidence and propose extending quirks `0x10`/`0x20` to high speed as a follow-up decision
  for the maintainer.
- Vendor is **clean** → the difference is in our EP0 path, and their register sequence is the
  map to transcribe.

## Verification

- usbtest **30/30 on three consecutive runs, each from its own fresh flash**
- Full HIL device suite on the nano at HS: currently 11/12, must be 12/12
- **SuperSpeed regression**: nano SS suite (12/12 + usbtest 29/29) unchanged. Free today
  because the HS file compiles out at `SPEED=super`; **not free if a usbd core change lands**
- **If usbd core changes**: CH569 SS and HS, plus at least one non-WCH board
  (`stm32f407disco` or `raspberry_pi_pico`) to catch fallout on other ports
- `ceedling` unit tests and `pre-commit run --all-files`

## Exit conditions

- **Done** — acceptance bar met and regressions clean.
- **Stop and report** — the vendor oracle shows silicon, or a fix would require modifying
  vendor SDK files. In both cases the deliverable is the evidence plus a recommendation.

## Out of scope

The SuperSpeed path, the CH569 port itself, and the uptime drift as a standalone
investigation. The drift is controlled for via fresh flashes; it becomes a work item only if
it turns out to *be* the drop mechanism.

## Appendix A — known-wrong approaches, do not reintroduce

All measured on hardware. Each is called out in a comment at the relevant code site.

| Approach                                                   | Why it is wrong                                                                                                             |
| ---------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| Gate SETUP detection on `!xfer_status[0][OUT].valid`        | Suppresses the SETUP a host uses to abort an in-progress control-OUT (USB 2.0 §8.5.3), where `valid` is legitimately true     |
| Gate SETUP detection on `EP_RX_LEN(0) == 8`                 | Misreads any 8-byte control-OUT data packet as a SETUP; usbtest's own `ctrl_out` varies data length 1..512                    |
| Make `arm_ep0_out()` preserve `RB_UEP_R_DONE`               | It is also the task-context arm path; a stale preserved DONE feeds a duplicate packet into the freshly armed transfer         |
| Software-set `DONE` bits in the RMW helpers                 | `DONE` is clear-only; no WCH demo does this                                                                                   |
| Enable `USBHS_UDIE_SOF_ACT` to run the ISR every microframe | Measured markedly worse                                                                                                       |
| Re-latch `UEP0_DMA` / `UEP0_MAX_LEN` on every arm           | Measured worse                                                                                                                |
| Clear `RB_UEP_x_NAK_ACT` everywhere                         | A genuinely never-cleared RW0 flag, but clearing it changed nothing                                                            |

Correct SETUP detection is `RB_UEP_R_SETUP_IS` alone, as every WCH demo does. Verified: 45
wire SETUP ACKs versus 45 deliveries to usbd, 0 extras. The hardware clears the flag on the
first non-SETUP packet (EP0 RX reads `0x34` right after a status OUT lands).

Also solidly excluded on non-statistical grounds: EP0 left in STALL (0 occurrences across
830 register samples); malformed or late handshakes; bus phase (median SOF→SETUP 5.28 µs for
both accepted and dropped, and 45/45 accepted plus 22/22 dropped were first-in-microframe);
the WCH-LinkE probe; the fallback ladder; USB2 LPM; the suspend handler.

## Appendix B — repro and tooling

```bash
cmake -B <dir> -DBOARD=nanoch32h417 -DSPEED=high -DCMAKE_BUILD_TYPE=MinSizeRel -G Ninja examples
python3 test/hil/wch_uart_flash.py --uid E8E68F066EEE <dir>/device/usbtest/usbtest.bin
timeout 18s usb_sniffer --capture --fifo cap.pcapng --speed hs --fold --limit 4000000 &
sudo lsusb -v -s <bus>:<dev>          # x8, the control hammer
python3 test/hil/usbtest.py --serial E8B0C3506C54175639E339E3 --json
```

Drop-rate extraction:

```bash
tshark -r cap.pcapng -T fields -e frame.time_relative -e usbll.pid -e usbll.src -e usbll.dst -e usbll.data
```

A SETUP is *dropped* when no `0xd2` ACK with `usbll.src` starting `1.` follows within two
frames. PIDs: SETUP `0x2d`, IN `0x69`, OUT `0xe1`, ACK `0xd2`, NAK `0x5a`, STALL `0x1e`,
PING `0xb4`, DATA0 `0xc3`, DATA1 `0x4b`, SOF `0xa5`. The device is wire address 1 on bus 5;
the sniffer taps between the board and the uPD720201. On "protocol desynchronization",
re-run the capture — it recovers.

Rig notes: keep `CFG_BOARD_UART_BAUDRATE` at 115200 (the UART loader and park magic speak
115200; a 921600 image needs a physical power cycle). RISC-V `TU_BREAKPOINT()` is an
unconditional `ebreak` — debug builds must override it with
`-DCFG_TUSB_DEBUG_BREAKPOINT=wch_dbg_breakpoint` or the board bricks.
