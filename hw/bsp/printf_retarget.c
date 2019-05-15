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
 * This file is part of the TinyUSB stack.
 */

#include "board.h"

#if CFG_PRINTF_TARGET != PRINTF_TARGET_SEMIHOST

#if CFG_PRINTF_TARGET == PRINTF_TARGET_UART
  #define retarget_getchar()     board_uart_getchar()
  #define retarget_putchar(c)    board_uart_putchar(c);
#elif CFG_PRINTF_TARGET == PRINTF_TARGET_SWO
  volatile int32_t ITM_RxBuffer;  // keil variable to read from SWO
	#define retarget_getchar()     ITM_ReceiveChar()
	#define retarget_putchar(c)    ITM_SendChar(c)
#else
	#error Target is not implemented yet
#endif

//--------------------------------------------------------------------+
// LPCXPRESSO / RED SUITE
//--------------------------------------------------------------------+
#if defined __CODE_RED

#if CFG_PRINTF_TARGET == PRINTF_TARGET_SWO
  #error author does not know how to retarget SWO with lpcxpresso/red-suite
#endif

// Called by bottom level of printf routine within RedLib C library to write
// a character. With the default semihosting stub, this would write the character
// to the debugger console window . But this version writes
// the character to the UART.
int __sys_write (int iFileHandle, char *buf, int length)
{
  (void) iFileHandle;

  for (int i=0; i<length; i++)
  {
    if (buf[i] == '\n') retarget_putchar('\r');

    retarget_putchar( buf[i] );
  }

  return length;
}

// Called by bottom level of scanf routine within RedLib C library to read
// a character. With the default semihosting stub, this would read the character
// from the debugger console window (which acts as stdin). But this version reads
// the character from the UART.
int __sys_readc (void)
{
	return (int) retarget_getchar();
}

#elif defined __SES_ARM && 0
#include <stdarg.h>
#include <stdio.h>
#include "__libc.h"

int printf(const char *fmt,...) {
  char buffer[128];
  va_list args;
  va_start (args, fmt);
  int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
  
  for(int i=0; i < n; i++)
  {
    retarget_putchar( buffer[i] );
  }

  va_end(args);
  return n;
}

int __putchar(int ch, __printf_tag_ptr ctx) 
{
  (void)ctx;
  retarget_putchar( (uint8_t) ch );
  return 1;
}

int __getchar()
{
  return retarget_getchar();
}

//--------------------------------------------------------------------+
// KEIL
//--------------------------------------------------------------------+
#elif defined __CC_ARM // keil

struct __FILE {
  uint32_t handle;
};

void _ttywrch(int ch)
{
  if ( ch == '\n' ) retarget_putchar('\r');

  retarget_putchar(ch);
}

int fgetc(FILE *f)
{
  return retarget_getchar();
}

int fputc(int ch, FILE *f)
{
  _ttywrch(ch);
  return ch;
}

//--------------------------------------------------------------------+
// IAR
//--------------------------------------------------------------------+
#elif defined __ICCARM__ // TODO could not able to retarget to UART with IAR

#if CFG_PRINTF_TARGET == PRINTF_TARGET_UART
#include <stddef.h>

size_t __write(int handle, const unsigned char *buf, size_t length)
{
  /* Check for the command to flush all handles */
  if (handle == -1) return 0;

  /* Check for stdout and stderr (only necessary if FILE descriptors are enabled.) */
  if (handle != 1 && handle != 2) return -1;

  for (size_t i=0; i<length; i++)
  {
    if (buf[i] == '\n') retarget_putchar('\r');

    retarget_putchar( buf[i] );
  }

  return length;
}

size_t __read(int handle, unsigned char *buf, size_t bufSize)
{
  /* Check for stdin (only necessary if FILE descriptors are enabled) */
  if (handle != 0) return -1;

  size_t i;
  for (i=0; i<bufSize; i++)
  {
    int8_t ch = board_uart_getchar();
    if (ch == -1) break;
    buf[i] = ch;
  }

  return i;
}

#endif

#endif

#endif // CFG_PRINTF_TARGET != PRINTF_TARGET_SEMIHOST
