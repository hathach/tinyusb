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

#include "../board.h"

#include "msp430.h"

#define LED_PORT              P1OUT
#define LED_PIN               BIT0
#define LED_STATE_ON          1

#define BUTTON_PORT           P1IN
#define BUTTON_PIN            BIT1
#define BUTTON_STATE_ACTIVE   1

static void SystemClock_Config(void)
{
  WDTCTL = WDTPW + WDTHOLD; // Disable watchdog.

  // Increase VCore to level 2- required for 16 MHz operation on this MCU.
  PMMCTL0 = PMMPW + PMMCOREV_2;

  UCSCTL3 = SELREF__XT2CLK; // FLL is fed by XT2.

  // XT1 used for ACLK (default- not used in this demo)
  P5SEL |= BIT4; // Required to enable XT1
  // Loop until XT1 fault flag is cleared.
  do
  {
    UCSCTL7 &= ~XT1LFOFFG;
  }while(UCSCTL7 & XT1LFOFFG);

  // XT2 is 4 MHz an external oscillator, use PLL to boost to 16 MHz.
  P5SEL |= BIT2; // Required to enable XT2.
  // Loop until XT2 fault flag is cleared
  do
  {
    UCSCTL7 &= ~XT2OFFG;
  }while(UCSCTL7 & XT2OFFG);

  // Kickstart the DCO into the correct frequency range, otherwise a
  // fault will occur.
  // FIXME: DCORSEL_6 should work according to datasheet params, but generates
  // a fault. I am not sure why it faults.
  UCSCTL1 = DCORSEL_7;
  UCSCTL2 = FLLD_2 + 3; // DCO freq = D * (N + 1) * (FLLREFCLK / n)
                        // DCOCLKDIV freq = (N + 1) * (FLLREFCLK / n)
                        // N = 3, D = 2, thus DCO freq = 32 MHz.

  // MCLK configured for 16 MHz using XT2.
  // SMCLK configured for 8 MHz using XT2.
  UCSCTL4 |= SELM__DCOCLKDIV + SELS__DCOCLKDIV;
  UCSCTL5 |= DIVM__16 + DIVS__2;

  // Now wait till everything's stabilized.
  do
  {
    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG);
    SFRIFG1 &= ~OFIFG;
  }while(SFRIFG1 & OFIFG);

  // Configure Timer A to use SMCLK as a source. Count 1000 ticks at 1 MHz.
  TA0CCTL0 |= CCIE;
  TA0CCR0 = 999; // 1000 ticks.
  TA0CTL |= TASSEL_2 + ID_3 + MC__UP; // Use SMCLK, divide by 8, start timer.
}

void board_init(void)
{
  SystemClock_Config();
  __bis_SR_register(GIE); // Enable interrupts.

  P1DIR |= LED_PIN; // LED output.
  P1REN |= BUTTON_PIN; // Internal resistor enable.
  P1OUT |= BUTTON_PIN; // Pullup.
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  if(state)
  {
    LED_PORT |= LED_PIN;
  }
  else
  {
    LED_PORT &= ~LED_PIN;
  }
}

uint32_t board_button_read(void)
{
  return (P1IN & BIT1);
}

#if CFG_TUSB_OS  == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) TIMER0_A0_ISR (void)
{
  system_ticks++;
  // TAxCCR0 CCIFG resets itself as soon as interrupt is invoked.
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif
