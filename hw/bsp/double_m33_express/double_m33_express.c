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
#include "fsl_sctimer.h"

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

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#define LED_PORT              0
#define LED_PIN               1
#define LED_STATE_ON          1

// WAKE button
#define BUTTON_PORT           0
#define BUTTON_PIN            5
#define BUTTON_STATE_ACTIVE   0

// Number of neopixels
#define NEOPIXEL_NUMBER       2
#define NEOPIXEL_PORT         0
#define NEOPIXEL_PIN          27

// UART
#define UART_DEV              USART0

// IOCON pin mux
#define IOCON_PIO_DIGITAL_EN 0x0100u  /*!<@brief Enables digital function */
#define IOCON_PIO_FUNC0 0x00u         /*!<@brief Selects pin function 0 */
#define IOCON_PIO_FUNC1 0x01u         /*!<@brief Selects pin function 1 */
#define IOCON_PIO_FUNC4 0x04u         /*!<@brief Selects pin function 4 */
#define IOCON_PIO_FUNC7 0x07u         /*!<@brief Selects pin function 7 */
#define IOCON_PIO_INV_DI 0x00u        /*!<@brief Input function is not inverted */
#define IOCON_PIO_MODE_INACT 0x00u    /*!<@brief No addition pin function */
#define IOCON_PIO_OPENDRAIN_DI 0x00u  /*!<@brief Open drain is disabled */
#define IOCON_PIO_SLEW_STANDARD 0x00u /*!<@brief Standard mode, output slew rate control is enabled */

#define IOCON_PIO_DIG_FUNC0_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC0) /*!<@brief Digital pin function 0 enabled */
#define IOCON_PIO_DIG_FUNC1_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC1) /*!<@brief Digital pin function 1 enabled */
#define IOCON_PIO_DIG_FUNC4_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC4) /*!<@brief Digital pin function 2 enabled */
#define IOCON_PIO_DIG_FUNC7_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC7) /*!<@brief Digital pin function 2 enabled */

//--------------------------------------------------------------------+
// Neopixel Driver
//--------------------------------------------------------------------+
#define NEO_SCT           SCT0
#define NEO_MATCH_PERIOD  0
#define NEO_MATCH_0       1
#define NEO_MATCH_1       2
#define NEO_EVENT_RISE    2
#define NEO_EVENT_FALL_0  0
#define NEO_EVENT_FALL_1  1
#define NEO_EVENT_NEXT    3
#define NEO_EVENT_START   4
#define NEO_SCT_OUTPUT    6
#define NEO_STATE_IDLE    24
//#define NEO_ARRAY_SIZE    (3 * NEOPIXEL_NUMBER)

//volatile uint32_t _neopixel_array[NEO_ARRAY_SIZE] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
volatile uint32_t _neopixel_array[NEOPIXEL_NUMBER] = {0x404040, 0x202020};
volatile uint32_t _neopixel_count = 0;

void neopixel_int_handler(void){
  uint32_t eventFlag = NEO_SCT->EVFLAG;
  if (eventFlag & (1 << NEO_EVENT_NEXT)) {
    _neopixel_count += 1;
    if (_neopixel_count < (NEOPIXEL_NUMBER)) {
      NEO_SCT->EV[NEO_EVENT_FALL_0].STATE = 0xFFFFFF & (~_neopixel_array[_neopixel_count]);
      NEO_SCT->CTRL &= ~(SCT_CTRL_HALT_L_MASK);
    }
  }
  NEO_SCT->EVFLAG = eventFlag;
}

void SCT0_DriverIRQHandler(void){
  neopixel_int_handler();
  SDK_ISR_EXIT_BARRIER;
}

void neopixel_set(uint32_t pixel, uint32_t color){
  if (pixel < NEOPIXEL_NUMBER) { 
    _neopixel_array[pixel] = color;
  }
}

void neopixel_update(void){
//  while (NEO_SCT->CTRL & SCT_CTRL_HALT_L_MASK);
  _neopixel_count = 0;
  NEO_SCT->EV[NEO_EVENT_FALL_0].STATE = 0xFFFFFF & (~_neopixel_array[0]);
  NEO_SCT->CTRL &= ~(SCT_CTRL_HALT_L_MASK);
}


