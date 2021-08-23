/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 * Copyright (c) 2020, Koji Kitayama
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
#include "board.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_lpuart.h"

#include "clock_config.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB0_IRQHandler(void)
{
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
// LED
/*
//#define LED_PINMUX            IOMUXC_GPIO_AD_B0_09_GPIO1_IO09
#define LED_PORT              GPIOB
#define LED_PIN_CLOCK         kCLOCK_PortD
#define LED_PIN_PORT          PORTD
#define LED_PIN               5U
#define LED_PIN_FUNCTION      kPORT_MuxAsGpio
#define LED_STATE_ON          0

// UART
#define UART_PORT             LPUART0
#define UART_PIN_CLOCK        kCLOCK_PortA
#define UART_PIN_PORT         PORTA
#define UART_PIN_RX           1u
#define UART_PIN_TX           2u
#define UART_PIN_FUNCTION     kPORT_MuxAlt2
*/
//#define SOPT5_UART0RXSRC_UART_RX      0x00u   /*!< UART0 receive data source select: UART0_RX pin */
//#define SOPT5_UART0TXSRC_UART_TX      0x00u   /*!< UART0 transmit data source select: UART0_TX pin */

//const uint8_t dcd_data[] = { 0x00 };

void board_init(void)
{
  /* Port A Clock Gate Control: Clock enabled */
  CLOCK_EnableClock(kCLOCK_PortA);
  /* Port B Clock Gate Control: Clock enabled */
  CLOCK_EnableClock(kCLOCK_PortB);
  /* Port C Clock Gate Control: Clock enabled */
  CLOCK_EnableClock(kCLOCK_PortC);
  /* Port D Clock Gate Control: Clock enabled */
  CLOCK_EnableClock(kCLOCK_PortD);
  /* Port E Clock Gate Control: Clock enabled */
  CLOCK_EnableClock(kCLOCK_PortE);

  gpio_pin_config_t led_config = { kGPIO_DigitalOutput, 0 };
  GPIO_PinInit(LED_GPIO, LED_PIN, &led_config);
  PORT_SetPinMux(LED_PORT, LED_PIN, kPORT_MuxAsGpio);

  gpio_pin_config_t button_config = { kGPIO_DigitalInput, 0 };
  GPIO_PinInit(BUTTON_GPIO, BUTTON_PIN, &button_config);
  const port_pin_config_t BUTTON_CFG = {
    kPORT_PullUp, 
    kPORT_FastSlewRate, 
    kPORT_PassiveFilterDisable, 
    kPORT_LowDriveStrength, 
    kPORT_MuxAsGpio
  };
  PORT_SetPinConfig(BUTTON_PORT, BUTTON_PIN, &BUTTON_CFG);

  /* PORTA1 (pin 23) is configured as LPUART0_RX */
  PORT_SetPinMux(PORTA, 1U, kPORT_MuxAlt2);
  /* PORTA2 (pin 24) is configured as LPUART0_TX */
  PORT_SetPinMux(PORTA, 2U, kPORT_MuxAlt2);
  
  SIM->SOPT5 = ((SIM->SOPT5 &
               /* Mask bits to zero which are setting */
               (~(SIM_SOPT5_LPUART0TXSRC_MASK | SIM_SOPT5_LPUART0RXSRC_MASK)))
               /* LPUART0 Transmit Data Source Select: LPUART0_TX pin. */
               | SIM_SOPT5_LPUART0TXSRC(SOPT5_LPUART0TXSRC_LPUART_TX)
               /* LPUART0 Receive Data Source Select: LPUART_RX pin. */
               | SIM_SOPT5_LPUART0RXSRC(SOPT5_LPUART0RXSRC_LPUART_RX));

  BOARD_BootClockRUN();
  SystemCoreClockUpdate();
  CLOCK_SetLpuart0Clock(1);

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  lpuart_config_t uart_config;
  LPUART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = CFG_BOARD_UART_BAUDRATE;
  uart_config.enableTx = true;
  uart_config.enableRx = true;
  LPUART_Init(UART_PORT, &uart_config, CLOCK_GetFreq(kCLOCK_McgIrc48MClk));

  // USB
//  SystemCoreClockUpdate();
  CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcIrc48M, 48000000U);
//  CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcPll0, CLOCK_GetFreq(kCLOCK_PllFllSelClk));
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  GPIO_PinWrite(LED_GPIO, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void)
{
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(BUTTON_GPIO, BUTTON_PIN);
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
