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
#include "fsl_power.h"
#include "fsl_iocon.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB0_IRQHandler(void)
{
  tud_irq_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#define LED_PORT      0
#define LED_PIN       29
#define LED_STATE_ON  0

// WAKE button
#define BUTTON_PORT   0
#define BUTTON_PIN    24

// IOCON pin mux
#define IOCON_PIO_DIGITAL_EN          0x80u   /*!< Enables digital function */
#define IOCON_PIO_FUNC1               0x01u   /*!< Selects pin function 1 */
#define IOCON_PIO_FUNC7               0x07u   /*!< Selects pin function 7 */
#define IOCON_PIO_INPFILT_OFF       0x0100u   /*!< Input filter disabled */
#define IOCON_PIO_INV_DI              0x00u   /*!< Input function is not inverted */
#define IOCON_PIO_MODE_INACT          0x00u   /*!< No addition pin function */
#define IOCON_PIO_OPENDRAIN_DI        0x00u   /*!< Open drain is disabled */
#define IOCON_PIO_SLEW_STANDARD       0x00u   /*!< Standard mode, output slew rate control is enabled */

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
  POWER_DisablePD(kPDRUNCFG_PD_FRO_EN); /*!< Ensure FRO is on  */
  CLOCK_AttachClk(kFRO12M_to_MAIN_CLK); /*!< Switch to FRO 12MHz first to ensure we can change voltage without
                                             accidentally being below the voltage for current speed */
  POWER_SetVoltageForFreq(96000000U); /*!< Set voltage for the one of the fastest clock outputs: System clock output */
  CLOCK_SetFLASHAccessCyclesForFreq(96000000U); /*!< Set FLASH wait states for core */

  CLOCK_SetupFROClocking(96000000U); /*!< Set up high frequency FRO output to selected frequency */

  /*!< Set up dividers */
  CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U, false);     /*!< Set AHBCLKDIV divider to value 1 */
  CLOCK_SetClkDiv(kCLOCK_DivSystickClk, 0U, true);  /*!< Reset SYSTICKCLKDIV divider counter and halt it */
  CLOCK_SetClkDiv(kCLOCK_DivSystickClk, 1U, false); /*!< Set SYSTICKCLKDIV divider to value 1 */

  /*!< Set up clock selectors - Attach clocks to the peripheries */
  CLOCK_AttachClk(kFRO_HF_to_MAIN_CLK); /*!< Switch MAIN_CLK to FRO_HF */
  /*!< Set SystemCoreClock variable. */
  SystemCoreClock = 96000000U;
}

void board_init(void)
{
  // Enable IOCON clock
  CLOCK_EnableClock(kCLOCK_Iocon);

  // Enable GPIO0 clock
  CLOCK_EnableClock(kCLOCK_Gpio0);

  // Init 96 MHz clock
  BootClockFROHF96M();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  GPIO_PortInit(GPIO, LED_PORT);
  GPIO_PortInit(GPIO, BUTTON_PORT);

  // LED
  gpio_pin_config_t const led_config = { kGPIO_DigitalOutput, 0};
  GPIO_PinInit(GPIO, LED_PORT, LED_PIN, &led_config);
  board_led_write(true);

  // Button
  gpio_pin_config_t const button_config = { kGPIO_DigitalInput, 0};
  GPIO_PinInit(GPIO, BUTTON_PORT, BUTTON_PIN, &button_config);

  // USB
  const uint32_t port1_pin6_config = (
    IOCON_PIO_FUNC7       | /* Pin is configured as USB0_VBUS */
    IOCON_PIO_MODE_INACT  | /* No addition pin function */
    IOCON_PIO_INV_DI      | /* Input function is not inverted */
    IOCON_PIO_DIGITAL_EN  | /* Enables digital function */
    IOCON_PIO_INPFILT_OFF | /* Input filter disabled */
    IOCON_PIO_OPENDRAIN_DI  /* Open drain is disabled */
  );
  IOCON_PinMuxSet(IOCON, 1, 6, port1_pin6_config); /* PORT1 PIN6 (coords: 26) is configured as USB0_VBUS */

  POWER_DisablePD(kPDRUNCFG_PD_USB0_PHY); /*Turn on USB Phy */
  CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcFro, CLOCK_GetFreq(kCLOCK_FroHf)); /* enable USB IP clock */
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  GPIO_PinWrite(GPIO, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void)
{
  // active low
  return 1-GPIO_PinRead(GPIO, BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
  (void) buf; (void) len;
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
#endif
