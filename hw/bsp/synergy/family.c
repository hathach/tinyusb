/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Pivot International
 * Copyright (c) 2022, Rafael Silva
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

/* metadata:
   manufacturer: Renesas
*/

#include <stdio.h>

#include "common_data.h"
#include "bsp_pin_cfg.h"

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Vector Data
//--------------------------------------------------------------------+

SSP_VECTOR_DEFINE_UNIT(usbfs_interrupt_handler, USB, FS, INT, 0);
SSP_VECTOR_DEFINE_UNIT(usbfs_resume_handler, USB, FS, RESUME, 0);

#ifdef BOARD_HAS_USB_HIGHSPEED
SSP_VECTOR_DEFINE_UNIT(usbhs_interrupt_handler, USB, HS, INT, 0);
#endif

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_init(void) {
  // Enable global interrupts in CPSR register since board with bootloader such as Arduino Uno R4
  // can transfer CPU control with CPSR.I bit set to 0 (disable IRQ)
  __enable_irq();

  /* Configure pins. */
  g_ioport_on_ioport.init(&g_bsp_pin_cfg);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  fmi_event_info_t event_info = {(IRQn_Type) 0U};
  g_fmi_on_fmi.eventInfoGet(&ssp_feature, SSP_SIGNAL_USB_FS_INT, &event_info);
  NVIC_SetPriority(event_info.irq, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  g_fmi_on_fmi.eventInfoGet(&ssp_feature, SSP_SIGNAL_USB_FS_RESUME, &event_info);
  NVIC_SetPriority(event_info.irq, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;
#endif

  //g_ioport_on_ioport.pinDirectionSet(LED1, IOPORT_DIRECTION_OUTPUT);
  board_led_write(true);
}

void board_init_after_tusb(void) {
  // For board that use USB LDO regulator
#if defined(BOARD_DK_S124)
  R_USBFS->USBMC_b.VDCEN = 1;
#endif
}

void board_led_write(bool state) {
  g_ioport_on_ioport.pinWrite(LED1, state ? LED_STATE_ON : !LED_STATE_ON);
}

uint32_t board_button_read(void) {
  ioport_level_t lvl = !BUTTON_STATE_ACTIVE;
  g_ioport_on_ioport.pinRead(SW1, &lvl);
  return lvl == BUTTON_STATE_ACTIVE;
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  fmi_unique_id_t uid;
  max_len = tu_min32(max_len, sizeof(fmi_unique_id_t));
  g_fmi.p_api->uniqueIdGet(&uid);
  memcpy(id, uid.unique_id, max_len);
  return max_len;
}

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return -1;
}

int board_uart_write(void const *buf, int len) {
  (void) buf;
  (void) len;
  return -1;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

//------------- USB0 FullSpeed -------------//
void usbfs_interrupt_handler(void) {
  
  printf("usbfs_interrupt_handler\n");
  IRQn_Type irq = R_SSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

  tusb_int_handler(0, true);
}

void usbfs_resume_handler(void) {
  printf("usbfs_resume_handler\n");
  IRQn_Type irq = R_SSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

  tusb_int_handler(0, true);
}

//------------- USB1 HighSpeed -------------//
#ifdef BOARD_HAS_USB_HIGHSPEED
void usbhs_interrupt_handler(void) {
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

  tusb_int_handler(1, true);
}
#endif

//--------------------------------------------------------------------+
// stdlib
//--------------------------------------------------------------------+

int _close(int fd) {
  (void) fd;
  return -1;
}

int _fstat(int fd, void *pstat) {
  (void) fd;
  (void) pstat;
  return 0;
}

off_t _lseek(int fd, off_t pos, int whence) {
  (void) fd;
  (void) pos;
  (void) whence;
  return 0;
}

int _isatty(int fd) {
  (void) fd;
  return 1;
}

int _getpid(void) {
  return 1;
}

int _kill(int id, int sig) {
  (void) id;
  (void) sig;
  return -1;
}
