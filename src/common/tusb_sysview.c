/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUD_SYSVIEW || CFG_TUH_SYSVIEW

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/tusb_common.h"     // TU_ARRAY_SIZE
#include "common/tusb_sysview.h"

#if CFG_TUSB_OS == OPT_OS_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
extern const SEGGER_SYSVIEW_OS_API SYSVIEW_X_OS_TraceAPI;
#define SYSVIEW_OS_API  (&SYSVIEW_X_OS_TraceAPI)
#define SYSVIEW_OS_DESC ",O=FreeRTOS"
#else
#define SYSVIEW_OS_API  0
#define SYSVIEW_OS_DESC ""
#endif

#ifndef SYSVIEW_APP_NAME
#define SYSVIEW_APP_NAME    "TinyUSB"
#endif
#ifndef SYSVIEW_DEVICE_NAME
#define SYSVIEW_DEVICE_NAME "Cortex-M"
#endif
#ifndef SYSVIEW_RAM_BASE
#define SYSVIEW_RAM_BASE    (0x20000000)
#endif



// CPU clock reported to SystemView (and, on DWT cores, the timestamp frequency). SystemCoreClock
// is a CMSIS global that non-CMSIS SDKs do not have -- the Pico SDK for one -- so route it through
// a weak accessor those BSPs override (hw/bsp/rp2040/family.c). Unused weak definitions are
// dropped by --gc-sections, taking the SystemCoreClock reference with them.
// Most targets' vendor CMSIS/device header already declares SystemCoreClock, but there is no
// single header this file can include to get it across vendors, so declare it and silence the
// duplicate for this line alone. Two narrower attempts do NOT work: gating on
// CFG_TUSB_OS != OPT_OS_FREERTOS (bare-metal mimxrt1064_evk pulls NXP's header too, and failed
// to build at SYSVIEW=4), and moving the extern to block scope (GCC still reports it redundant).
#if defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wredundant-decls"
#endif
extern uint32_t SystemCoreClock;
#if defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

TU_ATTR_WEAK uint32_t tusb_sysview_cpu_freq(void) {
  return SystemCoreClock;
}

// Kernel objects named for SystemView: sent immediately AND kept, so the names
// survive a recorder that attaches later (SEGGER only re-runs the
// system-description callback on connect). Names must be string literals or
// otherwise static -- only the pointer is kept.
#define TU_SV_NAMED_MAX 4
static struct {
  const void* handle;
  const char* name;
} _sv_named[TU_SV_NAMED_MAX];
static uint8_t _sv_named_count;

void tusb_sysview_name_resource(const void* handle, const char* name) {
  if (handle == NULL || name == NULL) {
    return; // silently ignored: naming is cosmetic, never worth failing a boot over
  }
  // Already registered -- e.g. tusb_deinit() + re-init (examples/dual/dynamic_switch's role
  // switch) re-registers the same static mutex handle every time. Update the name in place
  // instead of appending a duplicate: with TU_SV_NAMED_MAX this small, a few duplicates are
  // enough to permanently push out every other handle's name.
  for (uint8_t i = 0; i < _sv_named_count; i++) {
    if (_sv_named[i].handle == handle) {
      _sv_named[i].name = name;
      SEGGER_SYSVIEW_NameResource((U32) (uintptr_t) handle, name);
      return;
    }
  }
  if (_sv_named_count >= TU_SV_NAMED_MAX) {
    return; // table full and this is a genuinely new handle: drop it, cosmetic only
  }
  _sv_named[_sv_named_count].handle = handle;
  _sv_named[_sv_named_count].name = name;
  _sv_named_count++;
  // Send it now as well: tusb_rhport_init() runs tusb_sysview_init() -- and with it
  // SEGGER_SYSVIEW_Start()'s one-shot system-description callback -- BEFORE the stacks
  // create their mutexes, so registration always lands after that emission. The stored
  // copy exists for the replay on a later recorder connect, not for this first one.
  SEGGER_SYSVIEW_NameResource((U32) (uintptr_t) handle, name);
}

