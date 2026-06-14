/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32v30x_it.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/06/06
* Description        : Main Interrupt Service Routines.
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* SPDX-License-Identifier: Apache-2.0
*******************************************************************************/
#include "ch32v30x_it.h"
#include <stdio.h>
#include <stdint.h>

void NMI_Handler(void) __attribute__((naked));
void HardFault_Handler(void) __attribute__((naked));

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
void NMI_Handle(void){
      __asm volatile ("call NMI_Handler_impl; mret");
}

__attribute__((used)) void NMI_Handler_impl(void)
{

}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 */
void HardFault_Handler(void){
      __asm volatile ("call HardFault_Handler_impl; mret");
}

// Direct USART1 output — no printf/stack/buffers, safe in a corrupted fault state.
static void fault_putc(char c)
{
  volatile uint32_t* statr = (volatile uint32_t*) 0x40013800u;  // USART1 STATR
  volatile uint32_t* datar = (volatile uint32_t*) 0x40013804u;  // USART1 DATAR
  while (!(*statr & (1u << 7))) { }  // wait TXE
  *datar = (uint32_t) (uint8_t) c;
}
static void fault_puthex(uint32_t v)
{
  for (int i = 7; i >= 0; i--) {
    int n = (int) ((v >> (i * 4)) & 0xF);
    fault_putc((char) (n < 10 ? '0' + n : 'A' + n - 10));
  }
}

__attribute__((used)) void HardFault_Handler_impl(void)
{
  uint32_t mcause = 0, mepc = 0, mtval = 0;
  __asm volatile ("csrr %0, mcause" : "=r"(mcause));
  __asm volatile ("csrr %0, mepc"   : "=r"(mepc));
  __asm volatile ("csrr %0, mtval"  : "=r"(mtval));
  const char* tag = "\nFAULT cause="; while (*tag) fault_putc(*tag++);
  fault_puthex(mcause);
  tag = " mepc=";  while (*tag) fault_putc(*tag++);
  fault_puthex(mepc);
  tag = " mtval="; while (*tag) fault_putc(*tag++);
  fault_puthex(mtval);
  fault_putc('\n');
  while (1)
  {
  }
}
