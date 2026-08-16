# Bus-Reset Edge Events + Review Fix Wave Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the device stack a "bus reset started" event so ci_hs can tell usbd to stand down at the URI interrupt instead of up to 50 ms later, and clear the ten findings agreed from the max review.

**Architecture:** `DCD_EVENT_BUS_RESET` splits into `DCD_EVENT_BUS_RESET_START` / `_END` with a compatibility alias, so every other port stays byte-identical. `dcd_ci_hs.c`'s `bus_reset()` splits along the register/software line — registers at URI (`_START`), software structures at the port-change ending the reset (`_END`) — which eliminates the window where usbd believes it is configured over zeroed queue heads. A single bounded-flush helper absorbs the five flush sites. Seven mechanical fixes follow.

**Tech Stack:** C99, TinyUSB device stack (`src/device/`), ChipIdea HS DCD (`src/portable/chipidea/ci_hs/`), NXP IP3511 DCD (`src/portable/nxp/lpc_ip3511/`), CMake+Ninja and Make builds, J-Link flashing, `test/hil/` HIL harness.

## Global Constraints

- Branch `fix-ci-hs` in worktree `/home/hathach/.herdr/worktrees/tinyusb/fix-ci-hs`. Do NOT push; the user pushes.
- C99, 2-space indent, no tabs. Match each file's surrounding style (`dcd_lpc_ip3511.c` mixes styles — follow the immediate neighbourhood).
- Commit messages: imperative mood, no `Co-Authored-By:` or `Claude-Session:` trailers (repo rule: hathach is sole author).
- The repo pre-commit hook (trailing-whitespace, end-of-file-fixer, codespell, unique-PIDs, ceedling unit tests) must pass. If it rewrites a file, re-stage and retry the commit once.
- Comments: short, only the non-obvious "why". Cite manuals as `UM10503 25.10.3` / `Errata LPC546xx USB.13` style — never `ES_` prefixes.
- Never edit anything under `hw/mcu/` or `lib/` (vendor code).
- Build commands used throughout (each ~30-60 s):
  `cmake --build examples/cmake-build-<board>` for `mimxrt1064_evk`, `lpcxpresso18s37`, `lpcxpresso11u37`, `lpcxpresso55s28`.
- Design source of truth: `docs/superpowers/specs/2026-08-15-ci-hs-reset-edges-design.md`.

## File Structure

| File | Responsibility in this plan |
|---|---|
| `src/device/dcd.h` | Event enum + compatibility alias + contract comment |
| `src/device/usbd.c` | Handle both reset edges; log strings; stop breakpointing on DCD refusal |
| `src/portable/chipidea/ci_hs/dcd_ci_hs.c` | Flush helper; `bus_reset()` split; setup-flush wait; `dcd_set_address`; RESUME guard |
| `src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c` | Torn-setup delivery; USB.13 TODO token |
| `hw/bsp/lpc55/boards/lpcxpresso55s28/board.cmake` | Delete dead RHPORT block |
| `hw/bsp/lpc11/boards/lpcxpresso11u37/lpc11u37.ld` | Correct stale comment; relabel ASSERT |

Tasks 1-3 are ordered (each builds on the previous); Tasks 4-6 are independent of each other.

---

### Task 1: Split the bus-reset event into START/END edges

**Files:**
- Modify: `src/device/dcd.h` (enum at lines 23-34; contract comment above it)
- Modify: `src/device/usbd.c` (`_usbd_event_str[]` at line 457; the `DCD_EVENT_BUS_RESET` case at line 700)

**Interfaces:**
- Produces: `DCD_EVENT_BUS_RESET_START` and `DCD_EVENT_BUS_RESET_END` enum members; `#define DCD_EVENT_BUS_RESET DCD_EVENT_BUS_RESET_END`. Task 2 emits `_START` via the existing `dcd_event_bus_signal(uint8_t rhport, dcd_eventid_t eid, bool in_isr)` and `_END` via the existing `dcd_event_bus_reset(uint8_t rhport, tusb_speed_t speed, bool in_isr)`.

- [ ] **Step 1: Replace the enum member in `src/device/dcd.h`**

Replace:

