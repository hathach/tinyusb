/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// DADDR2 needs a second device slot; tusb.c, tusb_fifo.c and the HCD stubs do not depend on it
#define CFG_TUH_DEVICE_MAX 2

// OS_NONE single core defines osal_mutex_unlock as (true): route every unlock in usbh.c through a hook that forces
// an interleaving right after a critical section. The header guard keeps the override for usbh.c below.
#include "osal/osal.h"
#ifndef osal_mutex_unlock
  #error osal_mutex_unlock is no longer a macro: the unlock hook would not run
#endif
#undef osal_mutex_unlock
static bool unlock_hook(void);
#define osal_mutex_unlock(_h) unlock_hook()

#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("usbh_test_hcd_stubs.c")

enum {
  RHPORT  = 0,
  RHPORT2 = 1, // DADDR2 sits on another root port, so unplugging RHPORT leaves it attached
  DADDR   = 1,
  DADDR2  = 2,
  UD_ASYNC = 0xA5A5,
  UD_B     = 0xB0B0,
  SETUP_CALL_MAX = 4,
  RUNNER_INVOKE_RUN = 3,
  RUNNER_BUDGET = 1000,
};

TU_VERIFY_STATIC(TOTAL_DEVICES >= DADDR2, "DADDR2 needs a device slot");

typedef enum {
  HOOK_NONE = 0,
  HOOK_CLAIM_UNPLUG,         // the claim unlock: unplug DADDR
  HOOK_CLAIM_CAPTURE,        // the claim unlock: host task captures the slot, cb still to run
  HOOK_CLAIM_UNPLUG_RECLAIM, // the claim unlock: unplug DADDR, then an async xfer on DADDR2 takes the slot
  HOOK_RETIRE,               // the unlock that retires the slot after hcd_setup_send() failed
  HOOK_RETIRE_RECLAIM,       // the retire unlock: an async xfer on DADDR2 takes the slot, then a late close of DADDR
} hook_mode_t;

typedef enum {
  SETUP_ACCEPT = 0,          // (a) fail the test on a SETUP to an unplugged device, else accept
  SETUP_UNPLUG_TRUE,         // (b) MAX3421/EHCI/OHCI: unplug, still accepted
  SETUP_UNPLUG_FALSE,        // (c) dwc2/rp2040: unplug, refused
  SETUP_FAIL,                // (d) plain HCD error
  SETUP_STALL,               // (e) completes with a STALL
  SETUP_FORBIDDEN,           // (f) no SETUP may go out
  SETUP_CAPTURE_FAIL,        // (g) host task captures the slot, then the HCD refuses
  SETUP_UNPLUG_RECLAIM_FAIL, // (h) DADDR: unplug + reclaim for DADDR2, refused; DADDR2: accepted
  SETUP_REFUSE_DADDR,        // (i) DADDR: refused; DADDR2: accepted
} setup_mode_t;

static hook_mode_t  hook_mode;
static uint8_t      hook_fired;
static setup_mode_t setup_mode;
static bool         setup_returned_false;
static uint8_t      setup_count;
static uint8_t      setup_daddr[SETUP_CALL_MAX];
static uintptr_t    setup_user_data[SETUP_CALL_MAX];
static tusb_control_request_t setup_request[SETUP_CALL_MAX];

// host task simulation: control_xfer_complete() has snapshotted and released the slot, cb not invoked yet
static tuh_xfer_cb_t     captured_cb;
static uintptr_t         captured_user_data;
static volatile bool     runner_armed;
static volatile bool     cb_invoked;
static volatile uint16_t runner_runs;

static uint8_t   async_cb_count;
static uintptr_t async_cb_user_data;
static xfer_result_t async_cb_result;
static uint8_t   cb_b_count;

static uint8_t desc_buf[18];

