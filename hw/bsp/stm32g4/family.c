/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include "stm32g4xx_hal.h"
#include "stm32g4xx_ll_bus.h"

#include "bsp/board.h"
#include "board.h"


//--------------------------------------------------------------------+
// USB PD
//--------------------------------------------------------------------+

void usbpd_init(uint8_t port_num, tusb_typec_port_type_t port_type) {
  (void) port_num;

  // Initialization phase: CFG1
  UCPD1->CFG1 = (0x0d << UCPD_CFG1_HBITCLKDIV_Pos) | (0x10 << UCPD_CFG1_IFRGAP_Pos) | (0x07 << UCPD_CFG1_TRANSWIN_Pos) |
      (0x01 << UCPD_CFG1_PSC_UCPDCLK_Pos) | (0x1f << UCPD_CFG1_RXORDSETEN_Pos) |
      ( 0 << UCPD_CFG1_TXDMAEN_Pos) | (0 << UCPD_CFG1_RXDMAEN_Pos);
  UCPD1->CFG1 |= UCPD_CFG1_UCPDEN;

  // General programming sequence (with UCPD configured then enabled)
  if (port_type == TUSB_TYPEC_PORT_SNK) {
    // Enable both CC Phy
    UCPD1->CR = (0x01 << UCPD_CR_ANAMODE_Pos) | (0x03 << UCPD_CR_CCENABLE_Pos);

    // Read Voltage State on CC1 & CC2 fore initial state
    uint32_t vstate_cc[2];
    vstate_cc[0] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC1_Pos) & 0x03;
    vstate_cc[1] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC2_Pos) & 0x03;

    TU_LOG1_INT(vstate_cc[0]);
    TU_LOG1_INT(vstate_cc[1]);

    // Enable CC1 & CC2 Interrupt
    UCPD1->IMR = UCPD_IMR_TYPECEVT1IE | UCPD_IMR_TYPECEVT2IE;
  }

  // Enable interrupt
  //NVIC_SetPriority(UCPD1_IRQn, 0);
//  NVIC_EnableIRQ(UCPD1_IRQn);
}

uint8_t pd_rx_buf[262];
uint32_t pd_rx_count = 0;

void UCPD1_IRQHandler(void) {
  uint32_t sr = UCPD1->SR;
  sr &= UCPD1->IMR;

  TU_LOG1("UCPD1_IRQHandler: sr = 0x%08X\n", sr);

  if (sr & (UCPD_SR_TYPECEVT1 | UCPD_SR_TYPECEVT2)) {
    uint32_t vstate_cc[2];
    vstate_cc[0] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC1_Pos) & 0x03;
    vstate_cc[1] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC2_Pos) & 0x03;

    TU_LOG1("VState CC1 = %u, CC2 = %u\n", vstate_cc[0], vstate_cc[1]);

    uint32_t cr = UCPD1->CR;

    if ((sr & UCPD_SR_TYPECEVT1) && (vstate_cc[0] == 3)) {
      TU_LOG1("Attach CC1\n");
      cr &= ~UCPD_CR_PHYCCSEL;
      cr |= UCPD_CR_PHYRXEN;
    } else if ((sr & UCPD_SR_TYPECEVT2) && (vstate_cc[1] == 3)) {
      TU_LOG1("Attach CC2\n");
      cr |= UCPD_CR_PHYCCSEL;
      cr |= UCPD_CR_PHYRXEN;
    } else {
      TU_LOG1("Detach\n");
      cr &= ~UCPD_CR_PHYRXEN;
    }

    // Enable Interrupt
    UCPD1->IMR |= UCPD_IMR_TXMSGDISCIE | UCPD_IMR_TXMSGSENTIE | UCPD_IMR_TXMSGABTIE | UCPD_IMR_TXUNDIE |
        /*UCPD_IMR_RXNEIE |*/ UCPD_IMR_RXORDDETIE | UCPD_IMR_RXHRSTDETIE | UCPD_IMR_RXOVRIE | UCPD_IMR_RXMSGENDIE |
        UCPD_IMR_HRSTDISCIE | UCPD_IMR_HRSTSENTIE;

    // Enable PD RX
    UCPD1->CR = cr;

    // ack
    UCPD1->ICR = UCPD_ICR_TYPECEVT1CF | UCPD_ICR_TYPECEVT2CF;
  }

  if (sr & UCPD_SR_RXORDDET) {
    // SOP: Start of Packet
    // TODO DMA later
    uint8_t order_set = UCPD1->RX_ORDSET & UCPD_RX_ORDSET_RXORDSET_Msk;
    TU_LOG1_HEX(order_set);

    // ack
    UCPD1->ICR = UCPD_ICR_RXORDDETCF;
  }

  if ( sr & UCPD_SR_RXMSGEND) {
    // End of message
//    uint32_t payload_size = UCPD1->RX_PAYSZ;
//    TU_LOG1_HEX(payload_size);
//
//    for(uint32_t i=0; i<payload_size; i++) {
//      pd_rx_buf[i] = UCPD1->RXDR;
//
//      TU_LOG1("0x%02X ", pd_rx_buf[i]);
//    }
//    TU_LOG1("\n");

    // ack
    UCPD1->ICR = UCPD_ICR_RXMSGENDCF;
  }

