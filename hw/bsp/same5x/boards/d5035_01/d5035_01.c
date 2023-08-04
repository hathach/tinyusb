/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Jean Gressmann <jean@0x42.de>
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
#include "bsp/board_api.h"

#include <hal/include/hal_gpio.h>

#if CONF_CPU_FREQUENCY != 80000000
#	error "CONF_CPU_FREQUENCY" must 80000000
#endif

#if CONF_GCLK_USB_FREQUENCY != 48000000
#	error "CONF_GCLK_USB_FREQUENCY" must 48000000
#endif

#if !defined(HWREV)
#	error Define "HWREV"
#endif

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_0_Handler (void)
{
	tud_int_handler(0);
}

void USB_1_Handler (void)
{
	tud_int_handler(0);
}

void USB_2_Handler (void)
{
	tud_int_handler(0);
}

void USB_3_Handler (void)
{
	tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
#define LED_PIN PIN_PA02

#if HWREV < 3
# define BOARD_SERCOM SERCOM5
#else
# define BOARD_SERCOM SERCOM0
#endif

static inline void init_clock(void)
{
	/* AUTOWS is enabled by default in REG_NVMCTRL_CTRLA - no need to change the number of wait states when changing the core clock */
#if HWREV == 1
	/* configure XOSC1 for a 16MHz crystal connected to XIN1/XOUT1 */
	OSCCTRL->XOSCCTRL[1].reg =
		OSCCTRL_XOSCCTRL_STARTUP(6) |    // 1,953 ms
		OSCCTRL_XOSCCTRL_RUNSTDBY |
		OSCCTRL_XOSCCTRL_ENALC |
		OSCCTRL_XOSCCTRL_IMULT(4) |
		OSCCTRL_XOSCCTRL_IPTAT(3) |
		OSCCTRL_XOSCCTRL_XTALEN |
		OSCCTRL_XOSCCTRL_ENABLE;
	while(0 == OSCCTRL->STATUS.bit.XOSCRDY1);

	OSCCTRL->Dpll[0].DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_DIV(3) | OSCCTRL_DPLLCTRLB_REFCLK(OSCCTRL_DPLLCTRLB_REFCLK_XOSC1_Val); /* pre-scaler = 8, input = XOSC1 */
	OSCCTRL->Dpll[0].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDRFRAC(0x0) | OSCCTRL_DPLLRATIO_LDR(39); /* multiply by 40 -> 80 MHz */
	OSCCTRL->Dpll[0].DPLLCTRLA.reg = OSCCTRL_DPLLCTRLA_RUNSTDBY | OSCCTRL_DPLLCTRLA_ENABLE;
	while(0 == OSCCTRL->Dpll[0].DPLLSTATUS.bit.CLKRDY); /* wait for the PLL0 to be ready */

	OSCCTRL->Dpll[1].DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_DIV(7) | OSCCTRL_DPLLCTRLB_REFCLK(OSCCTRL_DPLLCTRLB_REFCLK_XOSC1_Val); /* pre-scaler = 16, input = XOSC1 */
	OSCCTRL->Dpll[1].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDRFRAC(0x0) | OSCCTRL_DPLLRATIO_LDR(47); /* multiply by 48 -> 48 MHz */
	OSCCTRL->Dpll[1].DPLLCTRLA.reg = OSCCTRL_DPLLCTRLA_RUNSTDBY | OSCCTRL_DPLLCTRLA_ENABLE;
	while(0 == OSCCTRL->Dpll[1].DPLLSTATUS.bit.CLKRDY); /* wait for the PLL1 to be ready */
#else // HWREV >= 1
	/* configure XOSC0 for a 16MHz crystal connected to XIN0/XOUT0 */
	OSCCTRL->XOSCCTRL[0].reg =
		OSCCTRL_XOSCCTRL_STARTUP(6) |    // 1,953 ms
		OSCCTRL_XOSCCTRL_RUNSTDBY |
		OSCCTRL_XOSCCTRL_ENALC |
		OSCCTRL_XOSCCTRL_IMULT(4) |
		OSCCTRL_XOSCCTRL_IPTAT(3) |
		OSCCTRL_XOSCCTRL_XTALEN |
		OSCCTRL_XOSCCTRL_ENABLE;
	while(0 == OSCCTRL->STATUS.bit.XOSCRDY0);

	OSCCTRL->Dpll[0].DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_DIV(3) | OSCCTRL_DPLLCTRLB_REFCLK(OSCCTRL_DPLLCTRLB_REFCLK_XOSC0_Val); /* pre-scaler = 8, input = XOSC1 */
	OSCCTRL->Dpll[0].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDRFRAC(0x0) | OSCCTRL_DPLLRATIO_LDR(39); /* multiply by 40 -> 80 MHz */
	OSCCTRL->Dpll[0].DPLLCTRLA.reg = OSCCTRL_DPLLCTRLA_RUNSTDBY | OSCCTRL_DPLLCTRLA_ENABLE;
	while(0 == OSCCTRL->Dpll[0].DPLLSTATUS.bit.CLKRDY); /* wait for the PLL0 to be ready */

	OSCCTRL->Dpll[1].DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_DIV(7) | OSCCTRL_DPLLCTRLB_REFCLK(OSCCTRL_DPLLCTRLB_REFCLK_XOSC0_Val); /* pre-scaler = 16, input = XOSC1 */
	OSCCTRL->Dpll[1].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDRFRAC(0x0) | OSCCTRL_DPLLRATIO_LDR(47); /* multiply by 48 -> 48 MHz */
	OSCCTRL->Dpll[1].DPLLCTRLA.reg = OSCCTRL_DPLLCTRLA_RUNSTDBY | OSCCTRL_DPLLCTRLA_ENABLE;
	while(0 == OSCCTRL->Dpll[1].DPLLSTATUS.bit.CLKRDY); /* wait for the PLL1 to be ready */
#endif // HWREV

	/* configure clock-generator 0 to use DPLL0 as source -> GCLK0 is used for the core */
	GCLK->GENCTRL[0].reg =
		GCLK_GENCTRL_DIV(0) |
		GCLK_GENCTRL_RUNSTDBY |
		GCLK_GENCTRL_GENEN |
		GCLK_GENCTRL_SRC_DPLL0 |  /* DPLL0 */
		GCLK_GENCTRL_IDC ;
	while(1 == GCLK->SYNCBUSY.bit.GENCTRL0); /* wait for the synchronization between clock domains to be complete */

	/* configure clock-generator 1 to use DPLL1 as source -> for use with some peripheral */
	GCLK->GENCTRL[1].reg =
		GCLK_GENCTRL_DIV(0) |
		GCLK_GENCTRL_RUNSTDBY |
		GCLK_GENCTRL_GENEN |
		GCLK_GENCTRL_SRC_DPLL1 |
		GCLK_GENCTRL_IDC ;
	while(1 == GCLK->SYNCBUSY.bit.GENCTRL1); /* wait for the synchronization between clock domains to be complete */

	/* configure clock-generator 2 to use DPLL0 as source -> for use with SERCOM */
	GCLK->GENCTRL[2].reg =
		GCLK_GENCTRL_DIV(1) |	/* 80MHz */
		GCLK_GENCTRL_RUNSTDBY |
		GCLK_GENCTRL_GENEN |
		GCLK_GENCTRL_SRC_DPLL0 |
		GCLK_GENCTRL_IDC ;
	while(1 == GCLK->SYNCBUSY.bit.GENCTRL2); /* wait for the synchronization between clock domains to be complete */
}

static inline void uart_init(void)
{
#if HWREV < 3
	/* configure SERCOM5 on PB02 */
	PORT->Group[1].WRCONFIG.reg =
		PORT_WRCONFIG_WRPINCFG |
		PORT_WRCONFIG_WRPMUX |
		PORT_WRCONFIG_PMUX(3) |    /* function D */
		PORT_WRCONFIG_DRVSTR |
		PORT_WRCONFIG_PINMASK(0x0004) | /* PB02 */
		PORT_WRCONFIG_PMUXEN;

	MCLK->APBDMASK.bit.SERCOM5_ = 1;
	GCLK->PCHCTRL[SERCOM5_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK2 | GCLK_PCHCTRL_CHEN; /* setup SERCOM to use GLCK2 -> 80MHz */

	SERCOM5->USART.CTRLA.reg = 0x00; /* disable SERCOM -> enable config */
	while(SERCOM5->USART.SYNCBUSY.bit.ENABLE);

	SERCOM5->USART.CTRLA.reg  =  /* CMODE = 0 -> async, SAMPA = 0, FORM = 0 -> USART frame, SMPR = 0 -> arithmetic baud rate */
		SERCOM_USART_CTRLA_SAMPR(1) | /* 0 = 16x / arithmetic baud rate, 1 = 16x / fractional baud rate */
//    SERCOM_USART_CTRLA_FORM(0) | /* 0 = USART Frame, 2 = LIN Master */
		SERCOM_USART_CTRLA_DORD | /* LSB first */
		SERCOM_USART_CTRLA_MODE(1) | /* 0 = Asynchronous, 1 = USART with internal clock */
		SERCOM_USART_CTRLA_RXPO(1) | /* SERCOM PAD[1] is used for data reception */
		SERCOM_USART_CTRLA_TXPO(0); /* SERCOM PAD[0] is used for data transmission */

	SERCOM5->USART.CTRLB.reg = /* RXEM = 0 -> receiver disabled, LINCMD = 0 -> normal USART transmission, SFDE = 0 -> start-of-frame detection disabled, SBMODE = 0 -> one stop bit, CHSIZE = 0 -> 8 bits */
		SERCOM_USART_CTRLB_TXEN; /* transmitter enabled */
	SERCOM5->USART.CTRLC.reg = 0x00;
	// 21.701388889 @ baud rate of 230400 bit/s, table 33-2, p 918 of DS60001507E
	SERCOM5->USART.BAUD.reg = SERCOM_USART_BAUD_FRAC_FP(7) | SERCOM_USART_BAUD_FRAC_BAUD(21);

//  SERCOM5->USART.INTENSET.reg = SERCOM_USART_INTENSET_TXC;
	SERCOM5->SPI.CTRLA.bit.ENABLE = 1; /* activate SERCOM */
	while(SERCOM5->USART.SYNCBUSY.bit.ENABLE); /* wait for SERCOM to be ready */
#else
/* configure SERCOM0 on PA08 */
	PORT->Group[0].WRCONFIG.reg =
		PORT_WRCONFIG_WRPINCFG |
		PORT_WRCONFIG_WRPMUX |
		PORT_WRCONFIG_PMUX(2) |    /* function C */
		PORT_WRCONFIG_DRVSTR |
		PORT_WRCONFIG_PINMASK(0x0100) | /* PA08 */
		PORT_WRCONFIG_PMUXEN;

	MCLK->APBAMASK.bit.SERCOM0_ = 1;
	GCLK->PCHCTRL[SERCOM0_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK2 | GCLK_PCHCTRL_CHEN; /* setup SERCOM to use GLCK2 -> 80MHz */

	SERCOM0->USART.CTRLA.reg = 0x00; /* disable SERCOM -> enable config */
	while(SERCOM0->USART.SYNCBUSY.bit.ENABLE);

	SERCOM0->USART.CTRLA.reg  =  /* CMODE = 0 -> async, SAMPA = 0, FORM = 0 -> USART frame, SMPR = 0 -> arithmetic baud rate */
		SERCOM_USART_CTRLA_SAMPR(1) | /* 0 = 16x / arithmetic baud rate, 1 = 16x / fractional baud rate */
//    SERCOM_USART_CTRLA_FORM(0) | /* 0 = USART Frame, 2 = LIN Master */
		SERCOM_USART_CTRLA_DORD | /* LSB first */
		SERCOM_USART_CTRLA_MODE(1) | /* 0 = Asynchronous, 1 = USART with internal clock */
		SERCOM_USART_CTRLA_RXPO(1) | /* SERCOM PAD[1] is used for data reception */
		SERCOM_USART_CTRLA_TXPO(0); /* SERCOM PAD[0] is used for data transmission */

	SERCOM0->USART.CTRLB.reg = /* RXEM = 0 -> receiver disabled, LINCMD = 0 -> normal USART transmission, SFDE = 0 -> start-of-frame detection disabled, SBMODE = 0 -> one stop bit, CHSIZE = 0 -> 8 bits */
		SERCOM_USART_CTRLB_TXEN; /* transmitter enabled */
	SERCOM0->USART.CTRLC.reg = 0x00;
	// 21.701388889 @ baud rate of 230400 bit/s, table 33-2, p 918 of DS60001507E
	SERCOM0->USART.BAUD.reg = SERCOM_USART_BAUD_FRAC_FP(7) | SERCOM_USART_BAUD_FRAC_BAUD(21);

//  SERCOM0->USART.INTENSET.reg = SERCOM_USART_INTENSET_TXC;
	SERCOM0->SPI.CTRLA.bit.ENABLE = 1; /* activate SERCOM */
	while(SERCOM0->USART.SYNCBUSY.bit.ENABLE); /* wait for SERCOM to be ready */
#endif
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

	// Led init
	gpio_set_pin_direction(LED_PIN, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(LED_PIN, 0);

#if CFG_TUSB_DEBUG >= 2
	uart_send_str(BOARD_NAME " LED pin configured\n");
#endif

#if CFG_TUSB_OS == OPT_OS_FREERTOS
	// If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
	NVIC_SetPriority(USB_0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_SetPriority(USB_1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_SetPriority(USB_2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_SetPriority(USB_3_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif


#if CFG_TUD_ENABLED
#if CFG_TUSB_DEBUG >= 2
	uart_send_str(BOARD_NAME " USB device enabled\n");
#endif

	/* USB clock init
	 * The USB module requires a GCLK_USB of 48 MHz ~ 0.25% clock
	 * for low speed and full speed operation. */
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
	gpio_set_pin_level(LED_PIN, state);
}

uint32_t board_button_read(void)
{
	// this board has no button
	return 0;
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
