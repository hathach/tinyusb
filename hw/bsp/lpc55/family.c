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
#include "board.h"
#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "fsl_power.h"
#include "fsl_iocon.h"
#include "fsl_usart.h"

#ifdef NEOPIXEL_PIN
#include "fsl_sctimer.h"
#include "sct_neopixel.h"
#endif

#ifdef BOARD_TUD_RHPORT
  #define PORT_SUPPORT_DEVICE(_n)  (BOARD_TUD_RHPORT == _n)
#else
  #define PORT_SUPPORT_DEVICE(_n)  0
#endif

#ifdef BOARD_TUH_RHPORT
  #define PORT_SUPPORT_HOST(_n)    (BOARD_TUH_RHPORT == _n)
#else
  #define PORT_SUPPORT_HOST(_n)    0
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

// IOCON pin mux
#define IOCON_PIO_DIGITAL_EN     0x0100u // Enables digital function
#define IOCON_PIO_FUNC0          0x00u   // Selects pin function 0
#define IOCON_PIO_FUNC1          0x01u   // Selects pin function 1
#define IOCON_PIO_FUNC4          0x04u   // Selects pin function 4
#define IOCON_PIO_FUNC7          0x07u   // Selects pin function 7
#define IOCON_PIO_INV_DI         0x00u   // Input function is not inverted
#define IOCON_PIO_MODE_INACT     0x00u   // No addition pin function
#define IOCON_PIO_OPENDRAIN_DI   0x00u   // Open drain is disabled
#define IOCON_PIO_SLEW_STANDARD  0x00u   // Standard mode, output slew rate control is enabled

#define IOCON_PIO_DIG_FUNC0_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC0) // Digital pin function 0 enabled
#define IOCON_PIO_DIG_FUNC1_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC1) // Digital pin function 1 enabled
#define IOCON_PIO_DIG_FUNC4_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC4) // Digital pin function 2 enabled
#define IOCON_PIO_DIG_FUNC7_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC7) // Digital pin function 2 enabled

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB0_IRQHandler(void)
{
  tud_int_handler(0);
}

void USB1_IRQHandler(void)
{
  tud_int_handler(1);
}

/****************************************************************
name: BOARD_BootClockFROHF96M
outputs:
- {id: SYSTICK_clock.outFreq, value: 96 MHz}
- {id: System_clock.outFreq, value: 96 MHz}
settings:
- {id: SYSCON.MAINCLKSELA.sel, value: SYSCON.fro_hf}
sources:
- {id: SYSCON.fro_hf.outFreq, value: 96 MHz}
******************************************************************/
void BootClockFROHF96M(void)
{
  /*!< Set up the clock sources */
  /*!< Set up FRO */
  POWER_DisablePD(kPDRUNCFG_PD_FRO192M); /*!< Ensure FRO is on  */
  CLOCK_SetupFROClocking(12000000U);     /*!< Set up FRO to the 12 MHz, just for sure */
  CLOCK_AttachClk(kFRO12M_to_MAIN_CLK); /*!< Switch to FRO 12MHz first to ensure we can change voltage without
                                             accidentally being below the voltage for current speed */

  CLOCK_SetupFROClocking(96000000U); /*!< Set up high frequency FRO output to selected frequency */

  POWER_SetVoltageForFreq(96000000U); /*!< Set voltage for the one of the fastest clock outputs: System clock output */
  CLOCK_SetFLASHAccessCyclesForFreq(96000000U); /*!< Set FLASH wait states for core */

  /*!< Set up dividers */
  CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U, false);     /*!< Set AHBCLKDIV divider to value 1 */

  /*!< Set up clock selectors - Attach clocks to the peripheries */
  CLOCK_AttachClk(kFRO_HF_to_MAIN_CLK); /*!< Switch MAIN_CLK to FRO_HF */

  /*!< Set SystemCoreClock variable. */
  SystemCoreClock = 96000000U;
}

