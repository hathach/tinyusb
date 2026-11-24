/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// Include the production implementation to set enumeration state directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("usbh_config_complete_callbacks.c")
TEST_SOURCE_FILE("usbh_test_hcd_stubs.c")

enum {
  RHPORT     = 0,
  DADDR      = 1,
  OTHER_ADDR = 2,
  ITF_COUNT  = 2,
};

uint8_t mount_cb_count;
uint8_t umount_cb_count;

static uint8_t set_config_count;
static uint8_t close_count;
static void (*set_config_hook)(void);
static void (*close_hook)(void);

//--------------------------------------------------------------------+
// HCD stubs: the stateless ones are shared, setup submission and clear-stall observation are this suite's
//--------------------------------------------------------------------+
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) daddr;
  (void) setup_packet;
  return true;
}

static uint8_t clear_stall_count;
static uint8_t clear_stall_rhport;
static uint8_t clear_stall_ep;
static bool    clear_stall_result;

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) daddr;
  clear_stall_count++;
  clear_stall_rhport = rhport;
  clear_stall_ep     = ep_addr;
  return clear_stall_result;
}

//--------------------------------------------------------------------+
// Fake class driver: its set_config runs the hook, standing in for the application's mount callback, before it
// reports completion as the class drivers do
//--------------------------------------------------------------------+
static bool fake_set_config(uint8_t dev_addr, uint8_t itf_num) {
  set_config_count++;
  if (set_config_hook != NULL) {
    set_config_hook();
    set_config_hook = NULL;
  }
  usbh_driver_set_config_complete(dev_addr, itf_num);
  return true;
}

// the hook stands in for the application's umount callback
static void fake_close(uint8_t dev_addr) {
  (void) dev_addr;
  close_count++;
  if (close_hook != NULL) {
    void (*hook)(void) = close_hook;
    close_hook = NULL;
    hook();
  }
}

// claims the whole rest of the configuration, recording the interfaces it is offered and the endpoint sizes it is
// given, before opening any
static uint16_t open_ep_size[4];
static uint8_t  open_ep_count;
static uint8_t  open_count;
static uint8_t  open_itf_num;

static uint16_t fake_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_interface_t *desc_itf, uint16_t max_len) {
  (void) rhport;
  (void) dev_addr;
  open_count++;
  open_itf_num = desc_itf->bInterfaceNumber;
  const uint8_t *p_desc   = (const uint8_t *) desc_itf;
  const uint8_t *desc_end = p_desc + max_len;
  for (; tu_desc_in_bounds(p_desc, desc_end); p_desc = tu_desc_next(p_desc)) {
    if (TUSB_DESC_ENDPOINT == tu_desc_type(p_desc) && open_ep_count < TU_ARRAY_SIZE(open_ep_size)) {
      open_ep_size[open_ep_count++] = tu_edpt_packet_size((const tusb_desc_endpoint_t *) p_desc);
    }
  }
  return max_len;
}

static const usbh_class_driver_t fake_driver = {
  .name       = "FAKE",
  .open       = fake_open,
  .set_config = fake_set_config,
  .close      = fake_close,
};

//--------------------------------------------------------------------+
// Hooks
//--------------------------------------------------------------------+
static void remove_device(void) {
  remove_device_tree(RHPORT, 0, 0);
}

// a second device attached meanwhile: it waits at address 0 for its debounce delay
static void remove_device_and_attach_another(void) {
  remove_device();
  _usbh_data.enum_gen++; // as enum_new_device() does
  _usbh_data.enumerating_daddr = 0;
  _usbh_data.root_retry_count  = 2;
  usbh_defer_func_ms_async(ENUM_DEBOUNCING_DELAY_MS, enum_delay_async, ENUM_AFTER_DEBOUNCING_DELAY);
}