static const tusb_control_request_t request_get_device = {
  .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_DEVICE, .type = TUSB_REQ_TYPE_STANDARD, .direction = TUSB_DIR_IN },
  .bRequest = TUSB_REQ_GET_DESCRIPTOR,
  .wValue   = TUSB_DESC_DEVICE << 8,
  .wIndex   = 0,
  .wLength  = sizeof(desc_buf)
};

static const tusb_control_request_t request_b = {
  .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_DEVICE, .type = TUSB_REQ_TYPE_STANDARD, .direction = TUSB_DIR_IN },
  .bRequest = TUSB_REQ_GET_STATUS,
  .wValue   = 0,
  .wIndex   = 0,
  .wLength  = 0
};

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void attach(uint8_t daddr, uint8_t rhport) {
  usbh_device_t *dev     = get_device(daddr);
  dev->bus_info.rhport   = rhport;
  dev->bus_info.hub_addr = 0;
  dev->bus_info.hub_port = 0;
  dev->connected         = 1;
  dev->addressed         = 1;
}

static void unplug(void) {
  remove_device_tree(RHPORT, 0, 0);
  TEST_ASSERT_FALSE(tuh_connected(DADDR));
  TEST_ASSERT_TRUE(tuh_connected(DADDR2));
}

static void runner(void *param) {
  (void) param;
  if (!runner_armed) {
    return;
  }
  runner_runs++;
  if (runner_runs >= RUNNER_BUDGET) {
    runner_armed = false;
    TEST_FAIL_MESSAGE("caller still waiting after deferred cb");
  }
  if (runner_runs == RUNNER_INVOKE_RUN && captured_cb != NULL) {
    tuh_xfer_t xfer = {
      .daddr       = DADDR,
      .ep_addr     = 0,
      .result      = XFER_RESULT_FAILED,
      .setup       = &request_get_device,
      .actual_len  = 0,
      .complete_cb = captured_cb,
      .user_data   = captured_user_data
    };
    captured_cb(&xfer);
    cb_invoked = true;
  }
  usbh_defer_func(runner, NULL, false);
}

static void capture(void) {
  usbh_ctrl_xfer_info_t *ctrl_info = &_usbh_data.ctrl_xfer_info;
  captured_cb        = ctrl_info->complete_cb;
  captured_user_data = ctrl_info->user_data;
  ctrl_info->stage   = CONTROL_STAGE_IDLE;
  runner_armed       = true;
  usbh_defer_func(runner, NULL, false);
}

static void async_cb(tuh_xfer_t *xfer) {
  async_cb_count++;
  async_cb_user_data = xfer->user_data;
  async_cb_result    = xfer->result;
}

static void cb_b(tuh_xfer_t *xfer) {
  (void) xfer;
  cb_b_count++;
}

static void reclaim_for_daddr2(void) {
  tuh_xfer_t xfer_b = {
    .daddr       = DADDR2,
    .ep_addr     = 0,
    .setup       = &request_b,
    .buffer      = NULL,
    .complete_cb = cb_b,
    .user_data   = UD_B
  };
  TEST_ASSERT_TRUE(tuh_control_xfer(&xfer_b));
}

static bool fail_setup(void) {
  setup_returned_false = true;
  return false;
}

static tuh_xfer_t sync_xfer(void) {
  tuh_xfer_t xfer = {
    .daddr       = DADDR,
    .ep_addr     = 0,
    .setup       = &request_get_device,
    .buffer      = desc_buf,
    .complete_cb = NULL,
    .user_data   = 0
  };
  return xfer;
}

static bool control_xfer_in_call(tuh_xfer_t *xfer) {
  const bool ret = tuh_control_xfer(xfer);
  runner_armed = false;
  return ret;
}

static void assert_slot_idle(void) {
  TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);
}

static void assert_setup(uint8_t idx, uint8_t daddr, const tusb_control_request_t *request) {
  TEST_ASSERT_EQUAL(daddr, setup_daddr[idx]);
  TEST_ASSERT_EQUAL_MEMORY(request, &setup_request[idx], sizeof(tusb_control_request_t));
}

