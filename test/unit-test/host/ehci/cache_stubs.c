// SPDX-License-Identifier: MIT
#include <stdbool.h>
#include <stdint.h>

// Match USBH's default hooks on a controller without data-cache maintenance.
// Strong definitions override EHCI's competing weak no-op hooks, as the
// linker does when it selects USBH's defaults on LPC43.
bool hcd_dcache_clean(void const* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
  return false;
}

bool hcd_dcache_invalidate(void const* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
  return false;
}

bool hcd_dcache_clean_invalidate(void const* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
  return false;
}