```c
typedef enum {
  DCD_EVENT_INVALID = 0,    // 0
  DCD_EVENT_BUS_RESET,      // 1
  DCD_EVENT_UNPLUGGED,      // 2
  DCD_EVENT_SOF,            // 3
  DCD_EVENT_SUSPEND,        // 4 TODO LPM Sleep L1 support
  DCD_EVENT_RESUME,         // 5
  DCD_EVENT_SETUP_RECEIVED, // 6
  DCD_EVENT_XFER_COMPLETE,  // 7
  USBD_EVENT_FUNC_CALL,     // 8 Not an DCD event, just a convenient way to defer ISR function
  DCD_EVENT_COUNT
} dcd_eventid_t;
```

with:

```c
// Bus reset is reported as two edges. BUS_RESET_START is optional: a controller that
// cannot tell the edges apart emits only BUS_RESET_END, which stays self-sufficient (it
// performs the full teardown with or without a preceding START). Emit START when reset
// signaling is detected - the link is unusable and the speed is not negotiated yet - so
// the stack stops using endpoints immediately instead of at the end of the reset.
typedef enum {
  DCD_EVENT_INVALID = 0,     // 0
  DCD_EVENT_BUS_RESET_START, // 1
  DCD_EVENT_BUS_RESET_END,   // 2 with negotiated speed
  DCD_EVENT_UNPLUGGED,       // 3
  DCD_EVENT_SOF,             // 4
  DCD_EVENT_SUSPEND,         // 5 TODO LPM Sleep L1 support
  DCD_EVENT_RESUME,          // 6
  DCD_EVENT_SETUP_RECEIVED,  // 7
  DCD_EVENT_XFER_COMPLETE,   // 8
  USBD_EVENT_FUNC_CALL,      // 9 Not an DCD event, just a convenient way to defer ISR function
  DCD_EVENT_COUNT
} dcd_eventid_t;

#define DCD_EVENT_BUS_RESET DCD_EVENT_BUS_RESET_END // backward compatibility
```

- [ ] **Step 2: Update the log-string table in `src/device/usbd.c`**

At line 457 the table is indexed by event id and MUST stay in enum order. Replace the
`"Bus Reset",` entry (line 459) with two entries:

```c
    "Bus Reset Start",
    "Bus Reset End",
```

- [ ] **Step 3: Handle both edges in the usbd task loop**

Replace the case at `src/device/usbd.c:700`:

```c
      case DCD_EVENT_BUS_RESET:
        TU_LOG_USBD(": %s Speed\r\n", tu_str_speed[event.bus_reset.speed]);
        usbd_reset(event.rhport);
        _usbd_dev.speed = event.bus_reset.speed;
        break;
```

with:

```c
      case DCD_EVENT_BUS_RESET_START:
        TU_LOG_USBD("\r\n");
        usbd_reset(event.rhport);
        break;

      case DCD_EVENT_BUS_RESET_END:
        TU_LOG_USBD(": %s Speed\r\n", tu_str_speed[event.bus_reset.speed]);
        // TODO a DCD that reports both edges pays for two teardowns: track a per-rhport
        // "start seen" flag and skip this reset, keeping it for the single-event DCDs.
        usbd_reset(event.rhport);
        _usbd_dev.speed = event.bus_reset.speed;
        break;
```

- [ ] **Step 4: Verify legacy ports still build (the alias must carry them)**

Run:

```bash
cd examples && cmake -B cmake-build-stm32f407disco -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . && cmake --build cmake-build-stm32f407disco
```

Expected: builds clean. This board's DCD (dwc2) still calls `dcd_event_bus_reset()`, which
now resolves to `_END` through the unchanged helper — proving the alias works.

- [ ] **Step 5: Verify the unit tests still build and pass**

Run: `cd test/unit-test && ceedling test:all`
Expected: all tests pass (they reference `DCD_EVENT_BUS_RESET` via the alias).

- [ ] **Step 6: Commit**

```bash
git add src/device/dcd.h src/device/usbd.c
git commit -m "usbd: split bus reset into start/end edge events

A DCD that can see reset signaling begin has no way to say so: the only
event carries the negotiated speed, which does not exist until the reset
ends. On ChipIdea that leaves the stack believing it is configured for the
whole reset window (3 ms minimum, tens of ms in practice) while the
controller has already torn its endpoints down.

Add DCD_EVENT_BUS_RESET_START for the leading edge and rename the existing
event to DCD_EVENT_BUS_RESET_END, keeping DCD_EVENT_BUS_RESET as an alias
so every other port and the unit tests are untouched. START is optional and
END stays self-sufficient, so single-event drivers keep working unchanged."
```