static void assert_replacement_slot(void) {
  const usbh_ctrl_xfer_info_t *ctrl_info = &_usbh_data.ctrl_xfer_info;
  TEST_ASSERT_NOT_EQUAL(CONTROL_STAGE_IDLE, ctrl_info->stage);
  TEST_ASSERT_EQUAL(DADDR2, ctrl_info->daddr);
  TEST_ASSERT_EQUAL_PTR(cb_b, ctrl_info->complete_cb);
  TEST_ASSERT_EQUAL(UD_B, ctrl_info->user_data);
  TEST_ASSERT_EQUAL(0, cb_b_count);
}

//--------------------------------------------------------------------+
// Hooks and HCD stub
//--------------------------------------------------------------------+
static bool unlock_hook(void) {
  const uint8_t stage = _usbh_data.ctrl_xfer_info.stage;
  const hook_mode_t mode = hook_mode;

  switch (mode) {
    case HOOK_CLAIM_UNPLUG:
    case HOOK_CLAIM_CAPTURE:
    case HOOK_CLAIM_UNPLUG_RECLAIM:
      if (stage != CONTROL_STAGE_SETUP) {
        return true;
      }
      hook_mode = HOOK_NONE;
      hook_fired++;
      if (mode == HOOK_CLAIM_CAPTURE) {
        capture();
      } else {
        unplug();
        if (mode == HOOK_CLAIM_UNPLUG_RECLAIM) {
          reclaim_for_daddr2();
        }
      }
      break;

    case HOOK_RETIRE: {
      if (stage != CONTROL_STAGE_IDLE || !setup_returned_false) {
        return true;
      }
      hook_mode = HOOK_NONE;
      hook_fired++;
      const usbh_ctrl_xfer_info_t *ctrl_info = &_usbh_data.ctrl_xfer_info;
      TEST_ASSERT_NULL(ctrl_info->complete_cb);
      TEST_ASSERT_EQUAL(0, ctrl_info->user_data);
      // a close that passed usbh_device_close()'s unlocked stage check before the retire
      control_xfer_complete(DADDR, XFER_RESULT_FAILED);
      TEST_ASSERT_EQUAL(1, setup_count);
      const volatile control_xfer_sync_param_t *s = (const volatile control_xfer_sync_param_t *) setup_user_data[0];
      TEST_ASSERT_EQUAL(XFER_RESULT_INVALID, s->result);
      break;
    }

    case HOOK_RETIRE_RECLAIM:
      if (stage != CONTROL_STAGE_IDLE || !setup_returned_false) {
        return true;
      }
      hook_mode = HOOK_NONE;
      hook_fired++;
      reclaim_for_daddr2();
      // a close of DADDR that passed usbh_device_close()'s unlocked stage check before the retire
      control_xfer_complete(DADDR, XFER_RESULT_FAILED);
      break;

    default:
      break;
  }
  return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  TEST_ASSERT_LESS_THAN(SETUP_CALL_MAX, setup_count);
  setup_daddr[setup_count]     = daddr;
  memcpy(&setup_request[setup_count], setup_packet, sizeof(tusb_control_request_t));
  setup_user_data[setup_count] = _usbh_data.ctrl_xfer_info.user_data;
  setup_count++;

  switch (setup_mode) {
    case SETUP_ACCEPT:
      if (!tuh_connected(daddr)) {
        TEST_FAIL_MESSAGE("setup sent to an unplugged device");
      }
      return true;

    case SETUP_UNPLUG_TRUE:
      unplug();
      return true;

    case SETUP_UNPLUG_FALSE:
      unplug();
      return fail_setup();

    case SETUP_FAIL:
      return fail_setup();

    case SETUP_STALL:
      hcd_event_xfer_complete(daddr, 0x00, 0, XFER_RESULT_STALLED, false);
      return true;

    case SETUP_FORBIDDEN:
      TEST_FAIL_MESSAGE("no SETUP may go out in this window");
      return false;

    case SETUP_CAPTURE_FAIL:
      capture();
      return fail_setup();

    case SETUP_UNPLUG_RECLAIM_FAIL:
      if (daddr == DADDR2) {
        return true;
      }
      unplug();
      reclaim_for_daddr2();
      return fail_setup();

    case SETUP_REFUSE_DADDR:
      return (daddr == DADDR2) ? true : fail_setup();

    default:
      TEST_FAIL_MESSAGE("unknown setup mode");
      return false;
  }
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
static void disarm(void) {
  hook_mode            = HOOK_NONE;
  setup_mode           = SETUP_ACCEPT;
  runner_armed         = false;
  captured_cb          = NULL;
  captured_user_data   = 0;
}

void setUp(void) {
  disarm();

  if (!tuh_inited()) {
    tusb_rhport_init_t host_init = {
      .role  = TUSB_ROLE_HOST,
      .speed = TUSB_SPEED_AUTO
    };
    TEST_ASSERT_TRUE(tusb_init(0, &host_init));
  }

  // discard what an earlier test left queued, without running it
  hcd_event_t ev;
  while (osal_queue_receive(_usbh_q, &ev, 0)) {}
  tu_fifo_clear(&_usbh_pending_ctrl_q);

  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
    clear_device(&_usbh_devices[i]);
  }
  _usbh_data.enumerating_daddr = TUSB_INDEX_INVALID_8;
  attach(DADDR, RHPORT);
  attach(DADDR2, RHPORT2);

  hook_fired           = 0;
  setup_returned_false = false;
  setup_count          = 0;
  tu_memclr(setup_daddr, sizeof(setup_daddr));
  tu_memclr(setup_user_data, sizeof(setup_user_data));
  tu_memclr(setup_request, sizeof(setup_request));
  cb_invoked         = false;
  runner_runs        = 0;
  async_cb_count     = 0;
  async_cb_user_data = 0;
  async_cb_result    = XFER_RESULT_INVALID;
  cb_b_count         = 0;
}

