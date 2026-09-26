// SPDX-License-Identifier: MIT
#include <stdbool.h>
#include <stdint.h>
#include "tusb_option.h"

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
