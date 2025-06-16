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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+

// Voltage and current for selecting PDO
// DANGEROUS: Please make sure your board can withstand the voltage and current
// defined here. Otherwise, you may damage your board, smoke can come out
#define VOLTAGE_MAX_MV       5000 // maximum voltage in mV
#define CURRENT_MAX_MA       500  // maximum current in mA
#define CURRENT_OPERATING_MA 100  // operating current in mA

/* Blink pattern
 * - 250 ms  : button is not pressed
 * - 1000 ms : button is pressed (and hold)
 */
enum  {
  BLINK_PRESSED = 250,
  BLINK_UNPRESSED = 1000
};

static uint32_t blink_interval_ms = BLINK_UNPRESSED;

void led_blinking_task(void);

#define HELLO_STR   "Hello from TinyUSB\r\n"

int main(void)
{
  board_init();
  board_led_write(true);

  tuc_init(0, TUSB_TYPEC_PORT_SNK);

  while (1) {
    led_blinking_task();

    // tinyusb typec task
    tuc_task();
  }
}

#if TUSB_MCU_VENDOR_ESPRESSIF
void app_main(void)
{
  main();
}
#endif

//--------------------------------------------------------------------+
// TypeC PD callbacks
//--------------------------------------------------------------------+

bool tuc_pd_data_received_cb(uint8_t rhport, pd_header_t const* header, uint8_t const* dobj, uint8_t const* p_end) {
  switch (header->msg_type) {
    case PD_DATA_SOURCE_CAP: {
      printf("PD Source Capabilities\r\n");
      // Examine source capability and select a suitable PDO (starting from 1 with safe5v)
      uint8_t selected_pos = 1;

      for(size_t i=0; i<header->n_data_obj; i++) {
        TU_VERIFY(dobj < p_end);
        uint32_t const pdo = tu_le32toh(tu_unaligned_read32(dobj));

        switch ((pdo >> 30) & 0x03ul) {
          case PD_PDO_TYPE_FIXED: {
            pd_pdo_fixed_t const* fixed = (pd_pdo_fixed_t const*) &pdo;
            uint32_t const voltage_mv = fixed->voltage_50mv*50;
            uint32_t const current_ma = fixed->current_max_10ma*10;
            printf("[Fixed] %"PRIu32" mV %"PRIu32" mA\r\n", voltage_mv, current_ma);

            if (voltage_mv <= VOLTAGE_MAX_MV && current_ma >= CURRENT_MAX_MA) {
              // Found a suitable PDO
              selected_pos = i+1;
            }

            break;
          }

          case PD_PDO_TYPE_BATTERY:
            break;

          case PD_PDO_TYPE_VARIABLE:
            break;

          case PD_PDO_TYPE_APDO:
            break;
        }

        dobj += 4;
      }

      //------------- Response with selected PDO -------------//
      // Be careful and make sure your board can withstand the selected PDO
      // voltage other than safe5v e.g 12v or 20v

      printf("Selected PDO %u\r\n", selected_pos);

      // Send request with selected PDO position as response to Source Cap
      pd_rdo_fixed_variable_t rdo = {
          .current_extremum_10ma = 50, // max 500mA
          .current_operate_10ma = 30, // 300mA
          .reserved = 0,
          .epr_mode_capable = 0,
          .unchunked_ext_msg_support = 0,
          .no_usb_suspend = 0,
          .usb_comm_capable = 1,
          .capability_mismatch = 0,
          .give_back_flag = 0, // exteremum is max
          .object_position = selected_pos,
      };
      tuc_msg_request(rhport, &rdo);

      break;
    }

    default: break;
  }

  return true;
}

bool tuc_pd_control_received_cb(uint8_t rhport, pd_header_t const* header) {
  (void) rhport;
  switch (header->msg_type) {
    case PD_CTRL_ACCEPT:
      printf("PD Request Accepted\r\n");
      // preparing for power transition
      break;

    case PD_CTRL_REJECT:
      printf("PD Request Rejected\r\n");
      // try to negotiate further power
      break;

    case PD_CTRL_PS_READY:
      printf("PD Power Ready\r\n");
      // Source is ready to supply power
      break;

    default:
      break;
  }

  return true;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