/*
void neopixel_int_handler(void){
  uint32_t eventFlag = NEO_SCT->EVFLAG;
//  if ((eventFlag & (1 << NEO_EVENT_NEXT)) && (_neopixel_count < (NEO_ARRAY_SIZE))) {
//    NEO_SCT->EV[NEO_EVENT_FALL_0].STATE = 0xFF & (~_neopixel_array[_neopixel_count]);
  if ((eventFlag & (1 << NEO_EVENT_NEXT)) && (_neopixel_count < (NEOPIXEL_NUMBER))) {
    _neopixel_count += 1;
    NEO_SCT->EV[NEO_EVENT_FALL_0].STATE = 0xFFFFFF & (~_neopixel_array[_neopixel_count]);
    NEO_SCT->EVFLAG = eventFlag;
    NEO_SCT->CTRL &= ~(SCT_CTRL_HALT_L_MASK);
  } else {
    NEO_SCT->EVFLAG = eventFlag;
  }
}

void SCT0_DriverIRQHandler(void){
  neopixel_int_handler();
  SDK_ISR_EXIT_BARRIER;
}

void neopixel_update(uint32_t pixel, uint32_t color){
  if (pixel < NEOPIXEL_NUMBER) { 
    _neopixel_array[pixel] = color;
    _neopixel_count = 0;
//    NEO_SCT->EV[NEO_EVENT_FALL_0].STATE = 0xFF & (~_neopixel_array[0]);
    NEO_SCT->EV[NEO_EVENT_FALL_0].STATE = 0xFFFFFF & (~_neopixel_array[0]);
    NEO_SCT->CTRL &= ~(SCT_CTRL_HALT_L_MASK);
  }
}
*/
void neopixel_init(void) {
  CLOCK_EnableClock(kCLOCK_Sct0);
  RESET_PeripheralReset(kSCT0_RST_SHIFT_RSTn);

  NEO_SCT->CONFIG = (
    SCT_CONFIG_UNIFY(1) | 
    SCT_CONFIG_CLKMODE(kSCTIMER_System_ClockMode) | 
    SCT_CONFIG_NORELOAD_L(1) );
  NEO_SCT->CTRL = ( 
    SCT_CTRL_HALT_L(1) |
    SCT_CTRL_CLRCTR_L(1) );

  NEO_SCT->MATCH[NEO_MATCH_PERIOD] = 120;
  NEO_SCT->MATCH[NEO_MATCH_0] = 30;
  NEO_SCT->MATCH[NEO_MATCH_1] = 60;
  NEO_SCT->EV[NEO_EVENT_START].STATE = (1 << NEO_STATE_IDLE);
  NEO_SCT->EV[NEO_EVENT_START].CTRL = (
    kSCTIMER_OutputLowEvent | SCT_EV_CTRL_IOSEL(NEO_SCT_OUTPUT) | 
    SCT_EV_CTRL_STATELD(1) | SCT_EV_CTRL_STATEV(23));
//  NEO_SCT->EV[NEO_EVENT_RISE].STATE = 0xFE;
  NEO_SCT->EV[NEO_EVENT_RISE].STATE = 0xFFFFFE;
  NEO_SCT->EV[NEO_EVENT_RISE].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_PERIOD) | 
    SCT_EV_CTRL_STATELD(0) | SCT_EV_CTRL_STATEV(31));
  NEO_SCT->EV[NEO_EVENT_FALL_0].STATE = 0x0;
  NEO_SCT->EV[NEO_EVENT_FALL_0].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) | 
    SCT_EV_CTRL_STATELD(0) );
//  NEO_SCT->EV[NEO_EVENT_FALL_1].STATE = 0xFF;
  NEO_SCT->EV[NEO_EVENT_FALL_1].STATE = 0xFFFFFF;
  NEO_SCT->EV[NEO_EVENT_FALL_1].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_1) | 
    SCT_EV_CTRL_STATELD(0) );
  NEO_SCT->EV[NEO_EVENT_NEXT].STATE = 0x1;
  NEO_SCT->EV[NEO_EVENT_NEXT].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_PERIOD) | 
    SCT_EV_CTRL_STATELD(1) | SCT_EV_CTRL_STATEV(NEO_STATE_IDLE));

  NEO_SCT->LIMIT = (1 << NEO_EVENT_START) | (1 << NEO_EVENT_RISE) | (1 << NEO_EVENT_NEXT);
  NEO_SCT->HALT = (1 << NEO_EVENT_NEXT);
  NEO_SCT->START = (1 << NEO_EVENT_START);

  NEO_SCT->OUT[NEO_SCT_OUTPUT].SET = (1 << NEO_EVENT_START) | (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[NEO_SCT_OUTPUT].CLR = (1 << NEO_EVENT_FALL_0) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_NEXT);
  
  NEO_SCT->STATE = NEO_STATE_IDLE; 
  NEO_SCT->OUTPUT = 0x0;
  NEO_SCT->RES = SCT_RES_O6RES(0x2);
  NEO_SCT->EVEN = (1 << NEO_EVENT_NEXT);
  EnableIRQ(SCT0_IRQn);

  neopixel_set(0, 0x101000);
  neopixel_set(1, 0x101000);
  neopixel_update();
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

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  GPIO_PortInit(GPIO, 0);
  GPIO_PortInit(GPIO, 1);

  // LED
  /* PORT0 PIN1 configured as PIO0_1 */
  IOCON_PinMuxSet(IOCON, 0U, 1U, IOCON_PIO_DIG_FUNC0_EN);

  gpio_pin_config_t const led_config = { kGPIO_DigitalOutput, 1};
  GPIO_PinInit(GPIO, LED_PORT, LED_PIN, &led_config);

  // Neopixel
  /* PORT0 PIN27 configured as SCT0_OUT6 */
  IOCON_PinMuxSet(IOCON, NEOPIXEL_PORT, NEOPIXEL_PIN, IOCON_PIO_DIG_FUNC4_EN);

  neopixel_init();

  // Button
  /* PORT0 PIN5 configured as PIO0_5 */
  IOCON_PinMuxSet(IOCON, BUTTON_PORT, BUTTON_PIN, IOCON_PIO_DIG_FUNC0_EN);

  gpio_pin_config_t const button_config = { kGPIO_DigitalInput, 0};
  GPIO_PinInit(GPIO, BUTTON_PORT, BUTTON_PIN, &button_config);

  // UART
  /* PORT0 PIN29 (coords: 92) is configured as FC0_RXD_SDA_MOSI_DATA */
  IOCON_PinMuxSet(IOCON, 0U, 29U, IOCON_PIO_DIG_FUNC1_EN);
  /* PORT0 PIN30 (coords: 94) is configured as FC0_TXD_SCL_MISO_WS */
  IOCON_PinMuxSet(IOCON, 0U, 30U, IOCON_PIO_DIG_FUNC1_EN);