void board_init(void)
{
  // Enable IOCON clock
  CLOCK_EnableClock(kCLOCK_Iocon);

  // Init 96 MHz clock
  BootClockFROHF96M();

  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  // Init all GPIO ports
  GPIO_PortInit(GPIO, 0);
  GPIO_PortInit(GPIO, 1);

  // LED
  IOCON_PinMuxSet(IOCON, LED_PORT, LED_PIN, IOCON_PIO_DIG_FUNC0_EN);
  gpio_pin_config_t const led_config = { kGPIO_DigitalOutput, 1};
  GPIO_PinInit(GPIO, LED_PORT, LED_PIN, &led_config);

  board_led_write(0);

#ifdef NEOPIXEL_PIN
  // Neopixel
  static uint32_t pixelData[NEOPIXEL_NUMBER];
  IOCON_PinMuxSet(IOCON, NEOPIXEL_PORT, NEOPIXEL_PIN, IOCON_PIO_DIG_FUNC4_EN);

  sctpix_init(NEOPIXEL_TYPE);
  sctpix_addCh(NEOPIXEL_CH, pixelData, NEOPIXEL_NUMBER);
  sctpix_setPixel(NEOPIXEL_CH, 0, 0x100010);
  sctpix_setPixel(NEOPIXEL_CH, 1, 0x100010);
  sctpix_show();
#endif

  // Button
  IOCON_PinMuxSet(IOCON, BUTTON_PORT, BUTTON_PIN, IOCON_PIO_DIG_FUNC0_EN);
  gpio_pin_config_t const button_config = { kGPIO_DigitalInput, 0};
  GPIO_PinInit(GPIO, BUTTON_PORT, BUTTON_PIN, &button_config);

#ifdef UART_DEV
  // UART
  IOCON_PinMuxSet(IOCON, UART_RX_PINMUX);
  IOCON_PinMuxSet(IOCON, UART_TX_PINMUX);

  // Enable UART when debug log is on
  CLOCK_AttachClk(kFRO12M_to_FLEXCOMM0);
  usart_config_t uart_config;
  USART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = CFG_BOARD_UART_BAUDRATE;
  uart_config.enableTx     = true;
  uart_config.enableRx     = true;
  USART_Init(UART_DEV, &uart_config, 12000000);
#endif

  // USB VBUS
  /* PORT0 PIN22 configured as USB0_VBUS */
  IOCON_PinMuxSet(IOCON, 0U, 22U, IOCON_PIO_DIG_FUNC7_EN);

#if PORT_SUPPORT_DEVICE(0)
  // Port0 is Full Speed

  /* Turn on USB0 Phy */
  POWER_DisablePD(kPDRUNCFG_PD_USB0_PHY);

  /* reset the IP to make sure it's in reset state. */
  RESET_PeripheralReset(kUSB0D_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB0HSL_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB0HMR_RST_SHIFT_RSTn);

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
#endif

#if PORT_SUPPORT_DEVICE(1)
  // Port1 is High Speed

  /* Turn on USB1 Phy */
  POWER_DisablePD(kPDRUNCFG_PD_USB1_PHY);

  /* reset the IP to make sure it's in reset state. */
  RESET_PeripheralReset(kUSB1H_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1D_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1RAM_RST_SHIFT_RSTn);

  /* According to reference manual, device mode setting has to be set by access usb host register */
  CLOCK_EnableClock(kCLOCK_Usbh1); // enable usb0 host clock

  USBHSH->PORTMODE = USBHSH_PORTMODE_SW_PDCOM_MASK; // Put PHY powerdown under software control
  USBHSH->PORTMODE |= USBHSH_PORTMODE_DEV_ENABLE_MASK;

  CLOCK_DisableClock(kCLOCK_Usbh1); // disable usb0 host clock

  /* enable USB Device clock */
  CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_UsbPhySrcExt, XTAL0_CLK_HZ);
  CLOCK_EnableUsbhs0DeviceClock(kCLOCK_UsbSrcUnused, 0U);
  CLOCK_EnableClock(kCLOCK_UsbRam1);

  // Enable PHY support for Low speed device + LS via FS Hub
  USBPHY->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK | USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;

  // Enable all power for normal operation
  USBPHY->PWD = 0;

  USBPHY->CTRL_SET = USBPHY_CTRL_SET_ENAUTOCLR_CLKGATE_MASK;
  USBPHY->CTRL_SET = USBPHY_CTRL_SET_ENAUTOCLR_PHY_PWD_MASK;

  // TX Timing
//  uint32_t phytx = USBPHY->TX;
//  phytx &= ~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK | USBPHY_TX_TXCAL45DP_MASK);
//  phytx |= USBPHY_TX_D_CAL(0x0C) | USBPHY_TX_TXCAL45DP(0x06) | USBPHY_TX_TXCAL45DM(0x06);
//  USBPHY->TX = phytx;
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  GPIO_PinWrite(GPIO, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));

#ifdef NEOPIXEL_PIN
  if (state) {
    sctpix_setPixel(NEOPIXEL_CH, 0, 0x100000);
    sctpix_setPixel(NEOPIXEL_CH, 1, 0x101010);
  } else {
    sctpix_setPixel(NEOPIXEL_CH, 0, 0x001000);
    sctpix_setPixel(NEOPIXEL_CH, 1, 0x000010);
  }
  sctpix_show();
#endif
}

uint32_t board_button_read(void)
{
  // active low
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(GPIO, BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
  USART_WriteBlocking(UART_DEV, (uint8_t const *) buf, len);
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