static void send_sys_desc(void) {
  SEGGER_SYSVIEW_SendSysDesc("N=" SYSVIEW_APP_NAME ",D=" SYSVIEW_DEVICE_NAME SYSVIEW_OS_DESC);
  SEGGER_SYSVIEW_SendSysDesc("I#15=SysTick");
  for (uint8_t i = 0; i < _sv_named_count; i++) {
    SEGGER_SYSVIEW_NameResource((U32) (uintptr_t) _sv_named[i].handle, _sv_named[i].name);
  }
}

#if defined(CFG_TUSB_SYSVIEW_TIMESTAMP_BSP) && CFG_TUSB_SYSVIEW_TIMESTAMP_BSP
  // ARMv7-M part whose DWT lacks CYCCNT (MAX32665/6: DWT_CTRL.NOCYCCNT reads 1 — a part like
  // this LINKS fine, so the fails-to-link signal below never fires; the symptom is every
  // duration silently decoding as 0). Its family.cmake builds SystemView with
  // SEGGER_SYSVIEW_CORE_OTHER so SEGGER calls SEGGER_SYSVIEW_X_GetTimestamp(), and — because
  // the fixed-1MHz microsecond contract below may be unreachable (MAX32 prescalers are
  // powers-of-two only) — the BSP reports the counter's rate as well:
  //
  //     uint32_t SEGGER_SYSVIEW_X_GetTimestamp(void);       // free-running counter
  //     uint32_t SEGGER_SYSVIEW_X_GetTimestampFreq(void);   // its rate, Hz
  extern uint32_t SEGGER_SYSVIEW_X_GetTimestampFreq(void);
  #define SYSVIEW_TIMESTAMP_FREQ SEGGER_SYSVIEW_X_GetTimestampFreq()
#elif !defined(SEGGER_SYSVIEW_CORE) || (SEGGER_SYSVIEW_CORE != SEGGER_SYSVIEW_CORE_CM3)
  // No DWT cycle counter on this core (ARMv6-M M0/M0+, RISC-V, ...), so the BSP must supply the
  // timestamp from a free-running hardware timer. Contract for hw/bsp/<family>/family.c:
  //
  //     uint32_t SEGGER_SYSVIEW_X_GetTimestamp(void);   // free-running, MICROSECONDS
  //
  // Microseconds (not core cycles) keeps one fixed frequency for every such family, so nothing
  // else has to be told which timer was used. A family without an implementation fails to link
  // naming this symbol — that is the intended "SYSVIEW not ported to this family yet" signal.
  #define SYSVIEW_TIMESTAMP_FREQ 1000000u
#else
  // DWT cycle counter: SEGGER_SYSVIEW_ConfDefaults.h reads it inline, no BSP code needed.
  #define SYSVIEW_TIMESTAMP_FREQ tusb_sysview_cpu_freq()
#endif

/* Active-interrupt id. ConfDefaults reads it from the Cortex-M ICSR itself on
 * both CM3 (bits [8:0]) and CM0 (bits [5:0]); only cores it doesn't know
 * (RISC-V here) need this hook. On RISC-V the analogous "what am I currently
 * servicing" register is mcause, whose low bits carry the trap/interrupt code
 * while an ISR runs. */
#if !defined(SEGGER_SYSVIEW_CORE) || \
    ((SEGGER_SYSVIEW_CORE != SEGGER_SYSVIEW_CORE_CM3) && (SEGGER_SYSVIEW_CORE != SEGGER_SYSVIEW_CORE_CM0))
