/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019
 *    Benjamin Evans
 *    William D. Jones (thor0505@comcast.net),
 *    Ha Thach (tinyusb.org)
 *    Uwe Bonnes (bon@elektron.ikp.physik.tu-darmstadt.de
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

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_tim.h"
#include "bsp/board.h"


/* ** BOARD SETUP **
 * 
 * NOTE: This board has bad signal integrity so you may experience some problems.
 * This setup assumes you have an openh743i-c Core and breakout board. For the HS
 * examples it also assumes you have a waveshare USB3300 breakout board plugged
 * into the ULPI PMOD header on the openh743i-c.
 * 
 * UART Debugging:
 * Due to pin conflicts in the HS configuration, this BSP uses USART3 (PD8, PD9).
 * As such, you won't be able to use the UART to USB converter on board and will
 * require an external UART to USB converter. You could use the waveshare FT232
 * USB UART Board (micro) but any 3.3V UART to USB converter will be fine.
 * 
 * Fullspeed:
 * If VBUS sense is enabled, ensure the PA9-VBUS jumper is connected on the core
 * board. Connect the PB6 jumper for the LED and the Wakeup - PA0 jumper for the
 * button. Connect the USB cable to the USB connector on the core board.
 * 
 * High Speed:
 * Remove all jumpers from the openh743i-c (especially the USART1 jumpers as the
 * pins conflict). Connect the PB6 jumper for the LED and the Wakeup - PA0
 * jumper for the button.
 * 
 * The reset pin on the ULPI PMOD port is not connected to the MCU. You'll need
 * to solder a wire from the RST pin on the USB3300 to a pin of your choosing on
 * the openh743i-c board (this example assumes you've used PD14 as specified with
 * the ULPI_RST_PORT and ULPI_RST_PIN defines below).
 * 
 * Preferably power the board using the external 5VDC jack. Connect the USB cable
 * to the USB connector on the ULPI board. Adjust delays in this file as required.
 * 
 * If you're having trouble, ask a question on the tinyUSB Github Discussion boards.
 * 
 * Have fun!
 *
*/

//--------------------------------------------------------------------+
// BOARD DEFINES
//--------------------------------------------------------------------+

//LED Pin
#define LED_PORT GPIOB
#define LED_PIN GPIO_PIN_6
#define LED_STATE_ON 1

//ULPI PHY reset pin
#define ULPI_RST_PORT GPIOD
#define ULPI_RST_PIN GPIO_PIN_14

// Tamper push-button
#define BUTTON_PORT GPIOA
#define BUTTON_PIN GPIO_PIN_0
#define BUTTON_STATE_ACTIVE 1

// Need external UART to USB converter as USART1 pins conflict with HS ULPI pins
#define UART_DEV USART3
#define UART_CLK_EN __HAL_RCC_USART3_CLK_ENABLE
#define UART_GPIO_PORT GPIOD
#define UART_GPIO_AF GPIO_AF7_USART3
#define UART_TX_PIN GPIO_PIN_8
#define UART_RX_PIN GPIO_PIN_9

// VBUS Sense detection
#define OTG_FS_VBUS_SENSE 1
#define OTG_HS_VBUS_SENSE 0


//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

UART_HandleTypeDef uartHandle;
TIM_HandleTypeDef tim2Handle;


//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// Despite being call USB2_OTG
// OTG_FS is marked as RHPort0 by TinyUSB to be consistent across stm32 port
void OTG_FS_IRQHandler(void)
{
  tud_int_handler(0);
}

// Despite being call USB2_OTG
// OTG_HS is marked as RHPort1 by TinyUSB to be consistent across stm32 port
void OTG_HS_IRQHandler(void)
{
  tud_int_handler(1);
}


//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void board_stm32h7_clock_init(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  __HAL_RCC_SYSCFG_CLK_ENABLE();

  // Supply configuration update enable
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  // Configure the main internal regulator output voltage
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
  {
  }
  // Macro to configure the PLL clock source
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);

  // Initializes the RCC Oscillators according to the specified parameters in the RCC_OscInitTypeDef structure.
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 240;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB | RCC_PERIPHCLK_USART3;
  PeriphClkInitStruct.PLL3.PLL3M = 8;
  PeriphClkInitStruct.PLL3.PLL3N = 336;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 7;
  PeriphClkInitStruct.PLL3.PLL3R = 2;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL3;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_PLL3;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

  // Initializes the CPU, AHB and APB buses clocks
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

  __HAL_RCC_CSI_ENABLE();

  // Enable SYSCFG clock mondatory for I/O Compensation Cell
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  // Enables the I/O Compensation Cell
  HAL_EnableCompensationCell();

  // Enable voltage detector
  HAL_PWREx_EnableUSBVoltageDetector();

  return;
}


//--------------------------------------------------------------------+
// Timer implementation for board delay
// This should be OS safe and doesn't require the scheduler to be running
//--------------------------------------------------------------------+
static void init_timer(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};

  __HAL_RCC_TIM2_CLK_ENABLE();

  //Assuming timer clock is running at 260Mhz this should configure the timer counter to 1000Hz
  tim2Handle.Instance = TIM2;
  tim2Handle.Init.Prescaler = 60000U - 1U;
  tim2Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
  tim2Handle.Init.Period = 0xFFFFFFFFU;
  tim2Handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4;
  tim2Handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&tim2Handle);

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&tim2Handle, &sClockSourceConfig);

  //Start the timer
  HAL_TIM_Base_Start(&tim2Handle);

  return;
}


