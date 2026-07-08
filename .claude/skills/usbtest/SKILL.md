---
name: usbtest
description: Use when running, debugging, or porting the Linux usbtest/testusb battery (examples/device/usbtest, cafe:4010) — device "did not bind", SET_CONFIGURATION fails, a case fails with errno 110/32/5/71, toggle-clear/halt/unlink/iso failures, iso packets dropped, or a new MCU/DCD needs the full 30/30 sign-off.
---

# usbtest — porting & debugging the Linux kernel USB battery

## Overview

`examples/device/usbtest` is the device-side peer of the Linux kernel's `usbtest.ko`/`testusb`
(gadget-zero source/sink protocol): 30 cases over bulk, EP0, interrupt, and isochronous, including
halt, data-toggle, and unlink storms. It is the most adversarial exerciser a DCD gets — every port
so far surfaced at least one real driver bug. Host runner: `test/hil/usbtest.py`; HIL integration
runs it per board and reports `✅ 30/30` cells.

**Core principle: the battery is a DCD test, not a firmware test.** When a case fails, suspect the
DCD path it exercises (table below), reproduce that one case, and root-cause on hardware before
changing anything (`superpowers:systematic-debugging`). One variable at a time; a fix is proven by
the failing case passing *and* the full battery still at 30/30 across reflash cycles.

## Run

```bash
# build (cmake); descriptor sizes auto-adapt per MCU via src/usb_descriptors.h + src/tusb_config.h
cd examples/device/usbtest && cmake -B build -DBOARD=<board> -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel && cmake --build build
# flash, wait ~3-5 s for enumeration to settle, then:
python3 test/hil/usbtest.py --serial <uid> --keep-binding            # full battery for the advertised tier
python3 test/hil/usbtest.py --serial <uid> --keep-binding --tests 29 # one case
```

- **Always `--keep-binding`**: the cleanup unbind path has wedged host xHCIs (`usb_hcd_alloc_bandwidth`).
- Always settle a few seconds after flashing — enumeration can bounce once; testusb into the gap sees
  the device drop mid-case.
- On a CI rig: stop the actions runner before touching hardware; restart after. Never run two
  batteries concurrently (hil_test.py serializes them; concurrent batteries have hard-frozen a rig
  via a fatal PCIe error on a VFIO-passed xHCI).

## Porting ladder — new MCU/DCD to 30/30

1. **Tier 1 (bulk)**: set `USBTEST_TIER 1`, get enumeration + cases 0,9,10 (EP0) + 1–8,17–20,27,28
   solid. EP0 correctness first — everything else reports through it.
2. **Tier 2 (ctrl_out 14/21)**, **tier 3 (interrupt 25/26)**, **tier 4 (iso 15/16/22/23)** — raise
   the tier only when the layer below is clean; run the *full* battery after each layer.
3. **Fit the endpoints**: tier 4 needs 6 endpoints + EP0. Small parts need per-MCU mps/epbuf
   overrides in `src/usb_descriptors.h` (`USBTEST_INT/ISO_EP_MPS_FS`) and `src/tusb_config.h`
   (`CFG_TUD_VENDOR_TX_EPSIZE`) — follow the existing CH32/LPC11 patterns. Parts that can't fit go
   in `skip.txt`.
4. **Sign-off = reliability, not one pass**: 3–10 full flash→battery cycles. One 30/30 proves
   nothing on a flaky bring-up; deterministic partial counts (e.g. exactly 1-in-8 lost) are a
   signature, not noise — chase them.
5. Register the board in `test/hil/tinyusb.json` so the HIL suite runs it.

## Case → DCD subsystem map

| Failing case(s) | Exercises | First suspect |
|---|---|---|
| 9, 10 | EP0 control storms | EP0 state machine, ZLP/status stage, control starvation under load |
| 1–8, 17–20, 27, 28 | bulk source/sink, sg, perf | FIFO handling, multi-packet, ZLP tolerance |
| 11, 12, 24 | URB unlink mid-transfer | abort/close paths leaving state half-armed |
| 13 | set/clear halt | stall must kill the transfer; halt on armed IN must flush the TX FIFO |
| **29** | clear-halt on an **armed, un-halted** ep | **the classic**: `dcd_edpt_clear_stall` resets toggle but disarms the queued receive → NAKs forever, errno 110. Fix: reset toggle to DATA0 *and* re-arm/preserve the pending transfer. Found independently on rp2040, fsdev, ch32_usbhs, rusb2 |
| 14, 21 | vendor EP0 write/readback | multi-packet control-OUT chunking, DCP flow control |
| 25, 26 | interrupt src/sink | usually free once bulk works |
| 15, 16, 22, 23 | isochronous | see iso rules below |

