#include <stdio.h>
#include "ch32v20x.h"

#include "bsp/board_api.h"
#include "board.h"

/* CH32v203 depending on variants can support 2 USB IPs: FSDEV and USBFS.
 * By default, we use FSDEV, but you can explicitly select by define:
 * - CFG_TUD_WCH_USBIP_FSDEV
 * - CFG_TUD_WCH_USBIP_USBFS
 */

// USBFS
__attribute__((interrupt)) __attribute__((used))
void USBHD_IRQHandler(void) {
  #if CFG_TUD_WCH_USBIP_USBFS
  tud_int_handler(0);
  #endif
}

__attribute__((interrupt)) __attribute__((used))
void USBHDWakeUp_IRQHandler(void) {
  #if CFG_TUD_WCH_USBIP_USBFS
  tud_int_handler(0);
  #endif
}

// USBD (fsdev)
__attribute__((interrupt)) __attribute__((used))
void USB_LP_CAN1_RX0_IRQHandler(void) {
  #if CFG_TUD_WCH_USBIP_FSDEV
  tud_int_handler(0);
  #endif
}

__attribute__((interrupt)) __attribute__((used))
void USB_HP_CAN1_TX_IRQHandler(void) {
  #if CFG_TUD_WCH_USBIP_FSDEV
  tud_int_handler(0);
  #endif

}

__attribute__((interrupt)) __attribute__((used))
void USBWakeUp_IRQHandler(void) {
  #if CFG_TUD_WCH_USBIP_FSDEV
  tud_int_handler(0);
  #endif
}


#if CFG_TUSB_OS == OPT_OS_NONE

volatile uint32_t system_ticks = 0;

__attribute__((interrupt))
void SysTick_Handler(void) {
  SysTick->SR = 0;
  system_ticks++;
}

uint32_t SysTick_Config(uint32_t ticks) {
  NVIC_EnableIRQ(SysTicK_IRQn);
  SysTick->CTLR = 0;
  SysTick->SR = 0;
  SysTick->CNT = 0;
  SysTick->CMP = ticks - 1;
  SysTick->CTLR = 0xF;
  return 0;
}

uint32_t board_millis(void) {
  return system_ticks;
}

#endif

void board_init(void) {
  __disable_irq();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / 1000);
#endif

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

  uint8_t usb_div;
  switch (SystemCoreClock) {
    case 48000000: usb_div = RCC_USBCLKSource_PLLCLK_Div1; break;
    case 96000000: usb_div = RCC_USBCLKSource_PLLCLK_Div2; break;
    case 144000000: usb_div = RCC_USBCLKSource_PLLCLK_Div3; break;
    default: TU_ASSERT(0,); break;
  }
  RCC_USBCLKConfig(usb_div);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);  // FSDEV
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_OTG_FS, ENABLE); // USB FS

  GPIO_InitTypeDef GPIO_InitStructure = {
    .GPIO_Pin = LED_PIN,
    .GPIO_Mode = GPIO_Mode_Out_OD,
    .GPIO_Speed = GPIO_Speed_50MHz,
  };
  GPIO_Init(LED_PORT, &GPIO_InitStructure);

  // UART TX is PA9
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  GPIO_InitTypeDef usart_init = {
    .GPIO_Pin = GPIO_Pin_9,
    .GPIO_Speed = GPIO_Speed_50MHz,
    .GPIO_Mode = GPIO_Mode_AF_PP,
  };
  GPIO_Init(GPIOA, &usart_init);

  USART_InitTypeDef usart = {
    .USART_BaudRate = 115200,
    .USART_WordLength = USART_WordLength_8b,
    .USART_StopBits = USART_StopBits_1,
    .USART_Parity = USART_Parity_No,
    .USART_Mode = USART_Mode_Tx,
    .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
  };
  USART_Init(USART1, &usart);
  USART_Cmd(USART1, ENABLE);

  __enable_irq();
  board_delay(2);
}

void board_led_write(bool state) {
  GPIO_WriteBit(LED_PORT, LED_PIN, state);
}

uint32_t board_button_read(void) {
  return false;
}

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const *buf, int len) {
  const char *bufc = (const char *) buf;
  for (int i = 0; i < len; i++) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
    USART_SendData(USART1, *bufc++);
  }

  return len;
}