// Custom board delay implementation using timer ticks
static inline void timer_board_delay(uint32_t ms)
{
  uint32_t startMs = __HAL_TIM_GET_COUNTER(&tim2Handle);
  while ((__HAL_TIM_GET_COUNTER(&tim2Handle) - startMs) < ms)
  {
    asm("nop"); //do nothing
  }
}


//Board initialisation
void board_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  // Init clocks
  board_stm32h7_clock_init();

  // Init timer for delays
  init_timer();

  //Disable systick for now
  //If using an RTOS and the systick interrupt fires without the scheduler running you might have an issue
  //Because this init code now introduces delays, the systick should be disabled until after board init
  SysTick->CTRL &= ~1U;

  // Enable GPIO clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();

  // Enable UART Clock
  UART_CLK_EN();

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  // LED
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  // PHY RST Pin
  GPIO_InitStruct.Pin = ULPI_RST_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ULPI_RST_PORT, &GPIO_InitStruct);
  HAL_GPIO_WritePin(ULPI_RST_PORT, ULPI_RST_PIN, 0U);

  // Button
  GPIO_InitStruct.Pin = BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

  // Uart
  GPIO_InitStruct.Pin = UART_TX_PIN | UART_RX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = UART_GPIO_AF;
  HAL_GPIO_Init(UART_GPIO_PORT, &GPIO_InitStruct);

  uartHandle.Instance = UART_DEV;
  uartHandle.Init.BaudRate = CFG_BOARD_UART_BAUDRATE;
  uartHandle.Init.WordLength = UART_WORDLENGTH_8B;
  uartHandle.Init.StopBits = UART_STOPBITS_1;
  uartHandle.Init.Parity = UART_PARITY_NONE;
  uartHandle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uartHandle.Init.Mode = UART_MODE_TX_RX;
  uartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&uartHandle);

#if BOARD_DEVICE_RHPORT_NUM == 0
  //Full Speed

  // Configure DM DP Pins
  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // This for ID line debug
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  __HAL_RCC_USB2_OTG_FS_CLK_ENABLE();

#if OTG_FS_VBUS_SENSE
  // Configure VBUS Pin
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Enable VBUS sense (B device) via pin PA9
  USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBDEN;
#else
  // Disable VBUS sense (B device) via pin PA9
  USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

  // B-peripheral session valid override enable
  USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
  USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#endif // vbus sense

#elif BOARD_DEVICE_RHPORT_NUM == 1
  //High Speed

  /**USB_OTG_HS GPIO Configuration
    PC0     ------> USB_OTG_HS_ULPI_STP
    PC2_C   ------> USB_OTG_HS_ULPI_DIR
    PC3_C   ------> USB_OTG_HS_ULPI_NXT
    PA3     ------> USB_OTG_HS_ULPI_D0
    PA5     ------> USB_OTG_HS_ULPI_CK
    PB1     ------> USB_OTG_HS_ULPI_D2
    PB10    ------> USB_OTG_HS_ULPI_D3
    PB11    ------> USB_OTG_HS_ULPI_D4
    PB12    ------> USB_OTG_HS_ULPI_D5
    PB13    ------> USB_OTG_HS_ULPI_D6
    PB5     ------> USB_OTG_HS_ULPI_D7
  */

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Peripheral clock enable
  __HAL_RCC_USB_OTG_HS_CLK_ENABLE();
  __HAL_RCC_USB_OTG_HS_ULPI_CLK_ENABLE();

#if OTG_HS_VBUS_SENSE
#error OTG HS VBUS Sense enabled is not implemented
#else
  // No VBUS sense
  USB_OTG_HS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

  // B-peripheral session valid override enable
  USB_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
  USB_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#endif

  // Force device mode
  USB_OTG_HS->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
  USB_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

  //Reset the PHY - Change the delays as you see fit
  timer_board_delay(5U); //Delay 5ms
  HAL_GPIO_WritePin(ULPI_RST_PORT, ULPI_RST_PIN, 1U);
  timer_board_delay(20U); //Delay 20ms
  HAL_GPIO_WritePin(ULPI_RST_PORT, ULPI_RST_PIN, 0U);
  timer_board_delay(20U); //Delay 20ms

#endif // rhport = 1

  //Disable the timer used for delays
  HAL_TIM_Base_Stop(&tim2Handle);
  __HAL_RCC_TIM2_CLK_DISABLE();

  // Configure systick 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);

  //Enable systick
  SysTick->CTRL |= ~1U;

  return;
}


//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+
void board_led_write(bool state)
{
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON));
}


uint32_t board_button_read(void)
{
  return (BUTTON_STATE_ACTIVE == HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN)) ? 1 : 0;
}


int board_uart_read(uint8_t *buf, int len)
{
  (void)buf;
  (void)len;
  return 0;
}


int board_uart_write(void const *buf, int len)
{
  HAL_UART_Transmit(&uartHandle, (uint8_t *)buf, len, 0xffff);
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


void HardFault_Handler(void)
{
  asm("bkpt");
}


// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void)
{
}