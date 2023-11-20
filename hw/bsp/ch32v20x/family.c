#include <stdio.h>
#include "ch32v20x.h"
#include "bsp/board_api.h"
#include "board.h"

__attribute__((interrupt))
void USBHD_IRQHandler(void) {
    tud_int_handler(0);
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
    SysTick->SR   = 0;
    SysTick->CNT  = 0;
    SysTick->CMP  = ticks-1;
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

    switch (SystemCoreClock) {
        case 48000000:  RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1); break;
        case 96000000:  RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div2); break;
        case 144000000: RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div3); break;
        default: TU_ASSERT(0,); break;
    }
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_OTG_FS, ENABLE);

    LED_CLOCK_EN();
    GPIO_InitTypeDef GPIO_InitStructure = {
        .GPIO_Pin   = LED_PIN,
        .GPIO_Mode  = GPIO_Mode_Out_OD,
        .GPIO_Speed = GPIO_Speed_50MHz,
    };
    GPIO_Init(LED_PORT, &GPIO_InitStructure);

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
    (void) buf;
    (void) len;
    return len;
}
