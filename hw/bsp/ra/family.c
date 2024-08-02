/*
 * The MIT License (MIT)
 *
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

#include <stdio.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic ignored "-Wundef"

// extra push due to https://github.com/renesas/fsp/pull/278
#pragma GCC diagnostic push
#endif

#include "bsp_api.h"
#include "r_ioport.h"
#include "r_ioport_api.h"
#include "renesas.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "bsp/board_api.h"
#include "board.h"

/* Key code for writing PRCR register. */
#define BSP_PRV_PRCR_KEY         (0xA500U)

static const ioport_cfg_t family_pin_cfg = {
    .number_of_pins = sizeof(board_pin_cfg) / sizeof(ioport_pin_cfg_t),
    .p_pin_cfg_data = board_pin_cfg,
};
static ioport_instance_ctrl_t port_ctrl;

//--------------------------------------------------------------------+
// Vector Data
//--------------------------------------------------------------------+

BSP_DONT_REMOVE BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS)
const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_MAX_ENTRIES] = {
    [0] = usbfs_interrupt_handler, /* USBFS INT (USBFS interrupt) */
    [1] = usbfs_resume_handler,    /* USBFS RESUME (USBFS resume interrupt) */

#ifndef BSP_MCU_GROUP_RA2A1
    [2] = usbfs_d0fifo_handler,    /* USBFS FIFO 0 (DMA transfer request 0) */
    [3] = usbfs_d1fifo_handler,    /* USBFS FIFO 1 (DMA transfer request 1) */
#endif

#ifdef BOARD_HAS_USB_HIGHSPEED
    [4] = usbhs_interrupt_handler, /* USBHS INT (USBHS interrupt) */
    [5] = usbhs_d0fifo_handler,    /* USBHS FIFO 0 (DMA transfer request 0) */
    [6] = usbhs_d1fifo_handler,    /* USBHS FIFO 1 (DMA transfer request 1) */
#endif
};

const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_MAX_ENTRIES] = {
    [0] = BSP_PRV_IELS_ENUM(EVENT_USBFS_INT),            /* USBFS INT (USBFS interrupt) */
    [1] = BSP_PRV_IELS_ENUM(EVENT_USBFS_RESUME),         /* USBFS RESUME (USBFS resume interrupt) */

#ifndef BSP_MCU_GROUP_RA2A1
    [2] = BSP_PRV_IELS_ENUM(EVENT_USBFS_FIFO_0),         /* USBFS FIFO 0 (DMA transfer request 0) */
    [3] = BSP_PRV_IELS_ENUM(EVENT_USBFS_FIFO_1),         /* USBFS FIFO 1 (DMA transfer request 1) */
#endif

#ifdef BOARD_HAS_USB_HIGHSPEED
    [4] = BSP_PRV_IELS_ENUM(EVENT_USBHS_USB_INT_RESUME), /* USBHS USB INT RESUME (USBHS interrupt) */
    [5] = BSP_PRV_IELS_ENUM(EVENT_USBHS_FIFO_0),         /* USBHS FIFO 0 (DMA transfer request 0) */
    [6] = BSP_PRV_IELS_ENUM(EVENT_USBHS_FIFO_1),         /* USBHS FIFO 1 (DMA transfer request 1) */
#endif
};

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_init(void) {
  // Enable global interrupts in CPSR register since board with bootloader such as Arduino Uno R4
  // can transfer CPU control with CPSR.I bit set to 0 (disable IRQ)
  __enable_irq();

  /* Configure pins. */
  R_IOPORT_Open(&port_ctrl, &family_pin_cfg);

#ifdef TRACE_ETM
  // TRCKCR is protected by PRCR bit0 register
  R_SYSTEM->PRCR = (uint16_t) (BSP_PRV_PRCR_KEY | 0x01);

  // Enable trace clock (max 100Mhz). Since PLL/CPU is 200Mhz, clock div = 2
  R_SYSTEM->TRCKCR = R_SYSTEM_TRCKCR_TRCKEN_Msk | 0x01;

  R_SYSTEM->PRCR = (uint16_t) BSP_PRV_PRCR_KEY;
#endif

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USBFS_INT_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBFS_RESUME_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBFS_FIFO_0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBFS_FIFO_1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / 1000);
#endif

  board_led_write(false);
}

void board_init_after_tusb(void) {
  // For board that use USB LDO regulator
#if defined(BOARD_UNO_R4)
  R_USB_FS0->USBMC |= R_USB_FS0_USBMC_VDCEN_Msk;
#endif
}

void board_led_write(bool state) {
  R_IOPORT_PinWrite(&port_ctrl, LED1, state ? LED_STATE_ON : !LED_STATE_ON);
}

uint32_t board_button_read(void) {
  bsp_io_level_t lvl = !BUTTON_STATE_ACTIVE;
  R_IOPORT_PinRead(&port_ctrl, SW1, &lvl);
  return lvl == BUTTON_STATE_ACTIVE;
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  max_len = tu_min32(max_len, sizeof(bsp_unique_id_t));
  bsp_unique_id_t const *uid = R_BSP_UniqueIdGet();
  memcpy(id, uid->unique_id_bytes, max_len);
  return max_len;
}

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t board_millis(void) {
  return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

#if CFG_TUD_ENABLED && defined(BOARD_TUD_RHPORT)
  #define PORT_SUPPORT_DEVICE(_n)  (BOARD_TUD_RHPORT == _n)
#else
  #define PORT_SUPPORT_DEVICE(_n)  0
#endif

#if CFG_TUH_ENABLED && defined(BOARD_TUH_RHPORT)
  #define PORT_SUPPORT_HOST(_n)    (BOARD_TUH_RHPORT == _n)
#else
  #define PORT_SUPPORT_HOST(_n)    0
#endif

//------------- USB0 FullSpeed -------------//
void usbfs_interrupt_handler(void) {
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

  #if PORT_SUPPORT_HOST(0)
  tuh_int_handler(0, true);
  #endif

  #if PORT_SUPPORT_DEVICE(0)
  tud_int_handler(0);
  #endif
}

void usbfs_resume_handler(void) {
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

  #if PORT_SUPPORT_HOST(0)
  tuh_int_handler(0, true);
  #endif

  #if PORT_SUPPORT_DEVICE(0)
  tud_int_handler(0);
  #endif
}

void usbfs_d0fifo_handler(void) {
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);
  // TODO not used yet
}

void usbfs_d1fifo_handler(void) {
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);
  // TODO not used yet
}

//------------- USB1 HighSpeed -------------//
#ifdef BOARD_HAS_USB_HIGHSPEED

void usbhs_interrupt_handler(void) {
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

  #if PORT_SUPPORT_HOST(1)
  tuh_int_handler(1, true);
  #endif

  #if PORT_SUPPORT_DEVICE(1)
  tud_int_handler(1);
  #endif
}

void usbhs_d0fifo_handler(void) {
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);
  // TODO not used yet
}

void usbhs_d1fifo_handler(void) {
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);
  // TODO not used yet
}

#endif

//--------------------------------------------------------------------+
// stdlib
//--------------------------------------------------------------------+

int close(int fd) {
  (void) fd;
  return -1;
}

int fstat(int fd, void *pstat) {
  (void) fd;
  (void) pstat;
  return 0;
}

off_t lseek(int fd, off_t pos, int whence) {
  (void) fd;
  (void) pos;
  (void) whence;
  return 0;
}

int isatty(int fd) {
  (void) fd;
  return 1;
}
