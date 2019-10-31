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
 * This file is part of the TinyUSB stack.
 */

#include "board.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

//#if CFG_PRINTF_TARGET == PRINTF_TARGET_UART
//  #define retarget_getchar    board_uart_getchar
//  #define retarget_putchar    board_uart_putchar
//#elif CFG_PRINTF_TARGET == PRINTF_TARGET_SWO
//  volatile int32_t ITM_RxBuffer;  // keil variable to read from SWO
//	#define retarget_getchar    ITM_ReceiveChar
//	#define retarget_putchar    ITM_SendChar
//#else
//	#error Target is not implemented yet
//#endif

//------------- IMPLEMENTATION -------------//

// newlib read()/write() retarget

TU_ATTR_USED int _write (int fhdl, const void *buf, size_t count)
{
  (void) fhdl;
  return board_uart_write(buf, count);
}

TU_ATTR_USED int _read (int fhdl, char *buf, size_t count)
{
  (void) fhdl;
  return board_uart_read((uint8_t*) buf, count);
}