//  if (sr & UCPD_SR_RXNE) {
//    uint8_t data = UCPD1->RXDR;
//    pd_rx_buf[pd_rx_count++] = data;
//    TU_LOG1_HEX(data);
//  }

//  else {
//    TU_LOG_LOCATION();
//  }
}

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_HP_IRQHandler(void)
{
  tud_int_handler(0);
}

void USB_LP_IRQHandler(void)
{
  tud_int_handler(0);
}

void USBWakeUp_IRQHandler(void)
{
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
UART_HandleTypeDef UartHandle;

void board_init(void)
{
  HAL_Init();
  board_clock_init();

  // Enable All GPIOs clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  UART_CLK_EN();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR runs before scheduler start
  SysTick->CTRL &= ~1U;

  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_HP_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USB_LP_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBWakeUp_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  GPIO_InitTypeDef GPIO_InitStruct;

  // LED
  memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  board_led_write(false);

  // Button
  memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
  GPIO_InitStruct.Pin = BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = BUTTON_STATE_ACTIVE ? GPIO_PULLDOWN : GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

#ifdef UART_DEV
  // UART
  memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
  GPIO_InitStruct.Pin       = UART_TX_PIN | UART_RX_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = UART_GPIO_AF;
  HAL_GPIO_Init(UART_GPIO_PORT, &GPIO_InitStruct);

  UartHandle = (UART_HandleTypeDef){
    .Instance        = UART_DEV,
    .Init.BaudRate   = CFG_BOARD_UART_BAUDRATE,
    .Init.WordLength = UART_WORDLENGTH_8B,
    .Init.StopBits   = UART_STOPBITS_1,
    .Init.Parity     = UART_PARITY_NONE,
    .Init.HwFlowCtl  = UART_HWCONTROL_NONE,
    .Init.Mode       = UART_MODE_TX_RX,
    .Init.OverSampling = UART_OVERSAMPLING_16
  };
  HAL_UART_Init(&UartHandle);
#endif

  // USB Pins TODO double check USB clock and pin setup
  // Configure USB DM and DP pins. This is optional, and maintained only for user guidance.
  memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
  GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  __HAL_RCC_USB_CLK_ENABLE();

  board_vbus_sense_init();

#if 0
  // USB PD
  /* PWR register access (for disabling dead battery feature) */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC);

  __HAL_RCC_UCPD1_CLK_ENABLE();

  // Default CC1/CC2 is PB4/PB6
  // PB4   ------> UCPD1_CC2
  // PB6   ------> UCPD1_CC1

  usbpd_init(0, TUSB_TYPEC_PORT_SNK);
#endif

}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  GPIO_PinState pin_state = (GPIO_PinState) (state ? LED_STATE_ON : (1-LED_STATE_ON));
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, pin_state);
}

uint32_t board_button_read(void)
{
  return BUTTON_STATE_ACTIVE == HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
#ifdef UART_DEV
  HAL_UART_Transmit(&UartHandle, (uint8_t*)(uintptr_t) buf, len, 0xffff);
  return len;
#else
  (void) buf; (void) len; (void) UartHandle;
  return 0;
#endif
}

#if CFG_TUSB_OS  == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler (void)
{
  HAL_IncTick();
  system_ticks++;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif

void HardFault_Handler (void)
{
  __asm("BKPT #0\n");
}

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void)
{

}