---

### Task 2: Split ci_hs `bus_reset()` across the two edges, behind one flush helper

**Files:**
- Modify: `src/portable/chipidea/ci_hs/dcd_ci_hs.c` (`bus_reset()`; `dcd_deinit()`; `dcd_edpt_iso_activate()`; the `INTR_RESET` and `INTR_PORT_CHANGE` branches of `dcd_int_handler()`)

**Interfaces:**
- Consumes: `DCD_EVENT_BUS_RESET_START` (Task 1), `dcd_event_bus_signal()`, `dcd_event_bus_reset()`.
- Produces: `static bool flush_endpoints(ci_hs_regs_t *dcd_reg, uint32_t mask)` — writes `ENDPTFLUSH = mask`, spins bounded by `CI_HS_BUSY_SPIN`, returns `true` if the bits cleared. Used by Task 3.

- [ ] **Step 1: Add the flush helper next to `bus_reset()`**

Insert above `bus_reset()`:

```c
// Flush endpoint buffers and wait for the controller to acknowledge. Callers proceed
// regardless of the result; the bound only prevents an ISR-context hang on dead hardware.
static bool flush_endpoints(ci_hs_regs_t *dcd_reg, uint32_t mask) {
  dcd_reg->ENDPTFLUSH = mask;
  uint32_t guard = CI_HS_BUSY_SPIN;
  while (dcd_reg->ENDPTFLUSH & mask) {
    if (!guard--) {
      return false;
    }
  }
  return true;
}
```

- [ ] **Step 2: Split `bus_reset()` into begin/complete**

Replace the whole `bus_reset()` function with these two. `bus_reset_begin()` keeps only
register work; `bus_reset_complete()` owns everything that touches `_dcd_data`:

```c
/// Register-side reset handling, must run inside the reset window (UM10503 25.10.3)
static void bus_reset_begin(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);

  // The reset value for all endpoint types is the control endpoint. If one endpoint
  // direction is enabled and the paired endpoint of opposite direction is disabled, then the
  // endpoint type of the unused direction must be changed from the control type to any other
  // type (e.g. bulk). Leaving an un-configured endpoint control will cause undefined behavior
  // for the data PID tracking on the active endpoint.
  const uint8_t ep_count = ci_ep_count(dcd_reg);
  for (uint8_t i = 1; i < ep_count; i++) {
    dcd_reg->ENDPTCTRL[i] = ENDPTCTRL_RESET_MASK;
  }

  //------------- Clear All Registers -------------//
  dcd_reg->ENDPTNAK       = dcd_reg->ENDPTNAK;
  dcd_reg->ENDPTNAKEN     = 0;
  dcd_reg->ENDPTSETUPSTAT = dcd_reg->ENDPTSETUPSTAT;
  dcd_reg->ENDPTCOMPLETE  = dcd_reg->ENDPTCOMPLETE;

  uint32_t guard = CI_HS_BUSY_SPIN;
  while (dcd_reg->ENDPTPRIME && guard--) {}
  flush_endpoints(dcd_reg, 0xFFFFFFFF);
}

/// Software-side reset handling, deferred to the port change ending the reset so the queue
/// heads stay coherent until the stack is told - and so a prime issued by a task that had
/// not yet seen BUS_RESET_START is flushed here rather than surviving re-enumeration.
static void bus_reset_complete(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  flush_endpoints(dcd_reg, 0xFFFFFFFF);

  //------------- Queue Head & Queue TD -------------//
  tu_memclr(&_dcd_data, sizeof(dcd_data_t));

  //------------- Set up Control Endpoints (0 OUT, 1 IN) -------------//
  _dcd_data.qhd[0][0].zero_length_termination = _dcd_data.qhd[0][1].zero_length_termination = 1;
  _dcd_data.qhd[0][0].max_packet_size = _dcd_data.qhd[0][1].max_packet_size = CFG_TUD_ENDPOINT0_SIZE;
  _dcd_data.qhd[0][0].qtd_overlay.next = _dcd_data.qhd[0][1].qtd_overlay.next = QTD_NEXT_INVALID;

  _dcd_data.qhd[0][0].int_on_setup = 1; // OUT only

  dcd_dcache_clean_invalidate(&_dcd_data, sizeof(dcd_data_t));
}
```

