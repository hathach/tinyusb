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

/**
 * This test program is designed to send and receive MIDI data concurrently
 * It send G, G#, A, A#, B over and over again on the highest cable number
 * of the MIDI device. The notes correspond to the Mackie Control transport
 * button LEDs, so most keyboards and controllers will react to these note
 * messages from the host. The host will print on the serial port console
 * any incoming messages along with the cable number that sent them.
 *
 * If you define MIDIH_TEST_KORG_NANOKONTROL2 to 1 and connect a Korg
 * nanoKONTROL2 controller to the host, then the host will request a dump
 * of the current scene, and the nanoKONTROL2 will respond with a long
 * sysex message (used to prove that sysex decoding appears to work)
 */
#include "bsp/board.h"
#include "tusb.h"
#include "class/midi/midi_host.h"
//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
#ifndef MIDIH_TEST_KORG_NANOKONTROL2
#define MIDIH_TEST_KORG_NANOKONTROL2 0
#endif

#define MIDIH_TEST_KORG_NANOKONTROL2_COUNT 5
//--------------------------------------------------------------------+
// STATIC GLOBALS DECLARATION
//--------------------------------------------------------------------+
static uint8_t midi_dev_addr = 0;
static uint8_t first_note = 0x5b; // Mackie Control rewind
static uint8_t last_note = 0x5f; // Mackie Control stop
static uint8_t message[6] =
{
  0x90, 0x5f, 0x00,
  0x90, 0x5b, 0x7f,
};
#if MIDIH_TEST_KORG_NANOKONTROL2
// This command will dump a scene from a Kork nanoKONTROL2 control surface
// Use it to test receiving long sysex messages
static uint8_t nanoKONTROL2_dump_req[] = 
{
  0xf0, 0x42, 0x40, 0x00, 0x01, 0x13, 0x00, 0x1f, 0x10, 0x00, 0xf7
};
// wait MIDIH_TEST_KORG_NANOKONTROL2_COUNT seconds before requesting a dump
static uint8_t dump_req_countdown = MIDIH_TEST_KORG_NANOKONTROL2_COUNT; 
#endif

static void test_tx(void)
{
  // toggle NOTE On, Note Off for the Mackie Control channels 1-8 REC LED
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  // device must be attached and have at least one endpoint ready to receive a message
  if (!midi_dev_addr || !tuh_midi_configured(midi_dev_addr))
  {
    return;
  }
  if (tuh_midih_get_num_tx_cables(midi_dev_addr) < 1)
  {
    return;
  }
  // transmit any previously queued bytes
  tuh_midi_stream_flush(midi_dev_addr);
  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms)
  {
    return; // not enough time
  }
  start_ms += interval_ms;

  uint32_t nwritten = 0;
#if MIDIH_TEST_KORG_NANOKONTROL2
  if (dump_req_countdown == 0)
  {
    nwritten = tuh_midi_stream_write(midi_dev_addr, 0, nanoKONTROL2_dump_req, sizeof(nanoKONTROL2_dump_req));
    if (nwritten != sizeof(nanoKONTROL2_dump_req))
    {
      _MESS_FAILED();
      TU_BREAKPOINT();
    }
    dump_req_countdown = MIDIH_TEST_KORG_NANOKONTROL2_COUNT;
  }
  else
  {
    nwritten = 0;
    nwritten += tuh_midi_stream_write(midi_dev_addr, 0, message, sizeof(message));
    --dump_req_countdown;
  }
#else
  uint8_t cable = tuh_midih_get_num_tx_cables(midi_dev_addr) - 1;
  nwritten = 0;
  /// @bug For some reason, Arturia Keylab-88 ignores these messages if
  /// they are transimitted on cable 0 first
  //for (cable = 0; cable < tuh_midih_get_num_tx_cables(midi_dev_addr); cable++)
    nwritten += tuh_midi_stream_write(midi_dev_addr, cable, message, sizeof(message));
#endif
 
#if MIDIH_TEST_KORG_NANOKONTROL2
  if (nwritten != 0 && dump_req_countdown != MIDIH_TEST_KORG_NANOKONTROL2_COUNT)
#else
  if (nwritten != 0)
#endif
  {
    ++message[1];
    ++message[4];
    if (message[1] > last_note)
      message[1] = first_note;
    if (message[4] > last_note)
      message[4] = first_note;
  }
}

static void test_rx(void)
{
  // device must be attached and have at least one endpoint ready to receive a message
  if (!midi_dev_addr || !tuh_midi_configured(midi_dev_addr))
  {
    return;
  }
  if (tuh_midih_get_num_rx_cables(midi_dev_addr) < 1)
  {
    return;
  }
  tuh_midi_read_poll(midi_dev_addr);
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
  message[1] = last_note;
  message[4] = first_note;
#if MIDIH_TEST_KORG_NANOKONTROL2
  dump_req_countdown = MIDIH_TEST_KORG_NANOKONTROL2_COUNT;
#endif
  midi_dev_addr = dev_addr;
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  midi_dev_addr = 0;
  printf("MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
  if (midi_dev_addr == dev_addr)
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
}

void tuh_midi_tx_cb(uint8_t dev_addr)
{
    (void ) dev_addr;
}