// the second device has since been given the address the first one freed
static void remove_device_and_readdress_another(void) {
  remove_device_and_attach_another();
  usbh_device_t *dev = get_device(DADDR);
  dev->bus_info.rhport = RHPORT;
  dev->connected       = 1;
  dev->addressed       = 1;
  _usbh_data.enumerating_daddr = DADDR;
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
void setUp(void) {
  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
    clear_device(&_usbh_devices[i]);
  }
  _app_driver                  = &fake_driver;
  _app_driver_count            = 1;

  mount_cb_count   = 0;
  umount_cb_count  = 0;
  set_config_count = 0;
  close_count      = 0;
  open_count       = 0;
  open_itf_num     = 0xff;
  set_config_hook  = NULL;
  close_hook       = NULL;
  clear_stall_count  = 0;
  clear_stall_rhport = 0xff;
  clear_stall_ep     = 0;
  clear_stall_result = true;
  open_ep_count      = 0;
  tu_memclr(open_ep_size, sizeof(open_ep_size));

  // DADDR is being enumerated and has reached the SET_CONFIGURATION stage
  usbh_device_t *dev    = get_device(DADDR);
  dev->bus_info.rhport  = RHPORT;
  dev->connected        = 1;
  dev->addressed        = 1;
  dev->configured       = 1;
  for (uint8_t i = 0; i < ITF_COUNT; i++) {
    dev->itf2drv[i] = 0;
  }
  _usbh_data.enumerating_daddr = DADDR;
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_config_complete_mounts_once(void) {
  usbh_driver_set_config_complete(DADDR, TUSB_INDEX_INVALID_8);

  TEST_ASSERT_EQUAL(ITF_COUNT, set_config_count);
  TEST_ASSERT_EQUAL(1, mount_cb_count);
  TEST_ASSERT_TRUE(get_device(DADDR)->mounted);
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
}

void test_removed_during_mount_callback_not_mounted(void) {
  set_config_hook = remove_device;

  usbh_driver_set_config_complete(DADDR, TUSB_INDEX_INVALID_8);

  TEST_ASSERT_EQUAL(1, set_config_count);
  TEST_ASSERT_EQUAL(1, close_count);
  TEST_ASSERT_EQUAL(0, mount_cb_count);
  TEST_ASSERT_EQUAL(0, umount_cb_count);
  TEST_ASSERT_FALSE(get_device(DADDR)->mounted);
}

// The second enumeration's state is installed by the hook, not reached through the deferred attach queue.
void test_removed_during_mount_callback_keeps_next_enumeration(void) {
  set_config_hook = remove_device_and_attach_another;

  usbh_driver_set_config_complete(DADDR, TUSB_INDEX_INVALID_8);

  TEST_ASSERT_EQUAL(0, mount_cb_count);
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL_PTR(enum_delay_async, _usbh_data.call_after.func);
  TEST_ASSERT_EQUAL(2, _usbh_data.root_retry_count);
}

void test_config_complete_ignored_when_not_enumerating(void) {
  _usbh_data.enumerating_daddr = OTHER_ADDR;

  usbh_driver_set_config_complete(DADDR, 0);

  TEST_ASSERT_EQUAL(0, set_config_count);
  TEST_ASSERT_EQUAL(0, mount_cb_count);
  TEST_ASSERT_EQUAL(OTHER_ADDR, _usbh_data.enumerating_daddr);
}

// a replacement given the same address that has not reached SET_CONFIGURATION yet
void test_config_complete_ignored_when_not_configured(void) {
  get_device(DADDR)->configured = 0;

  usbh_driver_set_config_complete(DADDR, 0);

  TEST_ASSERT_EQUAL(0, set_config_count);
  TEST_ASSERT_EQUAL(0, mount_cb_count);
  TEST_ASSERT_EQUAL(DADDR, _usbh_data.enumerating_daddr);
}

//--------------------------------------------------------------------+
// Configuration parsing: full-speed bulk 512 quirk
//--------------------------------------------------------------------+
// alt 0 with bulk OUT/IN 512, alt 1 with bulk IN 512 and an interrupt IN 512 (not a bulk size quirk)
static const uint8_t cfg_bulk_512[] = {
  9, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(9 + 9 + 7 + 7 + 9 + 7 + 7), 1, 1, 0, 0x80, 50,
  9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0, 0, 0,
  7, TUSB_DESC_ENDPOINT, 0x01, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
  7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
  9, TUSB_DESC_INTERFACE, 0, 1, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0, 0, 0,
  7, TUSB_DESC_ENDPOINT, 0x82, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
  7, TUSB_DESC_ENDPOINT, 0x83, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(512), 1,
};

static void parse_cfg_bulk_512(tusb_speed_t speed) {
  uint8_t desc[sizeof(cfg_bulk_512)];
  memcpy(desc, cfg_bulk_512, sizeof(desc));
  get_device(DADDR)->bus_info.speed = speed;
  TEST_ASSERT_TRUE(enum_parse_configuration_desc(DADDR, (const tusb_desc_configuration_t *) desc));
  TEST_ASSERT_EQUAL(4, open_ep_count);
}

// drivers see the size tuh_edpt_open() will use before they open, alternate settings included
void test_parse_config_fixes_fullspeed_bulk_512_before_open(void) {
  parse_cfg_bulk_512(TUSB_SPEED_FULL);
  TEST_ASSERT_EQUAL(64, open_ep_size[0]);
  TEST_ASSERT_EQUAL(64, open_ep_size[1]);
  TEST_ASSERT_EQUAL(64, open_ep_size[2]);
  TEST_ASSERT_EQUAL(512, open_ep_size[3]); // interrupt: left for tu_edpt_validate() to reject
}

void test_parse_config_keeps_highspeed_bulk_512(void) {
  parse_cfg_bulk_512(TUSB_SPEED_HIGH);
  TEST_ASSERT_EQUAL(512, open_ep_size[0]);
  TEST_ASSERT_EQUAL(512, open_ep_size[1]);
  TEST_ASSERT_EQUAL(512, open_ep_size[2]);
  TEST_ASSERT_EQUAL(512, open_ep_size[3]);
}

// drivers read interface fields before checking bLength: an interface shorter than defined is skipped (#4508)
void test_parse_config_skips_short_interface(void) {
  const uint8_t desc[] = {
    9, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(9 + 8 + 9), 2, 1, 0, 0x80, 50,
    8, TUSB_DESC_INTERFACE, 0, 0, 0, 0xFF, 0, 0,
    9, TUSB_DESC_INTERFACE, 1, 0, 0, 0xFF, 0, 0, 0,
  };

  TEST_ASSERT_TRUE(enum_parse_configuration_desc(DADDR, (const tusb_desc_configuration_t *) desc));
  TEST_ASSERT_EQUAL(1, open_count);
  TEST_ASSERT_EQUAL(1, open_itf_num);
}

//--------------------------------------------------------------------+
// usbh_edpt_clear_stall
//--------------------------------------------------------------------+
enum {
  EP_BULK_IN = 0x81,
  OTHER_RHPORT = 1,
};

void test_clear_stall_reaches_hcd_on_device_rhport(void) {
  get_device(DADDR)->bus_info.rhport = OTHER_RHPORT;

  TEST_ASSERT_TRUE(usbh_edpt_clear_stall(DADDR, EP_BULK_IN));
  TEST_ASSERT_EQUAL(1, clear_stall_count);
  TEST_ASSERT_EQUAL(OTHER_RHPORT, clear_stall_rhport);
  TEST_ASSERT_EQUAL_HEX8(EP_BULK_IN, clear_stall_ep);
}

void test_clear_stall_propagates_hcd_failure(void) {
  clear_stall_result = false;
  TEST_ASSERT_FALSE(usbh_edpt_clear_stall(DADDR, EP_BULK_IN));
  TEST_ASSERT_EQUAL(1, clear_stall_count);
}

//--------------------------------------------------------------------+
// Transfer buffer alignment (TUP_HCD_MEM_ALIGN)
//--------------------------------------------------------------------+
TU_ATTR_ALIGNED(TUP_HCD_MEM_ALIGN) static uint8_t xfer_buf[2 * TUP_HCD_MEM_ALIGN];

static void control_xfer_cb(tuh_xfer_t *xfer) {
  (void) xfer;
}

// The buffer of a control request is checked only when it has a data stage: SETUP is sent from _usbh_epbuf and the
// status stage has no buffer, so the HCD never touches it otherwise.
void test_control_xfer_checks_alignment_only_with_data_stage(void) {
  tusb_control_request_t request = {
    .bmRequestType = 0x80, .bRequest = TUSB_REQ_GET_DESCRIPTOR, .wValue = 0x0100, .wIndex = 0, .wLength = 8
  };
  tuh_xfer_t xfer = {.daddr = DADDR, .ep_addr = 0, .setup = &request, .buffer = xfer_buf + 1, .complete_cb = control_xfer_cb};
  TEST_ASSERT_FALSE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);

  request = (tusb_control_request_t) {
    .bmRequestType = 0x00, .bRequest = TUSB_REQ_SET_CONFIGURATION, .wValue = 1, .wIndex = 0, .wLength = 0
  };
  TEST_ASSERT_TRUE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(CONTROL_STAGE_SETUP, _usbh_data.ctrl_xfer_info.stage);
}

