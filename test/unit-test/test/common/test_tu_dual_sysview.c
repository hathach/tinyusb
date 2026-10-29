/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// tusb_int_handler()'s ISR span in a traced dual-role build: one span per interrupt in either role.

#include "unity.h"

#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "host/usbh_pvt.h"
TEST_SOURCE_FILE("tusb.c")

extern tusb_role_t _tusb_rhport_role[];

static unsigned enter_isr_count;
static unsigned exit_isr_count;
static unsigned dcd_irq_count;
static unsigned hcd_irq_count;

//--------------------------------------------------------------------+
// Stubs: the stacks tusb.c dispatches to, and SEGGER_SYSVIEW
//--------------------------------------------------------------------+
uint32_t tusb_time_millis_api(void) { return 0; }

void dcd_int_handler(uint8_t rhport) { (void) rhport; dcd_irq_count++; }
void hcd_int_handler(uint8_t rhport, bool in_isr) { (void) rhport; (void) in_isr; hcd_irq_count++; }

bool tud_rhport_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) { (void) rhport; (void) rh_init; return true; }
bool tud_deinit(uint8_t rhport) { (void) rhport; return true; }
bool tud_inited(void) { return true; }
bool tuh_rhport_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) { (void) rhport; (void) rh_init; return true; }
bool tuh_deinit(uint8_t rhport) { (void) rhport; return true; }
bool tuh_inited(void) { return true; }

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes, bool is_isr) {
  (void) rhport; (void) ep_addr; (void) buffer; (void) total_bytes; (void) is_isr;
  return true;
}
bool usbd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t* ff, uint16_t total_bytes, bool is_isr) {
  (void) rhport; (void) ep_addr; (void) ff; (void) total_bytes; (void) is_isr;
  return true;
}
bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr) { (void) rhport; (void) ep_addr; return true; }
bool usbd_edpt_release(uint8_t rhport, uint8_t ep_addr) { (void) rhport; (void) ep_addr; return true; }
bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) dev_addr; (void) ep_addr; (void) buffer; (void) total_bytes; (void) complete_cb; (void) user_data;
  return true;
}
bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) { (void) dev_addr; (void) ep_addr; return true; }
bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) { (void) dev_addr; (void) ep_addr; return true; }

void SEGGER_SYSVIEW_RecordEnterISR(void) { enter_isr_count++; }
void SEGGER_SYSVIEW_RecordExitISR(void)  { exit_isr_count++; }
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
uint32_t SystemCoreClock = 48000000;

void setUp(void) {
  enter_isr_count = 0;
  exit_isr_count = 0;
  dcd_irq_count = 0;
  hcd_irq_count = 0;
}

void tearDown(void) {
  _tusb_rhport_role[0] = TUSB_ROLE_INVALID;
}

void test_device_role_records_one_span(void) {
  _tusb_rhport_role[0] = TUSB_ROLE_DEVICE;
  tusb_int_handler(0, true);

  TEST_ASSERT_EQUAL_UINT(1, dcd_irq_count);
  TEST_ASSERT_EQUAL_UINT(1, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(1, exit_isr_count);
}

void test_host_role_records_one_span(void) {
  _tusb_rhport_role[0] = TUSB_ROLE_HOST;
  tusb_int_handler(0, true);

  TEST_ASSERT_EQUAL_UINT(1, hcd_irq_count);
  TEST_ASSERT_EQUAL_UINT(1, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(1, exit_isr_count);
}

// Brings a role's counter to the end of a lap, so its next 1024th call is due
static void stack_report_lap_end(tusb_role_t role) {
  while (!tu_sysview_stack_report_due(role)) {}
}

void test_stack_report_counters_are_per_role(void) {
  stack_report_lap_end(TUSB_ROLE_DEVICE);
  stack_report_lap_end(TUSB_ROLE_HOST);
  for (int i = 0; i < 1023; i++) {
    TEST_ASSERT_FALSE(tu_sysview_stack_report_due(TUSB_ROLE_DEVICE));
  }
  // the device's 1023 events advanced nothing on the host side
  for (int i = 0; i < 1023; i++) {
    TEST_ASSERT_FALSE(tu_sysview_stack_report_due(TUSB_ROLE_HOST));
  }
  TEST_ASSERT_TRUE(tu_sysview_stack_report_due(TUSB_ROLE_DEVICE));
  TEST_ASSERT_TRUE(tu_sysview_stack_report_due(TUSB_ROLE_HOST));
}

void test_stack_report_alternating_callers_keep_their_own_cadence(void) {
  stack_report_lap_end(TUSB_ROLE_DEVICE);
  stack_report_lap_end(TUSB_ROLE_HOST);
  for (int i = 1; i <= 1024; i++) {
    TEST_ASSERT_EQUAL(i == 1024, tu_sysview_stack_report_due(TUSB_ROLE_DEVICE));
    TEST_ASSERT_EQUAL(i == 1024, tu_sysview_stack_report_due(TUSB_ROLE_HOST));
  }
}

void test_stack_report_cadence_survives_counter_rollover(void) {
  stack_report_lap_end(TUSB_ROLE_DEVICE);
  unsigned due = 0;
  for (unsigned i = 1; i <= 3u * 65536u; i++) {
    bool const d = tu_sysview_stack_report_due(TUSB_ROLE_DEVICE);
    TEST_ASSERT_EQUAL(i % 1024u == 0, d);
    due += d;
  }
  TEST_ASSERT_EQUAL_UINT(3u * 64u, due);
}

void test_host_role_from_task_context_records_no_span(void) {
  _tusb_rhport_role[0] = TUSB_ROLE_HOST;
  tusb_int_handler(0, false);

  TEST_ASSERT_EQUAL_UINT(1, hcd_irq_count);
  TEST_ASSERT_EQUAL_UINT(0, enter_isr_count);
  TEST_ASSERT_EQUAL_UINT(0, exit_isr_count);
}