- [ ] **Step 3: Route the two ISR branches to the new functions**

In `dcd_int_handler()`, the `INTR_RESET` branch becomes:

```c
  if (int_status & INTR_RESET) {
    bus_reset_begin(rhport);
    _port_change_reason[rhport] = PORT_CHANGE_REASON_RESET;
    dcd_event_bus_signal(rhport, DCD_EVENT_BUS_RESET_START, true);
  }
```

and inside the `INTR_PORT_CHANGE` branch, the reset arm (the `else` of the resume test)
becomes:

```c
    } else {
      bus_reset_complete(rhport);
      // PSPD: 0 full, 1 low, 2 high, 3 undefined (treated as full)
      const uint32_t pspd = (dcd_reg->PORTSC1 & PORTSC1_PORT_SPEED) >> PORTSC1_PORT_SPEED_POS;
      const tusb_speed_t speed = (pspd == 1) ? TUSB_SPEED_LOW : (pspd == 2) ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL;
      dcd_event_bus_reset(rhport, speed, true);
    }
```

Delete the now-unused EP0 `ENDPTFLUSH` line that previously sat at the top of that arm —
`bus_reset_complete()` flushes all endpoints.

- [ ] **Step 4: Route the remaining flush sites through the helper**

In `dcd_deinit()`, replace the flush block with:

```c
  // flush all endpoints
  uint32_t guard = CI_HS_BUSY_SPIN;
  while (dcd_reg->ENDPTPRIME && guard--) {}
  flush_endpoints(dcd_reg, 0xFFFFFFFF);
```

In `dcd_edpt_iso_activate()`, replace the flush + spin with:

```c
  // Flush EP
  flush_endpoints(dcd_reg, TU_BIT(epnum + (dir ? 16 : 0)));
```

- [ ] **Step 5: Build both ci_hs board families**

Run:

```bash
cmake --build examples/cmake-build-mimxrt1064_evk && cmake --build examples/cmake-build-lpcxpresso18s37
```

Expected: both succeed with no new warnings.

- [ ] **Step 6: Commit**

```bash
git add src/portable/chipidea/ci_hs/dcd_ci_hs.c
git commit -m "dcd(ci_hs): report bus reset start at URI, finish at port change

The RM wants the reset cleanup inside the reset window, but the negotiated
speed only exists once the port reaches its operational state, so the stack
was told nothing for the whole window - it kept believing it was configured
while the queue heads had been zeroed under it, and a transfer a class
driver started in that gap stayed primed across re-enumeration.

Split the work along the register/software line: bus_reset_begin() does the
register cleanup at URI and signals BUS_RESET_START, bus_reset_complete()
re-flushes, resets the queue heads and reports BUS_RESET_END with the final
speed at the port change. Zeroing the queue heads now happens in the same
breath as telling the stack, and the second flush retires anything primed
in between.

Fold the five hand-rolled endpoint flushes into one bounded helper while
the reset path is open."
```

---

### Task 3: Make the setup-time EP0 flush wait, and stop dropping the SET_ADDRESS status prime

**Files:**
- Modify: `src/portable/chipidea/ci_hs/dcd_ci_hs.c` (`dcd_set_address()`; the `ENDPTSETUPSTAT` branch inside `dcd_int_handler()`)

**Interfaces:**
- Consumes: `flush_endpoints()` (Task 2); `qhd_start_xfer()` returning `bool`, already propagated by `dcd_edpt_xfer()`.

- [ ] **Step 1: Wait for the setup-time flush to complete**

In the ISR's setup branch, replace the fire-and-forget flush line

```c
      dcd_reg->ENDPTFLUSH = TU_BIT(0) | TU_BIT(16);
```

with

```c
      // Wait it out: the flush retires a status/handshake phase left primed by the previous
      // control sequence (UM10503 25.10.8.1.1), and an unfinished flush would otherwise
      // still be asserted when the task primes the response to this setup and would retire
      // that instead. A flush waits for any packet already in progress - microseconds at
      // high speed - and the guard caps wedged hardware.
      flush_endpoints(dcd_reg, TU_BIT(0) | TU_BIT(16));
```

- [ ] **Step 2: Honour the status-prime result in `dcd_set_address`**

Replace the body of `dcd_set_address()`:

