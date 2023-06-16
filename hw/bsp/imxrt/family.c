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

#include "bsp/board.h"
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

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "clock_config.h"

#if defined(BOARD_TUD_RHPORT) && CFG_TUD_ENABLED
  #define PORT_SUPPORT_DEVICE(_n)  (BOARD_TUD_RHPORT == _n)
#else
  #define PORT_SUPPORT_DEVICE(_n)  0
#endif

#if defined(BOARD_TUH_RHPORT) && CFG_TUH_ENABLED
  #define PORT_SUPPORT_HOST(_n)    (BOARD_TUH_RHPORT == _n)
#else
  #define PORT_SUPPORT_HOST(_n)    0
#endif

// needed by fsl_flexspi_nor_boot
TU_ATTR_USED
const uint8_t dcd_data[] = { 0x00 };

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static void init_usb_phy(USBPHY_Type* usb_phy) {
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
  NVIC_SetPriority(USB_OTG1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#ifdef USBPHY2
  NVIC_SetPriority(USB_OTG2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif
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
  gpio_pin_config_t button_config = { kGPIO_DigitalInput, 0, kGPIO_IntRisingEdge, };
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

  uint32_t freq;
  if (CLOCK_GetMux(kCLOCK_UartMux) == 0) /* PLL3 div6 80M */
  {
    freq = (CLOCK_GetPllFreq(kCLOCK_PllUsb1) / 6U) / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U);
  }
  else
  {
    freq = CLOCK_GetOscFreq() / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U);
  }

  if ( kStatus_Success != LPUART_Init(UART_PORT, &uart_config, freq) ) {
    // failed to init uart, probably baudrate is not supported
    // TU_BREAKPOINT();
  }

  //------------- USB -------------//
  // Note: RT105x RT106x and later have dual USB controllers.

  // Clock
  CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
  CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000U);

#ifdef USBPHY1
  init_usb_phy(USBPHY1);
#else
  init_usb_phy(USBPHY);
#endif

#ifdef USBPHY2
  // USB1
  CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
  CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, 480000000U);
  init_usb_phy(USBPHY2);
#endif
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USB_OTG1_IRQHandler(void)
{
  #if PORT_SUPPORT_DEVICE(0)
    tud_int_handler(0);
  #endif

  #if PORT_SUPPORT_HOST(0)
    tuh_int_handler(0);
  #endif
}

void USB_OTG2_IRQHandler(void)
{
  #if PORT_SUPPORT_DEVICE(1)
    tud_int_handler(1);
  #endif

  #if PORT_SUPPORT_HOST(1)
    tuh_int_handler(1);
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
  int count = 0;

  while( count < len )
  {
    uint8_t const rx_count = LPUART_GetRxFifoCount(UART_PORT);
    if (!rx_count)
    {
      // clear all error flag if any
      uint32_t status_flags = LPUART_GetStatusFlags(UART_PORT);
      status_flags  &= (kLPUART_RxOverrunFlag | kLPUART_ParityErrorFlag | kLPUART_FramingErrorFlag | kLPUART_NoiseErrorFlag);
      LPUART_ClearStatusFlags(UART_PORT, status_flags);
      break;
    }

    for(int i=0; i<rx_count; i++)
    {
      buf[count] = LPUART_ReadByte(UART_PORT);
      count++;
    }
  }

  return count;
}

int board_uart_write(void const * buf, int len)
{
  LPUART_WriteBlocking(UART_PORT, (uint8_t const*)buf, len);
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
