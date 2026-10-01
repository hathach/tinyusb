/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

// Behavioral test for tusb_sysview.c's ISR depth counter (F11 / round 2).
//
// Round 1 added tusb_sysview_isr_enter()/_exit(), a depth counter meant to collapse the
// ENTER,EXIT,ENTER,EXIT a dual-role BSP's vector ISR emits when it calls
// tud_int_handler()+tuh_int_handler() back-to-back (each self-wraps). That counter only
// collapses NESTED pairs, though -- on its own, with no OUTER bracket, two back-to-back
// self-wrapped pairs are still two separate 0->1->0 excursions and get recorded twice. Round 2
// fixed the ten affected BSPs (hw/bsp/{tm4c,msp432e4,ch32v20x,rx,samd2x_l2x,kinetis_k,
// kinetis_kl,samd5x_e5x,lpc17,lpc40}/family.c) by adding one more OUTER ENTER/EXIT bracket
// around the whole tud_+tuh_int_handler() call pair, which makes the two inner self-wrapped
// calls nest inside it for this counter to actually collapse.
//
// This test exercises tusb_sysview_isr_enter()/_exit() directly (bypassing the
// TU_SYSVIEW_ISR_ENTER()/_EXIT() macro layer, which only adds an ISR-level compile-time gate)
// against a host-side stub standing in for SEGGER_SYSVIEW_Record{Enter,Exit}ISR() that counts
// calls, and asserts the two call patterns that matter: nested (one outer bracket wrapping two
// inner self-wrapped calls, matching a BSP after the round-2 fix) collapses to exactly one
// ENTER/EXIT pair, while two bare back-to-back pairs with no outer bracket (what those same ten
// BSPs looked like before the round-2 fix, or what any BSP with no bracket at all still looks
// like) do NOT collapse -- proving the depth counter alone was never sufficient, and that the
// per-BSP bracket is what makes it work.

#include "unity.h"

// Files to test
#include "common/tusb_sysview.h"
TEST_SOURCE_FILE("tusb_sysview.c")

//--------------------------------------------------------------------+
// SEGGER_SYSVIEW stub (test/support/fake_vendor/SEGGER_SYSVIEW.h declares these; not
// CMock-generated -- a hand-written call counter is simpler and more robust than parsing the
// real vendored header's full dependency chain just to unit-test a depth counter).
//--------------------------------------------------------------------+
static unsigned enter_isr_count;
static unsigned exit_isr_count;

void SEGGER_SYSVIEW_RecordEnterISR(void) { enter_isr_count++; }
void SEGGER_SYSVIEW_RecordExitISR(void)  { exit_isr_count++; }

// Unused by these tests (tusb_sysview_isr_enter/_exit is the only surface exercised), but
// tusb_sysview.c's OTHER functions (tusb_sysview_init/heap_alloc/heap_free/name_resource) still
// need to link even though this test never calls them -- Ceedling compiles/links the whole
// tusb_sysview.c translation unit, not just the functions a given test happens to call.
void SEGGER_SYSVIEW_Init(U32 SysFreq, U32 CPUFreq, const SEGGER_SYSVIEW_OS_API *pOSAPI,
                          SEGGER_SYSVIEW_SEND_SYS_DESC_FUNC pfSendSysDesc) {
  (void) SysFreq; (void) CPUFreq; (void) pOSAPI; (void) pfSendSysDesc;
}
void SEGGER_SYSVIEW_SetRAMBase(U32 RAMBaseAddress) { (void) RAMBaseAddress; }
void SEGGER_SYSVIEW_Start(void) {}
void SEGGER_SYSVIEW_DisableEvents(U32 DisableMask) { (void) DisableMask; }
void SEGGER_SYSVIEW_SendSysDesc(const char *sSysDesc) { (void) sSysDesc; }
int  SEGGER_SYSVIEW_IsStarted(void) { return 0; }
void SEGGER_SYSVIEW_RecordVoid(unsigned int Id) { (void) Id; }
void SEGGER_SYSVIEW_RecordEndCall(unsigned int Id) { (void) Id; }
void SEGGER_SYSVIEW_HeapDefine(void *pHeap, void *pBase, unsigned int HeapSize, unsigned int MetadataSize) {
  (void) pHeap; (void) pBase; (void) HeapSize; (void) MetadataSize;
}
void SEGGER_SYSVIEW_HeapAlloc(void *pHeap, void *pUserData, unsigned int UserDataLen) {
  (void) pHeap; (void) pUserData; (void) UserDataLen;
}
void SEGGER_SYSVIEW_HeapFree(void *pHeap, void *pUserData) { (void) pHeap; (void) pUserData; }
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
  tusb_sysview_isr_enter();
  tusb_sysview_isr_exit();

  TEST_ASSERT_EQUAL_UINT(1, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(1, exit_isr_count);
}

