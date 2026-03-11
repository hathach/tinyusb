/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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
   manufacturer: NXP
*/

#include "bsp/board_api.h"
#include "board.h"
#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "fsl_power.h"
#include "fsl_usart.h"

#include "board/pin_mux.h"
#include "board/clock_config.h"
#include "board/peripherals.h"

#ifdef NEOPIXEL_PIN
#include "fsl_sctimer.h"
#include "sct_neopixel.h"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB0_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void USB1_IRQHandler(void) {
  tusb_int_handler(1, true);
}

void board_init(void) {
  BOARD_InitBootPins();
  BOARD_InitBootClocks();
  BOARD_InitBootPeripherals();

  board_led_write(0);

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);

#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;

  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
  NVIC_SetPriority(USB1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

#if (CFG_TUD_ENABLED && BOARD_TUD_RHPORT == 0) || (CFG_TUH_ENABLED && BOARD_TUH_RHPORT == 0)
  // Port0 is Full Speed
  NVIC_ClearPendingIRQ(USB0_IRQn);
  NVIC_ClearPendingIRQ(USB0_NEEDCLK_IRQn);

  /* Turn on USB0 Phy */
  POWER_DisablePD(kPDRUNCFG_PD_USB0_PHY);

  /* reset the IP to make sure it's in reset state. */
  RESET_PeripheralReset(kUSB0D_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB0HSL_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB0HMR_RST_SHIFT_RSTn);

  if (CFG_TUD_ENABLED && BOARD_TUD_RHPORT == 0) {
    // Enable USB Clock Adjustments to trim the FRO for the full speed controller
    ANACTRL->FRO192M_CTRL |= ANACTRL_FRO192M_CTRL_USBCLKADJ_MASK;
    CLOCK_SetClkDiv(kCLOCK_DivUsb0Clk, 1, false);
    CLOCK_AttachClk(kFRO_HF_to_USB0_CLK);

    /*According to reference manual, device mode setting has to be set by access usb host register */
    CLOCK_EnableClock(kCLOCK_Usbhsl0);  // enable usb0 host clock
    USBFSH->PORTMODE |= USBFSH_PORTMODE_DEV_ENABLE_MASK;
    CLOCK_DisableClock(kCLOCK_Usbhsl0); // disable usb0 host clock
    /* enable USB Device clock */
    CLOCK_EnableUsbfs0DeviceClock(kCLOCK_UsbfsSrcFro, CLOCK_GetFreq(kCLOCK_FroHf));
  } else {
    CLOCK_EnableUsbfs0HostClock(kCLOCK_UsbfsSrcPll1, 48000000U);
    USBFSH->PORTMODE &= ~USBFSH_PORTMODE_DEV_ENABLE_MASK;
  }
#endif

#if (CFG_TUD_ENABLED && BOARD_TUD_RHPORT == 1) || (CFG_TUH_ENABLED && BOARD_TUH_RHPORT == 1)
  // Port1 is High Speed

  /* Turn on USB1 Phy */
  POWER_DisablePD(kPDRUNCFG_PD_USB1_PHY);

  /* reset the IP to make sure it's in reset state. */
  RESET_PeripheralReset(kUSB1H_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1D_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1RAM_RST_SHIFT_RSTn);

  if (CFG_TUD_ENABLED && BOARD_TUD_RHPORT == 1) {
    /* According to reference manual, device mode setting has to be set by access usb host register */
    CLOCK_EnableClock(kCLOCK_Usbh1); // enable usb0 host clock

    USBHSH->PORTMODE = USBHSH_PORTMODE_SW_PDCOM_MASK; // Put PHY powerdown under software control
    USBHSH->PORTMODE |= USBHSH_PORTMODE_DEV_ENABLE_MASK;

    CLOCK_DisableClock(kCLOCK_Usbh1); // disable usb0 host clock
    /* enable USB Device clock */
    CLOCK_EnableUsbhs0DeviceClock(kCLOCK_UsbSrcUnused, 0U);
  } else {
    CLOCK_EnableUsbhs0HostClock(kCLOCK_UsbSrcUnused, 0U);
  }

  CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_UsbPhySrcExt, XTAL0_CLK_HZ);

  // Enable PHY support for Low speed device + LS via FS Hub
  USBPHY->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK | USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;

  // Enable all power for normal operation
  USBPHY->PWD = 0;

  USBPHY->CTRL_SET = USBPHY_CTRL_SET_ENAUTOCLR_CLKGATE_MASK;
  USBPHY->CTRL_SET = USBPHY_CTRL_SET_ENAUTOCLR_PHY_PWD_MASK;

  // PHY Tx calibration
  USBPHY->TX = ((USBPHY->TX & (~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK | USBPHY_TX_TXCAL45DP_MASK))) |
               (USBPHY_TX_D_CAL(0x05U) | USBPHY_TX_TXCAL45DP(0x0AU) | USBPHY_TX_TXCAL45DM(0x0AU)));

  ARM_MPU_SetMemAttr(0, 0x44); // Normal memory, non-cacheable (inner and outer)
  ARM_MPU_SetRegion(0, ARM_MPU_RBAR(0x40100000, ARM_MPU_SH_NON, 0, 1, 1), ARM_MPU_RLAR(0x40104000, 0));
  ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_HFNMIENA_Msk);
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  GPIO_PinWrite(GPIO, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON));
}

uint32_t board_button_read(void) {
  // active low
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(GPIO, BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const* buf, int len) {
  USART_WriteBlocking(UART_DEV, (uint8_t const*) buf, len);
  return len;
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


#ifndef __ICCARM__
// Implement _start() since we use linker flag '-nostartfiles'.
// Requires defined __STARTUP_CLEAR_BSS,
extern int main(void);

TU_ATTR_UNUSED void _start(void) {
  // called by startup code
  main();
  while (1) {}
}

#ifdef __clang__
void	_exit (int __status) {
  (void) __status;
  while (1) {}
}
#endif

#endif
