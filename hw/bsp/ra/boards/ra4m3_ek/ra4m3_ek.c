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

#include "bsp/board.h"
#include "bsp_api.h"
#include "r_ioport.h"
#include "r_ioport_api.h"
#include "renesas.h"

/* Key code for writing PRCR register. */
#define BSP_PRV_PRCR_KEY	 (0xA500U)
#define BSP_PRV_PRCR_PRC1_UNLOCK ((BSP_PRV_PRCR_KEY) | 0x2U)
#define BSP_PRV_PRCR_LOCK	 ((BSP_PRV_PRCR_KEY) | 0x0U)

#define SW1  (BSP_IO_PORT_00_PIN_05)
#define SW2  (BSP_IO_PORT_00_PIN_06)
#define LED1 (BSP_IO_PORT_04_PIN_15)
#define LED3 (BSP_IO_PORT_04_PIN_00)
#define LED2 (BSP_IO_PORT_04_PIN_04)

/* ISR prototypes */
void usbfs_interrupt_handler(void);
void usbfs_resume_handler(void);
void usbfs_d0fifo_handler(void);
void usbfs_d1fifo_handler(void);

BSP_DONT_REMOVE const
  fsp_vector_t g_vector_table[BSP_ICU_VECTOR_MAX_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) = {
    [0] = usbfs_interrupt_handler, /* USBFS INT (USBFS interrupt) */
    [1] = usbfs_resume_handler,    /* USBFS RESUME (USBFS resume interrupt) */
    [2] = usbfs_d0fifo_handler,    /* USBFS FIFO 0 (DMA transfer request 0) */
    [3] = usbfs_d1fifo_handler,    /* USBFS FIFO 1 (DMA transfer request 1) */
};
const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_MAX_ENTRIES] = {
  [0] = BSP_PRV_IELS_ENUM(EVENT_USBFS_INT),    /* USBFS INT (USBFS interrupt) */
  [1] = BSP_PRV_IELS_ENUM(EVENT_USBFS_RESUME), /* USBFS RESUME (USBFS resume interrupt) */
  [2] = BSP_PRV_IELS_ENUM(EVENT_USBFS_FIFO_0), /* USBFS FIFO 0 (DMA transfer request 0) */
  [3] = BSP_PRV_IELS_ENUM(EVENT_USBFS_FIFO_1)  /* USBFS FIFO 1 (DMA transfer request 1) */
};

const ioport_pin_cfg_t g_bsp_pin_cfg_data[] = {
  {.pin = BSP_IO_PORT_04_PIN_07,
   .pin_cfg = ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN | (uint32_t) IOPORT_PERIPHERAL_USB_FS)},
  {.pin = BSP_IO_PORT_05_PIN_00,
   .pin_cfg = ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN | (uint32_t) IOPORT_PERIPHERAL_USB_FS)},
  {.pin = BSP_IO_PORT_05_PIN_01,
   .pin_cfg = ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN | (uint32_t) IOPORT_PERIPHERAL_USB_FS)},
  {.pin = LED1, .pin_cfg = ((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_PORT_OUTPUT_LOW)},
  {.pin = LED2, .pin_cfg = ((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_PORT_OUTPUT_LOW)},
  {.pin = LED3, .pin_cfg = ((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_PORT_OUTPUT_LOW)},
  {.pin = SW1, .pin_cfg = ((uint32_t) IOPORT_CFG_PORT_DIRECTION_INPUT)},
  {.pin = SW2, .pin_cfg = ((uint32_t) IOPORT_CFG_PORT_DIRECTION_INPUT)}};

const ioport_cfg_t g_bsp_pin_cfg = {
  .number_of_pins = sizeof(g_bsp_pin_cfg_data) / sizeof(ioport_pin_cfg_t),
  .p_pin_cfg_data = &g_bsp_pin_cfg_data[0],
};
ioport_instance_ctrl_t g_ioport_ctrl;
const ioport_instance_t g_ioport = {.p_api = &g_ioport_on_ioport, .p_ctrl = &g_ioport_ctrl, .p_cfg = &g_bsp_pin_cfg};

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void usbfs_interrupt_handler(void)
{
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

#if CFG_TUH_ENABLED
  tuh_int_handler(0);
#endif

#if CFG_TUD_ENABLED
  tud_int_handler(0);
#endif
}
void usbfs_resume_handler(void)
{
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

#if CFG_TUH_ENABLED
  tuh_int_handler(0);
#endif

#if CFG_TUD_ENABLED
  tud_int_handler(0);
#endif
}

void usbfs_d0fifo_handler(void)
{
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

#if CFG_TUH_ENABLED
  tuh_int_handler(0);
#endif

#if CFG_TUD_ENABLED
  tud_int_handler(0);
#endif
}

void usbfs_d1fifo_handler(void)
{
  IRQn_Type irq = R_FSP_CurrentIrqGet();
  R_BSP_IrqStatusClear(irq);

#if CFG_TUH_ENABLED
  tuh_int_handler(0);
#endif

#if CFG_TUD_ENABLED
  tud_int_handler(0);
#endif
}

void board_init(void)
{
  /* Configure pins. */
  R_IOPORT_Open(&g_ioport_ctrl, &g_bsp_pin_cfg);

  /* Enable USB_BASE */
  R_SYSTEM->PRCR = (uint16_t) BSP_PRV_PRCR_PRC1_UNLOCK;
  R_MSTP->MSTPCRB &= ~(1U << 11U);
  R_SYSTEM->PRCR = (uint16_t) BSP_PRV_PRCR_LOCK;

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(TU_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBFS_RESUME_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBFS_FIFO_0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBFS_FIFO_1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

#if CFG_TUSB_OS == OPT_OS_NONE
  /* Init systick */
  SysTick_Config(SystemCoreClock / 1000);
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  R_IOPORT_PinWrite(&g_ioport_ctrl, LED1, state);
  R_IOPORT_PinWrite(&g_ioport_ctrl, LED2, state);
  R_IOPORT_PinWrite(&g_ioport_ctrl, LED3, state);
}

uint32_t board_button_read(void)
{
  bsp_io_level_t lvl;
  R_IOPORT_PinRead(&g_ioport_ctrl, SW1, &lvl);
  return lvl;
}

int board_uart_read(uint8_t *buf, int len)
{
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const *buf, int len)
{
  (void) buf;
  (void) len;
  return 0;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler(void)
{
  system_ticks++;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#else
#endif

int close(int fd)
{
  (void) fd;
  return -1;
}
int fstat(int fd, void *pstat)
{
  (void) fd;
  (void) pstat;
  return 0;
}
off_t lseek(int fd, off_t pos, int whence)
{
  (void) fd;
  (void) pos;
  (void) whence;
  return 0;
}
int isatty(int fd)
{
  (void) fd;
  return 1;
}
