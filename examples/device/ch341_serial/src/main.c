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
#include <ctype.h>

#include "bsp/board.h"
#include "tusb.h"

// Change this to 1 and event callback will be logged to stdout
#define CH341_LOG_EVENTS 0

#if (CH341_LOG_EVENTS)
static const char * _par_str[] =
{
	"N", 
	"O", 
	"E", 
	"M", 
	"S"
};
#endif

//------------- prototypes -------------//
static void ch341_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  while (1)
  {
    tud_task(); // tinyusb device task
    ch341_task();
  }

  return 0;
}

// Echo back to terminal
static void echo_serial_port( uint8_t buf[], uint32_t count)
{
	tud_ch341_write(buf, count);
	tud_ch341_write_flush();
}

//--------------------------------------------------------------------+
// USB CH341
//--------------------------------------------------------------------+
static void ch341_task(void)
{
	// connected() The serial port has been opened atleast once
	// Will continue to return true even after the serial port is closed. 
	if ( tud_ch341_connected())
	{
		if ( tud_ch341_available() )
		{
			uint8_t buf[64];

			uint32_t count = tud_ch341_read(buf, sizeof(buf));

			// echo back to terminal
			echo_serial_port(buf, count);
		}
	}
}

// Invoked when DTR/RTS changes
void tud_ch341_line_state_cb(ch341_line_state_t line_state)
{
#if (CH341_LOG_EVENTS)
	printf("DTR=%u, RTS=%u\r\n", 
		line_state & CH341_LINE_STATE_DTR_ACTIVE ? 1 : 0,
		line_state & CH341_LINE_STATE_RTS_ACTIVE ? 1 : 0);
#else
	(void)(line_state);
#endif
}

// Invoked when line coding changes
void tud_ch341_line_coding_cb(ch341_line_coding_t const* p_line_coding)
{
#if (CH341_LOG_EVENTS)
	printf("BITRATE=%lu (%s%u%u) RX:%s TX:%s\r\n", 
		(unsigned long)p_line_coding->bit_rate,
		_par_str[p_line_coding->parity],
		p_line_coding->data_bits,
		p_line_coding->stop_bits ? 2 : 1,
		p_line_coding->rx_en ? "ON":"OFF",
		p_line_coding->tx_en ? "ON":"OFF");
#else
	(void)(p_line_coding);
#endif
}

// Invoked when a break signal is received
void tud_ch341_send_break_cb(bool is_break_active)
{
#if (CH341_LOG_EVENTS)
	printf("RCV BREAK=%s\r\n", is_break_active ? "ON" : "OFF");
#else
	(void)(is_break_active);
#endif
}
