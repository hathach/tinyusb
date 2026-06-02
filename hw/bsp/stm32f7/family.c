/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 William D. Jones (thor0505@comcast.net),
 * Uwe Bonnes (bon@elektron.ikp.physik.tu-darmstadt.de),
 * Ha Thach (tinyusb.org)
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
   manufacturer: STMicroelectronics
*/

#include "stm32f7xx_hal.h"
#include "bsp/board_api.h"
#include "common/tusb_fifo.h"

typedef struct {
  GPIO_TypeDef    *port;
  GPIO_InitTypeDef pin_init;
  uint8_t          active_state;
} board_pindef_t;

#include "board.h"

#ifdef UART_ID
  #if UART_ID == 1
    #define USARTn            USART1
    #define USARTn_IRQn       USART1_IRQn
    #define USARTn_IRQHandler USART1_IRQHandler
    #define UARTn_CLK_ENABLE  __HAL_RCC_USART1_CLK_ENABLE
  #elif UART_ID == 2
    #define USARTn            USART2
    #define USARTn_IRQn       USART2_IRQn
    #define USARTn_IRQHandler USART2_IRQHandler
    #define UARTn_CLK_ENABLE  __HAL_RCC_USART2_CLK_ENABLE
  #elif UART_ID == 3
    #define USARTn            USART3
    #define USARTn_IRQn       USART3_IRQn
    #define USARTn_IRQHandler USART3_IRQHandler
    #define UARTn_CLK_ENABLE  __HAL_RCC_USART3_CLK_ENABLE
  #elif UART_ID == 6
    #define USARTn            USART6
    #define USARTn_IRQn       USART6_IRQn
    #define USARTn_IRQHandler USART6_IRQHandler
    #define UARTn_CLK_ENABLE  __HAL_RCC_USART6_CLK_ENABLE
  #endif
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

#ifdef UART_ID
static UART_HandleTypeDef UartHandle = {.Instance = USARTn,
                                        .Init     = {
                                              .BaudRate     = CFG_BOARD_UART_BAUDRATE,
                                              .WordLength   = UART_WORDLENGTH_8B,
                                              .StopBits     = UART_STOPBITS_1,
                                              .Parity       = UART_PARITY_NONE,
                                              .HwFlowCtl    = UART_HWCONTROL_NONE,
                                              .Mode         = UART_MODE_TX_RX,
                                              .OverSampling = UART_OVERSAMPLING_16,
                                        }};

// RX ring buffer via RXNE interrupt — no HAL IT functions used (avoid HAL state conflicts)
static uint8_t   uart_rx_ff_buf[32];
static tu_fifo_t uart_rx_ff;

void USARTn_IRQHandler(void) {
  uint32_t isr = USARTn->ISR;
  if (isr & USART_ISR_RXNE) {
    uint8_t byte = (uint8_t) USARTn->RDR;
    tu_fifo_write(&uart_rx_ff, &byte);
  }
  if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) {
    USARTn->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
  }
}
#endif

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void OTG_FS_IRQHandler(void) {
  tusb_int_handler(0, true);
}

// Despite being call USB2_OTG
// OTG_HS is marked as RHPort1 by TinyUSB to be consistent across stm32 port
void OTG_HS_IRQHandler(void) {
  tusb_int_handler(1, true);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

void board_init(void) {
  SCB_EnableICache();

  HAL_Init();

  board_clock_init();

  // Enable All GPIOs clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE(); // ULPI NXT
  __HAL_RCC_GPIOI_CLK_ENABLE(); // ULPI NXT
#ifdef __HAL_RCC_GPIOJ_CLK_ENABLE
  __HAL_RCC_GPIOJ_CLK_ENABLE();
#endif

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(board_pindef); i++) {
    HAL_GPIO_Init(board_pindef[i].port, &board_pindef[i].pin_init);
  }

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);

#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;

  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

