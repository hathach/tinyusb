/**************************************************************************/
/*!
    @file     board.c
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

#if TUSB_CFG_OS == TUSB_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler (void)
{
  system_ticks++;
  tusb_tick_tock(); // TODO temporarily
}
#endif

//void board_delay_blocking(uint32_t ms)
//{
//  volatile uint32_t delay_us = 1000*ms;
//  delay_us *= (SystemCoreClock / 1000000) / 3;
//  while(delay_us--);
//}

void check_failed(uint8_t *file, uint32_t line)
{
  (void) file;
  (void) line;
}

/**
 * HardFault_HandlerAsm:
 * Alternative Hard Fault handler to help debug the reason for a fault.
 * To use, edit the vector table to reference this function in the HardFault vector
 * This code is suitable for Cortex-M3 and Cortex-M0 cores
 */

// Use the 'naked' attribute so that C stacking is not used.
__attribute__((naked))
void HardFault_HandlerAsm(void){
  /*
   * Get the appropriate stack pointer, depending on our mode,
   * and use it as the parameter to the C handler. This function
   * will never return
   */

  __asm(  ".syntax unified\n"
      "MOVS   R0, #4  \n"
      "MOV    R1, LR  \n"
      "TST    R0, R1  \n"
      "BEQ    _MSP    \n"
      "MRS    R0, PSP \n"
      "B      HardFault_HandlerC      \n"
      "_MSP:  \n"
      "MRS    R0, MSP \n"
      "B      HardFault_HandlerC      \n"
      ".syntax divided\n") ;
}

/**
 * HardFaultHandler_C:
 * This is called from the HardFault_HandlerAsm with a pointer the Fault stack
 * as the parameter. We can then read the values from the stack and place them
 * into local variables for ease of reading.
 * We then read the various Fault Status and Address Registers to help decode
 * cause of the fault.
 * The function ends with a BKPT instruction to force control back into the debugger
 */
void HardFault_HandlerC(unsigned long *hardfault_args){
  ATTR_UNUSED volatile unsigned long stacked_r0 ;
  ATTR_UNUSED volatile unsigned long stacked_r1 ;
  ATTR_UNUSED volatile unsigned long stacked_r2 ;
  ATTR_UNUSED volatile unsigned long stacked_r3 ;
  ATTR_UNUSED volatile unsigned long stacked_r12 ;
  ATTR_UNUSED volatile unsigned long stacked_lr ;
  ATTR_UNUSED volatile unsigned long stacked_pc ;
  ATTR_UNUSED volatile unsigned long stacked_psr ;
  ATTR_UNUSED volatile unsigned long _CFSR ;
  ATTR_UNUSED volatile unsigned long _HFSR ;
  ATTR_UNUSED volatile unsigned long _DFSR ;
  ATTR_UNUSED volatile unsigned long _AFSR ;
  ATTR_UNUSED volatile unsigned long _BFAR ;
  ATTR_UNUSED volatile unsigned long _MMAR ;

  stacked_r0  = ((unsigned long)hardfault_args[0]) ;
  stacked_r1  = ((unsigned long)hardfault_args[1]) ;
  stacked_r2  = ((unsigned long)hardfault_args[2]) ;
  stacked_r3  = ((unsigned long)hardfault_args[3]) ;
  stacked_r12 = ((unsigned long)hardfault_args[4]) ;
  stacked_lr  = ((unsigned long)hardfault_args[5]) ;
  stacked_pc  = ((unsigned long)hardfault_args[6]) ;
  stacked_psr = ((unsigned long)hardfault_args[7]) ;

  // Configurable Fault Status Register
  // Consists of MMSR, BFSR and UFSR
  _CFSR = (*((volatile unsigned long *)(0xE000ED28))) ;

  // Hard Fault Status Register
  _HFSR = (*((volatile unsigned long *)(0xE000ED2C))) ;

  // Debug Fault Status Register
  _DFSR = (*((volatile unsigned long *)(0xE000ED30))) ;

  // Auxiliary Fault Status Register
  _AFSR = (*((volatile unsigned long *)(0xE000ED3C))) ;

  // Read the Fault Address Registers. These may not contain valid values.
  // Check BFARVALID/MMARVALID to see if they are valid values
  // MemManage Fault Address Register
  _MMAR = (*((volatile unsigned long *)(0xE000ED34))) ;
  // Bus Fault Address Register
  _BFAR = (*((volatile unsigned long *)(0xE000ED38))) ;

//  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)==CoreDebug_DHCSR_C_DEBUGEN_Msk) /* if there is debugger connected */
//  {
//    __asm("BKPT #0\n");
//  }

  hal_debugger_breakpoint();
}
