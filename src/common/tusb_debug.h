/*
 * SPDX-FileCopyrightText: Copyright (c) 2022, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_DEBUG_H_
#define TUSB_DEBUG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+

// CFG_TUSB_DEBUG for debugging
// 0 : no debug
// 1 : print error
// 2 : print warning
// 3 : print info
#if CFG_TUSB_DEBUG

// Enum to String for debugging purposes
#if CFG_TUSB_DEBUG >= CFG_TUH_LOG_LEVEL || CFG_TUSB_DEBUG >= CFG_TUD_LOG_LEVEL
extern char const* const tu_str_speed[];
extern char const* const tu_str_std_request[];
extern char const* const tu_str_xfer_result[];
#endif

void tu_print_mem(void const *buf, uint32_t count, uint8_t indent);

#ifdef CFG_TUSB_DEBUG_PRINTF
  extern int CFG_TUSB_DEBUG_PRINTF(const char *format, ...);
  #define tu_printf    CFG_TUSB_DEBUG_PRINTF
#else
  #include <stdio.h>
  #define tu_printf(...)    (void) printf(__VA_ARGS__)
#endif

TU_ATTR_ALWAYS_INLINE static inline void tu_print_buf(uint8_t const* buf, uint32_t bufsize) {
  for(uint32_t i=0; i<bufsize; i++) {
    tu_printf("%02X ", buf[i]);
  }
  tu_printf("\r\n");
}

// Log with Level
#define TU_LOG(n, ...)        TU_XSTRCAT(TU_LOG, n)(__VA_ARGS__)
#define TU_LOG_MEM(n, ...)    TU_XSTRCAT3(TU_LOG, n, _MEM)(__VA_ARGS__)
#define TU_LOG_BUF(n, ...)    TU_XSTRCAT3(TU_LOG, n, _BUF)(__VA_ARGS__)
#define TU_LOG_INT(n, ...)    TU_XSTRCAT3(TU_LOG, n, _INT)(__VA_ARGS__)
#define TU_LOG_HEX(n, ...)    TU_XSTRCAT3(TU_LOG, n, _HEX)(__VA_ARGS__)
#define TU_LOG_LOCATION()     tu_printf("%s: %d:\r\n", __PRETTY_FUNCTION__, __LINE__)
#define TU_LOG_FAILED()       tu_printf("%s: %d: Failed\r\n", __PRETTY_FUNCTION__, __LINE__)

// Log Level 1: Error
#define TU_LOG1               tu_printf
#define TU_LOG1_MEM           tu_print_mem
#define TU_LOG1_BUF(_x, _n)   tu_print_buf((uint8_t const*)(_x), _n)
#define TU_LOG1_INT(_x)       tu_printf(#_x " = %ld\r\n", (unsigned long) (_x) )
#define TU_LOG1_HEX(_x)       tu_printf(#_x " = 0x%lX\r\n", (unsigned long) (_x) )

// Log Level 2: Warn
#if CFG_TUSB_DEBUG >= 2
  #define TU_LOG2             TU_LOG1
  #define TU_LOG2_MEM         TU_LOG1_MEM
  #define TU_LOG2_BUF         TU_LOG1_BUF
  #define TU_LOG2_INT         TU_LOG1_INT
  #define TU_LOG2_HEX         TU_LOG1_HEX
#endif

// Log Level 3: Info
#if CFG_TUSB_DEBUG >= 3
  #define TU_LOG3             TU_LOG1
  #define TU_LOG3_MEM         TU_LOG1_MEM
  #define TU_LOG3_BUF         TU_LOG1_BUF
  #define TU_LOG3_INT         TU_LOG1_INT
  #define TU_LOG3_HEX         TU_LOG1_HEX
#endif

typedef struct {
  uint32_t key;
  const char* data;
} tu_lookup_entry_t;

typedef struct {
  uint16_t count;
  tu_lookup_entry_t const* items;
} tu_lookup_table_t;

static inline const char* tu_lookup_find(tu_lookup_table_t const* p_table, uint32_t key) {
  for(uint16_t i=0; i<p_table->count; i++) {
    if (p_table->items[i].key == key) {
      return p_table->items[i].data;
    }
  }

  #ifndef CFG_TUSB_DEBUG_PRINTF
  // not found return the key value in hex if no custom printf is defined
  static char not_found[11];
  if (snprintf(not_found, sizeof(not_found), "0x%08lX", (unsigned long)key) <= 0) {
    not_found[0] = 0;
  }
  return not_found;
  #else
  return "NotFound";
  #endif
}

#endif // CFG_TUSB_DEBUG

#ifndef TU_LOG
  #define TU_LOG(n, ...)
  #define TU_LOG_MEM(n, ...)
  #define TU_LOG_BUF(n, ...)
  #define TU_LOG_INT(n, ...)
  #define TU_LOG_HEX(n, ...)
  #define TU_LOG_LOCATION()
  #define TU_LOG_FAILED()
#endif

#define TU_LOG0(...)
#define TU_LOG0_MEM(...)
#define TU_LOG0_BUF(...)
#define TU_LOG0_INT(...)
#define TU_LOG0_HEX(...)

#ifndef TU_LOG1
  #define TU_LOG1(...)
  #define TU_LOG1_MEM(...)
  #define TU_LOG1_BUF(...)
  #define TU_LOG1_INT(...)
  #define TU_LOG1_HEX(...)
#endif

#ifndef TU_LOG2
  #define TU_LOG2(...)
  #define TU_LOG2_MEM(...)
  #define TU_LOG2_BUF(...)
  #define TU_LOG2_INT(...)
  #define TU_LOG2_HEX(...)
#endif

#ifndef TU_LOG3
  #define TU_LOG3(...)
  #define TU_LOG3_MEM(...)
  #define TU_LOG3_BUF(...)
  #define TU_LOG3_INT(...)
  #define TU_LOG3_HEX(...)
#endif

//--------------------------------------------------------------------+
// SystemView
//--------------------------------------------------------------------+

// Category levels — overridable, must expand to a literal 1..4. Defined
// unconditionally (not gated on CFG_TUSB_SYSVIEW_ENABLED) so usbd.h/usbh.h can
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

// a role's ISR spans are recorded only when its stack is enabled and its level reaches ISR
#define TUD_SYSVIEW_ISR_TRACED (CFG_TUD_ENABLED && CFG_TUD_SYSVIEW >= CFG_TUSB_SYSVIEW_LEVEL_ISR)
#define TUH_SYSVIEW_ISR_TRACED (CFG_TUH_ENABLED && CFG_TUH_SYSVIEW >= CFG_TUSB_SYSVIEW_LEVEL_ISR)

#if CFG_TUSB_SYSVIEW_ENABLED
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

// Function-timing event ids, recorded as TU_SV_EVENT_BASE + id (see below). The order is fixed:
// test/hil/helper/sysview_events.json names them host-side (agentrc's sysview_report.py --event-map).
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

void tu_sysview_init(void);
void tu_sysview_stack_report(void);

// Give a kernel object (mutex, semaphore) a name in SystemView, so a blocked task reads
// "waiting on usbd_mutex" instead of a bare address. Sent now and kept for the system description
// a later recorder connect replays.
void tu_sysview_name_resource(const void* handle, const char* name);

// CPU clock reported to SystemView. Weak default returns the CMSIS SystemCoreClock; BSPs whose
// SDK has no such global (e.g. rp2040's Pico SDK) override it in hw/bsp/<family>/family.c.
uint32_t tusb_sysview_cpu_freq(void);

// Fixed event base instead of a registered SEGGER_SYSVIEW_MODULE: SystemView 4.10b on Linux greys
// out Save Recording / Export Data once any module is registered. 512 is MODULE_EVENT_OFFSET, the
// id SEGGER_SYSVIEW_RegisterModule() would give the first module.
#define TU_SV_EVENT_BASE 512

// _level is one of the CFG_TUSB_SYSVIEW_LEVEL_* constants, so a site above the configured level
// compiles to nothing
#define _TU_SV_RECORD(_id) SEGGER_SYSVIEW_RecordVoid(TU_SV_EVENT_BASE + (_id))
#define _TU_SV_END(_id)    SEGGER_SYSVIEW_RecordEndCall(TU_SV_EVENT_BASE + (_id))

#define TUD_SYSVIEW_CALL(_level, _id) do { if (CFG_TUD_SYSVIEW >= (_level)) { _TU_SV_RECORD(_id); } } while (0)
#define TUD_SYSVIEW_RET(_level, _id)  do { if (CFG_TUD_SYSVIEW >= (_level)) { _TU_SV_END(_id); } } while (0)
#define TUH_SYSVIEW_CALL(_level, _id) do { if (CFG_TUH_SYSVIEW >= (_level)) { _TU_SV_RECORD(_id); } } while (0)
#define TUH_SYSVIEW_RET(_level, _id)  do { if (CFG_TUH_SYSVIEW >= (_level)) { _TU_SV_END(_id); } } while (0)

// ISR wrap for the shared tusb_int_handler entry, through tusb.c's depth counter
// (tu_sysview_isr_enter() has why): a dual-role BSP's nested tud_/tuh_int_handler() wraps
// record one span.
#if TUD_SYSVIEW_ISR_TRACED || TUH_SYSVIEW_ISR_TRACED
  void tu_sysview_isr_enter(void);
  void tu_sysview_isr_exit(void);
  #define TU_SYSVIEW_ISR_ENTER() tu_sysview_isr_enter()
  #define TU_SYSVIEW_ISR_EXIT()  tu_sysview_isr_exit()
#else
  #define TU_SYSVIEW_ISR_ENTER()
  #define TU_SYSVIEW_ISR_EXIT()
#endif

#else // !CFG_TUSB_SYSVIEW_ENABLED

#define TU_SYSVIEW_ISR_ENTER()
#define TU_SYSVIEW_ISR_EXIT()
#define TUD_SYSVIEW_CALL(_level, _id)
#define TUD_SYSVIEW_RET(_level, _id)
#define TUH_SYSVIEW_CALL(_level, _id)
#define TUH_SYSVIEW_RET(_level, _id)
#define tu_sysview_init()
#define tu_sysview_stack_report()
#define tu_sysview_name_resource(_handle, _name)

#endif // CFG_TUSB_SYSVIEW_ENABLED

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

#endif
