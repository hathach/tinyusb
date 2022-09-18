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

#include "tusb_option.h"

#if (CFG_TUH_ENABLED && CFG_TUH_VENDOR)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "host/usbh.h"
#include "host/usbh_classdriver.h"
#include "vendor_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t ep_in;
  uint8_t ep_out;
} custom_interface_info_t;
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
custom_interface_info_t custom_interface[CFG_TUH_DEVICE_MAX];

static bool cush_validate_paras(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void *p_buffer, uint16_t length)
{
  if (!tusbh_custom_is_mounted(dev_addr, vendor_id, product_id))
  {
    return false;
  }

  TU_ASSERT(p_buffer != NULL && length != 0, 0);

  return true;
}
//--------------------------------------------------------------------+
// APPLICATION API (need to check parameters)
//--------------------------------------------------------------------+
bool tusbh_custom_read(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void *p_buffer, uint16_t length)
{
  TU_VERIFY(tuh_mounted(dev_addr));
  TU_VERIFY(p_buffer != NULL && length);

  uint8_t const ep_in = custom_interface[dev_addr - 1].ep_in;
  if (usbh_edpt_busy(dev_addr, ep_in))
    return false;

  return usbh_edpt_xfer(dev_addr, ep_in, (void *)(uintptr_t)p_buffer, (uint16_t)length);
}

bool tusbh_custom_write(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void const *p_data, uint16_t length)
{
  TU_VERIFY(tuh_mounted(dev_addr));
  TU_VERIFY(p_data != NULL && length);

  uint8_t const ep_out = custom_interface[dev_addr - 1].ep_out;
  if (usbh_edpt_busy(dev_addr, ep_out))
    return false;

  return usbh_edpt_xfer(dev_addr, ep_out, (void *)(uintptr_t)p_data, (uint16_t)length);
}

//--------------------------------------------------------------------+
// USBH-CLASS API
//--------------------------------------------------------------------+
void cush_init(void)
{
  tu_memclr(&custom_interface, sizeof(custom_interface_info_t) * CFG_TUH_DEVICE_MAX);
}

bool cush_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *p_interface_desc, uint16_t max_len)
{
  (void) max_len;
  (void) rhport;
  // FIXME quick hack to test lpc1k custom class with 2 bulk endpoints
  uint8_t const *p_desc = (uint8_t const *)p_interface_desc;
  p_desc = tu_desc_next(p_desc);

  tusb_desc_endpoint_t const *p_endpoint = (tusb_desc_endpoint_t const *)p_desc;
  TU_ASSERT(TUSB_DESC_ENDPOINT == p_endpoint->bDescriptorType, 0);
  uint8_t ep = (p_endpoint->bEndpointAddress & TUSB_DIR_IN_MASK) ? custom_interface[dev_addr - 1].ep_in : custom_interface[dev_addr - 1].ep_out;
  TU_ASSERT(tuh_edpt_open(dev_addr, p_endpoint), 0);

  p_desc = tu_desc_next(p_desc);
  return true;
}

void cush_close(uint8_t dev_addr)
{
  custom_interface_info_t *p_cush = &custom_interface[dev_addr - 1];
  tu_memclr(p_cush, sizeof(custom_interface_info_t));
}

#endif
