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

#include "bsp/board_api.h"
#include "board/clock_config.h"
#include "board/pin_mux.h"
#include "board.h"

// Suppress warning caused by mcu driver
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "fsl_clock.h"
#include "fsl_lpuart.h"
#include "fsl_ocotp.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// needed by fsl_flexspi_nor_boot
TU_ATTR_USED const uint8_t dcd_data[] = { 0x00 };

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// unify naming convention
#if !defined(USBPHY1) && defined(USBPHY)
  #define USBPHY1 USBPHY
#endif

static void init_usb_phy(uint8_t usb_id) {
  USBPHY_Type* usb_phy;

  if (usb_id == 0) {
    usb_phy = USBPHY1;
    CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, BOARD_XTAL0_CLK_HZ);
    CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, BOARD_XTAL0_CLK_HZ);
  }
  #ifdef USBPHY2
  else if (usb_id == 1) {
    usb_phy = USBPHY2;
    CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M, BOARD_XTAL0_CLK_HZ);
    CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, BOARD_XTAL0_CLK_HZ);
  }
  #endif
  else {
    return;
  }

  // Enable PHY support for Low speed device + LS via FS Hub
  usb_phy->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK | USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;

  // Enable all power for normal operation
  // TODO may not be needed since it is called within CLOCK_EnableUsbhs0PhyPllClock()
  usb_phy->PWD = 0;

  // TX Timing
  uint32_t phytx = usb_phy->TX;
  phytx &= ~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK | USBPHY_TX_TXCAL45DP_MASK);
  phytx |= USBPHY_TX_D_CAL(0x0C) | USBPHY_TX_TXCAL45DP(0x06) | USBPHY_TX_TXCAL45DM(0x06);
  usb_phy->TX = phytx;
}

void board_init(void)
{
  // make sure the dcache is on.
#if defined(__DCACHE_PRESENT) && __DCACHE_PRESENT
  if (SCB_CCR_DC_Msk != (SCB_CCR_DC_Msk & SCB->CCR)) SCB_EnableDCache();
#endif

  BOARD_InitPins();
  BOARD_BootClockRUN();
  SystemCoreClockUpdate();

#ifdef TRACE_ETM
  //CLOCK_EnableClock(kCLOCK_Trace);
#endif

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);

#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_OTG1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#ifdef USBPHY2
  NVIC_SetPriority(USB_OTG2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif
#endif

  board_led_write(true);

  // UART
  lpuart_config_t uart_config;
  LPUART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = CFG_BOARD_UART_BAUDRATE;
  uart_config.enableTx = true;
  uart_config.enableRx = true;

  if ( kStatus_Success != LPUART_Init(UART_PORT, &uart_config, UART_CLK_ROOT) ) {
    // failed to init uart, probably baudrate is not supported
    // TU_BREAKPOINT();
  }

  //------------- USB -------------//
  // Note: RT105x RT106x and later have dual USB controllers.
  init_usb_phy(0); // USB0
#ifdef USBPHY2
  init_usb_phy(1); // USB1
#endif
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USB_OTG1_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void USB_OTG2_IRQHandler(void) {
  tusb_int_handler(1, true);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  GPIO_PinWrite(LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON));
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(BUTTON_PORT, BUTTON_PIN);
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void) max_len;

  #if FSL_FEATURE_OCOTP_HAS_TIMING_CTRL
  OCOTP_Init(OCOTP, CLOCK_GetFreq(kCLOCK_IpgClk));
  #else
  OCOTP_Init(OCOTP, 0u);
  #endif

  // Reads shadow registers 0x01 - 0x04 (Configuration and Manufacturing Info)
  // into 8 bit wide destination, avoiding punning.
  for (int i = 0; i < 4; ++i) {
    uint32_t wr = OCOTP_ReadFuseShadowRegister(OCOTP, i + 1);
    for (int j = 0; j < 4; j++) {
      id[i*4+j] = wr & 0xff;
      wr >>= 8;
    }
  }
  OCOTP_Deinit(OCOTP);

  return 16;
}

int board_uart_read(uint8_t* buf, int len) {
  int count = 0;

  while (count < len) {
    uint8_t const rx_count = LPUART_GetRxFifoCount(UART_PORT);
    if (!rx_count) {
      // clear all error flag if any
      uint32_t status_flags = LPUART_GetStatusFlags(UART_PORT);
      status_flags &= (kLPUART_RxOverrunFlag | kLPUART_ParityErrorFlag | kLPUART_FramingErrorFlag |
                       kLPUART_NoiseErrorFlag);
      LPUART_ClearStatusFlags(UART_PORT, status_flags);
      break;
    }

    for (int i = 0; i < rx_count; i++) {
      buf[count] = LPUART_ReadByte(UART_PORT);
      count++;
    }
  }

  return count;
}

int board_uart_write(void const * buf, int len) {
  LPUART_WriteBlocking(UART_PORT, (uint8_t const*)buf, len);
  return len;
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
  while (1) {}
}
#endif

#endif
