/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Nathaniel Brough
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
 */

#include "tusb_config.h"
#if defined(CFG_TUD_ECM_RNDIS) || defined(CFG_TUD_NCM)

#include "class/net/net_device.h"
#include "fuzz_private.h"
#include <cassert>
#include <cstdint>
#include <vector>
#include "lwip/sys.h"

extern "C" {
bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  assert(_fuzz_data_provider.has_value());
  (void)src;
  (void)size;
  return _fuzz_data_provider->ConsumeBool();
}

// client must provide this: copy from network stack packet pointer to dst
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  (void)ref;
  (void)arg;

  assert(_fuzz_data_provider.has_value());

  uint16_t size = _fuzz_data_provider->ConsumeIntegral<uint16_t>();
  std::vector<uint8_t> temp = _fuzz_data_provider->ConsumeBytes<uint8_t>(size);
  memcpy(dst, temp.data(), temp.size());
  return size;
}

/* lwip has provision for using a mutex, when applicable */
sys_prot_t sys_arch_protect(void) { return 0; }
void sys_arch_unprotect(sys_prot_t pval) { (void)pval; }

//------------- ECM/RNDIS -------------//

// client must provide this: initialize any network state back to the beginning
void tud_network_init_cb(void) {
  // NoOp.
}

// client must provide this: 48-bit MAC address
// TODO removed later since it is not part of tinyusb stack
uint8_t tud_network_mac_address[6] = {0};

//------------- NCM -------------//

// callback to client providing optional indication of internal state of network
// driver
void tud_network_link_state_cb(bool state) {
  (void)state;
  // NoOp.
}
}

#endif
