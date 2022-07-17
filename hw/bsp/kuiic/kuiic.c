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
#include "fsl_smc.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_lpuart.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define SIM_OSC32KSEL_LPO_CLK         3U        /*!< OSC32KSEL select: LPO clock */
#define SOPT5_LPUART1RXSRC_LPUART_RX  0x00u     /*!<@brief LPUART1 Receive Data Source Select: LPUART_RX pin */
#define SOPT5_LPUART1TXSRC_LPUART_TX  0x00u     /*!<@brief LPUART1 Transmit Data Source Select: LPUART_TX pin */
#define BOARD_BOOTCLOCKRUN_CORE_CLOCK 48000000U /*!< Core clock frequency: 48000000Hz */

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* System clock frequency. */
// extern uint32_t SystemCoreClock;

/*******************************************************************************
 * Variables for BOARD_BootClockRUN configuration
 ******************************************************************************/
const mcglite_config_t mcgliteConfig_BOARD_BootClockRUN = {
    .outSrc          = kMCGLITE_ClkSrcHirc,  /* MCGOUTCLK source is HIRC */
    .irclkEnableMode = kMCGLITE_IrclkEnable, /* MCGIRCLK enabled, MCGIRCLK disabled in STOP mode */
    .ircs            = kMCGLITE_Lirc8M,      /* Slow internal reference (LIRC) 8 MHz clock selected */
    .fcrdiv          = kMCGLITE_LircDivBy1,  /* Low-frequency Internal Reference Clock Divider: divided by 1 */
    .lircDiv2        = kMCGLITE_LircDivBy1,  /* Second Low-frequency Internal Reference Clock Divider: divided by 1 */
    .hircEnableInNotHircMode = true,         /* HIRC source is enabled */
};
const sim_clock_config_t simConfig_BOARD_BootClockRUN = {
    .er32kSrc = SIM_OSC32KSEL_LPO_CLK,       /* OSC32KSEL select: LPO clock */
    .clkdiv1  = 0x10000U,                    /* SIM_CLKDIV1 - OUTDIV1: /1, OUTDIV4: /2 */
};

/*******************************************************************************
 * Code for BOARD_BootClockRUN configuration
 ******************************************************************************/
void BOARD_BootClockRUN(void)
{
    /* Set the system clock dividers in SIM to safe value. */
    CLOCK_SetSimSafeDivs();
    /* Set MCG to HIRC mode. */
    CLOCK_SetMcgliteConfig(&mcgliteConfig_BOARD_BootClockRUN);
    /* Set the clock configuration in SIM module. */
    CLOCK_SetSimConfig(&simConfig_BOARD_BootClockRUN);
    /* Set SystemCoreClock variable. */
    SystemCoreClock = BOARD_BOOTCLOCKRUN_CORE_CLOCK;
}


//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB0_IRQHandler(void)
{
  tud_int_handler(0);
}

void board_init(void)
{
  /* Enable port clocks for GPIO pins */
  CLOCK_EnableClock(kCLOCK_PortA);
  CLOCK_EnableClock(kCLOCK_PortB);
  CLOCK_EnableClock(kCLOCK_PortC);
  CLOCK_EnableClock(kCLOCK_PortD);
  CLOCK_EnableClock(kCLOCK_PortE);

  
  gpio_pin_config_t led_config = { kGPIO_DigitalOutput, 1 };
  GPIO_PinInit(GPIOA, 1U, &led_config);
  PORT_SetPinMux(PORTA, 1U, kPORT_MuxAsGpio);  
  led_config.outputLogic = 0;
  GPIO_PinInit(GPIOA, 2U, &led_config);
  PORT_SetPinMux(PORTA, 2U, kPORT_MuxAsGpio);

#ifdef BUTTON_PIN
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
#endif

  /* PORTC3 is configured as LPUART0_RX */
  PORT_SetPinMux(PORTC, 3U, kPORT_MuxAlt3);
  /* PORTA2 (pin 24) is configured as LPUART0_TX */
  PORT_SetPinMux(PORTE, 0U, kPORT_MuxAlt3);
  
  SIM->SOPT5 = ((SIM->SOPT5 &
               /* Mask bits to zero which are setting */
               (~(SIM_SOPT5_LPUART1TXSRC_MASK | SIM_SOPT5_LPUART1RXSRC_MASK)))
               /* LPUART0 Transmit Data Source Select: LPUART0_TX pin. */
               | SIM_SOPT5_LPUART1TXSRC(SOPT5_LPUART1TXSRC_LPUART_TX)
               /* LPUART0 Receive Data Source Select: LPUART_RX pin. */
               | SIM_SOPT5_LPUART1RXSRC(SOPT5_LPUART1RXSRC_LPUART_RX));

  BOARD_BootClockRUN();
  SystemCoreClockUpdate();
  CLOCK_SetLpuart1Clock(1);

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
  CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcIrc48M, 48000000U);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  if (state) {
    LED_GPIO->PDDR |= GPIO_FIT_REG((1UL << LED_PIN));
  } else {
    LED_GPIO->PDDR &= GPIO_FIT_REG(~(1UL << LED_PIN));
  }
//  GPIO_PinWrite(GPIOA, 1, state ? LED_STATE_ON : (1-LED_STATE_ON) );
//  GPIO_PinWrite(GPIOA, 2, state ? (1-LED_STATE_ON) : LED_STATE_ON );
}

uint32_t board_button_read(void)
{
#ifdef BUTTON_PIN
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(BUTTON_GPIO, BUTTON_PIN);
#else
  return 0;
#endif
}

int board_uart_read(uint8_t* buf, int len)
{
  LPUART_ReadBlocking(UART_PORT, buf, len);
  return len;
}

int board_uart_write(void const * buf, int len)
{
  LPUART_WriteBlocking(UART_PORT, (uint8_t const*) buf, len);
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
