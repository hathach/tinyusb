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
 * Test me with:
 * 
 * ceedling test:pattern[hid_host_joy]
 */

#include "hid_host_parse.h"
#include "hid_host_utils.h"

static tusb_hid_host_info_t hid_info[HID_HOST_MAX_SIMPLE_DEVICES];

tusb_hid_host_info_t* tuh_hid_get_info(uint8_t dev_addr, uint8_t instance)
{
  tusb_hid_host_info_key_t key;
  key.combined = 0;
  key.elements.dev_addr = dev_addr;
  key.elements.instance = instance;
  key.elements.in_use = 1;
  uint32_t combined  = key.combined;
  
  for(uint8_t i = 0; i < HID_HOST_MAX_SIMPLE_DEVICES; ++i) {
    tusb_hid_host_info_t* info = &hid_info[i];
    if (info->key.combined == combined) return info;
  }
  return NULL;
}

void tuh_hid_free_sinlge_info(tusb_hid_host_info_t* info)
{
  info->key.elements.in_use = 0;
  // May also want to call some unmount function
  // ...
}

void tuh_hid_free_all_info()
{
  for(uint8_t i = 0; i < HID_HOST_MAX_SIMPLE_DEVICES; ++i) {
    tuh_hid_free_sinlge_info(&hid_info[i]);
  } 
}

void tuh_hid_free_info(uint8_t dev_addr, uint8_t instance)
{
  for(uint8_t i = 0; i < HID_HOST_MAX_SIMPLE_DEVICES; ++i) {
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
  void (*handler)(struct tusb_hid_host_info* info, const uint8_t* report, uint8_t report_length, uint8_t report_id))
{
  for(uint8_t i = 0; i < HID_HOST_MAX_SIMPLE_DEVICES; ++i) {
    tusb_hid_host_info_t* info = &hid_info[i];
    if (!info->key.elements.in_use) {
      tu_memclr(info, sizeof(tusb_hid_host_info_t));
      info->key.elements.in_use = true;
      info->key.elements.dev_addr = dev_addr;
      info->key.elements.instance = instance;
      info->has_report_id = has_report_id;
      info->handler = handler;
      return info;
    }
  }
  printf("FAILED TO ALLOCATE INFO\n");
  return NULL;
}


