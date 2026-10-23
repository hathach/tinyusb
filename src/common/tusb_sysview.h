/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_SYSVIEW_H_
#define TUSB_SYSVIEW_H_

#include "tusb_option.h"
#include "common/tusb_compiler.h"   // TU_XSTRCAT
#include "common/tusb_verify.h"     // TU_ASSERT, TU_MESS_FAILED, TU_BREAKPOINT (TUD_SYSVIEW_ASSERT)

#ifdef __cplusplus
 extern "C" {
#endif

#define TU_SYSVIEW_ENABLED (CFG_TUD_SYSVIEW || CFG_TUH_SYSVIEW)

// Category levels — overridable, must expand to a literal 1..4. Defined
// unconditionally (not gated on TU_SYSVIEW_ENABLED) so usbd.h/usbh.h can
// compare CFG_TUD_SYSVIEW/CFG_TUH_SYSVIEW against them even when SYSVIEW is
// fully disabled; an undefined macro reads as 0 in #if, which would make
// "CFG_TUD_SYSVIEW >= CFG_TUSB_SYSVIEW_LEVEL_ISR" true (0 >= 0) instead of
// false when the level macro is otherwise unavailable.
#ifndef CFG_TUSB_SYSVIEW_LEVEL_ISR
  #define CFG_TUSB_SYSVIEW_LEVEL_ISR    1   // USB interrupt enter/exit
#endif
#ifndef CFG_TUSB_SYSVIEW_LEVEL_USB
  #define CFG_TUSB_SYSVIEW_LEVEL_USB    2   // usbd/usbh core functions
#endif
#ifndef CFG_TUSB_SYSVIEW_LEVEL_PORT
  #define CFG_TUSB_SYSVIEW_LEVEL_PORT   3   // dcd/hcd API (wrapped at usbd/usbh call sites)
#endif
#ifndef CFG_TUSB_SYSVIEW_LEVEL_CLASS
  #define CFG_TUSB_SYSVIEW_LEVEL_CLASS  4   // class driver API
#endif

#if TU_SYSVIEW_ENABLED
// SEGGER.h defines INLINE (under #ifndef) and leaves it defined; nothing here uses it, but a
// vendor SDK header included later in the same TU (lpcopen's lpc_types.h, unconditional
// #define INLINE inline) redefines it and fails -Werror. Drop only the copy SEGGER.h added --
// an INLINE the SDK defined earlier is the SDK's to keep.
#ifdef INLINE
  #define TU_SV_INLINE_PREDEFINED
#endif
#include "SEGGER_SYSVIEW.h"
#ifndef TU_SV_INLINE_PREDEFINED
  #undef INLINE
#endif
#undef TU_SV_INLINE_PREDEFINED

// Function-timing event ids, recorded as TU_SV_EVENT_BASE + id (see below).
// Not registered as a SEGGER_SYSVIEW_MODULE (SystemView shows them as raw
// "Event(txxx)" instead of a name) — kept here, in this fixed order, and named host-side by
// test/hil/helper/sysview_events.json (agentrc's sysview_report.py --event-map):
//   0 tud_task, 1 usbd_edpt_xfer, 2 dcd_edpt_xfer, 3 tud_cdc_write_flush,
//   4 tud_cdc_read, 5 mscd_xfer_cb, 6 tuh_task, 7 hcd_edpt_xfer
typedef enum {
  TU_SV_ID_TUD_TASK = 0,   // one usbd event processed        (level USB)
  TU_SV_ID_USBD_XFER,      // usbd_edpt_xfer                  (level USB)
  TU_SV_ID_DCD_XFER,       // dcd_edpt_xfer call               (level PORT)
  TU_SV_ID_CDC_FLUSH,      // tud_cdc_n_write_flush           (level CLASS)
  TU_SV_ID_CDC_READ,       // tud_cdc_n_read                  (level CLASS)
  TU_SV_ID_MSC_XFER,       // mscd_xfer_cb                    (level CLASS)
  TU_SV_ID_TUH_TASK,       // one usbh event processed        (level USB)
  TU_SV_ID_HCD_XFER,       // hcd_edpt_xfer call               (level PORT)
  TU_SV_ID_COUNT
} tu_sysview_id_t;

void tusb_sysview_init(void);
void tusb_sysview_stack_report(void);

// Give a kernel object (mutex, semaphore) a name in SystemView, so a blocked task reads
// "waiting on usbd_mutex" instead of a bare address. Sent now and kept for the system description
// a later recorder connect replays.
void tusb_sysview_name_resource(const void* handle, const char* name);

// CPU clock reported to SystemView. Weak default returns the CMSIS SystemCoreClock; BSPs whose
// SDK has no such global (e.g. rp2040's Pico SDK) override it in hw/bsp/<family>/family.c.
uint32_t tusb_sysview_cpu_freq(void);

// FreeRTOSConfig.h's traceMALLOC/traceFREE hooks declare these too, for TUs that never include
// this header; the shared guard avoids -Wredundant-decls.
#ifndef TU_SYSVIEW_HEAP_HOOKS_DECLARED
#define TU_SYSVIEW_HEAP_HOOKS_DECLARED
void tusb_sysview_heap_alloc(void* ptr, unsigned size);
void tusb_sysview_heap_free(void* ptr);
#endif

// Fixed event base instead of a registered SEGGER_SYSVIEW_MODULE: SystemView 4.10b on Linux greys
// out Save Recording / Export Data once any module is registered. 512 is MODULE_EVENT_OFFSET, the
// id SEGGER_SYSVIEW_RegisterModule() would give the first module.
#define TU_SV_EVENT_BASE 512

// Per-level backends: _TUD_SV_CALL_<n> is live iff CFG_TUD_SYSVIEW >= n.
// TU_LOG-style: TUD_SYSVIEW_CALL(level, id) token-pastes to the backend, so a
// site whose level exceeds the config expands to nothing. The level argument
// is one of the CFG_TUSB_SYSVIEW_LEVEL_* macros (expands to 1..4 first).
#define _TU_SV_RECORD(_id) SEGGER_SYSVIEW_RecordVoid(TU_SV_EVENT_BASE + (_id))
#define _TU_SV_END(_id)    SEGGER_SYSVIEW_RecordEndCall(TU_SV_EVENT_BASE + (_id))

#if CFG_TUD_SYSVIEW >= 1
  #define _TUD_SV_CALL_1(_id) _TU_SV_RECORD(_id)
  #define _TUD_SV_RET_1(_id)  _TU_SV_END(_id)
#else
  #define _TUD_SV_CALL_1(_id)
  #define _TUD_SV_RET_1(_id)
#endif
#if CFG_TUD_SYSVIEW >= 2
  #define _TUD_SV_CALL_2(_id) _TU_SV_RECORD(_id)
  #define _TUD_SV_RET_2(_id)  _TU_SV_END(_id)
#else
  #define _TUD_SV_CALL_2(_id)
  #define _TUD_SV_RET_2(_id)
#endif
#if CFG_TUD_SYSVIEW >= 3
  #define _TUD_SV_CALL_3(_id) _TU_SV_RECORD(_id)
  #define _TUD_SV_RET_3(_id)  _TU_SV_END(_id)
#else
  #define _TUD_SV_CALL_3(_id)
  #define _TUD_SV_RET_3(_id)
#endif
#if CFG_TUD_SYSVIEW >= 4
  #define _TUD_SV_CALL_4(_id) _TU_SV_RECORD(_id)
  #define _TUD_SV_RET_4(_id)  _TU_SV_END(_id)
#else
  #define _TUD_SV_CALL_4(_id)
  #define _TUD_SV_RET_4(_id)
#endif
#if CFG_TUH_SYSVIEW >= 1
  #define _TUH_SV_CALL_1(_id) _TU_SV_RECORD(_id)
  #define _TUH_SV_RET_1(_id)  _TU_SV_END(_id)
#else
  #define _TUH_SV_CALL_1(_id)
  #define _TUH_SV_RET_1(_id)
#endif
#if CFG_TUH_SYSVIEW >= 2
  #define _TUH_SV_CALL_2(_id) _TU_SV_RECORD(_id)
  #define _TUH_SV_RET_2(_id)  _TU_SV_END(_id)
#else
  #define _TUH_SV_CALL_2(_id)
  #define _TUH_SV_RET_2(_id)
#endif
#if CFG_TUH_SYSVIEW >= 3
  #define _TUH_SV_CALL_3(_id) _TU_SV_RECORD(_id)
  #define _TUH_SV_RET_3(_id)  _TU_SV_END(_id)
#else
  #define _TUH_SV_CALL_3(_id)
  #define _TUH_SV_RET_3(_id)
#endif
#if CFG_TUH_SYSVIEW >= 4
  #define _TUH_SV_CALL_4(_id) _TU_SV_RECORD(_id)
  #define _TUH_SV_RET_4(_id)  _TU_SV_END(_id)
#else
  #define _TUH_SV_CALL_4(_id)
  #define _TUH_SV_RET_4(_id)
#endif

#define TUD_SYSVIEW_CALL(_level, _id) TU_XSTRCAT(_TUD_SV_CALL_, _level)(_id)
#define TUD_SYSVIEW_RET(_level, _id)  TU_XSTRCAT(_TUD_SV_RET_, _level)(_id)
#define TUH_SYSVIEW_CALL(_level, _id) TU_XSTRCAT(_TUH_SV_CALL_, _level)(_id)
#define TUH_SYSVIEW_RET(_level, _id)  TU_XSTRCAT(_TUH_SV_RET_, _level)(_id)

// ISR wrap serves the shared tusb_int_handler entry (device and/or host). Routed through a
// depth-counted pair of functions (tusb_sysview.c), not straight to SEGGER_SYSVIEW_Record*ISR():
// ten dual-role BSPs call tud_int_handler()+tuh_int_handler() back-to-back from ONE hardware
// ISR, and family_support.cmake keeps CFG_TUD_SYSVIEW == CFG_TUH_SYSVIEW, so both self-wrap and
// one real interrupt would otherwise emit ENTER,EXIT,ENTER,EXIT -- double activation count, each
// span timed at roughly half its true duration. The depth counter alone only fixes NESTED
// pairs, though: those ten BSPs' vector ISRs (hw/bsp/<family>/family.c) each add one more outer
// ENTER/EXIT bracket around the whole tud_+tuh_int_handler() body, which is what makes the two
// self-wrapped inner calls nest inside it for this counter to collapse.
#if (CFG_TUD_SYSVIEW >= CFG_TUSB_SYSVIEW_LEVEL_ISR) || (CFG_TUH_SYSVIEW >= CFG_TUSB_SYSVIEW_LEVEL_ISR)
  void tusb_sysview_isr_enter(void);
  void tusb_sysview_isr_exit(void);
  #define TU_SYSVIEW_ISR_ENTER() tusb_sysview_isr_enter()
  #define TU_SYSVIEW_ISR_EXIT()  tusb_sysview_isr_exit()
#else
  #define TU_SYSVIEW_ISR_ENTER()
  #define TU_SYSVIEW_ISR_EXIT()
#endif

#else // !TU_SYSVIEW_ENABLED

#define TU_SYSVIEW_ISR_ENTER()
#define TU_SYSVIEW_ISR_EXIT()
#define TUD_SYSVIEW_CALL(_level, _id)
#define TUD_SYSVIEW_RET(_level, _id)
#define TUH_SYSVIEW_CALL(_level, _id)
#define TUH_SYSVIEW_RET(_level, _id)
#define tusb_sysview_init()
#define tusb_sysview_stack_report()

#endif // TU_SYSVIEW_ENABLED

// TU_ASSERT that closes the instrumented function's CALL with its RET before returning, so a failed
// assertion leaves no dangling pair. _cond is evaluated once and an application's TU_ASSERT override
// still applies: one that continues instead of returning gets the CALL reopened. _ret is empty for a
// void function.
#if CFG_TUD_SYSVIEW
#define TUD_SYSVIEW_ASSERT(_cond, _level, _id, _ret) \
  do { if (!(_cond)) { TUD_SYSVIEW_RET(_level, _id); TU_ASSERT(false, _ret); TUD_SYSVIEW_CALL(_level, _id); } } while (0)
#else
#define TUD_SYSVIEW_ASSERT(_cond, _level, _id, _ret) TU_ASSERT(_cond, _ret)
#endif
#if CFG_TUH_SYSVIEW
#define TUH_SYSVIEW_ASSERT(_cond, _level, _id, _ret) \
  do { if (!(_cond)) { TUH_SYSVIEW_RET(_level, _id); TU_ASSERT(false, _ret); TUH_SYSVIEW_CALL(_level, _id); } } while (0)
#else
#define TUH_SYSVIEW_ASSERT(_cond, _level, _id, _ret) TU_ASSERT(_cond, _ret)
#endif

#ifdef __cplusplus
 }
#endif

#endif // TUSB_SYSVIEW_H_
