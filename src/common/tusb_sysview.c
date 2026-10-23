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
// duplicate for this line alone.
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

// Kernel objects named for SystemView, kept for the system description a later recorder connect
// replays. Names must be static: only the pointer is kept.
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
  // a re-init (dynamic_switch's role switch) names the same static mutex again: update in place
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
  // sent now too: Start()'s system description already ran, before the stacks created their mutexes
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

// SCB->ICSR.PENDSVSET (Cortex-M0/M3+ only -- see the SEGGER_SYSVIEW_CORE guard below) tells
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
  // no SEGGER_SYSVIEW_RegisterModule(): see TU_SV_EVENT_BASE
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
#if CFG_TUSB_OS == OPT_OS_FREERTOS && defined(configTOTAL_HEAP_SIZE) && (configTOTAL_HEAP_SIZE > 0)
  // capacity is the heap_4/heap_5 arena; without a known one (heap_3, another OS) the events are
  // still recorded, with no used/free percentage
  static bool heap_defined = false;
  if (!heap_defined) {
    SEGGER_SYSVIEW_HeapDefine(&_sv_heap_handle, &_sv_heap_handle, configTOTAL_HEAP_SIZE, 0);
    heap_defined = true;
  }
#endif
  SEGGER_SYSVIEW_HeapAlloc(&_sv_heap_handle, ptr, size);
}

void tusb_sysview_heap_free(void* ptr) {
  if (!SEGGER_SYSVIEW_IsStarted()) { return; } // mirrors the alloc side's guard above
  SEGGER_SYSVIEW_HeapFree(&_sv_heap_handle, ptr);
}

void tusb_sysview_stack_report(void) {
// Bytes used at peak needs pxEndOfStack, which TaskStatus_t has only under this condition (task.h);
// sysview_freertos_hooks.h forces configRECORD_STACK_HIGH_ADDRESS=1. Headroom alone would read as usage.
#if CFG_TUSB_OS == OPT_OS_FREERTOS && (configUSE_TRACE_FACILITY == 1) && \
    ((portSTACK_GROWTH > 0) || (configRECORD_STACK_HIGH_ADDRESS == 1))
  // Same cap SEGGER's own FreeRTOS table uses (hw/bsp/family_support.cmake's
  // SYSVIEW_FREERTOS_MAX_NOF_TASKS) -- one number instead of two that can drift apart.
  // uxTaskGetSystemState() returns 0 -- not a truncated list -- when status[] is smaller than
  // the actual task count, silently emptying this table above the cap; report nothing rather
  // than guess. Raise SYSVIEW_FREERTOS_MAX_NOF_TASKS if that ceiling is hit.
  #ifndef SYSVIEW_FREERTOS_MAX_NOF_TASKS
  #define SYSVIEW_FREERTOS_MAX_NOF_TASKS 16
  #endif
  // static: 16 TaskStatus_t is ~700 bytes, too much for usbd's stack. Single writer (usbd's
  // periodic report, or usbh's in a host-only build), so no locking.
  static TaskStatus_t status[SYSVIEW_FREERTOS_MAX_NOF_TASKS];
  static char names[SYSVIEW_FREERTOS_MAX_NOF_TASKS][configMAX_TASK_NAME_LEN];
  // One task per call: a burst of SendTaskInfo() calls (each locking and writing the RTT ring)
  // dropped same54_xplained off the bus.
  static UBaseType_t next_idx = 0;
  // The snapshot suspends the scheduler to scan every stack, so take it once per lap. The scheduler
  // stays suspended while names are copied: vTaskDelete() of another task frees its TCB at once.
  static UBaseType_t n = 0;
  if (next_idx == 0) {
    vTaskSuspendAll();
    n = uxTaskGetSystemState(status, TU_ARRAY_SIZE(status), NULL);
    for (UBaseType_t t = 0; t < n; t++) {
      strncpy(names[t], status[t].pcTaskName, configMAX_TASK_NAME_LEN - 1);
      names[t][configMAX_TASK_NAME_LEN - 1] = '\0';
    }
    (void) xTaskResumeAll();
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
  uint32_t const stack_size = (uint32_t)(uintptr_t) status[i].pxEndOfStack -
                               (uint32_t)(uintptr_t) status[i].pxStackBase + sizeof(StackType_t);
  info.StackSize  = stack_size;
  info.StackUsage = (free_bytes < stack_size) ? (stack_size - free_bytes) : 0;
  SEGGER_SYSVIEW_SendTaskInfo(&info);
#endif
}

#endif /* CFG_TUD_SYSVIEW || CFG_TUH_SYSVIEW */
