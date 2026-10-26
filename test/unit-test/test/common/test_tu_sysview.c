/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// tusb.c's SystemView ISR depth counter, against a stub for SEGGER_SYSVIEW_Record{Enter,Exit}ISR().
// A dual-role BSP's vector ISR calls tud_int_handler() and tuh_int_handler() back to back, each
// self-wrapped: its outer ENTER/EXIT bracket nests them so the counter records one span, while two
// unbracketed pairs stay two.

#include "unity.h"

// Files to test
#include "tusb.h"
TEST_SOURCE_FILE("tusb.c")

//--------------------------------------------------------------------+
// SEGGER_SYSVIEW stub (test/support/fake_vendor/SEGGER_SYSVIEW.h declares these; not
// CMock-generated -- a hand-written call counter is simpler and more robust than parsing the
// real vendored header's full dependency chain just to unit-test a depth counter).
//--------------------------------------------------------------------+
static unsigned enter_isr_count;
static unsigned exit_isr_count;

void SEGGER_SYSVIEW_RecordEnterISR(void) { enter_isr_count++; }
void SEGGER_SYSVIEW_RecordExitISR(void)  { exit_isr_count++; }

// Unused by these tests (tu_sysview_isr_enter/_exit is the only surface exercised), but
// tusb.c's OTHER SystemView functions (tu_sysview_init/name_resource) still
// need to link even though this test never calls them -- Ceedling compiles/links the whole
// tusb.c translation unit, not just the functions a given test happens to call.
void SEGGER_SYSVIEW_Init(U32 SysFreq, U32 CPUFreq, const SEGGER_SYSVIEW_OS_API *pOSAPI,
                          SEGGER_SYSVIEW_SEND_SYS_DESC_FUNC pfSendSysDesc) {
  (void) SysFreq; (void) CPUFreq; (void) pOSAPI; (void) pfSendSysDesc;
}
void SEGGER_SYSVIEW_SetRAMBase(U32 RAMBaseAddress) { (void) RAMBaseAddress; }
void SEGGER_SYSVIEW_Start(void) {}
void SEGGER_SYSVIEW_DisableEvents(U32 DisableMask) { (void) DisableMask; }
void SEGGER_SYSVIEW_SendSysDesc(const char *sSysDesc) { (void) sSysDesc; }
void SEGGER_SYSVIEW_RecordVoid(unsigned int Id) { (void) Id; }
void SEGGER_SYSVIEW_RecordEndCall(unsigned int Id) { (void) Id; }
void SEGGER_SYSVIEW_NameResource(U32 ResourceId, const char *sName) { (void) ResourceId; (void) sName; }

// tusb_sysview_cpu_freq()'s weak default (CFG_TUSB_OS != OPT_OS_FREERTOS branch, which is
// always true for this test suite -- test/support/tusb_config.h hardcodes CFG_TUSB_OS to
// OPT_OS_NONE) references this CMSIS-style global; never actually called by these tests, but
// still needs a definition for the link.
uint32_t SystemCoreClock = 48000000;

//--------------------------------------------------------------------+
// setUp/tearDown
//--------------------------------------------------------------------+
void setUp(void) {
  enter_isr_count = 0;
  exit_isr_count = 0;
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+

// Simple single self-wrapped call: ENTER, EXIT -- sanity check, not the interesting case.
void test_single_pair_records_once(void) {
  tu_sysview_isr_enter();
  tu_sysview_isr_exit();

  TEST_ASSERT_EQUAL_UINT(1, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(1, exit_isr_count);
}

// A dual-role BSP's vector ISR: one outer bracket around
// tud_int_handler()+tuh_int_handler(), each of which self-wraps -- i.e.
//   TU_SYSVIEW_ISR_ENTER();                       // outer (family.c)
//     TU_SYSVIEW_ISR_ENTER(); ... TU_SYSVIEW_ISR_EXIT();  // tud_int_handler's own wrap
//     TU_SYSVIEW_ISR_ENTER(); ... TU_SYSVIEW_ISR_EXIT();  // tuh_int_handler's own wrap
//   TU_SYSVIEW_ISR_EXIT();                        // outer (family.c)
// This is the exact sequence one real hardware interrupt now produces. It must record exactly
// one ENTER/EXIT pair, not three.
void test_outer_bracket_collapses_nested_dual_role_pair(void) {
  tu_sysview_isr_enter();  // outer
  tu_sysview_isr_enter();  //   tud_int_handler's self-wrap: enter
  tu_sysview_isr_exit();   //   tud_int_handler's self-wrap: exit
  tu_sysview_isr_enter();  //   tuh_int_handler's self-wrap: enter
  tu_sysview_isr_exit();   //   tuh_int_handler's self-wrap: exit
  tu_sysview_isr_exit();   // outer

  TEST_ASSERT_EQUAL_UINT(1, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(1, exit_isr_count);
}

// without the outer bracket, two back-to-back self-wrapped pairs are two spans
void test_two_sequential_pairs_without_outer_bracket_do_not_collapse(void) {
  tu_sysview_isr_enter();  // tud_int_handler's self-wrap: enter
  tu_sysview_isr_exit();   // tud_int_handler's self-wrap: exit
  tu_sysview_isr_enter();  // tuh_int_handler's self-wrap: enter
  tu_sysview_isr_exit();   // tuh_int_handler's self-wrap: exit

  TEST_ASSERT_EQUAL_UINT(2, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(2, exit_isr_count);
}

// Genuine nesting from an unrelated cause (e.g. a higher-priority ISR preempting a lower one
// that's mid-span) collapses the same way as the deliberate outer bracket above -- the counter
// doesn't distinguish why it's nested, only that it is.
void test_preempting_isr_nests_and_collapses(void) {
  tu_sysview_isr_enter();  // low-priority ISR's own wrap: enter
  tu_sysview_isr_enter();  //   preempting high-priority ISR's wrap: enter
  tu_sysview_isr_exit();   //   preempting high-priority ISR's wrap: exit
  tu_sysview_isr_exit();   // low-priority ISR's own wrap: exit

  TEST_ASSERT_EQUAL_UINT(1, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(1, exit_isr_count);
}
