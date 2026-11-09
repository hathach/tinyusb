// Scratch HIL firmware for PR #4265: abort a bulk IN while the device NAKs (#4405) or while data is in flight, then
// check the next transfers get the right data and the aborted buffer is never written after the abort returns.
// Prints "ABORT_TEST PASS" or "ABORT_TEST FAIL <reason>".

#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "host/hcd.h"

#ifndef ROUNDS
#define ROUNDS 200
#endif

#define ALIGNED __attribute__((aligned(64)))

typedef struct {
  uint8_t  daddr;
  uint8_t  ep_in;
  uint8_t  ep_out;
  uint16_t mps_in;
} bulk_dev_t;

typedef struct {
  volatile bool          done;
  volatile xfer_result_t result;
  volatile uint32_t      len;
} xfer_state_t;

static bulk_dev_t msc, ser;
static volatile bool mounted[CFG_TUH_DEVICE_MAX + CFG_TUH_HUB + 1];
static volatile bool enum_dirty;

CFG_TUH_MEM_SECTION ALIGNED static uint8_t cfg_buf[512];
CFG_TUH_MEM_SECTION ALIGNED static uint8_t cbw[64];
CFG_TUH_MEM_SECTION ALIGNED static uint8_t csw[64];
CFG_TUH_MEM_SECTION ALIGNED static uint8_t ref[2][512];
CFG_TUH_MEM_SECTION ALIGNED static uint8_t data[576];
CFG_TUH_MEM_SECTION ALIGNED static uint8_t victim[576];
CFG_TUH_MEM_SECTION ALIGNED static tusb_desc_device_t desc_dev;
static uint8_t snap[576];

static xfer_state_t xs_victim, xs_data;
static uint32_t     tag = 0x1000;
static uint32_t     cnt_completed, cnt_intact, cnt_partial, cnt_lost, cnt_ser_data;

//--------------------------------------------------------------------+
// helpers
//--------------------------------------------------------------------+
static void idle_ms(uint32_t ms) {
#if CFG_TUSB_OS == OPT_OS_FREERTOS
  vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1));
#else
  const uint32_t t0 = board_millis();
  do {
    tuh_task();
  } while (board_millis() - t0 < ms);
#endif
}

static void xfer_cb(tuh_xfer_t* xfer) {
  xfer_state_t* s = (xfer_state_t*) xfer->user_data;
  s->result = xfer->result;
  s->len    = xfer->actual_len;
  s->done   = true;
}

static bool bulk_submit(uint8_t daddr, uint8_t ep, uint8_t* buf, uint16_t len, xfer_state_t* s) {
  s->done   = false;
  s->len    = 0;
  s->result = XFER_RESULT_INVALID;
  tuh_xfer_t xfer = {
    .daddr = daddr, .ep_addr = ep, .buflen = len, .buffer = buf, .complete_cb = xfer_cb, .user_data = (uintptr_t) s};
  return tuh_edpt_xfer(&xfer);
}

static bool wait_done(xfer_state_t* s, uint32_t ms) {
  const uint32_t t0 = board_millis();
  while (!s->done) {
    if (board_millis() - t0 > ms) {
      return false;
    }
    idle_ms(1);
  }
  return true;
}

static void fail(const char* fmt, uint32_t round, uint32_t a, uint32_t b) {
  printf("ABORT_TEST FAIL ");
  printf(fmt, (unsigned) round, (unsigned) a, (unsigned) b);
  printf("\r\n");
  while (1) {
    idle_ms(1000);
  }
}

static bool bulk_xfer_sync(uint8_t daddr, uint8_t ep, uint8_t* buf, uint16_t len, uint32_t* got) {
  xfer_state_t s;
  if (!bulk_submit(daddr, ep, buf, len, &s) || !wait_done(&s, 2000) || s.result != XFER_RESULT_SUCCESS) {
    return false;
  }
  if (got) {
    *got = s.len;
  }
  return true;
}

//--------------------------------------------------------------------+
// MSC bulk-only transport
//--------------------------------------------------------------------+
static bool msc_cbw(const uint8_t* cdb, uint8_t cdb_len, uint32_t xfer_len, bool dir_in) {
  memset(cbw, 0, 31);
  tu_unaligned_write32(cbw, 0x43425355);
  tu_unaligned_write32(cbw + 4, ++tag);
  tu_unaligned_write32(cbw + 8, xfer_len);
  cbw[12] = dir_in ? 0x80 : 0;
  cbw[14] = cdb_len;
  memcpy(cbw + 15, cdb, cdb_len);
  return bulk_xfer_sync(msc.daddr, msc.ep_out, cbw, 31, NULL);
}