```c
void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  // Response with status first before changing device address. A refused prime means a new
  // setup superseded this transfer; staging an address whose ACK will never arrive would
  // leave the device answering on it, so only arm the address when the status went out.
  if (dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0, false)) {
    ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
    dcd_reg->DEVICEADDR   = (dev_addr << 25) | TU_BIT(24);
  }
}
```

- [ ] **Step 3: Build and commit**

Run: `cmake --build examples/cmake-build-mimxrt1064_evk && cmake --build examples/cmake-build-lpcxpresso18s37`
Expected: both succeed.

```bash
git add src/portable/chipidea/ci_hs/dcd_ci_hs.c
git commit -m "dcd(ci_hs): wait out the setup flush, honour the set-address prime

The flush issued on every new setup was fire-and-forget. A flush waits for
a packet already in progress, so it could still be asserted when the task
primed the response to that setup and retire the fresh prime instead -
leaving EP0 silent until the host gave up.

dcd_set_address() also armed DEVICEADDR unconditionally, but the status
prime can now be refused when a newer setup supersedes the transfer; the
address was then staged behind an ACK that never came and the device sat at
address 0. Only arm it when the status transfer actually started."
```

---

### Task 4: Emit RESUME only when the port really left suspend

**Files:**
- Modify: `src/portable/chipidea/ci_hs/dcd_ci_hs.c` (the resume arm of the `INTR_PORT_CHANGE` branch in `dcd_int_handler()`)

**Interfaces:** none consumed or produced.

- [ ] **Step 1: Restore the hardware guard**

In the `INTR_PORT_CHANGE` branch, the resume arm currently reads:

```c
    if (pci_reason == PORT_CHANGE_REASON_SUSPEND) {
      dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
    } else {
```

Replace that condition with one that also consults live hardware:

```c
    if (pci_reason == PORT_CHANGE_REASON_SUSPEND) {
      // Only when the port actually left suspend: a starved snapshot can hold the resume's
      // port change together with a second suspend, and reporting a resume there would
      // leave the stack awake on a sleeping bus with no further event to correct it.
      if (!(dcd_reg->PORTSC1 & PORTSC1_SUSPEND)) {
        dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
      }
    } else {
```

- [ ] **Step 2: Build and commit**

Run: `cmake --build examples/cmake-build-mimxrt1064_evk && cmake --build examples/cmake-build-lpcxpresso18s37`
Expected: both succeed.

```bash
git add src/portable/chipidea/ci_hs/dcd_ci_hs.c
git commit -m "dcd(ci_hs): only report resume when the port left suspend

A suspend, resume and second suspend collapsed into one interrupt pass
queued suspend then resume from the recorded cause alone, so the stack
ended up awake while the bus was still suspended and nothing arrived to
correct it. Consult PORTSC1 before reporting the resume."
```

---

### Task 5: ip3511 — never deliver a knowingly-torn setup packet

**Files:**
- Modify: `src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c` (setup branch of `dcd_int_handler()`; the `dcd_edpt_clear_stall()` comment)

**Interfaces:** none consumed or produced.

- [ ] **Step 1: Deliver only when the copy is known good**

Replace:

```c
    // a SETUP that raced in after the acks (its bit0 consumed above, this copy possibly torn):
    // its latch is visible again - re-raise the endpoint interrupt so the next pass redelivers
    // the newer payload
    if (dcd_reg->DEVCMDSTAT & DEVCMDSTAT_SETUP_RECEIVED_MASK) {
      dcd_reg->INTSETSTAT = TU_BIT(0);
    }

    dcd_event_setup_received(rhport, setup_copy, true);
```

with:

```c
    // a SETUP that raced in after the acks (its bit0 consumed above) makes this copy suspect:
    // its latch is visible again, so re-raise the endpoint interrupt and let the next pass
    // deliver the newer payload rather than passing up bytes that may be torn between the two
    if (dcd_reg->DEVCMDSTAT & DEVCMDSTAT_SETUP_RECEIVED_MASK) {
      dcd_reg->INTSETSTAT = TU_BIT(0);
    } else {
      dcd_event_setup_received(rhport, setup_copy, true);
    }
```

- [ ] **Step 2: Add the TODO token to the USB.13 deferral**

In `dcd_edpt_clear_stall()`, change the caveat's opening line from

```c
  // Known caveat (Errata LPC546xx USB.13, same semantics in UM11126): with RF/TV preserved at 1, TR
```

to

