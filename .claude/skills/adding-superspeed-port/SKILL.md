---
name: adding-superspeed-port
description: Use when adding USB3 SuperSpeed device support for a new MCU to TinyUSB (a dcd port, usually with runtime USB2 high-speed fallback), extending SuperSpeed descriptors to more class drivers or examples, or debugging SuperSpeed enumeration failures (link stuck in Polling, "can't read configurations, error -110", hot-reset loops, host never sees 5000M).
---

# Adding a SuperSpeed Device Port to TinyUSB

## Overview

TinyUSB gained SuperSpeed device support with the WCH CH569 port. The work is layered so
that a new SS MCU only writes **one dcd file + a BSP**: the core stack (usbd, types,
templates), the class drivers and the bulk/interrupt examples are already SS-aware.
Reference implementation for everything below: `src/portable/wch/dcd_ch56x_usb30.c`
(+ `ch56x_usb30_reg.h`, `dcd_ch56x.h`, `dcd_ch56x_usbhs.c`, `hw/bsp/ch569/`).

**Core principle:** to the stack, SuperSpeed is just another speed. The dcd hides all
link-layer work (LTSSM, LMP, resets) and reports ordinary dcd events; usbd only needs to
know the operating speed to size EP0 transactions and pick descriptors.

## The core contract (already in the tree — your dcd must satisfy it)

| Item | Definition | Semantics |
|---|---|---|
| `OPT_MODE_SUPER_SPEED` (0x0800) | `src/tusb_option.h` | ORed into `CFG_TUD_MAX_SPEED` |
| `TUD_OPT_SUPER_SPEED` | `src/tusb_option.h` | build-time "SS possible" switch |
| `TUP_RHPORT_SUPERSPEED` | your block in `src/common/tusb_mcu.h` | declares port capability (default 0) |
| `TUSB_SPEED_SUPER` (=3) | `src/common/tusb_types.h` | report via `dcd_event_bus_reset` |
| EP0 = 512 fixed | `CFG_TUD_ENDPOINT0_SIZE` must be 512 on SS builds | usbd chunks EP0 with `ep0_xact_limit()` (512 when `speed==SUPER`, else 64) |
| Bulk mps = exactly 1024 | enforced by `tu_edpt_validate` | int/iso ≤ 1024 |
| SS descriptor structs | `tusb_desc_ss_ep_companion_t` etc. in `tusb_types.h` | companion follows EVERY endpoint descriptor |
| `SET_SEL` / `SET_ISOCH_DELAY` / U1,U2 features | handled in `usbd.c` | nothing to do in the dcd |
| Companion skipping | `usbd_open_edpt_pair` + `usbd_skip_ss_ep_companion()` (`usbd_pvt.h`) | class drivers already tolerate companions and count them in `drv_len` |
| SS templates | `TUD_{CDC,MSC,VENDOR,HID,MIDI,MTP,PRINTER,NCM,USBTMC}_SS_*` in `usbd.h` | bulk sizes hardcoded 1024, `_maxburst` = bursts−1 |

The dcd reports the **actual operating speed** in every `dcd_event_bus_reset`; the
speed-switched example descriptor callbacks (`tud_speed_get()`) do the rest.

## Steps for a new SuperSpeed MCU

Work in stages; each is independently buildable and hardware-verifiable.

1. **Plumbing** — `OPT_MCU_*` in `tusb_option.h`; an mcu block in `tusb_mcu.h` setting
   `TUP_USBIP_*` tokens, `TUP_RHPORT_HIGHSPEED 1`, `TUP_RHPORT_SUPERSPEED 1`,
   `TUP_DCD_ENDPOINT_MAX`, and a default `CFG_TUD_ENDPOINT0_SIZE 512`. If the chip has
   separate USB2/USB3 controllers, add selector defines (see the CH569 block:
   `CFG_TUD_WCH_USBIP_USB30` / `..._USBHS` / `..._USB30_FALLBACK`).
2. **USB2 dcd first** (`SPEED=high`) — port or reuse the family's high-speed dcd, but
   expose its internals as non-static `<mcu>_usb2_*` functions through a small private
   header so stage 4 can drive them (`dcd_ch56x.h` pattern). Get it enumerating on
   hardware before touching USB3.
3. **USB3 dcd** (`SPEED=super`) — one file. Structure it like `dcd_ch56x_usb30.c`:
   - `link_*` static functions for the LTSSM/LMP work, isolated from the endpoint
     engine (this is the extraction seam if a sibling chip shares the link IP — e.g.
     CH32H417 shares the CH569 LINK block but has a different endpoint engine).
   - All link/LMP register work happens **in the ISR** (timing critical, µs budgets);
     only `dcd_event_*` calls defer to task context.
   - "Bus reset" for the stack = the link first reaching U0
     (`dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true)` after the LMP exchange starts).
   - SET_ADDRESS applies at the **status stage**, not when the request arrives.
   - DMA constraints (dedicated RAM regions, alignment) → per-endpoint bounce slots from
     a small pool; zero-copy only when the app buffer qualifies.
