/**************************************************************************/
/*!
    @file     printf_retarget.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "board.h"

//-------------------------------------------------------------------- +
//                    LPCXpresso printf redirection                    +
//-------------------------------------------------------------------- +
#if CFG_PRINTF_TARGET != PRINTF_TARGET_DEBUG_CONSOLE

#if defined __CODE_RED
// Called by bottom level of printf routine within RedLib C library to write
// a character. With the default semihosting stub, this would write the character
// to the debugger console window . But this version writes
// the character to the UART.
int __sys_write (int iFileHandle, char *pcBuffer, int iLength)
{
  (void) iFileHandle;

#if CFG_PRINTF_TARGET == PRINTF_TARGET_UART
  // following code to make \n --> \r\n
  int length = iLength;
  char* p_newline_pos = memchr(pcBuffer, '\n', length);

  while(p_newline_pos != NULL)
  {
    uint32_t chunk_len = p_newline_pos - pcBuffer;

    board_uart_send((uint8_t*)pcBuffer, chunk_len);
    board_uart_send(&"\r\n", 2);

    pcBuffer += (chunk_len + 1);
    length   -= (chunk_len + 1);
    p_newline_pos = memchr(pcBuffer, '\n', length);
  }

   board_uart_send((uint8_t*)pcBuffer, length);

  return iLength;

#elif CFG_PRINTF_TARGET == PRINTF_TARGET_SWO
  #error author does not know how to retarget SWO with lpcxpresso/red-suite
#else
	#error Thach, did you forget something
#endif

}

// Called by bottom level of scanf routine within RedLib C library to read
// a character. With the default semihosting stub, this would read the character
// from the debugger console window (which acts as stdin). But this version reads
// the character from the UART.
int __sys_readc (void)
{
	uint8_t c;

#if CFG_PRINTF_TARGET == PRINTF_TARGET_UART
	board_uart_recv(&c, 1);
#elif CFG_PRINTF_TARGET == PRINTF_TARGET_SWO
	c = ITM_ReceiveChar();
#else
	#error Thach, did you forget something
#endif

	return (int)c;
}

#elif defined __CC_ARM // keil

#if CFG_PRINTF_TARGET == PRINTF_TARGET_UART
  #define retarget_putc(c)  board_uart_send( (uint8_t*) &c, 1);
#elif CFG_PRINTF_TARGET == PRINTF_TARGET_SWO
	#define retarget_putc(c)  ITM_SendChar(c)
#else
	#error Thach, did you forget something
#endif



struct __FILE {
  uint32_t handle;
};

int fputc(int ch, FILE *f)
{
  if (//CFG_PRINTF_NEWLINE[0] == '\r' &&
      ch == '\n')
  {
    uint8_t carry = '\r';
    retarget_putc(carry);
  }

  retarget_putc(ch);
  
  return ch;
}

void _ttywrch(int ch)
{
  if (//CFG_PRINTF_NEWLINE[0] == '\r' &&
      ch == '\n')
  {
    uint8_t carry = '\r';
    retarget_putc(carry);
  }

  retarget_putc(ch);
}

#endif

#endif // CFG_PRINTF_TARGET != PRINTF_TARGET_DEBUG_CONSOLE
