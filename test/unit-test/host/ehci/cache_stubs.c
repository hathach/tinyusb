// SPDX-License-Identifier: MIT
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "tusb_option.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"

unsigned test_cache_calls[3];

void test_cache_buffer(void const* addr, uint32_t size);

// Reject cache maintenance on descriptor storage in every build. With cache
// disabled, retain USBH's false-returning default hooks used on LPC43.
bool hcd_dcache_clean(void const* addr, uint32_t data_size) {
  test_cache_calls[0]++;
  test_cache_buffer(addr, data_size);
  return CFG_TUH_MEM_DCACHE_ENABLE != 0;
}

bool hcd_dcache_invalidate(void const* addr, uint32_t data_size) {
  test_cache_calls[1]++;
  test_cache_buffer(addr, data_size);
  return CFG_TUH_MEM_DCACHE_ENABLE != 0;
}

bool hcd_dcache_clean_invalidate(void const* addr, uint32_t data_size) {
  test_cache_calls[2]++;
  test_cache_buffer(addr, data_size);
  return CFG_TUH_MEM_DCACHE_ENABLE != 0;
}

#if CFG_TUH_HUB && defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
// Driver fixtures use the real hub allocator, but never enumerate or transfer
// through the hub class driver. Fail immediately if those paths are called.
bool tuh_descriptor_get_device_local(uint8_t daddr, tusb_desc_device_t* desc_device) {
  (void) daddr;
  (void) desc_device;
  abort();
}

bool tuh_edpt_open(uint8_t daddr, tusb_desc_endpoint_t const* desc_ep) {
  (void) daddr;
  (void) desc_ep;
  abort();
}

uint8_t usbh_get_rhport(uint8_t daddr) {
  (void) daddr;
  abort();
}

bool tuh_control_xfer(tuh_xfer_t* xfer) {
  (void) xfer;
  abort();
}

bool tuh_interface_set(uint8_t daddr, uint8_t itf_num, uint8_t itf_alt,
                       tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) daddr;
  (void) itf_num;
  (void) itf_alt;
  (void) complete_cb;
  (void) user_data;
  abort();
}

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  (void) ep_addr;
  abort();
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  (void) ep_addr;
  abort();
}

bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) dev_addr;
  (void) ep_addr;
  (void) buffer;
  (void) total_bytes;
  (void) complete_cb;
  (void) user_data;
  abort();
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void) dev_addr;
  (void) itf_num;
  abort();
}

#endif
