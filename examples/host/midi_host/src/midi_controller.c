/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 BlueChip
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

#include <stdio.h>
#include <stdint.h>

#include "bsp/board.h"
#include "tusb.h"

//+============================================================================ ========================================
// Blink LED
// This code is not initialised properly - and does not consider uint32 wrap
//
void  blink (void)
{
	const   uint32_t  period = 1000;
	static  uint32_t  start  = 0;
	static  bool      state  = false;
	        uint32_t  ms     = board_millis();

	if (ms > (start + period)) {
		board_led_write((state = !state));
		start = ms;
	}
	return;
}

//+============================================================================ ========================================
int  main (void)
{
	board_init();

	printf("TinyUSB MIDI Data Driver example\r\n");

	tusb_init();

	while (1) {
		tuh_task();
		blink();
	}

	return 0;
}