// 13-byte CSW at p: signature, tag; returns status or -1
static int csw_check(const uint8_t* p) {
  if (tu_unaligned_read32(p) != 0x53425355 || tu_unaligned_read32(p + 4) != tag) {
    return -1;
  }
  return p[12];
}

static int msc_csw(void) {
  uint32_t got = 0;
  if (!bulk_xfer_sync(msc.daddr, msc.ep_in, csw, 13, &got) || got != 13) {
    return -2;
  }
  return csw_check(csw);
}

static void cdb_read10(uint8_t* cdb, uint32_t lba) {
  memset(cdb, 0, 10);
  cdb[0] = 0x28;
  cdb[2] = (uint8_t) (lba >> 24);
  cdb[3] = (uint8_t) (lba >> 16);
  cdb[4] = (uint8_t) (lba >> 8);
  cdb[5] = (uint8_t) lba;
  cdb[8] = 1;
}

static int msc_test_unit_ready(void) {
  const uint8_t cdb[6] = {0};
  if (!msc_cbw(cdb, 6, 0, false)) {
    return -3;
  }
  return msc_csw();
}

static bool msc_read_block(uint32_t lba, uint8_t* dst) {
  uint8_t cdb[10];
  cdb_read10(cdb, lba);
  uint32_t got = 0;
  return msc_cbw(cdb, 10, 512, true) && bulk_xfer_sync(msc.daddr, msc.ep_in, dst, 512, &got) && got == 512 &&
         msc_csw() == 0;
}

static bool msc_ready(void) {
  for (int i = 0; i < 10; i++) {
    const int st = msc_test_unit_ready();
    if (st == 0) {
      return true;
    }
    if (st < 0) {
      return false;
    }
    const uint8_t cdb[6] = {0x03, 0, 0, 0, 18, 0}; // REQUEST SENSE
    uint32_t      got;
    if (!msc_cbw(cdb, 6, 18, true) || !bulk_xfer_sync(msc.daddr, msc.ep_in, data, 18, &got) || msc_csw() < 0) {
      return false;
    }
    idle_ms(100);
  }
  return false;
}

//--------------------------------------------------------------------+
// abort helpers
//--------------------------------------------------------------------+
// After the abort returns, the controller must not write the aborted buffer any more.
static void check_victim_quiet(uint32_t round, uint16_t len) {
  hcd_dcache_invalidate(victim, sizeof(victim));
  memcpy(snap, victim, len);
  idle_ms(3);
  hcd_dcache_invalidate(victim, sizeof(victim));
  if (memcmp(snap, victim, len) != 0) {
    fail("round %u: aborted buffer written after the abort returned (len %u/%u)", round, len, 0);
  }
}

static void prep_victim(void) {
  memset(victim, 0xA5, sizeof(victim));
  hcd_dcache_clean(victim, sizeof(victim));
}

static bool all_a5(const uint8_t* p, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) {
    if (p[i] != 0xA5) {
      return false;
    }
  }
  return true;
}

