/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Jean Gressmann <jean@0x42.de>
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
 */

#include <sam.h>
#include "bsp/board.h"

#include <hal/include/hal_gpio.h>


//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_0_Handler(void)
{
	tud_int_handler(0);
}

void USB_1_Handler(void)
{
	tud_int_handler(0);
}

void USB_2_Handler(void)
{
	tud_int_handler(0);
}

void USB_3_Handler(void)
{
	tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
#define LED_PIN PIN_PC18
#define BUTTON_PIN PIN_PB31
#define BOARD_SERCOM SERCOM2
#define BOARD_NAME "Microchip SAM E54 Xplained Pro"

static inline void init_clock(void)
{
	/* configure for a 12MHz crystal connected to XIN1/XOUT1 */
	OSCCTRL->XOSCCTRL[1].reg =
		OSCCTRL_XOSCCTRL_STARTUP(6) | // 1.953 ms
		OSCCTRL_XOSCCTRL_RUNSTDBY |
		OSCCTRL_XOSCCTRL_ENALC |
		OSCCTRL_XOSCCTRL_IMULT(4) | OSCCTRL_XOSCCTRL_IPTAT(3) | // 8MHz to 16MHz
		OSCCTRL_XOSCCTRL_XTALEN |
		OSCCTRL_XOSCCTRL_ENABLE;
	while(0 == OSCCTRL->STATUS.bit.XOSCRDY1);

	OSCCTRL->Dpll[0].DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_DIV(5) | OSCCTRL_DPLLCTRLB_REFCLK_XOSC1; /* 12MHz / 12 = 1Mhz, input = XOSC1 */
	OSCCTRL->Dpll[0].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDRFRAC(0x0) | OSCCTRL_DPLLRATIO_LDR((CONF_CPU_FREQUENCY / 1000000)-1); /* multiply to get CONF_CPU_FREQUENCY (default = 120MHz) */
	OSCCTRL->Dpll[0].DPLLCTRLA.reg = OSCCTRL_DPLLCTRLA_RUNSTDBY | OSCCTRL_DPLLCTRLA_ENABLE;
	while(0 == OSCCTRL->Dpll[0].DPLLSTATUS.bit.CLKRDY); /* wait for the PLL0 to be ready */

	OSCCTRL->Dpll[1].DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_DIV(5) | OSCCTRL_DPLLCTRLB_REFCLK_XOSC1; /* 12MHz / 12 = 1Mhz, input = XOSC1 */
	OSCCTRL->Dpll[1].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDRFRAC(0x0) | OSCCTRL_DPLLRATIO_LDR(47); /* multiply by 48 -> 48 MHz */
	OSCCTRL->Dpll[1].DPLLCTRLA.reg = OSCCTRL_DPLLCTRLA_RUNSTDBY | OSCCTRL_DPLLCTRLA_ENABLE;
	while(0 == OSCCTRL->Dpll[1].DPLLSTATUS.bit.CLKRDY); /* wait for the PLL1 to be ready */

	/* configure clock-generator 0 to use DPLL0 as source -> GCLK0 is used for the core */
	GCLK->GENCTRL[0].reg =
		GCLK_GENCTRL_DIV(0) |
		GCLK_GENCTRL_RUNSTDBY |
		GCLK_GENCTRL_GENEN |
		GCLK_GENCTRL_SRC_DPLL0 |
		GCLK_GENCTRL_IDC;
	while(1 == GCLK->SYNCBUSY.bit.GENCTRL0); /* wait for the synchronization between clock domains to be complete */

	/* configure clock-generator 1 to use DPLL1 as source -> for use with some peripheral */
	GCLK->GENCTRL[1].reg =
		GCLK_GENCTRL_DIV(0) |
		GCLK_GENCTRL_RUNSTDBY |
		GCLK_GENCTRL_GENEN |
		GCLK_GENCTRL_SRC_DPLL1 |
		GCLK_GENCTRL_IDC ;
	while(1 == GCLK->SYNCBUSY.bit.GENCTRL1); /* wait for the synchronization between clock domains to be complete */
}

static inline void uart_init(void)
{
	gpio_set_pin_function(PIN_PB24, PINMUX_PB24D_SERCOM2_PAD1);
	gpio_set_pin_function(PIN_PB25, PINMUX_PB25D_SERCOM2_PAD0);

	MCLK->APBBMASK.bit.SERCOM2_ = 1;
	GCLK->PCHCTRL[SERCOM2_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK1 | GCLK_PCHCTRL_CHEN; /* setup SERCOM to use GLCK1 -> 48MHz */

	BOARD_SERCOM->USART.CTRLA.bit.SWRST = 1; /* reset and disable SERCOM -> enable configuration */
	while (BOARD_SERCOM->USART.SYNCBUSY.bit.SWRST);

	BOARD_SERCOM->USART.CTRLA.reg  =
		SERCOM_USART_CTRLA_SAMPR(0) | /* 0 = 16x / arithmetic baud rate, 1 = 16x / fractional baud rate */
		SERCOM_USART_CTRLA_SAMPA(0) | /* 16x over sampling */
		SERCOM_USART_CTRLA_FORM(0) | /* 0x0 USART frame, 0x1 USART frame with parity, ... */
		SERCOM_USART_CTRLA_DORD | /* LSB first */
		SERCOM_USART_CTRLA_MODE(1) | /* 0x0 USART with external clock, 0x1 USART with internal clock */
		SERCOM_USART_CTRLA_RXPO(1) | /* SERCOM PAD[1] is used for data reception */
		SERCOM_USART_CTRLA_TXPO(0); /* SERCOM PAD[0] is used for data transmission */

	BOARD_SERCOM->USART.CTRLB.reg = /* RXEM = 0 -> receiver disabled, LINCMD = 0 -> normal USART transmission, SFDE = 0 -> start-of-frame detection disabled, SBMODE = 0 -> one stop bit, CHSIZE = 0 -> 8 bits */
		SERCOM_USART_CTRLB_TXEN | /* transmitter enabled */
		SERCOM_USART_CTRLB_RXEN; /* receiver enabled */
	// BOARD_SERCOM->USART.BAUD.reg = SERCOM_USART_BAUD_FRAC_FP(0) | SERCOM_USART_BAUD_FRAC_BAUD(26); /* 48000000/(16*115200) = 26.041666667 */
	BOARD_SERCOM->USART.BAUD.reg = SERCOM_USART_BAUD_BAUD(63019); /* 65536*(1−16*115200/48000000) */

	BOARD_SERCOM->SPI.CTRLA.bit.ENABLE = 1; /* activate SERCOM */
	while (BOARD_SERCOM->USART.SYNCBUSY.bit.ENABLE); /* wait for SERCOM to be ready */
}

static inline void uart_send_buffer(uint8_t const *text, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		BOARD_SERCOM->USART.DATA.reg = text[i];
		while((BOARD_SERCOM->USART.INTFLAG.reg & SERCOM_SPI_INTFLAG_TXC) == 0);
	}
}

static inline void uart_send_str(const char* text)
{
	while (*text) {
		BOARD_SERCOM->USART.DATA.reg = *text++;
		while((BOARD_SERCOM->USART.INTFLAG.reg & SERCOM_SPI_INTFLAG_TXC) == 0);
	}
}


void board_init(void)
{
	init_clock();

	SystemCoreClock = CONF_CPU_FREQUENCY;

#if CFG_TUSB_OS  == OPT_OS_NONE
	SysTick_Config(CONF_CPU_FREQUENCY / 1000);
#endif

	uart_init();

#if CFG_TUSB_DEBUG >= 2
	uart_send_str(BOARD_NAME " UART initialized\n");
	tu_printf(BOARD_NAME " reset cause %#02x\n", RSTC->RCAUSE.reg);
#endif

	// LED0 init
	gpio_set_pin_function(LED_PIN, GPIO_PIN_FUNCTION_OFF);
	gpio_set_pin_direction(LED_PIN, GPIO_DIRECTION_OUT);
	board_led_write(0);

#if CFG_TUSB_DEBUG >= 2
	uart_send_str(BOARD_NAME " LED pin configured\n");
#endif

	// BTN0 init
	gpio_set_pin_function(BUTTON_PIN, GPIO_PIN_FUNCTION_OFF);
	gpio_set_pin_direction(BUTTON_PIN, GPIO_DIRECTION_IN);
	gpio_set_pin_pull_mode(BUTTON_PIN, GPIO_PULL_UP);

#if CFG_TUSB_DEBUG >= 2
	uart_send_str(BOARD_NAME " Button pin configured\n");
#endif

#if CFG_TUSB_OS == OPT_OS_FREERTOS
	// If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
	NVIC_SetPriority(USB_0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_SetPriority(USB_1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_SetPriority(USB_2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_SetPriority(USB_3_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif


#if TUSB_OPT_DEVICE_ENABLED
#if CFG_TUSB_DEBUG >= 2
	uart_send_str(BOARD_NAME " USB device enabled\n");
#endif

	/* USB clock init
	 * The USB module requires a GCLK_USB of 48 MHz ~ 0.25% clock
	 * for low speed and full speed operation.
	 */
	hri_gclk_write_PCHCTRL_reg(GCLK, USB_GCLK_ID, GCLK_PCHCTRL_GEN_GCLK1_Val | GCLK_PCHCTRL_CHEN);
	hri_mclk_set_AHBMASK_USB_bit(MCLK);
	hri_mclk_set_APBBMASK_USB_bit(MCLK);

	// USB pin init
	gpio_set_pin_direction(PIN_PA24, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PIN_PA24, false);
	gpio_set_pin_pull_mode(PIN_PA24, GPIO_PULL_OFF);
	gpio_set_pin_direction(PIN_PA25, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PIN_PA25, false);
	gpio_set_pin_pull_mode(PIN_PA25, GPIO_PULL_OFF);

	gpio_set_pin_function(PIN_PA24, PINMUX_PA24H_USB_DM);
	gpio_set_pin_function(PIN_PA25, PINMUX_PA25H_USB_DP);


#if CFG_TUSB_DEBUG >= 2
	uart_send_str(BOARD_NAME " USB device configured\n");
#endif
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
	gpio_set_pin_level(LED_PIN, !state);
}

uint32_t board_button_read(void)
{
	return (PORT->Group[1].IN.reg & 0x80000000) != 0x80000000;
}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
	if (len < 0) {
		uart_send_str(buf);
	} else {
		uart_send_buffer(buf, len);
	}
	return len;
}

#if CFG_TUSB_OS  == OPT_OS_NONE
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

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void)
{

}
