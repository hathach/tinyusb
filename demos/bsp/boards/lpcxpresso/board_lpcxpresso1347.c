/**************************************************************************/
/*!
    @file     board_lpcexpresso1347.c
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

#include "../board.h"

#if BOARD == BOARD_LPCXPRESSO1347

void board_init(void)
{
  SystemInit();
  SysTick_Config(SystemCoreClock / CFG_TICKS_PER_SECOND); // 1 msec tick timer
  GPIOInit();

  // Leds Init
  GPIOSetDir(CFG_LED_PORT, CFG_LED_PIN, 1);
  LPC_GPIO->CLR[CFG_LED_PORT] = (1 << CFG_LED_PIN);

#if CFG_UART_ENABLE
  UARTInit(CFG_UART_BAUDRATE);
#endif
}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  if (on_mask & BIT_(0))
  {
    GPIOSetBitValue(CFG_LED_PORT, CFG_LED_PIN, CFG_LED_ON);
  }else if (off_mask & BIT_(0))
  {
    GPIOSetBitValue(CFG_LED_PORT, CFG_LED_PIN, CFG_LED_OFF);
  }
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
#if CFG_UART_ENABLE
uint32_t board_uart_send(uint8_t *buffer, uint32_t length)
{
  UARTSend(buffer, length);
  return length;
}

uint32_t board_uart_recv(uint8_t *buffer, uint32_t length)
{
  *buffer = get_key();
  return 1;
}
#endif

#endif
