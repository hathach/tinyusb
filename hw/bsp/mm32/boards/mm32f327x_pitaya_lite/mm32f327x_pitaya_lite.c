/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 MM32 SE TEAM
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

/* DshanMCU Pitaya Lite with MM32F3273 */

#include "mm32_device.h"
#include "hal_conf.h"
#include "tusb.h"
#include "../board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void OTG_FS_IRQHandler (void)
{
  tud_int_handler(0);

}
void USB_DeviceClockInit (void)
{
  /* Select USBCLK source */
  //  RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1);
  RCC->CFGR &= ~(0x3 << 22);
  RCC->CFGR |= (0x1 << 22);

  /* Enable USB clock */
  RCC->AHB2ENR |= 0x1 << 7;
}
//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
// LED

extern u32 SystemCoreClock;
const int baudrate = 115200;

void board_init (void)
{
//   usb clock
// requires SYSCLK_FREQ_XXMHz  (HSE_VALUE*8) in system_mm32f327x.c
  USB_DeviceClockInit();

  if ( SysTick_Config(SystemCoreClock / 1000) )
  {
    while ( 1 )
      ;
  }
  NVIC_SetPriority(SysTick_IRQn, 0x0);

  // LED on PA1
  GPIO_InitTypeDef GPIO_InitStruct;
  RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOA, ENABLE);
  GPIO_StructInit(&GPIO_InitStruct);

  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  board_led_write(true);

  // KEY on PA0
  GPIO_StructInit(&GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  // UART
  UART_InitTypeDef UART_InitStruct;

  RCC_APB2PeriphClockCmd(RCC_APB2ENR_UART1, ENABLE);    //enableUART1,GPIOAclock
  RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOA, ENABLE);      //
  //UART initialset

  GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_7);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_7);

  UART_StructInit(&UART_InitStruct);
  UART_InitStruct.UART_BaudRate = baudrate;
  UART_InitStruct.UART_WordLength = UART_WordLength_8b;
  UART_InitStruct.UART_StopBits = UART_StopBits_1;    //one stopbit
  UART_InitStruct.UART_Parity = UART_Parity_No;    //none odd-even  verify bit
  UART_InitStruct.UART_HardwareFlowControl = UART_HardwareFlowControl_None;    //No hardware flow control
  UART_InitStruct.UART_Mode = UART_Mode_Rx | UART_Mode_Tx;    // receive and sent  mode

  UART_Init(UART1, &UART_InitStruct);    //initial uart 1
  UART_Cmd(UART1, ENABLE);                    //enable uart 1

  //UART1_TX   GPIOA.9
  GPIO_StructInit(&GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  //UART1_RX    GPIOA.10
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

}


//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write (bool state)
{
  state ? (GPIO_ResetBits(GPIOA, GPIO_Pin_1)) : (GPIO_SetBits(GPIOA, GPIO_Pin_1));
}

uint32_t board_button_read (void)
{
  uint32_t key = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET;
  return key;
}

int board_uart_read (uint8_t *buf, int len)
{
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write (void const *buf, int len)
{
  const char *buff = buf;
  while ( len )
  {
    while ( (UART1->CSR & UART_IT_TXIEN) == 0 )
      ;    //The loop is sent until it is finished
    UART1->TDR = (*buff & 0xFF);
    buff++;
    len--;
  }
  return len;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t board_millis (void)
{
  return system_ticks;
}
#endif

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void)
{

}