// Unity runs it after a failed assertion too: a runner left queued must do nothing
void tearDown(void) {
  disarm();
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
// unplug between the claim and submit: the caller must fail instead of waiting on a slot nobody completes
void test_sync_unplug_after_claim_returns_false(void) {
  hook_mode = HOOK_CLAIM_UNPLUG;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(1, hook_fired);
  TEST_ASSERT_EQUAL(0, setup_count);
  assert_slot_idle();
}

void test_sync_api_unplug_after_claim_returns_timeout(void) {
  hook_mode = HOOK_CLAIM_UNPLUG;

  TEST_ASSERT_EQUAL(XFER_RESULT_TIMEOUT, tuh_descriptor_get_device_sync(DADDR, desc_buf, sizeof(desc_buf)));
  TEST_ASSERT_EQUAL(1, hook_fired);
  TEST_ASSERT_EQUAL(0, setup_count);
  assert_slot_idle();
}

// MAX3421/EHCI/OHCI: the HCD accepts the SETUP while the device goes away
void test_sync_unplug_in_setup_send_accepted(void) {
  setup_mode = SETUP_UNPLUG_TRUE;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_TRUE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, xfer.result);
  TEST_ASSERT_EQUAL(0, xfer.actual_len);
  assert_slot_idle();
}

// dwc2/rp2040: the HCD refuses the SETUP for a closed device
void test_sync_unplug_in_setup_send_refused(void) {
  setup_mode = SETUP_UNPLUG_FALSE;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(tuh_control_xfer(&xfer));
  assert_slot_idle();
}

void test_sync_setup_send_fails_clears_slot(void) {
  setup_mode = SETUP_FAIL;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(tuh_control_xfer(&xfer));
  assert_slot_idle();
  TEST_ASSERT_NULL(_usbh_data.ctrl_xfer_info.complete_cb);
  TEST_ASSERT_EQUAL(0, _usbh_data.ctrl_xfer_info.user_data);
}

