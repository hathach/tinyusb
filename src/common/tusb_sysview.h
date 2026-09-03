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
// "Event(txxx)" instead of a name) — kept here, in this fixed order, so
// Task 5's host-side reporter (sysview_report.py) can map id -> name itself:
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

// Give a kernel object (mutex, semaphore) a name in SystemView, so a blocked task
// reads "waiting on usbd_mutex" instead of a bare address. Registered, not sent
// immediately: objects are created in tud_init()/tuh_init(), long before the
// recorder attaches, and SEGGER only replays the system-description callback on
// connect -- a name sent at creation time is gone from the ring by then.
void tusb_sysview_name_resource(const void* handle, const char* name);

// CPU clock reported to SystemView. Weak default returns the CMSIS SystemCoreClock; BSPs whose
// SDK has no such global (e.g. rp2040's Pico SDK) override it in hw/bsp/<family>/family.c.
uint32_t tusb_sysview_cpu_freq(void);

// FreeRTOSConfig.h's traceMALLOC/traceFREE hooks (Task 4) declare these too, for
// TUs that never include this header; shared guard avoids -Wredundant-decls
// regardless of which header a given TU includes first.
#ifndef TU_SYSVIEW_HEAP_HOOKS_DECLARED
#define TU_SYSVIEW_HEAP_HOOKS_DECLARED
void tusb_sysview_heap_alloc(void* ptr, unsigned size);
void tusb_sysview_heap_free(void* ptr);
#endif

// Fixed event base instead of a registered SEGGER_SYSVIEW_MODULE: SystemView
// 4.10b on Linux greys out File > Save Recording / Export Data as soon as ANY
// module is registered (bench-proven with 5 module configurations spanning
// content/callback/ordering/description-file), which silently kills every
// export. 512 == MODULE_EVENT_OFFSET, the value SEGGER_SYSVIEW_RegisterModule()
// would hand out to the first (and only) module here anyway
// (lib/SystemView/SYSVIEW/SEGGER_SYSVIEW.c:182),
// so recorded event ids are unchanged — only the RegisterModule() call itself
// is gone, restoring host-side export.
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

// TU_ASSERT with a SystemView RET inserted before the return, so a failing assertion inside an
// instrumented function still closes its CALL/RET pair instead of leaving it dangling for the
// decoder to splice onto a later invocation. _ret is the returned value, left empty for a void
// function. Callers bind the level/id/return once with a file-local alias so the expansion
// itself lives in exactly one place.
#if CFG_TUD_SYSVIEW
// SYSVIEW compiled in: the RET must run before the return, which means evaluating _cond exactly
// once and branching before any return happens. TU_ASSERT's own expansion (tusb_verify.h,
// itself an application override point via #ifndef TU_ASSERT) is a self-contained do/while with
// the return baked in -- calling into it and splicing a statement before its return would mean
// either re-evaluating _cond (unsafe: several call sites pass a function call with side effects,
// e.g. usbd_edpt_xfer()/prepare_cbw()) or duplicating its body. So this hard-codes TU_ASSERT's
// DEFAULT failure sequence instead of routing through an application's TU_ASSERT override --
// documented limitation, not fixed here. Concretely: an application whose TU_ASSERT override
// recovers instead of trapping (e.g. logs and continues, or resets) still hits TU_BREAKPOINT()
// at every ON site in a SYSVIEW build, so a failure a production build would have survived can
// instead halt a SYSVIEW build.
// Level-gated the same way the CALL/RET backends are (token-paste on the level): only a site
// whose level is actually compiled in needs the hard-coded sequence. Below its level the RET
// expands to nothing, so there is no reason to bypass the override -- defer to TU_ASSERT and
// keep it working. Without this, -DSYSVIEW=1 silently disabled an application's TU_ASSERT for
// all six class/USB-level sites while inserting no RET at all.
#define _TUD_SV_ASSERT_ON(_cond, _level, _id, _ret) \
  do { if (!(_cond)) { TU_MESS_FAILED(); TU_BREAKPOINT(); \
    TUD_SYSVIEW_RET(_level, _id); return _ret; } } while(0)
#define _TUD_SV_ASSERT_OFF(_cond, _level, _id, _ret) TU_ASSERT(_cond, _ret)

#define _TUD_SV_ASSERT_1 _TUD_SV_ASSERT_ON
#if CFG_TUD_SYSVIEW >= 2
  #define _TUD_SV_ASSERT_2 _TUD_SV_ASSERT_ON
#else
  #define _TUD_SV_ASSERT_2 _TUD_SV_ASSERT_OFF
#endif
#if CFG_TUD_SYSVIEW >= 3
  #define _TUD_SV_ASSERT_3 _TUD_SV_ASSERT_ON
#else
  #define _TUD_SV_ASSERT_3 _TUD_SV_ASSERT_OFF
#endif
#if CFG_TUD_SYSVIEW >= 4
  #define _TUD_SV_ASSERT_4 _TUD_SV_ASSERT_ON
#else
  #define _TUD_SV_ASSERT_4 _TUD_SV_ASSERT_OFF
#endif

#define TUD_SYSVIEW_ASSERT(_cond, _level, _id, _ret) \
  TU_XSTRCAT(_TUD_SV_ASSERT_, _level)(_cond, _level, _id, _ret)
#else
// SYSVIEW off: TUD_SYSVIEW_RET is already a no-op here, so defer entirely to TU_ASSERT --
// this is what lets an application's #define TU_ASSERT override (tusb_verify.h) take effect.
#define TUD_SYSVIEW_ASSERT(_cond, _level, _id, _ret) TU_ASSERT(_cond, _ret)
#endif

#ifdef __cplusplus
 }
#endif

#endif // TUSB_SYSVIEW_H_