#if defined(UART_DEV) && CFG_TUSB_DEBUG
  // Enable UART when debug log is on
  CLOCK_AttachClk(kFRO12M_to_FLEXCOMM0);
  usart_config_t uart_config;
  USART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = BOARD_UART_BAUDRATE;
  uart_config.enableTx     = true;
  uart_config.enableRx     = true;
  USART_Init(UART_DEV, &uart_config, 12000000);
#endif

  // USB VBUS
  /* PORT0 PIN22 configured as USB0_VBUS */
  IOCON_PinMuxSet(IOCON, 0U, 22U, IOCON_PIO_DIG_FUNC7_EN);

  // USB Controller
  POWER_DisablePD(kPDRUNCFG_PD_USB0_PHY); /*Turn on USB0 Phy */
  POWER_DisablePD(kPDRUNCFG_PD_USB1_PHY); /*< Turn on USB1 Phy */

  /* reset the IP to make sure it's in reset state. */
  RESET_PeripheralReset(kUSB0D_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB0HSL_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB0HMR_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1H_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1D_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1RAM_RST_SHIFT_RSTn);

#if (defined CFG_TUSB_RHPORT1_MODE) && (CFG_TUSB_RHPORT1_MODE & OPT_MODE_DEVICE)
  CLOCK_EnableClock(kCLOCK_Usbh1);
  /* Put PHY powerdown under software control */
  USBHSH->PORTMODE = USBHSH_PORTMODE_SW_PDCOM_MASK;
  /* According to reference mannual, device mode setting has to be set by access usb host register */
  USBHSH->PORTMODE |= USBHSH_PORTMODE_DEV_ENABLE_MASK;
  /* enable usb1 host clock */
  CLOCK_DisableClock(kCLOCK_Usbh1);
#endif

#if (defined CFG_TUSB_RHPORT0_MODE) && (CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE)
  // Enable USB Clock Adjustments to trim the FRO for the full speed controller
  ANACTRL->FRO192M_CTRL |= ANACTRL_FRO192M_CTRL_USBCLKADJ_MASK;
  CLOCK_SetClkDiv(kCLOCK_DivUsb0Clk, 1, false);
  CLOCK_AttachClk(kFRO_HF_to_USB0_CLK);
  /* enable usb0 host clock */
  CLOCK_EnableClock(kCLOCK_Usbhsl0);
  /*According to reference mannual, device mode setting has to be set by access usb host register */
  USBFSH->PORTMODE |= USBFSH_PORTMODE_DEV_ENABLE_MASK;
  /* disable usb0 host clock */
  CLOCK_DisableClock(kCLOCK_Usbhsl0);
  CLOCK_EnableUsbfs0DeviceClock(kCLOCK_UsbfsSrcFro, CLOCK_GetFreq(kCLOCK_FroHf)); /* enable USB Device clock */
#endif

}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  GPIO_PinWrite(GPIO, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
  if (state) {
    neopixel_set(0, 0x100000);
    neopixel_set(1, 0x101010);
  } else {
    neopixel_set(0, 0x001000);
    neopixel_set(1, 0x000010);
  }
  neopixel_update();
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
