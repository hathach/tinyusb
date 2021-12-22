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
  // transmit any previously queued bytes
  tuh_midi_stream_flush();
  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms)
  {
    return; // not enough time
  }
  start_ms += interval_ms;

  uint32_t nwritten = tuh_midi_stream_write(0, message, sizeof(message));
 
  char off_on[4] = {'O','n','\0'};
  if (nwritten != 0)
  {
    if (message[2] == 0x7f)
    {
      off_on[1] = 'n';
      off_on[2] = '\0';
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
      off_on[1] = 'f';
      off_on[2] = 'f';
      off_on[3] = '\0';
      message[2] = 0x7f;
      message[5] = 0x7f;
      message[8] = 0x7f;
      message[11] = 0x7f;
      message[14] = 0x7f;
      message[17] = 0x7f;
      message[20] = 0x7f;
      message[23] = 0x7f;
    }
    TU_LOG1("Switched lights %s\r\n", off_on);
  }
}

static void test_rx(void)
{
  // device must be attached and have at least one endpoint ready to receive a message
  if (!tuh_midi_configured())
  {
    return;
  }
  if (tuh_midih_get_num_rx_cables() < 1)
  {
    return;
  }
  tuh_midi_read_poll();
}

void midi_host_app_task(void)
{
  test_tx();

  test_rx();
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

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
  if (num_packets != 0)
  {
    uint8_t cable_num;
    uint8_t buffer[48];
    uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, sizeof(buffer));
    TU_LOG1("Read bytes %u cable %u", bytes_read, cable_num);
    TU_LOG1_MEM(buffer, bytes_read, 2);
  }
}

void tuh_midi_tx_cb(uint8_t dev_addr)
{

}
