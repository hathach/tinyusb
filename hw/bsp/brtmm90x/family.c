/* 
 * The MIT License (MIT)
 *
 * Copyright 2021 Bridgetek Pte Ltd
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

#include "bsp/board.h"
#include "board.h"

#include <registers/ft900_registers.h>
#include <ft900.h>

#if CFG_TUD_ENABLED
int8_t board_ft90x_vbus(void); // Board specific implementation of VBUS detection for USB device.
extern void ft90x_usbd_pm_ISR(uint16_t pmcfg); // Interrupt handler for USB device power management
#endif

#ifdef GPIO_REMOTE_WAKEUP
void gpio_ISR(void);
#endif
void timer_ISR(void);
volatile unsigned int timer_ms = 0;
void board_pm_ISR(void);

#define WELCOME_MSG "\x1B[2J\x1B[H" \
                    "MM900EVxB board\r\n"

// Initialize on-board peripherals : led, button, uart and USB
void board_init(void)
{
	sys_reset_all();
    // Enable the UART Device.
    sys_enable(sys_device_uart0);
    // Set UART0 GPIO functions to UART0_TXD and UART0_RXD.
    gpio_function(GPIO_UART0_TX, pad_uart0_txd); /* UART0 TXD */
    gpio_function(GPIO_UART0_RX, pad_uart0_rxd); /* UART0 RXD */
    uart_open(UART0,                             /* Device */
              1,                                 /* Prescaler = 1 */
              UART_DIVIDER_19200_BAUD,           /* Divider = 1302 */
              uart_data_bits_8,                  /* No. Data Bits */
              uart_parity_none,                  /* Parity */
              uart_stop_bits_1);                 /* No. Stop Bits */
	// Print out a welcome message.
    // Use sizeof to avoid pulling in strlen unnecessarily.
    board_uart_write(WELCOME_MSG, sizeof(WELCOME_MSG));

#if 0
    // Ethernet LEDs
    gpio_function(GPIO_ETH_LED0, pad_gpio4); /* ETH LED0 */
    gpio_dir(GPIO_ETH_LED0, pad_dir_open_drain);
    gpio_function(GPIO_ETH_LED1, pad_gpio5); /* ETH LED1 */
    gpio_dir(GPIO_ETH_LED1, pad_dir_output);
#endif

	sys_enable(sys_device_timer_wdt);
	/* Timer A = 1ms */
	timer_prescaler(timer_select_a, 1000);
	timer_init(timer_select_a, 100, timer_direction_down, timer_prescaler_select_on, timer_mode_continuous);
	timer_enable_interrupt(timer_select_a);
	timer_start(timer_select_a);
	interrupt_attach(interrupt_timers, (int8_t)interrupt_timers, timer_ISR);

    // Setup VBUS detect GPIO. If the device is connected then this
    // will set the MASK_SYS_PMCFG_DEV_DETECT_EN bit in PMCFG.
    gpio_interrupt_disable(USBD_VBUS_DTC_PIN);
    gpio_function(USBD_VBUS_DTC_PIN, pad_vbus_dtc);
    gpio_pull(USBD_VBUS_DTC_PIN, pad_pull_pulldown);
    gpio_dir(USBD_VBUS_DTC_PIN, pad_dir_input);

    interrupt_attach(interrupt_0, (int8_t)interrupt_0, board_pm_ISR);

#ifdef GPIO_REMOTE_WAKEUP
    //Configuring GPIO pin to wakeup.
    // Set up the wakeup pin.
    gpio_dir(GPIO_REMOTE_WAKEUP_PIN, pad_dir_input);
    gpio_pull(GPIO_REMOTE_WAKEUP_PIN, pad_pull_pullup);

    // Attach an interrupt handler.
    interrupt_attach(interrupt_gpio, (uint8_t)interrupt_gpio, gpio_ISR);
    gpio_interrupt_enable(GPIO_REMOTE_WAKEUP_PIN, gpio_int_edge_falling);
#endif

	uart_disable_interrupt(UART0, uart_interrupt_tx);
	uart_disable_interrupt(UART0, uart_interrupt_rx);

    // Enable all peripheral interrupts.
    interrupt_enable_globally();

    TU_LOG1("MM900EV1B board setup complete\r\n");
};

void timer_ISR(void)
{
    if (timer_is_interrupted(timer_select_a))
    {
        timer_ms++;
    }
}

#ifdef GPIO_REMOTE_WAKEUP
void gpio_ISR(void)
{
    if (gpio_is_interrupted(GPIO_REMOTE_WAKEUP_PIN))
    {
    }
}
#endif

/* Power management ISR */
void board_pm_ISR(void)
{
    uint16_t pmcfg = SYS->PMCFG_H;

#if defined(__FT930__)
    if (pmcfg & MASK_SYS_PMCFG_SLAVE_PERI_IRQ_PEND)
    {
        // Clear d2xx hw engine wakeup.
        SYS->PMCFG_H = MASK_SYS_PMCFG_SLAVE_PERI_IRQ_PEND;
    }
#endif
    if (pmcfg & MASK_SYS_PMCFG_PM_GPIO_IRQ_PEND)
    {
        // Clear GPIO wakeup pending.
        SYS->PMCFG_H = MASK_SYS_PMCFG_PM_GPIO_IRQ_PEND;
    }

#if defined(__FT900__)
    // USB device power management interrupts.
    if (pmcfg & (MASK_SYS_PMCFG_DEV_CONN_DEV |
              MASK_SYS_PMCFG_DEV_DIS_DEV |
              MASK_SYS_PMCFG_HOST_RST_DEV |
              MASK_SYS_PMCFG_HOST_RESUME_DEV)
    )
    {
#if CFG_TUD_ENABLED
        ft90x_usbd_pm_ISR(pmcfg);
#endif
    }
#endif
}

#if CFG_TUD_ENABLED
int8_t board_ft90x_vbus(void)
{
	return gpio_read(USBD_VBUS_DTC_PIN);
}
#endif

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

// Turn LED on or off
void board_led_write(bool state)
{
    gpio_write(GPIO_ETH_LED0, state);
}

// Get the current state of button
// a '1' means active (pressed), a '0' means inactive.
uint32_t board_button_read(void)
{
    return 0;
}

// Get characters from UART
int board_uart_read(uint8_t *buf, int len)
{
    int r = uart_readn(UART0, (uint8_t *)buf, len);

    return r;
}

// Send characters to UART
int board_uart_write(void const *buf, int len)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual" // uart_writen does not have const for buffer parameter.
    int r = uart_writen(UART0, (uint8_t *)((const void *)buf), len);
#pragma GCC diagnostic pop

    return r;
}

// Get current milliseconds
uint32_t board_millis(void)
{
    uint32_t safe_ms;
    
    CRITICAL_SECTION_BEGIN
    safe_ms = timer_ms;
    CRITICAL_SECTION_END

    return safe_ms;
}