void test_sync_completes_with_stall(void) {
  setup_mode = SETUP_STALL;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_TRUE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(XFER_RESULT_STALLED, xfer.result);
  assert_slot_idle();
}

void test_async_keeps_cb_and_user_data(void) {
  setup_mode = SETUP_STALL;
  tuh_xfer_t xfer = sync_xfer();
  xfer.complete_cb = async_cb;
  xfer.user_data   = UD_ASYNC;

  TEST_ASSERT_TRUE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(1, setup_count);
  TEST_ASSERT_EQUAL(UD_ASYNC, setup_user_data[0]);
  TEST_ASSERT_EQUAL(0, async_cb_count);

  tuh_task();
  TEST_ASSERT_EQUAL(1, async_cb_count);
  TEST_ASSERT_EQUAL(UD_ASYNC, async_cb_user_data);
  TEST_ASSERT_EQUAL(XFER_RESULT_STALLED, async_cb_result);
}

// host task captured the slot before submit, its cb still to run: no SETUP, and return only once the cb ran
void test_sync_captured_before_submit_waits_for_cb(void) {
  hook_mode  = HOOK_CLAIM_CAPTURE;
  setup_mode = SETUP_FORBIDDEN;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(control_xfer_in_call(&xfer));
  TEST_ASSERT_EQUAL(1, hook_fired);
  TEST_ASSERT_EQUAL(0, setup_count);
  TEST_ASSERT_EQUAL_PTR(control_xfer_sync_complete, captured_cb);
  TEST_ASSERT_TRUE(cb_invoked);
}

// same window on the setup-failure path
void test_sync_captured_on_setup_failure_waits_for_cb(void) {
  setup_mode = SETUP_CAPTURE_FAIL;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(control_xfer_in_call(&xfer));
  TEST_ASSERT_EQUAL(1, setup_count);
  TEST_ASSERT_EQUAL_PTR(control_xfer_sync_complete, captured_cb);
  TEST_ASSERT_TRUE(cb_invoked);
  assert_slot_idle();
}

// a close racing the retire must not write the caller's sync_state
void test_sync_retire_then_late_close(void) {
  setup_mode = SETUP_FAIL;
  hook_mode  = HOOK_RETIRE;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(1, hook_fired);
}

// slot reclaimed by another device before the caller resumes: leave the replacement alone
void test_sync_reclaimed_before_submit_keeps_replacement(void) {
  hook_mode = HOOK_CLAIM_UNPLUG_RECLAIM;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(1, hook_fired);
  TEST_ASSERT_EQUAL(1, setup_count);
  TEST_ASSERT_EQUAL(DADDR2, setup_daddr[0]);
  assert_replacement_slot();
}

void test_sync_reclaimed_on_setup_failure_keeps_replacement(void) {
  setup_mode = SETUP_UNPLUG_RECLAIM_FAIL;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(2, setup_count);
  TEST_ASSERT_EQUAL(DADDR, setup_daddr[0]);
  TEST_ASSERT_EQUAL(DADDR2, setup_daddr[1]);
  assert_replacement_slot();
}

// retire after a refused SETUP, reclaim for DADDR2, then a late close of DADDR: DADDR2's transfer is not completed
void test_sync_late_close_after_reclaim_keeps_replacement(void) {
  setup_mode = SETUP_REFUSE_DADDR;
  hook_mode  = HOOK_RETIRE_RECLAIM;
  tuh_xfer_t xfer = sync_xfer();

  TEST_ASSERT_FALSE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(1, hook_fired);
  TEST_ASSERT_EQUAL(2, setup_count);
  assert_setup(0, DADDR, &request_get_device);
  assert_setup(1, DADDR2, &request_b);
  assert_replacement_slot();
}