## Iso rules (most-violated contract)

- **DATA0-only in BOTH directions** at FS — never run bulk-style toggle logic on an iso endpoint
  (manual-toggle parts: skip the ISR toggle flip for iso IN *and* the toggle-mismatch drop for iso
  OUT). Symptom of violating it: exactly every-other packet lost.
- **No handshake** — iso never NAKs/STALLs; parts with response fields use their "no response"
  encoding (e.g. NYET on WCH).
- `dcd_edpt_iso_alloc`/`iso_activate` **must not be stubs returning false** — usbd fails the
  interface open and the kernel logs "did not bind"/SET_CONFIG times out. If a DCD refuses iso
  "because the hardware can't", **verify against the datasheet — the manual outranks the code
  comment** (two "no iso support" claims in this tree were false, incl. a per-endpoint exception
  the RM documents for one endpoint number only).
- A multi-packet iso IN submit is legal: the DCD streams it one packet per frame, refilling in the
  ISR. Slow cores may need double-buffered iso to make the frame deadline.

## Debug ladder (escalate in order)

| errno | Meaning |
|---|---|
| 110 | timeout — endpoint NAKing forever / device wedged |
| 32 | EPIPE — unexpected STALL |
| 5 | EIO — iso packet errors (check `dmesg`: "N errors out of M") |
| 71 | EPROTO — device answered wrong / too slow (after HC retries) |

1. `usbtest.py` per-case output + its captured `dmesg` (`TEST n` markers bracket each case).
2. **usbmon** (`usbmon` skill): URB-level ground truth. **It cannot show data toggles or NAKs** —
   a toggle desync and a dead endpoint look identical (Submits without Completes); distinguish
   device-side with GDB.
3. **On-device gdb/openocd**: read the EP control registers and DCD structs at the hang.
4. Heisenbugs (vanish under logging): RAM ring-buffer trace dumped over openocd; for silent lockups
   JLink PC-sampling (`halt`+`regs` repeatedly — a pinned PC names the spin).
5. **Cross-check the reference manual** (calibre library) before changing any register-level code —
   per CLAUDE.md, and because comments/assumptions in DCDs have been wrong about hardware caps.
6. Check the vendor's **silicon errata** early for timing/DMA hangs (an unimplemented erratum
   workaround caused a case-10 hang on one port).

## Traps that pass gcc/desk review but fail elsewhere

- `TUD_OPT_HIGH_SPEED` is a **compile-time capability, not the live speed**: the FS config
  descriptor (and OTHER_SPEED) must use FS-legal sizes (int ≤ 64, iso IN+OUT ≤ 1023 B/frame) even
  on HS builds — use separate `_FS`/`_HS` descriptor macros.
- Unused `static inline` helpers: clang `-Wunused-function` and IAR `Pe177` error where gcc stays
  quiet → `TU_ATTR_UNUSED`.
- A symbol referenced only inside naked asm is invisible to LTO and gets dropped in `-flto` make
  builds → keep a `TU_ATTR_USED` C reference to it.
- Nested USB IRQs on cores with hardware context stacks (QingKe HWSTK): plain
  `__attribute__((interrupt))` corrupts the return — use naked handlers relying on the HW stack.
- Dedicated USB RAM budgets (PMA/USB-RAM) differ per part *and* per build system section placement:
  check the link map, not just that it builds.

## Red flags — stop and re-examine

- "One pass = done" → run reflash cycles.
- "The DCD comment says the hardware can't" → open the datasheet.
- "usbmon shows no toggle problem" → usbmon can't see toggles.
- "It works on gcc" → clang/IAR/LTO/make still pending.
- "Fixed iso IN" → apply the same exemption to iso OUT (toggle logic is symmetric).
- A clean single-board run does not validate concurrent/fleet behavior — batteries serialize.
