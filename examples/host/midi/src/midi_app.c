/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#include "bsp/board.h"
#include "tusb.h"
#include "class/midi/midi_host.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
static bool device_mounted = false;

static void test_tx(void)
{
  // toggle NOTE On, Note Off for the Mackie Control channels 1-8 REC LED
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static uint8_t message[] =
  {
    0x90, 0x00, 0x7f,
    0x90, 0x01, 0x7f,
    0x90, 0x02, 0x7f,
    0x90, 0x03, 0x7f,
    0x90, 0x04, 0x7f,
    0x90, 0x05, 0x7f,
    0x90, 0x06, 0x7f,
    0x90, 0x07, 0x7f,
  };

  // device must be attached and have at least one endpoint ready to receive a message
  if (!tuh_midi_configured())
  {
    return;
  }
  if (tuh_midih_get_num_tx_cables() < 1)
  {
    return;
  }

  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms)
  {
    return; // not enough time
  }
  start_ms += interval_ms;

  uint32_t nwritten = tuh_midi_stream_write(0, message, sizeof(message));
  printf ("wrote %d bytes to MIDI device\r\n", nwritten);
  if (message[2] == 0x7f)
  {
    message[2] = 0;
    message[5] = 0;
    message[8] = 0;
    message[11] = 0;
    message[14] = 0;
    message[17] = 0;
    message[20] = 0;
    message[23] = 0;
  }
  else
  {
    message[2] = 0x7f;
    message[5] = 0x7f;
    message[8] = 0x7f;
    message[11] = 0x7f;
    message[14] = 0x7f;
    message[17] = 0x7f;
    message[20] = 0x7f;
    message[23] = 0x7f;
  }
}

static void test_rx(void)
{
  #if 1
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;
  #endif
  // device must be attached and have at least one endpoint ready to receive a message
  if (!tuh_midi_configured())
  {
    return;
  }
  if (tuh_midih_get_num_rx_cables() < 1)
  {
    return;
  }
  #if 1
  // poll every interval_ms ms
  if ( board_millis() - start_ms < interval_ms)
  {
    return; // not enough time
  }
  start_ms += interval_ms;
  #endif
  tuh_midi_read_poll();
}

void midi_host_app_task(void)
{
  test_tx();
  //test_rx();
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
  printf("MIDI device address = %u, IN endpoint %u has %u cables, OUT endpoint %u has %u cables\r\n",
      dev_addr, in_ep & 0xf, num_cables_rx, out_ep & 0xf, num_cables_tx);
  device_mounted = true;
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  device_mounted = false;
  printf("MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

#if 0
// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  switch (itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      TU_LOG2("HID receive boot keyboard report\r\n");
      process_kbd_report( (hid_keyboard_report_t const*) report );
    break;

    case HID_ITF_PROTOCOL_MOUSE:
      TU_LOG2("HID receive boot mouse report\r\n");
      process_mouse_report( (hid_mouse_report_t const*) report );
    break;

    default:
      // Generic report requires matching ReportID and contents with previous parsed report info
      process_generic_report(dev_addr, instance, report, len);
    break;
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}
#endif