// M1 (#4405): abort a bulk IN while the device has nothing to send, then read a block
static void test_msc_nak_abort(uint32_t round) {
  if (msc_test_unit_ready() != 0) {
    fail("round %u: TEST UNIT READY failed before the abort %u%u", round, 0, 0);
  }

  prep_victim();
  if (!bulk_submit(msc.daddr, msc.ep_in, victim, 512, &xs_victim)) {
    fail("round %u: submit IN failed %u%u", round, 0, 0);
  }
  idle_ms(1 + round % 20);
  if (xs_victim.done) {
    fail("round %u: idle IN completed (result %u len %u)", round, xs_victim.result, xs_victim.len);
  }
  tuh_edpt_abort_xfer(msc.daddr, msc.ep_in);
  check_victim_quiet(round, 512);
  if (!all_a5(victim, 512)) {
    fail("round %u: aborted idle IN received data %u%u", round, 0, 0);
  }
  if (xs_victim.done) {
    fail("round %u: aborted IN completed later (result %u len %u)", round, xs_victim.result, xs_victim.len);
  }

  const uint32_t lba = round & 1;
  uint8_t        cdb[10];
  cdb_read10(cdb, lba);
  if (!msc_cbw(cdb, 10, 512, true)) {
    fail("round %u: READ10 CBW failed after the abort %u%u", round, 0, 0);
  }
  memset(data, 0, sizeof(data));
  if (!bulk_submit(msc.daddr, msc.ep_in, data, 512, &xs_data) || !wait_done(&xs_data, 2000)) {
    fail("round %u: READ10 data IN timed out after the abort %u%u", round, 0, 0);
  }
  if (xs_data.result != XFER_RESULT_SUCCESS || xs_data.len != 512) {
    fail("round %u: READ10 data IN result %u len %u", round, xs_data.result, xs_data.len);
  }
  if (memcmp(data, ref[lba], 512) != 0) {
    fail("round %u: READ10 data mismatch lba %u (stale data?) %u", round, lba, 0);
  }
  const int st = msc_csw();
  if (st != 0) {
    fail("round %u: CSW after the abort bad (%u) %u", round, (uint32_t) st, 0);
  }
}

// M2: abort a READ10 data IN at a varying point while the block is being sent
static void test_msc_race_abort(uint32_t round) {
  const uint32_t lba = round & 1;
  uint8_t        cdb[10];
  cdb_read10(cdb, lba);
  if (!msc_cbw(cdb, 10, 512, true)) {
    fail("round %u: READ10 CBW failed %u%u", round, 0, 0);
  }
  prep_victim();
  if (!bulk_submit(msc.daddr, msc.ep_in, victim, 512, &xs_victim)) {
    fail("round %u: submit IN failed %u%u", round, 0, 0);
  }
  for (volatile uint32_t spin = 0; spin < (round % 16) * 500u; spin++) {}
  tuh_edpt_abort_xfer(msc.daddr, msc.ep_in);
  check_victim_quiet(round, 512);
  idle_ms(2);

  if (xs_victim.done) {
    // completed before the abort took it
    cnt_completed++;
    if (xs_victim.result != XFER_RESULT_SUCCESS || xs_victim.len != 512) {
      fail("round %u: raced IN completed with result %u len %u", round, xs_victim.result, xs_victim.len);
    }
    hcd_dcache_invalidate(victim, sizeof(victim));
    if (memcmp(victim, ref[lba], 512) != 0) {
      fail("round %u: raced IN completed with wrong data lba %u %u", round, lba, 0);
    }
    if (msc_csw() != 0) {
      fail("round %u: CSW after a completed raced IN bad %u%u", round, 0, 0);
    }
    return;
  }

  // the rest of the block (whole, partial or none) then the CSW
  memset(data, 0, sizeof(data));
  if (!bulk_submit(msc.daddr, msc.ep_in, data, 512, &xs_data) || !wait_done(&xs_data, 2000)) {
    fail("round %u: re-read after a raced abort timed out %u%u", round, 0, 0);
  }
  if (xs_data.result != XFER_RESULT_SUCCESS) {
    fail("round %u: re-read after a raced abort result %u len %u", round, xs_data.result, xs_data.len);
  }
  const uint32_t n = xs_data.len;
  if (n == 512) {
    cnt_intact++;
    if (memcmp(data, ref[lba], 512) != 0) {
      fail("round %u: re-read block mismatch lba %u %u", round, lba, 0);
    }
    if (msc_csw() != 0) {
      fail("round %u: CSW after the re-read bad %u%u", round, 0, 0);
    }
    return;
  }
  if (n < 13 || csw_check(data + n - 13) != 0) {
    fail("round %u: re-read after a raced abort neither block nor CSW (len %u) %u", round, n, 0);
  }
  const uint32_t rest = n - 13;
  if (rest == 0) {
    cnt_lost++;
  } else {
    cnt_partial++;
  }
  if (memcmp(data, ref[lba] + 512 - rest, rest) != 0) {
    fail("round %u: re-read tail of %u bytes mismatch lba %u", round, rest, lba);
  }
}