```c
  // TODO implement the Errata LPC546xx USB.13 work-around (same semantics in UM11126): with RF/TV preserved at 1, TR
```

- [ ] **Step 3: Build and commit**

Run: `cmake --build examples/cmake-build-lpcxpresso11u37 && cmake --build examples/cmake-build-lpcxpresso55s28`
Expected: both succeed.

```bash
git add src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c
git commit -m "dcd(ip3511): drop a setup packet the hardware may have overwritten

The handler already notices when a new setup landed while it was copying
the previous one, and re-raises the endpoint interrupt so the newer payload
is delivered next pass - but it then passed the suspect copy up anyway.
Usually harmless, since the redelivery supersedes it, but if that second
event cannot be queued the torn bytes are processed as a real request.
Deliver the copy only when no newer setup is pending."
```

---

### Task 6: BSP cleanups — dead RHPORT block and the stale linker comment

**Files:**
- Modify: `hw/bsp/lpc55/boards/lpcxpresso55s28/board.cmake`
- Modify: `hw/bsp/lpc11/boards/lpcxpresso11u37/lpc11u37.ld`

**Interfaces:** none consumed or produced.

- [ ] **Step 1: Delete the redundant RHPORT block**

`hw/bsp/lpc55/family.cmake` already applies the identical guarded defaults (`RHPORT_DEVICE 1`,
`RHPORT_HOST 0`) after including the board file, so remove these lines from
`board.cmake` entirely:

```cmake
# device highspeed, host fullspeed; guarded so a -D override on the cmake command line wins
if (NOT DEFINED RHPORT_DEVICE)
  set(RHPORT_DEVICE 1)
endif ()
if (NOT DEFINED RHPORT_HOST)
  set(RHPORT_HOST 0)
endif ()
```

Leave `board.mk`'s `RHPORT_DEVICE ?= 1` / `RHPORT_HOST ?= 0` alone — `?=` is the idiomatic
Make form and matches sibling boards.

- [ ] **Step 2: Prove the defaults and the override still work**

Run:

```bash
cd examples && rm -rf /tmp/rh-default /tmp/rh-override
cmake -B /tmp/rh-default -DBOARD=lpcxpresso55s28 -G Ninja . > /tmp/rh-default.log 2>&1
grep -m1 "RHPORT_DEVICE" /tmp/rh-default.log || cmake -B /tmp/rh-default -DBOARD=lpcxpresso55s28 -G Ninja -LA . | grep -E "^RHPORT_(DEVICE|HOST)"
cmake -B /tmp/rh-override -DBOARD=lpcxpresso55s28 -DRHPORT_DEVICE=0 -DRHPORT_HOST=1 -G Ninja -LA . | grep -E "^RHPORT_(DEVICE|HOST)"
```

Expected: the default configure yields device 1 / host 0; the override configure yields
device 0 / host 1. Then rebuild the real tree: `cmake --build cmake-build-lpcxpresso55s28`.

- [ ] **Step 3: Correct the linker-script comment and relabel the ASSERT**

In `lpc11u37.ld`, replace the comment block above `__user_stack_top` and the ASSERT with:

```text
    /* Main (MSP/ISR) stack lives at the top of the USB SRAM bank: the 8K main bank is packed so
       tight that only ~280 B remained above .bss, and ISR frames overflowed into the topmost task
       stack (cdc_msc_freertos hard fault). Nothing else is placed in this bank in either build
       system, so the stack owns all 2 KB; the ASSERT is future-proofing in case USB buffers are
       ever mapped here again. */
    __user_stack_top = ORIGIN(RamUsb2) + LENGTH(RamUsb2);
    ASSERT(__user_stack_top - (ADDR(.noinit_RAM2) + SIZEOF(.noinit_RAM2)) >= 0x200,
           "main stack headroom in RamUsb2 below 512 bytes")
```

- [ ] **Step 4: Build both build systems for lpc11u37**

Run:

```bash
cmake --build examples/cmake-build-lpcxpresso11u37
cd examples/device/cdc_msc_freertos && make -j8 BOARD=lpcxpresso11u37 all && cd ../../..
```

Expected: both succeed.

- [ ] **Step 5: Commit**

