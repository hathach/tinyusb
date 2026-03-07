/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 TinyUSB contributors
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

#include "debug_uart.h"
#include "CH58x_common.h"

//--------------------------------------------------------------------+
// Ring buffer based UART TX for non-blocking writes
//--------------------------------------------------------------------+

#define UART_RINGBUFFER_SIZE_TX  128
#define UART_RINGBUFFER_MASK_TX  (UART_RINGBUFFER_SIZE_TX - 1)

static char tx_buf[UART_RINGBUFFER_SIZE_TX];
static unsigned int tx_produce;
static volatile unsigned int tx_consume;

void uart_write(char c) {
  unsigned int tx_produce_next = (tx_produce + 1) & UART_RINGBUFFER_MASK_TX;

  // If ring buffer is full, wait
  while (tx_produce_next == tx_consume) {}

  // If UART TX FIFO is empty and no pending data, send directly
  if ((tx_consume == tx_produce) && (R8_UART1_LSR & RB_LSR_TX_FIFO_EMP)) {
    R8_UART1_THR = c;
  } else {
    tx_buf[tx_produce] = c;
    tx_produce = tx_produce_next;
  }
}

void uart_sync(void) {
  // Wait for ring buffer to drain
  while (tx_consume != tx_produce) {
    if (R8_UART1_LSR & RB_LSR_TX_FIFO_EMP) {
      R8_UART1_THR = tx_buf[tx_consume];
      tx_consume = (tx_consume + 1) & UART_RINGBUFFER_MASK_TX;
    }
  }
  // Wait for last byte to finish transmitting
  while (!(R8_UART1_LSR & RB_LSR_TX_ALL_EMP)) {}
}

void usart_printf_init(uint32_t baudrate) {
  tx_produce = 0;
  tx_consume = 0;

  // Configure UART1 pins: TX=PA9, RX=PA8
  GPIOA_SetBits(GPIO_Pin_9);
  GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
  GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);

  // Init UART1 with specified baud rate
  UART1_DefInit();
  UART1_BaudRateCfg(baudrate);
}
