/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// SEGGER SystemView instrumentation for FreeRTOSConfig.h (SYSVIEW=<level> build). #include this
// from the bottom of FreeRTOSConfig.h, after every other config option -- CFG_TUD_SYSVIEW /
// CFG_TUH_SYSVIEW must already be visible (from tusb_option.h by way of tusb_config.h) for the
// guard below to see them, and traceMALLOC/traceFREE must be defined before any FreeRTOS header
// that reads them is included. Shared verbatim across every FreeRTOS-capable family instead of
// pasting the same block into each FreeRTOSConfig.h; hw/bsp/family_support.cmake's SYSVIEW
// detector greps for the #include line this replaces, not this file's content.
#if (defined(CFG_TUD_SYSVIEW) && CFG_TUD_SYSVIEW) || (defined(CFG_TUH_SYSVIEW) && CFG_TUH_SYSVIEW)
  #undef  INCLUDE_uxTaskPriorityGet
  #define INCLUDE_uxTaskPriorityGet       1
  #undef  INCLUDE_xTaskGetIdleTaskHandle
  #define INCLUDE_xTaskGetIdleTaskHandle  1
  // Both macros below are generic FreeRTOS.h knobs (FreeRTOS.h supplies a "#ifndef ... 0"
  // default for each, identically on every port -- verified against lib/FreeRTOS-Kernel), not
  // architecture-specific, so forcing them here is safe for any family this header is wired
  // into. tusb_sysview_stack_report() (src/common/tusb_sysview.c) needs
  // configRECORD_STACK_HIGH_ADDRESS=1 to compute true "bytes used at peak" instead of
  // (inverted) headroom; without it the number it publishes is the exact opposite of the
  // "stack high-water" label the PR-comment legend gives it. configUSE_TRACE_FACILITY=1 is
  // what makes TaskStatus_t/uxTaskGetSystemState() available at all for that same report.
  #undef  configUSE_TRACE_FACILITY
  #define configUSE_TRACE_FACILITY        1
  #undef  configRECORD_STACK_HIGH_ADDRESS
  #define configRECORD_STACK_HIGH_ADDRESS 1
  #ifndef TU_SYSVIEW_HEAP_HOOKS_DECLARED /* avoid -Werror=redundant-decls vs tusb_sysview.h */
  #define TU_SYSVIEW_HEAP_HOOKS_DECLARED
  extern void tusb_sysview_heap_alloc(void* ptr, unsigned size);
  extern void tusb_sysview_heap_free(void* ptr);
  #endif
  #define traceMALLOC(pvAddress, uiSize) tusb_sysview_heap_alloc(pvAddress, uiSize)
  #define traceFREE(pvAddress, uiSize)   tusb_sysview_heap_free(pvAddress)
  #include "SEGGER_SYSVIEW_FreeRTOS.h"
#endif
