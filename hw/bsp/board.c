/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

#include "board.h"

#if 0
#define LED_PHASE_MAX   8

static struct
{
  uint32_t phase[LED_PHASE_MAX];
  uint8_t phase_count;

  bool led_state;
  uint8_t current_phase;
  uint32_t current_ms;
}led_pattern;

void board_led_pattern(uint32_t const phase_ms[], uint8_t count)
{
  memcpy(led_pattern.phase, phase_ms, 4*count);
  led_pattern.phase_count = count;

  // reset with 1st phase is on
  led_pattern.current_ms = board_millis();
  led_pattern.current_phase = 0;
  led_pattern.led_state = true;
  board_led_on();
}

void board_led_task(void)
{
  if ( led_pattern.phase_count == 0 ) return;

  uint32_t const duration = led_pattern.phase[led_pattern.current_phase];

  // return if not enough time
  if (board_millis() - led_pattern.current_ms < duration) return;

  led_pattern.led_state = !led_pattern.led_state;
  board_led_write(led_pattern.led_state);

  led_pattern.current_ms += duration;
  led_pattern.current_phase++;

  if (led_pattern.current_phase == led_pattern.phase_count)
  {
    led_pattern.current_phase = 0;
    led_pattern.led_state = true;
    board_led_on();
  }
}
#endif

//--------------------------------------------------------------------+
// newlib read()/write() retarget
//--------------------------------------------------------------------+

#if defined(__MSP430__) || defined(__RX__)
  #define sys_write   write
  #define sys_read    read
#else
  #define sys_write   _write
  #define sys_read    _read
#endif

#if defined(LOGGER_RTT)
// Logging with RTT

// If using SES IDE, use the Syscalls/SEGGER_RTT_Syscalls_SES.c instead
#if !(defined __SES_ARM) && !(defined __SES_RISCV) && !(defined __CROSSWORKS_ARM)
#include "SEGGER_RTT.h"

TU_ATTR_USED int sys_write (int fhdl, const void *buf, size_t count)
{
  (void) fhdl;
  SEGGER_RTT_Write(0, (char*) buf, (int) count);
  return count;
}

TU_ATTR_USED int sys_read (int fhdl, char *buf, size_t count)
{
  (void) fhdl;
  return SEGGER_RTT_Read(0, buf, count);
}
#endif

#elif defined(LOGGER_SWO)
// Logging with SWO for ARM Cortex

#include "board_mcu.h"

TU_ATTR_USED int sys_write (int fhdl, const void *buf, size_t count)
{
  (void) fhdl;
  uint8_t const* buf8 = (uint8_t const*) buf;
  for(size_t i=0; i<count; i++)
  {
    ITM_SendChar(buf8[i]);
  }
  return count;
}

TU_ATTR_USED int sys_read (int fhdl, char *buf, size_t count)
{
  (void) fhdl;
  return 0;
}

#else

// Default logging with on-board UART
TU_ATTR_USED int sys_write (int fhdl, const void *buf, size_t count)
{
  (void) fhdl;
  return board_uart_write(buf, count);
}

TU_ATTR_USED int sys_read (int fhdl, char *buf, size_t count)
{
  (void) fhdl;
  return board_uart_read((uint8_t*) buf, count);
}

#endif