#ifdef UART_ID
  UARTn_CLK_ENABLE();
  HAL_UART_Init(&UartHandle);
  tu_fifo_config(&uart_rx_ff, uart_rx_ff_buf, sizeof(uart_rx_ff_buf), false);
  USARTn->CR1 |= USART_CR1_RXNEIE;
  NVIC_SetPriority(USARTn_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
  NVIC_EnableIRQ(USARTn_IRQn);
#endif

  GPIO_InitTypeDef GPIO_InitStruct;

  //------------- rhport0: OTG_FS -------------//
  /* Configure DM DP Pins */
  GPIO_InitStruct.Pin       = (GPIO_PIN_11 | GPIO_PIN_12);
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Configure OTG-FS ID pin */
  GPIO_InitStruct.Pin       = GPIO_PIN_10;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

// Suppress warning caused by mcu driver
#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
#endif

  /* Enable USB FS Clocks */
  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif

#if OTG_FS_VBUS_SENSE
  /* Configure VBUS Pin */
  GPIO_InitStruct.Pin  = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
#endif // vbus sense

#if CFG_TUD_ENABLED && BOARD_TUD_RHPORT == 0
  tud_configure_dwc2_t cfg = CFG_TUD_CONFIGURE_DWC2_DEFAULT;
  cfg.vbus_sensing         = OTG_FS_VBUS_SENSE;
  tud_configure(0, TUD_CFGID_DWC2, &cfg);
#endif

  //------------- rhport1: OTG_HS -------------//
#ifdef USB_HS_PHYC
  // MCU with built-in HS PHY such as F723, F733, F730

  /* Configure DM DP Pins */
  GPIO_InitStruct.Pin       = (GPIO_PIN_14 | GPIO_PIN_15);
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Configure OTG-HS ID pin */
  GPIO_InitStruct.Pin       = GPIO_PIN_13;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Enable PHYC Clocks */
  __HAL_RCC_OTGPHYC_CLK_ENABLE();

#else
  // MCU with external ULPI PHY

  /* ULPI CLK */
  GPIO_InitStruct.Pin       = GPIO_PIN_5;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* ULPI D0 */
  GPIO_InitStruct.Pin       = GPIO_PIN_3;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* ULPI D1 D2 D3 D4 D5 D6 D7 */
  GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* ULPI STP */
  GPIO_InitStruct.Pin       = GPIO_PIN_0 | GPIO_PIN_2;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* NXT */
  GPIO_InitStruct.Pin       = GPIO_PIN_4;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /* ULPI DIR */
  GPIO_InitStruct.Pin       = GPIO_PIN_11;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
#endif // USB_HS_PHYC

  // Enable USB HS & ULPI Clocks
  __HAL_RCC_USB_OTG_HS_ULPI_CLK_ENABLE();
  __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

#if CFG_TUD_ENABLED && BOARD_TUD_RHPORT == 1
  tud_configure_dwc2_t cfg = CFG_TUD_CONFIGURE_DWC2_DEFAULT;
  cfg.vbus_sensing         = OTG_HS_VBUS_SENSE;
  tud_configure(1, TUD_CFGID_DWC2, &cfg);
#endif

  // Turn off device vbus
#if CFG_TUD_ENABLED
  board_vbus_set(BOARD_TUD_RHPORT, false);
#endif
  // Turn on host vbus
#if CFG_TUH_ENABLED
  board_vbus_set(BOARD_TUH_RHPORT, true);
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
#ifdef PINID_LED
  board_pindef_t *pindef    = &board_pindef[PINID_LED];
  GPIO_PinState   pin_state = state == pindef->active_state ? GPIO_PIN_SET : GPIO_PIN_RESET;
  HAL_GPIO_WritePin(pindef->port, pindef->pin_init.Pin, pin_state);
#else
  (void)state;
#endif
}

uint32_t board_button_read(void) {
#ifdef PINID_BUTTON
  board_pindef_t *pindef = &board_pindef[PINID_BUTTON];
  return pindef->active_state == HAL_GPIO_ReadPin(pindef->port, pindef->pin_init.Pin);
#else
  return 0;
#endif
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void)max_len;
  volatile uint32_t *stm32_uuid = (volatile uint32_t *)UID_BASE;
  uint32_t          *id32       = (uint32_t *)(uintptr_t)id;
  const uint8_t      len        = 12;

  id32[0] = stm32_uuid[0];
  id32[1] = stm32_uuid[1];
  id32[2] = stm32_uuid[2];

  return len;
}

int board_uart_read(uint8_t *buf, int len) {
#ifdef UART_ID
  return (int)tu_fifo_read_n(&uart_rx_ff, buf, (uint16_t)len);
#else
  (void)buf;
  (void)len;
  return 0;
#endif
}

int board_uart_write(const void *buf, int len) {
#ifdef UART_ID
  const uint8_t *p     = (const uint8_t *)buf;
  int            count = 0;
  while (count < len) {
    if (__HAL_UART_GET_FLAG(&UartHandle, UART_FLAG_TXE)) {
      UartHandle.Instance->TDR = p[count];
      count++;
    } else {
      break;
    }
  }
  return count;
#else
  (void)buf;
  (void)len;
  return -1;
#endif
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  HAL_IncTick();
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}

#elif CFG_TUSB_OS == OPT_OS_THREADX
// Keep HAL_GetTick() working for HAL functions called from board_init()
void osal_threadx_tick_cb(void) {
  HAL_IncTick();
}
#endif

void HardFault_Handler(void) {
  __asm("BKPT #0\n");
}

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void) {
}