U32 SEGGER_SYSVIEW_X_GetInterruptId(void) {
  #if defined(__riscv) || defined(__riscv__)
  uint32_t mcause;
  __asm volatile ("csrr %0, mcause" : "=r" (mcause));
  return (U32) (mcause & 0xFFFu);
  #elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_MAIN__) || \
        defined(__ARM_ARCH_8_1M_MAIN__) || defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_8M_BASE__)
  // A Cortex-M built as SEGGER_SYSVIEW_CORE_OTHER (CFG_TUSB_SYSVIEW_TIMESTAMP_BSP parts,
  // e.g. MAX3266x) lands here: same ICSR.VECTACTIVE read ConfDefaults would have inlined.
  // 9 bits on v7-M/v8-M-mainline, 6 on v6-M — 0x1FF covers both (upper bits read 0 there).
  return (*(volatile uint32_t*) 0xE000ED04u) & 0x1FFu;
  #else
  return 0; // unknown core: report "no interrupt" rather than a bogus id
  #endif
}
#endif

// Depth-counted ISR enter/exit (tusb_sysview.h's TU_SYSVIEW_ISR_ENTER/_EXIT): ten dual-role
// BSPs call tud_int_handler()+tuh_int_handler() back-to-back from ONE hardware ISR, each of
// which self-wraps -- so one real interrupt would otherwise emit ENTER,EXIT,ENTER,EXIT to
// SEGGER's recorder (double activation count, each span timed at roughly half its true
// duration). This counter only collapses NESTED enter/exit pairs, though -- two back-to-back
// (non-nested) pairs still emit twice on their own. The actual fix is two-part: each of those
// ten BSPs' vector ISR (hw/bsp/<family>/family.c) wraps its whole tud_+tuh_int_handler() body in
// one more outer TU_SYSVIEW_ISR_ENTER()/_EXIT() pair, which nests the two inner (self-wrapped)
// calls inside it -- THIS counter is what then collapses that nesting down to a single
// ENTER/EXIT. Only the outermost 0->1 transition records ENTER, matching the 1->0 transition
// that records EXIT.
//
// volatile because it is written from ISR context, and on parts whose USB IRQs nest (ch32v20x
// documents HP preempting LP) two different ISRs can reach it. The increment/decrement stay
// non-atomic: a lost update miscounts one span and self-corrects on the next balanced pair
// rather than wedging, which is not worth a critical section on every ISR entry.
static volatile uint8_t _sv_isr_depth;

void tusb_sysview_isr_enter(void) {
  if (_sv_isr_depth++ == 0) {
    SEGGER_SYSVIEW_RecordEnterISR();
  }
}

// W4: SCB->ICSR.PENDSVSET (Cortex-M0/M3+ only -- see the SEGGER_SYSVIEW_CORE guard below) tells
// us, at the moment the outermost ISR span ends, whether a context switch was requested while it
// ran (e.g. a FreeRTOS ISR-context give/notify that pends PendSV). If it was, this interrupt will
// NOT return to the task it preempted -- it falls through to PendSV and the scheduler picks the
// next task, possibly a different one. RecordExitISRToScheduler() is the record that tells
// SystemView that story; the plain RecordExitISR() below claims the interrupted task resumes
// directly, which is wrong whenever PendSV is pending -- every FreeRTOS capture showed the USB
// ISR "returning" to the interrupted task, immediately followed by a causeless switch. RISC-V
// (SEGGER_SYSVIEW_CORE_OTHER here) has no PendSV analogue and no ICSR, so it always takes the
// plain exit record.
#if defined(SEGGER_SYSVIEW_CORE) && \
    ((SEGGER_SYSVIEW_CORE == SEGGER_SYSVIEW_CORE_CM3) || (SEGGER_SYSVIEW_CORE == SEGGER_SYSVIEW_CORE_CM0))
  #define SV_ICSR_PENDSVSET_PENDING() \
    ((*(volatile uint32_t*) 0xE000ED04 /* SCB->ICSR */) & (1u << 28) /* PENDSVSET */)
#endif