// S1: abort a bulk IN on the serial adapter (full speed, behind the hub) while it NAKs, then talk to it again
static void test_serial_nak_abort(uint32_t round) {
  prep_victim();
  if (!bulk_submit(ser.daddr, ser.ep_in, victim, ser.mps_in, &xs_victim)) {
    fail("round %u: serial submit IN failed %u%u", round, 0, 0);
  }
  idle_ms(1 + round % 20);
  const bool done_before = xs_victim.done;
  tuh_edpt_abort_xfer(ser.daddr, ser.ep_in);
  check_victim_quiet(round, ser.mps_in);
  if (done_before) {
    cnt_ser_data++;
  } else if (xs_victim.done) {
    fail("round %u: aborted serial IN completed later (result %u len %u)", round, xs_victim.result, xs_victim.len);
  } else if (!all_a5(victim, ser.mps_in)) {
    fail("round %u: aborted serial IN received data %u%u", round, 0, 0);
  }

  memset(&desc_dev, 0, sizeof(desc_dev));
  if (XFER_RESULT_SUCCESS != tuh_descriptor_get_device_sync(ser.daddr, &desc_dev, 18) ||
      desc_dev.bLength != 18) {
    fail("round %u: serial GET_DESCRIPTOR failed after the abort %u%u", round, 0, 0);
  }
  if (msc.daddr && msc_test_unit_ready() != 0) {
    fail("round %u: MSC TEST UNIT READY failed after the serial abort %u%u", round, 0, 0);
  }
}

//--------------------------------------------------------------------+
// enumeration
//--------------------------------------------------------------------+
static void scan_device(uint8_t daddr) {
  if (XFER_RESULT_SUCCESS != tuh_descriptor_get_device_sync(daddr, &desc_dev, 18)) {
    return;
  }
  const uint16_t vid = desc_dev.idVendor, pid = desc_dev.idProduct;
  if (XFER_RESULT_SUCCESS != tuh_descriptor_get_configuration_sync(daddr, 0, cfg_buf, sizeof(cfg_buf))) {
    return;
  }
  tuh_bus_info_t bus;
  tuh_bus_info_get(daddr, &bus);
  printf("dev %u %04x:%04x speed %u hub %u port %u", daddr, vid, pid, bus.speed, bus.hub_addr, bus.hub_port);
  if (bus.hub_addr) {
    tuh_bus_info_t hub_bus;
    tuh_bus_info_get(bus.hub_addr, &hub_bus);
    printf(" (hub speed %u)", hub_bus.speed);
  }
  printf("\r\n");

  const tusb_desc_configuration_t* desc_cfg = (const tusb_desc_configuration_t*) cfg_buf;
  const uint8_t*                   p        = cfg_buf;
  const uint8_t*                   end      = cfg_buf + tu_min16(desc_cfg->wTotalLength, sizeof(cfg_buf));
  uint8_t                          itf_class = 0;
  bulk_dev_t                       found    = {0};
  while (p < end && p[0] >= 2) {
    if (p[1] == TUSB_DESC_INTERFACE) {
      itf_class = ((const tusb_desc_interface_t*) p)->bInterfaceClass;
    } else if (p[1] == TUSB_DESC_ENDPOINT) {
      const tusb_desc_endpoint_t* ep = (const tusb_desc_endpoint_t*) p;
      const bool is_data_itf = itf_class == TUSB_CLASS_MSC || itf_class == TUSB_CLASS_CDC_DATA ||
                               itf_class == TUSB_CLASS_VENDOR_SPECIFIC;
      if (is_data_itf && ep->bmAttributes.xfer == TUSB_XFER_BULK && tuh_edpt_open(daddr, ep)) {
        found.daddr = daddr;
        if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN) {
          found.ep_in  = ep->bEndpointAddress;
          found.mps_in = tu_edpt_packet_size(ep);
        } else {
          found.ep_out = ep->bEndpointAddress;
        }
      }
      if (found.ep_in && found.ep_out) {
        if (itf_class == TUSB_CLASS_MSC && !msc.daddr) {
          msc = found;
        } else if (itf_class != TUSB_CLASS_MSC && !ser.daddr) {
          ser = found;
        }
        memset(&found, 0, sizeof(found));
      }
    }
    p += p[0];
  }
}

void tuh_mount_cb(uint8_t daddr) {
  if (daddr < TU_ARRAY_SIZE(mounted)) {
    mounted[daddr] = true;
    enum_dirty     = true;
  }
}