// A zero-length packet moves no data: its buffer is not checked
void test_edpt_xfer_checks_alignment_only_with_data(void) {
  TEST_ASSERT_FALSE(usbh_edpt_xfer(DADDR, EP_BULK_IN, xfer_buf + 1, 8));
  TEST_ASSERT_FALSE(usbh_edpt_busy(DADDR, EP_BULK_IN));

  TEST_ASSERT_TRUE(usbh_edpt_xfer(DADDR, EP_BULK_IN, xfer_buf + 1, 0));
  TEST_ASSERT_TRUE(usbh_edpt_busy(DADDR, EP_BULK_IN));
}

void test_clear_stall_rejects_busy_endpoint(void) {
  get_device(DADDR)->ep_status[1][TUSB_DIR_IN] = TU_EDPT_STATE_BUSY;
  TEST_ASSERT_FALSE(usbh_edpt_clear_stall(DADDR, EP_BULK_IN));
  TEST_ASSERT_EQUAL(0, clear_stall_count);
}

void test_clear_stall_rejects_invalid_target(void) {
  TEST_ASSERT_NOT_EQUAL(0, tu_edpt_number(0x80 | CFG_TUH_ENDPOINT_MAX));
  TEST_ASSERT_FALSE(usbh_edpt_clear_stall(DADDR, 0x80));                         // EP0
  TEST_ASSERT_FALSE(usbh_edpt_clear_stall(DADDR, 0x80 | CFG_TUH_ENDPOINT_MAX));  // beyond ep_status, not EP0
  TEST_ASSERT_FALSE(usbh_edpt_clear_stall(0, EP_BULK_IN));
  TEST_ASSERT_FALSE(usbh_edpt_clear_stall(TOTAL_DEVICES + 1, EP_BULK_IN));
  TEST_ASSERT_FALSE(usbh_edpt_clear_stall(OTHER_ADDR, EP_BULK_IN));              // not connected
  TEST_ASSERT_EQUAL(0, clear_stall_count);
}