void tusb_sysview_isr_exit(void) {
  if (_sv_isr_depth != 0 && --_sv_isr_depth == 0) {
#if defined(SV_ICSR_PENDSVSET_PENDING)
    if (SV_ICSR_PENDSVSET_PENDING()) {
      SEGGER_SYSVIEW_RecordExitISRToScheduler();
    } else {
      SEGGER_SYSVIEW_RecordExitISR();
    }
#else
    SEGGER_SYSVIEW_RecordExitISR();
#endif
  }
}

void tusb_sysview_init(void) {
  static bool inited = false;
  if (inited) { return; }
  inited = true;
#if defined(SEGGER_SYSVIEW_CORE) && (SEGGER_SYSVIEW_CORE == SEGGER_SYSVIEW_CORE_CM3)
  /* DWT cycle counter drives timestamps; enable on-target so recording never
   * depends on a debugger-side enable (also needed post-mortem). CYCCNT is
   * optional even on ARMv7-M: DWT_CTRL.NOCYCCNT (bit 25) reads 1 when the
   * counter is absent, in which case enabling it is a no-op and timestamps
   * would stall — check first, as SEGGER's own sample config does. */
  (*(volatile unsigned int*) 0xE000EDFC) |= (1u << 24);   /* DEMCR.TRCENA */
  if (((*(volatile unsigned int*) 0xE0001000) & (1u << 25)) == 0) {
    (*(volatile unsigned int*) 0xE0001000) |= 1u;         /* DWT_CTRL.CYCCNTENA */
  }
#endif
  SEGGER_SYSVIEW_Init(SYSVIEW_TIMESTAMP_FREQ, tusb_sysview_cpu_freq(), SYSVIEW_OS_API, send_sys_desc);
  SEGGER_SYSVIEW_SetRAMBase(SYSVIEW_RAM_BASE);
  /* Deliberately no SEGGER_SYSVIEW_RegisterModule(): SystemView 4.10b (Linux)
   * greys out File > Save Recording / Export Data as soon as ANY module is
   * registered, regardless of module content — bench-proven with 5 module
   * configurations. Function-timing
   * events are recorded at the fixed TU_SV_EVENT_BASE offset instead
   * (tusb_sysview.h) so host-side export keeps working. */
  SEGGER_SYSVIEW_Start(); /* self-start: host recorders only drain */
#ifdef SYSVIEW_DISABLE_EVENTS
  SEGGER_SYSVIEW_DisableEvents(SYSVIEW_DISABLE_EVENTS);
#endif
}

// pHeap is a handle identifying the heap, not the block being (de)allocated -- it must stay the
// same across every Define/Alloc/Free call or SystemView cannot associate them. A stable dummy
// object's address serves that purpose (its own bytes are never touched); the actual base is
// unknowable from here regardless (events still carry the real ptr+size).
static uint8_t _sv_heap_handle;

void tusb_sysview_heap_alloc(void* ptr, unsigned size) {
  // _SendPacket() (SystemView's SEGGER_SYSVIEW.c) reads SEGGER_SYSVIEW_GET_TIMESTAMP() before
  // it checks EnableState, so a call reaching SEGGER_SYSVIEW_HeapAlloc() below before
  // tusb_sysview_init() has run -- e.g. FreeRTOS's own early heap allocations -- can read a
  // still clock-gated timer (TIM2 on stm32f0/g0/u0, CT32B0 on lpc11): a bus fault. Guarded here
  // (mirrored on the free side below) instead of relying on every caller to have run
  // board_init()/tusb_sysview_init() first.
  if (!SEGGER_SYSVIEW_IsStarted()) { return; }
  static bool heap_defined = false;
  // heap_defined only ever latches true below, on a build that CAN define the heap (FreeRTOS
  // with a known configTOTAL_HEAP_SIZE); every other build leaves it false forever, so this
  // block harmlessly re-checks (and re-skips) the #if on every call -- same as before the
  // IsStarted() guard above existed, just now only reached once SystemView has actually
  // started.
  if (!heap_defined) {
#if CFG_TUSB_OS == OPT_OS_FREERTOS && defined(configTOTAL_HEAP_SIZE) && (configTOTAL_HEAP_SIZE > 0)
    // Heap capacity is the FreeRTOS heap_4/heap_5 arena size -- NOT this first allocation's own
    // size, which used to be passed here and made SystemView's "used/free" bookkeeping wrong
    // for every allocation after the first.
    SEGGER_SYSVIEW_HeapDefine(&_sv_heap_handle, &_sv_heap_handle, configTOTAL_HEAP_SIZE, 0);
    heap_defined = true;
#endif
    // else: no known capacity (heap_3, a non-FreeRTOS OS, or configTOTAL_HEAP_SIZE undefined) --
    // omit the define rather than guess; SEGGER_SYSVIEW_HeapAlloc() below still records the
    // event, SystemView just can't show a used/free percentage for it.
  }
  SEGGER_SYSVIEW_HeapAlloc(&_sv_heap_handle, ptr, size);
}