void tuh_umount_cb(uint8_t daddr) {
  printf("Device removed, address = %u\r\n", daddr);
  if (daddr < TU_ARRAY_SIZE(mounted)) {
    mounted[daddr] = false;
  }
}

static void run_tests(void) {
  // let every device on the hub enumerate
  uint32_t quiet = board_millis();
  while (board_millis() - quiet < 3000) {
    if (enum_dirty) {
      enum_dirty = false;
      quiet      = board_millis();
    }
    idle_ms(10);
  }
  for (uint8_t daddr = 1; daddr <= CFG_TUH_DEVICE_MAX; daddr++) {
    if (mounted[daddr]) {
      scan_device(daddr);
    }
  }
  printf("msc dev %u in %02x out %02x mps %u; serial dev %u in %02x out %02x mps %u\r\n", msc.daddr, msc.ep_in,
         msc.ep_out, msc.mps_in, ser.daddr, ser.ep_in, ser.ep_out, ser.mps_in);
  if (!msc.daddr) {
    fail("no MSC device %u%u%u", 0, 0, 0);
  }

  if (!msc_ready() || !msc_read_block(0, ref[0]) || !msc_read_block(1, ref[1])) {
    fail("MSC not ready or reference read failed %u%u%u", 0, 0, 0);
  }
  if (memcmp(ref[0], ref[1], 512) == 0) {
    printf("WARN: lba 0 and 1 are identical, stale-data check is weaker\r\n");
  }

  const uint32_t t0 = board_millis();
  for (uint32_t r = 0; r < ROUNDS; r++) {
    test_msc_nak_abort(r);
  }
  printf("M1 msc NAK abort: %u rounds ok (%lu ms)\r\n", ROUNDS, (unsigned long) (board_millis() - t0));

  for (uint32_t r = 0; r < ROUNDS; r++) {
    test_msc_race_abort(r);
  }
  printf("M2 msc raced abort: %u rounds ok: completed %lu intact %lu partial %lu lost %lu\r\n", ROUNDS,
         (unsigned long) cnt_completed, (unsigned long) cnt_intact, (unsigned long) cnt_partial,
         (unsigned long) cnt_lost);

  if (ser.daddr) {
    for (uint32_t r = 0; r < ROUNDS; r++) {
      test_serial_nak_abort(r);
    }
    printf("S1 serial NAK abort: %u rounds ok (data before abort %lu)\r\n", ROUNDS, (unsigned long) cnt_ser_data);
  } else {
    printf("S1 skipped: no serial device\r\n");
  }

  // the MSC still works after everything
  for (uint32_t r = 0; r < 10; r++) {
    test_msc_nak_abort(r);
  }
  printf("ABORT_TEST PASS\r\n");
}

//--------------------------------------------------------------------+
// main
//--------------------------------------------------------------------+
static void init_tinyusb(void) {
  tusb_rhport_init_t host_init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUH_RHPORT, &host_init);
  board_init_after_tusb();
}

#if CFG_TUSB_OS == OPT_OS_FREERTOS
static void usb_host_task(void* param) {
  (void) param;
  board_delay(100);
  init_tinyusb();
  while (1) {
    tuh_task();
  }
}

static void test_task(void* param) {
  (void) param;
  vTaskDelay(pdMS_TO_TICKS(500));
  run_tests();
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

  #ifdef ESP_PLATFORM
    #define USB_STACK_SIZE 4096
  #else
    #define USB_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * 2
  #endif
static StackType_t  usb_stack[USB_STACK_SIZE], test_stack[USB_STACK_SIZE];
static StaticTask_t usb_taskdef, test_taskdef;
#endif

int main(void) {
  board_init();
  printf("ABORT_TEST start (PR #4265 HIL)\r\n");
#if CFG_TUSB_OS == OPT_OS_FREERTOS
  xTaskCreateStatic(usb_host_task, "usbh", USB_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, usb_stack, &usb_taskdef);
  xTaskCreateStatic(test_task, "test", USB_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, test_stack, &test_taskdef);
  #ifndef ESP_PLATFORM
  vTaskStartScheduler();
  #endif
#else
  board_delay(100);
  init_tinyusb();
  run_tests();
  while (1) {
    tuh_task();
  }
#endif
  return 0;
}

#ifdef ESP_PLATFORM
void app_main(void) {
  main();
}
#endif