```bash
git add hw/bsp/lpc55/boards/lpcxpresso55s28/board.cmake hw/bsp/lpc11/boards/lpcxpresso11u37/lpc11u37.ld
git commit -m "bsp: drop duplicated lpc55s28 rhport defaults, fix lpc11u37 comment

hw/bsp/lpc55/family.cmake already applies the same guarded rhport defaults
after including the board file, so the board-level copy only added a second
place to keep in sync.

The lpc11u37 linker comment still described USB buffers living in RamUsb2,
a placement the same branch removed; nothing lands there now, so say so and
label the headroom assert as future-proofing."
```

---

### Task 7: Stop halting the target when a DCD legitimately refuses a transfer

**Files:**
- Modify: `src/device/usbd.c` (`usbd_edpt_xfer()` failure arm)

**Interfaces:** none consumed or produced.

- [ ] **Step 1: Remove the breakpoint from the DCD-refusal path**

Replace the failure arm of `usbd_edpt_xfer()`:

```c
  } else {
    // DCD error, mark endpoint as ready to allow next transfer
    _usbd_dev.ep_status[epnum][dir] &= (uint8_t) ~(TU_EDPT_STATE_BUSY | TU_EDPT_STATE_CLAIMED);
    TU_LOG_USBD("FAILED\r\n");
    TU_BREAKPOINT();
    return false;
  }
```

with:

```c
  } else {
    // Driver refused the transfer, mark endpoint as ready to allow next transfer. This is a
    // recoverable condition (e.g. a new setup superseding a control response), not a bug, so
    // do not break into the debugger - TU_BREAKPOINT() halts the CPU whenever a probe is
    // attached, which on a test rig is always.
    _usbd_dev.ep_status[epnum][dir] &= (uint8_t) ~(TU_EDPT_STATE_BUSY | TU_EDPT_STATE_CLAIMED);
    TU_LOG_USBD("FAILED\r\n");
    return false;
  }
```

- [ ] **Step 2: Confirm no other stack path relies on that breakpoint**

Run: `grep -n "TU_BREAKPOINT" src/device/*.c src/device/*.h`
Expected: no remaining hits inside `usbd_edpt_xfer`; other occurrences (if any) are in
unrelated assert macros and stay as they are.

- [ ] **Step 3: Build and run unit tests**

Run:

```bash
cmake --build examples/cmake-build-mimxrt1064_evk
cd test/unit-test && ceedling test:all && cd ../..
```

Expected: build succeeds, all unit tests pass.

- [ ] **Step 4: Commit**

```bash
git add src/device/usbd.c
git commit -m "usbd: do not breakpoint when a driver refuses a transfer

TU_BREAKPOINT() is not gated on CFG_TUSB_DEBUG - it halts the CPU whenever
a debugger is attached, which on a test rig is always. A driver declining a
transfer is recoverable (a new setup superseding a control response, for
one) and the endpoint is already released for the retry, so a halted target
turns a self-healing case into a dead board."
```

---

### Task 8: Full validation on hardware

**Files:** none modified — this task produces the evidence for the PR description.

**Interfaces:** consumes the firmware built by Tasks 1-7.

- [ ] **Step 1: Software gate**

Run:

```bash
pre-commit run --all-files
cd examples
for b in mimxrt1064_evk lpcxpresso18s37 lpcxpresso11u37 lpcxpresso55s28; do
  rm -rf cmake-build-$b && cmake -B cmake-build-$b -DBOARD=$b -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . && cmake --build cmake-build-$b || echo "FAILED $b"
done
cd ..
```

Expected: pre-commit all green; all four boards build every example.

- [ ] **Step 2: Make-build regression checks**

Run:

```bash
cd examples/host/cdc_msc_hid && make -j8 BOARD=lpcxpresso55s28 all && cd ../../..
cd examples/device/cdc_msc_throughput && make -j8 BOARD=lpcxpresso11u37 all && cd ../../..
```

Expected: both link (these two were broken earlier in the branch and are the regression
canaries for the BSP changes).

- [ ] **Step 3: Flash with verification (mandatory)**

The mimxrt1064_evk has twice accepted a flash that silently did not take, so every load in
this task uses `verifyfile`. For each board, write a J-Link script of this shape and run it:

```
r
h
loadfile examples/cmake-build-<board>/device/usbtest/usbtest.elf
verifyfile examples/cmake-build-<board>/device/usbtest/usbtest.elf
r
g
qc
```