// A dual-role BSP's vector ISR AFTER the round-2 fix: one outer bracket around
// tud_int_handler()+tuh_int_handler(), each of which self-wraps -- i.e.
//   TU_SYSVIEW_ISR_ENTER();                       // outer (family.c)
//     TU_SYSVIEW_ISR_ENTER(); ... TU_SYSVIEW_ISR_EXIT();  // tud_int_handler's own wrap
//     TU_SYSVIEW_ISR_ENTER(); ... TU_SYSVIEW_ISR_EXIT();  // tuh_int_handler's own wrap
//   TU_SYSVIEW_ISR_EXIT();                        // outer (family.c)
// This is the exact sequence one real hardware interrupt now produces. It must record exactly
// one ENTER/EXIT pair, not three.
void test_outer_bracket_collapses_nested_dual_role_pair(void) {
  tusb_sysview_isr_enter();  // outer
  tusb_sysview_isr_enter();  //   tud_int_handler's self-wrap: enter
  tusb_sysview_isr_exit();   //   tud_int_handler's self-wrap: exit
  tusb_sysview_isr_enter();  //   tuh_int_handler's self-wrap: enter
  tusb_sysview_isr_exit();   //   tuh_int_handler's self-wrap: exit
  tusb_sysview_isr_exit();   // outer

  TEST_ASSERT_EQUAL_UINT(1, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(1, exit_isr_count);
}

// The bug the round-2 fix addresses: with NO outer bracket, two back-to-back (not nested)
// self-wrapped pairs -- what every one of the ten affected BSPs looked like before round 2 --
// are NOT collapsed by the depth counter alone. This documents why the per-BSP bracket was
// necessary, not just the counter.
void test_two_sequential_pairs_without_outer_bracket_do_not_collapse(void) {
  tusb_sysview_isr_enter();  // tud_int_handler's self-wrap: enter
  tusb_sysview_isr_exit();   // tud_int_handler's self-wrap: exit
  tusb_sysview_isr_enter();  // tuh_int_handler's self-wrap: enter
  tusb_sysview_isr_exit();   // tuh_int_handler's self-wrap: exit

  TEST_ASSERT_EQUAL_UINT(2, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(2, exit_isr_count);
}

// Genuine nesting from an unrelated cause (e.g. a higher-priority ISR preempting a lower one
// that's mid-span) collapses the same way as the deliberate outer bracket above -- the counter
// doesn't distinguish why it's nested, only that it is.
void test_preempting_isr_nests_and_collapses(void) {
  tusb_sysview_isr_enter();  // low-priority ISR's own wrap: enter
  tusb_sysview_isr_enter();  //   preempting high-priority ISR's wrap: enter
  tusb_sysview_isr_exit();   //   preempting high-priority ISR's wrap: exit
  tusb_sysview_isr_exit();   // low-priority ISR's own wrap: exit

  TEST_ASSERT_EQUAL_UINT(1, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(1, exit_isr_count);
}