//--------------------------------------------------------------------+
// Enumeration failures end the enumeration: the next attach is not held behind it
//--------------------------------------------------------------------+
static void assert_enumeration_ended(void) {
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
  TEST_ASSERT_NULL(_usbh_data.call_after.func);
  TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_EQUAL(0, mount_cb_count);
}

static void run_enumeration(uint8_t daddr, uintptr_t state, const void *ctrl_data, uint16_t len) {
  static const tusb_control_request_t req_get_config = {
    .bmRequestType = 0x80, .bRequest = TUSB_REQ_GET_DESCRIPTOR, .wValue = TUSB_DESC_CONFIGURATION << 8, .wLength = 9
  };
  _usbh_data.root_retry_count = USBH_ENUM_ROOT_RETRY_MAX; // no root port retry: it would post an attach event
  memcpy(_usbh_epbuf.ctrl, ctrl_data, len);
  tuh_xfer_t xfer = {
    .daddr = daddr, .result = XFER_RESULT_SUCCESS, .setup = &req_get_config, .actual_len = len, .user_data = state
  };
  process_enumeration(&xfer);
}

// every address taken: the device at address 0 fails instead of holding the enumeration
void test_enum_no_free_address_ends_enumeration(void) {
  static const uint8_t desc_dev8[8] = {18, TUSB_DESC_DEVICE, U16_TO_U8S_LE(0x0200), 0, 0, 0, 64};
  for (uint8_t daddr = 1; daddr <= CFG_TUH_DEVICE_MAX; daddr++) {
    get_device(daddr)->connected = 1;
  }
  _usbh_data.enumerating_daddr = 0;

  run_enumeration(0, ENUM_SET_ADDR, desc_dev8, sizeof(desc_dev8));

  assert_enumeration_ended();
}

