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
 */

/* This example current worked and tested with following controller
 * - Sony DualShock 4 [CUH-ZCT2x] VID = 0x054c, PID = 0x09cc
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  printf("TinyUSB Host HID Controller Example\r\n");
  printf("Note: Events only displayed for explictly supported controllers\r\n");

  tusb_init();

  while (1)
  {
    // tinyusb host task
    tuh_task();
    led_blinking_task();
  }

  return 0;
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

uint8_t usb_buf[256] TU_ATTR_ALIGNED(4);

tusb_desc_device_t desc_device;

bool print_device_descriptor(uint8_t daddr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;

  if ( XFER_RESULT_SUCCESS != result )
  {
    printf("Failed to get device descriptor\r\n");
    return false;
  }

  printf("Rhport %u Device %u: ID %04x:%04x\r\n", 0, daddr, desc_device.idVendor, desc_device.idProduct);
  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n", desc_device.bLength);
  printf("  bDescriptorType     %u\r\n", desc_device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n", desc_device.bcdUSB);

  printf("  bDeviceClass        %u\r\n", desc_device.bDeviceClass);
  printf("  bDeviceSubClass     %u\r\n", desc_device.bDeviceSubClass);
  printf("  bDeviceProtocol     %u\r\n", desc_device.bDeviceProtocol);
  printf("  bMaxPacketSize0     %u\r\n", desc_device.bMaxPacketSize0);

  printf("  idVendor            0x%04x\r\n", desc_device.idVendor);
  printf("  idProduct           0x%04x\r\n", desc_device.idProduct);
  printf("  bcdDevice           %04x\r\n", desc_device.bcdDevice);

  printf("  iManufacturer       %u\r\n", desc_device.iManufacturer);
  printf("  iProduct            %u\r\n", desc_device.iProduct);
  printf("  iSerialNumber       %u\r\n", desc_device.iSerialNumber);

  printf("  bNumConfigurations  %u\r\n", desc_device.bNumConfigurations);

  return true;
}

// Invoked when device is mounted (configured)
void tuh_mount_cb (uint8_t daddr)
{
  printf("Device attached, address = %d\r\n", daddr);

  // get device descriptor
  tuh_descriptor_device_get(daddr, &desc_device, 18, print_device_descriptor);
}

/// Invoked when device is unmounted (bus reset/unplugged)
void tuh_umount_cb(uint8_t daddr)
{
  printf("Device removed, address = %d\r\n", daddr);
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
