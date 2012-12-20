/*
 * board_at86rf2xx.c
 *
 *  Created on: Dec 7, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#include "board.h"

#if BOARD == BOARD_AT86RF2XX

#include "LPC11Uxx.h"
#include "lpc11uxx/gpio.h"

#define CFG_LED_PORT                  (1)
#define CFG_LED_PIN                   (31)
#define CFG_LED_ON                    (0)
#define CFG_LED_OFF                   (1)

void board_init(void)
{
  SystemInit();
  SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SECOND); // 1 msec tick timer
  GPIOInit();

  GPIOSetDir(CFG_LED_PORT, CFG_LED_PIN, 1);
  board_leds(0x01, 0); // turn off the led first
}

void board_leds(uint32_t mask, uint32_t state)
{
  if (mask)
    GPIOSetBitValue(CFG_LED_PORT, CFG_LED_PIN, mask & state ? CFG_LED_ON : CFG_LED_OFF);
}

#endif
