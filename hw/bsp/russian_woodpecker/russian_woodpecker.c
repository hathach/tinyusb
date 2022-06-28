/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, hathach (tinyusb.org)
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

#include "../board.h"

#include <sam.h>

#include <stdint.h>
#include <stdbool.h>

#define CONF_CPU_FREQUENCY 96000000

#define LED_R_PIN 1U
#define LED_R_PORT PIOA
#define LED_G_PIN 5U
#define LED_G_PORT PIOB
#define LED_B_PIN 30U
#define LED_B_PORT PIOA

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

//------------- IMPLEMENTATION -------------//
void board_init(void)
{
	/* wait states 96MHz */
	EFC0->EEFC_FMR = (EFC0->EEFC_FMR & ~EEFC_FMR_FWS_Msk) | (3 << EEFC_FMR_FWS_Pos);
#ifdef _SAM3U_EFC1_INSTANCE_
	EFC1->EEFC_FMR = (EFC0->EEFC_FMR & ~EEFC_FMR_FWS_Msk) | (3 << EEFC_FMR_FWS_Pos);
#endif

	/* enable external xtal */
	PMC->CKGR_MOR = (PMC->CKGR_MOR & ~(CKGR_MOR_MOSCXTST_Msk | CKGR_MOR_MOSCXTBY)) | CKGR_MOR_KEY_PASSWD |
					CKGR_MOR_MOSCXTEN | CKGR_MOR_MOSCXTST(0xFF);
	while (!(PMC->PMC_SR & PMC_SR_MOSCXTS))
		continue;

	/* select as mainck */
	PMC->CKGR_MOR = (PMC->CKGR_MOR & ~CKGR_MOR_MOSCSEL) | CKGR_MOR_KEY_PASSWD | CKGR_MOR_MOSCSEL;
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~(PMC_MCKR_PRES_Msk | PMC_MCKR_CSS_Msk)) | PMC_MCKR_CSS_MAIN_CLK |
					PMC_MCKR_PRES(0);
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY))
		continue;

	/* plla setup, 12MHz x 8 = 96MHz */
	PMC->CKGR_PLLAR = CKGR_PLLAR_ONE | CKGR_PLLAR_MULA(7) | CKGR_PLLAR_DIVA(1) | CKGR_PLLAR_PLLACOUNT(63);
	while (!(PMC->PMC_SR & PMC_SR_LOCKA))
		continue;

	/* upll config */
	PMC->CKGR_UCKR = CKGR_UCKR_UPLLCOUNT(255) | CKGR_UCKR_UPLLEN;
	while (!(PMC->PMC_SR & PMC_SR_LOCKU))
		continue;

	/* plla as mck */
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~(PMC_MCKR_PRES_Msk | PMC_MCKR_CSS_Msk)) | PMC_MCKR_CSS_PLLA_CLK |
					PMC_MCKR_PRES(0);

	/* Disable Watchdog */
	WDT->WDT_MR |= WDT_MR_WDDIS;

	/* LED */
	PMC->PMC_PCER0 = 1 << ID_PIOA;
	PMC->PMC_PCER0 = 1 << ID_PIOB;
	LED_R_PORT->PIO_PER = (1 << LED_R_PIN);
	LED_R_PORT->PIO_OER = (1 << LED_R_PIN);
	LED_G_PORT->PIO_PER = (1 << LED_G_PIN);
	LED_G_PORT->PIO_OER = (1 << LED_G_PIN);
	LED_B_PORT->PIO_PER = (1 << LED_B_PIN);
	LED_B_PORT->PIO_OER = (1 << LED_B_PIN);

	// Button
	//   _pmc_enable_periph_clock(ID_PIOA);
	//   gpio_set_pin_direction(BUTTON_PIN, GPIO_DIRECTION_IN);
	//   gpio_set_pin_pull_mode(BUTTON_PIN, GPIO_PULL_UP);
	//   gpio_set_pin_function(BUTTON_PIN, GPIO_PIN_FUNCTION_OFF);

	// Uart via EDBG Com
	//   _pmc_enable_periph_clock(ID_USART1);
	//   gpio_set_pin_function(UART_RX_PIN, MUX_PA21A_USART1_RXD1);
	//   gpio_set_pin_function(UART_TX_PIN, MUX_PB4D_USART1_TXD1);

	//   usart_async_init(&edbg_com, USART1, edbg_com_buffer, sizeof(edbg_com_buffer), _usart_get_usart_async());
	//   usart_async_set_baud_rate(&edbg_com, CFG_BOARD_UART_BAUDRATE);
	//   usart_async_register_callback(&edbg_com, USART_ASYNC_TXC_CB, tx_cb_EDBG_COM);
	//   usart_async_enable(&edbg_com);

#if CFG_TUSB_OS == OPT_OS_NONE
	// 1ms tick timer
	SysTick_Config(96000000 / 1000);
#endif

	// Enable USB clock
	PMC->PMC_PCER0 = 1 << ID_UDPHS;
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void UDPHS_Handler(void)
{
	tud_int_handler(0);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
	static uint8_t curr_led;

	if (state)
	{
		switch (curr_led)
		{
		case 0:
			LED_R_PORT->PIO_CODR = (1 << LED_R_PIN);
			LED_G_PORT->PIO_SODR = (1 << LED_G_PIN);
			LED_B_PORT->PIO_SODR = (1 << LED_B_PIN);
			break;

		case 1:
			LED_R_PORT->PIO_SODR = (1 << LED_R_PIN);
			LED_G_PORT->PIO_CODR = (1 << LED_G_PIN);
			LED_B_PORT->PIO_SODR = (1 << LED_B_PIN);
			break;

		case 2:
			LED_R_PORT->PIO_SODR = (1 << LED_R_PIN);
			LED_G_PORT->PIO_SODR = (1 << LED_G_PIN);
			LED_B_PORT->PIO_CODR = (1 << LED_B_PIN);
			break;
		}
	}
	else
	{
		LED_R_PORT->PIO_SODR = (1 << LED_R_PIN);
		LED_G_PORT->PIO_SODR = (1 << LED_G_PIN);
		LED_B_PORT->PIO_SODR = (1 << LED_B_PIN);
	}

	curr_led++;
	curr_led %= 3;
}

uint32_t board_button_read(void)
{
	return 0;
	//   return BUTTON_STATE_ACTIVE == gpio_get_pin_level(BUTTON_PIN);
}

int board_uart_read(uint8_t *buf, int len)
{
	(void)buf;
	(void)len;
	return 0;
}

int board_uart_write(void const *buf, int len)
{
	(void)buf;
	(void)len;
	// while until previous transfer is complete
	//   while(uart_busy) {}
	//   uart_busy = true;

	//   io_write(&edbg_com.io, buf, len);
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

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void)
{
}