Probes and devices: `mimxrt1064_evk` = `-USB 000725299165 -device MIMXRT1064xxx6A`,
`lpcxpresso55s28` = `-USB 000727031389 -device LPC55S28`,
`lpcxpresso11u37` = `-USB 000724441579 -device LPC11U37/401`.
Invoke as `JLinkExe <probe/device args> -if swd -speed 4000 -autoconnect 1 -NoGui 1 -CommandFile <script>`.
Expected: `Verify` reports O.K. and the board re-enumerates as `cafe:4010` with its own
serial before any test runs.

- [ ] **Step 4: HIL batteries and stress**

Hold each board's lock for its own leg (`python3 test/hil/hil_lock.py hold <board> --reason "reset-edge validation"`,
release after), never run two batteries at once, and abort if CI is active
(`pgrep -f "hil_test.py [-]-retry"`).

```bash
# per board: full battery
timeout 700 python3 test/hil/usbtest.py --serial <serial> --json --keep-binding --timeout 60

# mimxrt1064_evk only: queued-control stress and the unlink storm
for i in $(seq 1 50); do timeout 200 python3 test/hil/usbtest.py --serial BAE96FB95AFA6DBB8F00005002001200 --tests 9,10 --json --keep-binding --timeout 60 > /dev/null || break; done
for i in $(seq 1 10); do timeout 300 python3 test/hil/usbtest.py --serial BAE96FB95AFA6DBB8F00005002001200 --tests 11,12,24 --json --keep-binding --timeout 60 > /dev/null || break; done
```

Serials: 1064 `BAE96FB95AFA6DBB8F00005002001200`, 55s28 `2BF1839A7D51F553A15AB03FD08F70AB`,
11u37 `17121919`.
Expected: 30/30 on all three boards, 50/50 and 10/10 loops, and
`ps -eo stat,comm | awk '$1 ~ /^D/'` empty after each leg.

- [ ] **Step 5: Reset-path evidence with logging**

Build and flash `device/cdc_msc` for `mimxrt1064_evk` with `-DLOG=2 -DLOGGER=rtt`, capture
RTT during one unplug/replug cycle (`timeout 20s JLinkRTTClient > /tmp/reset.log`), then:

```bash
grep -cE "Bus Reset Start" /tmp/reset.log
grep -cE "Bus Reset End" /tmp/reset.log
grep -c "Resume" /tmp/reset.log
```

Expected: equal non-zero counts for start and end (one pair per enumeration) and no
`Resume` lines during a plain plug-in.

- [ ] **Step 6: Suspend/resume pairing**

With the same RTT build attached, suspend the port from the host and resume it:

```bash
# find the 1064's busport, then:
echo auto | sudo tee /sys/bus/usb/devices/<busport>/power/control
sleep 5
echo on | sudo tee /sys/bus/usb/devices/<busport>/power/control
```

Expected in the log: one `Suspend` followed by one `Resume`, and no `Bus Reset` of either
edge from the suspend cycle alone.

- [ ] **Step 7: Record the evidence**

Append the numbers from Steps 1-6 to the PR description draft. No commit.

## Self-Review

**Spec coverage:** §1 event split → Task 1. §2 ci_hs bus_reset split → Task 2. §3 flush
helper → Task 2 (Steps 1, 4). §4 mechanical: setup-flush wait and `dcd_set_address` → Task 3;
RESUME guard → Task 4; ip3511 torn setup and USB.13 TODO → Task 5; usbd breakpoint → Task 7;
BSP pair → Task 6. Verification matrix → Task 8 (legacy-DCD build guard is Task 1 Step 4).
Deferred items are deliberately absent from every task. No gaps.

**Placeholder scan:** no TBD/TODO-as-placeholder; the two literal `TODO` strings are
deliverable code comments (Task 1 Step 3, Task 5 Step 2). Every code step carries the exact
text to write; every run step carries the command and expected result.

**Type consistency:** `flush_endpoints(ci_hs_regs_t *dcd_reg, uint32_t mask) -> bool` is
defined in Task 2 Step 1 and used with that exact signature in Task 2 Steps 2/4 and Task 3
Step 1. `DCD_EVENT_BUS_RESET_START` / `_END` are defined in Task 1 and used in Task 2 Step 3
via `dcd_event_bus_signal()` / `dcd_event_bus_reset()`, whose signatures are quoted in Task 1's
Interfaces block. `bus_reset_begin()` / `bus_reset_complete()` are defined and called with
matching names in Task 2.
