/*
 * The MIT License (MIT)
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

#include "hid_host_info.h"

static tusb_hid_host_info_t hid_info[HCD_MAX_ENDPOINT];

tusb_hid_host_info_t* tuh_hid_get_info(uint8_t dev_addr, uint8_t instance)
{
  tusb_hid_host_info_key_t key;
  key.elements.dev_addr = dev_addr;
  key.elements.instance = instance;
  key.elements.in_use = 1;
  uint32_t combined  = key.combined;
  
  // Linear search for endpoint, which is fine while HCD_MAX_ENDPOINT 
  // is small. Perhaps a 'bsearch' version should be used if HCD_MAX_ENDPOINT
  // is large.
  for(uint8_t i = 0; i < HCD_MAX_ENDPOINT; ++i) {
    tusb_hid_host_info_t* info = &hid_info[i];
    if (info->key.combined == combined) return info;
  }
  return NULL;
}

void tuh_hid_free_sinlge_info(tusb_hid_host_info_t* info)
{
  if (info->key.elements.in_use && info->unmount != NULL) {
    info->unmount(info);
  }
  info->key.elements.in_use = 0;
}

void tuh_hid_free_all_info(void)
{
  for(uint8_t i = 0; i < HCD_MAX_ENDPOINT; ++i) {
    tuh_hid_free_sinlge_info(&hid_info[i]);
  } 
}

void tuh_hid_free_info(uint8_t dev_addr, uint8_t instance)
{
  for(uint8_t i = 0; i < HCD_MAX_ENDPOINT; ++i) {
    tusb_hid_host_info_t* info = &hid_info[i];
    if (info->key.elements.instance == instance && info->key.elements.dev_addr == dev_addr && info->key.elements.in_use) {
      tuh_hid_free_sinlge_info(info);
    }
  }
}

tusb_hid_host_info_t* tuh_hid_allocate_info(
  uint8_t dev_addr, 
  uint8_t instance, 
  bool has_report_id,
  void (*handler)(struct tusb_hid_host_info* info, const uint8_t* report, uint8_t report_length, uint8_t report_id),
  void (*unmount)(struct tusb_hid_host_info* info))
{
  for(uint8_t i = 0; i < HCD_MAX_ENDPOINT; ++i) {
    tusb_hid_host_info_t* info = &hid_info[i];
    if (!info->key.elements.in_use) {
      tu_memclr(info, sizeof(tusb_hid_host_info_t));
      info->key.elements.in_use = true;
      info->key.elements.dev_addr = dev_addr;
      info->key.elements.instance = instance;
      info->has_report_id = has_report_id;
      info->handler = handler;
      info->unmount = unmount;
      return info;
    }
  }
  TU_LOG1("FAILED TO ALLOCATE INFO for dev_addr=%d, instance=%d\r\n", dev_addr, instance);
  return NULL;
}