4. **Runtime fallback** (single rhport, `SPEED=super` default) — a hardware timer bounds
   SS training; expiry tears USB3 down and brings the USB2 controller up on the same
   rhport; usbd re-inits transparently on the USB2 bus reset. See the `_fb_state`
   machine in `dcd_ch56x_usb30.c`. Make the deadline **termination-aware**: if far-end
   RX terminations were ever seen, an SS host exists — retry training (fresh detect
   cycle) longer before settling for HS; no terminations = USB2-only host = fall back
   fast. Provide a `..._FALLBACK=0` build for SuperSpeed-only (train indefinitely).
5. **BSP/board** — nothing SS-specific beyond IRQ forwarding and (if needed) placing
   `CFG_TUSB_MEM_SECTION` in the DMA-visible RAM. Examples/descriptors need no work:
   they are already speed-switched.

## Validated gotchas (each cost hours on real hardware)

| Symptom | Cause / rule |
|---|---|
| `can't read configurations, error -110`; device desc reads OK, config full read times out | `CFG_TUD_ENDPOINT0_SIZE` left at 64: usbd sends a 64-byte chunk = **short packet** on the 512-mps SS EP0 → host ends the data stage early → status stage deadlocks. EP0 must be 512 (example configs use `(TUD_OPT_SUPER_SPEED ? 512 : 64)` and clamp the FS/HS device-descriptor field back to 64). |
| Host warm-resets in a loop right after training | Wrong LMP payload. PORT_CAPABILITY and PORT_CFG_RES are different subtypes — verify against a reference manual, not guesswork. |
| Link trains (hub port shows U0) but enumeration never starts; RDY/HOT_RESET/RECOV ISR flags cycle | LMP PORT_CONFIGURATION must be answered within µs — do it directly in the ISR. After a **hot reset** do NOT resend PORT_CAP (config is retained); after a **warm reset** the exchange restarts. |
| SS trains on some boots only; hub port stuck in `Polling` | Some hubs (Renesas uPD720201) need a training **re-attempt**: on timer tick, `hw_deinit(); hw_init();` for a fresh RX-detect cycle instead of waiting passively. |
| Device dead after switching to fallback; queue-full asserts | `dcd_int_enable/disable` run on **every** osal_none queue op — they must dispatch on the fallback state, and controller switch must clear stale pending IRQs, or old flags refire forever. |
| Stack corruption, crash after first IRQ | gcc identical-code-folding rewrote one `__attribute__((interrupt))` handler into a `jal` to another (inner `mret` skips the outer epilogue). Multiple identical vectors must be **aliases of one handler**. |
| Burst (NUMP>1) data corruption or config failure | Re-arm DMA every burst (address regs auto-increment); validate burst with read-back integrity tests, and respect the link's header-packet buffer count (CH569: 4 → burst ≤ 4). |
| Enumeration works, wrong power draw shown | `TUD_CONFIG_SS_DESCRIPTOR` takes mA like the FS one but encodes 8 mA units — don't pre-divide. |

## Adding SS descriptors to a new class driver / example

- **Driver:** its `open()` walk must skip `TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION` and
  include companion bytes in the returned `drv_len` (helper: `usbd_skip_ss_ep_companion`;
  pattern examples: `cdc_device.c` pointer-walk, `msc_device.c` fixed-size probe).
  Wire order is EP → companion → CS-EP descriptors.
- **Example:** copy the canonical pattern from `examples/device/cdc_msc/src/usb_descriptors.c`:
  EP0 clamp (`EP0_SIZE_FSHS`), `desc_device_ss` (bcdUSB 0x0320, `bMaxPacketSize0 = 9` — an
  exponent), SS config array from `TUD_*_SS_DESCRIPTOR` templates, BOS
  (USB2-ext + SuperSpeed caps; extend an existing BOS rather than duplicating), and
  `tud_speed_get()` branches in the device/config/BOS callbacks. ISO-class examples
  (audio/video/uac2/bth) are out of scope until a dcd supports SS ISO.

## Verification checklist

- Build the full example set for both `SPEED=super` and `SPEED=high`.
- Confirm `wTotalLength == sizeof(desc_ss_configuration)` in the built ELFs (a length
  bug enumerates wrong at runtime but never fails the build).
- `ceedling test:all` — `test_usbd.c` has SS cases (edpt_validate, companion skipping,
  SET_SEL/SET_ISOCH_DELAY).
- Code-size compare on a non-SS board must be **0-delta** (all SS code folds out under
  `TUD_OPT_SUPER_SPEED == 0`).
- Hardware: `dmesg` shows "new SuperSpeed USB device … 5000M"; `lsusb -t` 5000M;
  replug/suspend cycles; then data-integrity before throughput numbers.
- Debugging aids: hub-port LTSSM state via `sudo uhubctl` (`Rx.Detect` = no terminations
  seen, `Polling` = LFPS but not trained, `U0` = trained); raw control-transfer truth via
  usbmon text (`sudo cat /sys/kernel/debug/usb/usbmon/<bus>u`); count link-ISR flags into
  spare RAM when the wire view isn't enough.