void tusb_sysview_heap_free(void* ptr) {
  if (!SEGGER_SYSVIEW_IsStarted()) { return; } // mirrors the alloc side's guard above
  SEGGER_SYSVIEW_HeapFree(&_sv_heap_handle, ptr);
}

void tusb_sysview_stack_report(void) {
#if CFG_TUSB_OS == OPT_OS_FREERTOS && (configUSE_TRACE_FACILITY == 1)
  // Same cap SEGGER's own FreeRTOS table uses (hw/bsp/family_support.cmake's
  // SYSVIEW_FREERTOS_MAX_NOF_TASKS) -- one number instead of two that can drift apart.
  // uxTaskGetSystemState() returns 0 -- not a truncated list -- when status[] is smaller than
  // the actual task count, silently emptying this table above the cap; report nothing rather
  // than guess. Raise SYSVIEW_FREERTOS_MAX_NOF_TASKS if that ceiling is hit.
  #ifndef SYSVIEW_FREERTOS_MAX_NOF_TASKS
  #define SYSVIEW_FREERTOS_MAX_NOF_TASKS 16
  #endif
  /* static, not a local array: sizeof(TaskStatus_t)*16 = 704 bytes is a large
   * fraction of a small task's stack (e.g. usbd's default 1024-byte stack).
   * An automatic array here was one contributor to a real stack-overflow
   * HardFault during USB enumeration on hardware (root-caused on same54_xplained:
   * default 1024-byte usbd/cdc FreeRTOS task stacks are marginal for a
   * SYSVIEW=4 build in general -- removing just this array was not sufficient
   * by itself; see the paired USBD_STACK_SIZE/CDC_STACK_SIZE bump in
   * examples/device/cdc_msc_freertos/src/main.c, which is the change that
   * actually restores enumeration). Moving it to .bss costs the same RAM but
   * none of the caller's stack, which is still worth doing on its own merits.
   * Single writer (usbd's periodic report, or usbh's in a host-only build --
   * never both, never reentered), so no locking is needed. */
  static TaskStatus_t status[SYSVIEW_FREERTOS_MAX_NOF_TASKS];
  /* pcTaskName points into the live TCB, which a task deleted later in the lap frees; copy the
   * name at snapshot time so SendTaskInfo() below never dereferences a dead TCB. */
  static char names[SYSVIEW_FREERTOS_MAX_NOF_TASKS][configMAX_TASK_NAME_LEN];
  /* Report one task per call instead of looping over all of them: even with
   * the array off the stack, up to SYSVIEW_FREERTOS_MAX_NOF_TASKS back-to-back
   * SEGGER_SYSVIEW_SendTaskInfo() calls (each locking + writing the RTT ring buffer) in a
   * single burst is still enough cumulative time inside this task-context call to disturb the
   * USB peripheral on hardware -- a same54_xplained
   * cdc_msc_freertos SYSVIEW=4 build enumerated fine but then reproducibly
   * dropped off the bus tens of seconds later, exactly when this call first
   * fired; disabling the call site entirely made that disappear (100 s
   * stable vs. a consistent ~30-45 s failure), isolating it to this burst.
   * Spreading the SendTaskInfo() calls across separate invocations (still one
   * every 1024 tud_task_ext events) keeps each invocation short. */
  static UBaseType_t next_idx = 0;
  // W12: uxTaskGetSystemState() suspends the scheduler for a byte-scan of every task's stack --
  // real time, even though this function only ever publishes ONE task per call. Cache its
  // result and refresh only when the rotation wraps back to index 0 (once per full lap over the
  // task list, not every call): same set of Stack Info events published, over the same rotation,
  // at roughly 1/SYSVIEW_FREERTOS_MAX_NOF_TASKS the scheduler-suspended time. Everything published
  // from a cached entry is a value copy (the name into names[] here, the rest plain integers), so a
  // task deleted mid-lap only makes its own entry stale, never a dangling dereference.
  static UBaseType_t n = 0;
  if (next_idx == 0) {
    n = uxTaskGetSystemState(status, TU_ARRAY_SIZE(status), NULL);
    for (UBaseType_t t = 0; t < n; t++) {
      strncpy(names[t], status[t].pcTaskName, configMAX_TASK_NAME_LEN - 1);
      names[t][configMAX_TASK_NAME_LEN - 1] = '\0';
    }
  }
  if (n == 0) { return; }
  UBaseType_t const i = next_idx;
  next_idx = (next_idx + 1 < n) ? next_idx + 1 : 0;

  SEGGER_SYSVIEW_TASKINFO info = {0};
  info.TaskID    = (U32)(uintptr_t) status[i].xHandle;
  info.sName     = names[i];
  info.Prio      = status[i].uxCurrentPriority;
  info.StackBase = (U32)(uintptr_t) status[i].pxStackBase;
  uint32_t const free_bytes = status[i].usStackHighWaterMark * sizeof(StackType_t);
  /* pxEndOfStack (highest valid stack address) is only in TaskStatus_t under
   * this same condition (task.h); when available, derive the true configured
   * stack depth so StackUsage is bytes used at peak rather than bytes free
   * (usStackHighWaterMark alone is headroom, the opposite of "usage"). */
  #if (portSTACK_GROWTH > 0) || (configRECORD_STACK_HIGH_ADDRESS == 1)
  uint32_t const stack_size = (uint32_t)(uintptr_t) status[i].pxEndOfStack -
                               (uint32_t)(uintptr_t) status[i].pxStackBase + sizeof(StackType_t);
  info.StackSize  = stack_size;
  info.StackUsage = (free_bytes < stack_size) ? (stack_size - free_bytes) : 0;
  #else
  // pxEndOfStack is unavailable (this port's stack does not grow upward, and
  // configRECORD_STACK_HIGH_ADDRESS is 0), so "bytes used at peak" cannot be computed here.
  // free_bytes is headroom -- the opposite of usage -- and SEGGER_SYSVIEW_TASKINFO has no
  // "unknown" sentinel for StackUsage, so there is no way to send it without it being read as
  // a (wrong) usage figure under the "stack high-water" label. Every family wired through
  // hw/bsp/sysview_freertos_hooks.h forces configRECORD_STACK_HIGH_ADDRESS=1 for exactly this
  // reason and never reaches this branch; a family that wires SYSVIEW without going through
  // that shared header can still land here, so refuse to publish rather than mislabel: skip
  // this task's Stack Info event for this cycle (next_idx above already advanced, so it is
  // retried on a later call, same as every other task in rotation).
  return;
  #endif
  SEGGER_SYSVIEW_SendTaskInfo(&info);
#endif
}

#endif /* CFG_TUD_SYSVIEW || CFG_TUH_SYSVIEW */
