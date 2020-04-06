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

#include "../board.h"
#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "fsl_clock.h"
#include "fsl_lpuart.h"

#include "clock_config.h"

#define LED_PINMUX            IOMUXC_GPIO_B0_03_GPIO2_IO03 // D13
#define LED_PORT              GPIO2
#define LED_PIN               3
#define LED_STATE_ON          0

// no button
#define BUTTON_PINMUX         IOMUXC_GPIO_B0_01_GPIO2_IO01 // D12
#define BUTTON_PORT           GPIO2
#define BUTTON_PIN            1
#define BUTTON_STATE_ACTIVE   0

// UART
#define UART_PORT             LPUART6
#define UART_RX_PINMUX        IOMUXC_GPIO_AD_B0_03_LPUART6_RX  // D0
#define UART_TX_PINMUX        IOMUXC_GPIO_AD_B0_02_LPUART6_TX  // D1

const uint8_t dcd_data[] = { 0x00 };

void board_init(void)
{
  // Init clock
  BOARD_BootClockRUN();
  SystemCoreClockUpdate();

  // Enable IOCON clock
  CLOCK_EnableClock(kCLOCK_Iomuxc);

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
//  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  // LED
  IOMUXC_SetPinMux( LED_PINMUX, 0U);
  IOMUXC_SetPinConfig( LED_PINMUX, 0x10B0U);

  gpio_pin_config_t led_config = { kGPIO_DigitalOutput, 0, kGPIO_NoIntmode };
  GPIO_PinInit(LED_PORT, LED_PIN, &led_config);
  board_led_write(true);

  // Button
  IOMUXC_SetPinMux( BUTTON_PINMUX, 0U);
  IOMUXC_SetPinConfig(BUTTON_PINMUX, 0x01B0A0U);
  gpio_pin_config_t button_config = { kGPIO_DigitalInput, 0, kGPIO_NoIntmode };
  GPIO_PinInit(BUTTON_PORT, BUTTON_PIN, &button_config);

  // UART
  IOMUXC_SetPinMux( UART_TX_PINMUX, 0U);
  IOMUXC_SetPinMux( UART_RX_PINMUX, 0U);
  IOMUXC_SetPinConfig( UART_TX_PINMUX, 0x10B0u);
  IOMUXC_SetPinConfig( UART_RX_PINMUX, 0x10B0u);

  lpuart_config_t uart_config;
  LPUART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = CFG_BOARD_UART_BAUDRATE;
  uart_config.enableTx = true;
  uart_config.enableRx = true;
  LPUART_Init(UART_PORT, &uart_config, (CLOCK_GetPllFreq(kCLOCK_PllUsb1) / 6U) / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U));

  //------------- USB0 -------------//
  // Clock
  CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
  CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000U);

  USBPHY_Type* usb_phy = USBPHY1;

  // Enable PHY support for Low speed device + LS via FS Hub
  usb_phy->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK | USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;

  // Enable all power for normal operation
  usb_phy->PWD = 0;

  // TX Timing
  uint32_t phytx = usb_phy->TX;
  phytx &= ~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK | USBPHY_TX_TXCAL45DP_MASK);
  phytx |= USBPHY_TX_D_CAL(0x0C) | USBPHY_TX_TXCAL45DP(0x06) | USBPHY_TX_TXCAL45DM(0x06);
  usb_phy->TX = phytx;

  // USB1
//  CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
//  CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, 480000000U);
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USB_OTG1_IRQHandler(void)
{
  #if CFG_TUSB_RHPORT0_MODE & OPT_MODE_HOST
    tuh_isr(0);
  #endif

  #if CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE
    tud_irq_handler(0);
  #endif
}

void USB_OTG2_IRQHandler(void)
{
  #if CFG_TUSB_RHPORT1_MODE & OPT_MODE_HOST
    tuh_isr(1);
  #endif

  #if CFG_TUSB_RHPORT1_MODE & OPT_MODE_DEVICE
    tud_irq_handler(1);
  #endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  GPIO_PinWrite(LED_PORT, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void)
{
  // active low
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len)
{
  LPUART_ReadBlocking(UART_PORT, buf, len);
  return len;
}

int board_uart_write(void const * buf, int len)
{
  LPUART_WriteBlocking(UART_PORT, (uint8_t*)buf, len);
  return len;
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
#endif