// a configuration larger than the enumeration buffer is not read, and fails the device
void test_enum_config_larger_than_buffer_ends_enumeration(void) {
  static const uint8_t desc_cfg9[9] = {
    9, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(CFG_TUH_ENUMERATION_BUFSIZE + 1), 1, 1, 0, 0x80, 50
  };

  run_enumeration(DADDR, ENUM_GET_FULL_CONFIG_DESC, desc_cfg9, sizeof(desc_cfg9));

  assert_enumeration_ended();
  TEST_ASSERT_FALSE(get_device(DADDR)->connected);
}

// a vendor interface whose endpoint is beyond ep_status: the driver opens it but the binding is refused
static const uint8_t desc_cfg_refused_binding[] = {
  9, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(9 + 9 + 7), 1, 1, 0, 0x80, 50,
  9, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_VENDOR_SPECIFIC, 0, 0, 0,
  7, TUSB_DESC_ENDPOINT, 0x80 | CFG_TUH_ENDPOINT_MAX, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
};

// the driver opened, but its endpoint cannot be bound: the driver is closed and the device fails
void test_enum_refused_binding_closes_driver_and_ends_enumeration(void) {
  run_enumeration(DADDR, ENUM_CONFIG_DRIVER, desc_cfg_refused_binding, sizeof(desc_cfg_refused_binding));

  assert_enumeration_ended();
  TEST_ASSERT_EQUAL(1, close_count);
  TEST_ASSERT_EQUAL(0, set_config_count);
  TEST_ASSERT_FALSE(get_device(DADDR)->connected);
}

// a close callback that drives the usbh task (a sync transfer on OS_NONE) handles the unplug and the next attach: the
// failed attempt must not then tear down the new one
void test_enum_refused_binding_keeps_enumeration_started_by_close_callback(void) {
  close_hook = remove_device_and_attach_another;

  run_enumeration(DADDR, ENUM_CONFIG_DRIVER, desc_cfg_refused_binding, sizeof(desc_cfg_refused_binding));

  TEST_ASSERT_EQUAL(2, close_count); // the failed binding, then the unplug
  TEST_ASSERT_FALSE(get_device(DADDR)->connected);
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL_PTR(enum_delay_async, _usbh_data.call_after.func);
  TEST_ASSERT_EQUAL(2, _usbh_data.root_retry_count);
}

// the close callback only handles the unplug: the attempt has ended already, with no root port retry
void test_enum_refused_binding_ended_by_close_callback_unplug(void) {
  const uint8_t enum_gen = _usbh_data.enum_gen;
  close_hook = remove_device;

  run_enumeration(DADDR, ENUM_CONFIG_DRIVER, desc_cfg_refused_binding, sizeof(desc_cfg_refused_binding));

  TEST_ASSERT_EQUAL(2, close_count);
  TEST_ASSERT_FALSE(get_device(DADDR)->connected);
  TEST_ASSERT_EQUAL(enum_gen, _usbh_data.enum_gen);
  TEST_ASSERT_EQUAL(0, _usbh_data.root_retry_count);
  assert_enumeration_ended();
}

// the new attempt already holds the failed device's address: only enum_gen tells it apart
void test_enum_refused_binding_keeps_new_attempt_at_same_address(void) {
  close_hook = remove_device_and_readdress_another;

  run_enumeration(DADDR, ENUM_CONFIG_DRIVER, desc_cfg_refused_binding, sizeof(desc_cfg_refused_binding));

  TEST_ASSERT_EQUAL(2, close_count);
  TEST_ASSERT_TRUE(get_device(DADDR)->connected);
  TEST_ASSERT_EQUAL(DADDR, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL_PTR(enum_delay_async, _usbh_data.call_after.func);
}
