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

#include "sam.h"
#include "bsp/board_api.h"
#include "board.h"

// Suppress warning caused by mcu driver
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#include "hal/include/hal_gpio.h"
#include "hal/include/hal_init.h"
#include "hpl/gclk/hpl_gclk_base.h"
#include "hpl_mclk_config.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

/* Referenced GCLKs, should be initialized firstly */
#define _GCLK_INIT_1ST 0xFFFFFFFF

/* Not referenced GCLKs, initialized last */
#define _GCLK_INIT_LAST (~_GCLK_INIT_1ST)

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_0_Handler(void) {
  tud_int_handler(0);
}

void USB_1_Handler(void) {
  tud_int_handler(0);
}

void USB_2_Handler(void) {
  tud_int_handler(0);
}

void USB_3_Handler(void) {
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

#if CFG_TUH_ENABLED && defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421
void max3421_init(void)
{
  //------------- SPI Init -------------//

  // Enable the APB clock for SERCOM2
  MCLK->APBBMASK.reg |= MCLK_APBBMASK_SERCOM2;

  // Configure GCLK for SERCOM2, initClockNVIC()
  GCLK->PCHCTRL[SERCOM2_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);
  GCLK->PCHCTRL[SERCOM2_GCLK_ID_SLOW].reg = GCLK_PCHCTRL_GEN_GCLK3_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);

  // Disable the SPI module
  SERCOM2->SPI.CTRLA.bit.ENABLE = 0;

  // Reset the SPI module
  SERCOM2->SPI.CTRLA.bit.SWRST = 1;
  while (SERCOM2->SPI.SYNCBUSY.bit.SWRST);

  // Set up SPI in master mode, MSB first, SPI mode 0
  uint8_t const mosi_pad = 0;
  uint8_t const miso_pad = 2;
  SERCOM2->SPI.CTRLA.reg = SERCOM_SPI_CTRLA_MODE(3) | SERCOM_SPI_CTRLA_DOPO(mosi_pad) | SERCOM_SPI_CTRLA_DIPO(miso_pad);

  SERCOM2->SPI.CTRLB.reg = SERCOM_SPI_CTRLB_CHSIZE(0) | SERCOM_SPI_CTRLB_RXEN;
  while( SERCOM2->SPI.SYNCBUSY.bit.CTRLB == 1 );

  // Set the baud rate
  uint32_t baudrate = 4000000u;
  SERCOM2->SPI.BAUD.reg = (uint8_t)(SystemCoreClock / (2 * baudrate) - 1); // Replace 1000000 with your desired baud rate

  // Configure PA12 as MOSI (PAD0), PA13 as SCK (PAD1), PA14 as MISO (PAD2)
  // 2 function C: PIO_SERCOM
  gpio_set_pin_direction(MAX3421E_SCK_PIN, GPIO_DIRECTION_OUT);
  gpio_set_pin_pull_mode(MAX3421E_SCK_PIN, GPIO_PULL_OFF);
  gpio_set_pin_function(MAX3421E_SCK_PIN, 2);

  gpio_set_pin_direction(MAX3421E_MOSI_PIN, GPIO_DIRECTION_OUT);
  gpio_set_pin_pull_mode(MAX3421E_MOSI_PIN, GPIO_PULL_OFF);
  gpio_set_pin_function(MAX3421E_MOSI_PIN, 2);

  gpio_set_pin_direction(MAX3421E_MISO_PIN, GPIO_DIRECTION_IN);
  gpio_set_pin_pull_mode(MAX3421E_MISO_PIN, GPIO_PULL_OFF);
  gpio_set_pin_function(MAX3421E_MISO_PIN, 2);

  // Enable the SPI module
  SERCOM2->SPI.CTRLA.bit.ENABLE = 1;
  while (SERCOM2->SPI.SYNCBUSY.bit.ENABLE);
}
#endif

void board_init(void) {
  // Clock init ( follow hpl_init.c )
  hri_nvmctrl_set_CTRLA_RWS_bf(NVMCTRL, 0);

  _osc32kctrl_init_sources();
  _oscctrl_init_sources();
  _mclk_init();
#if _GCLK_INIT_1ST
  _gclk_init_generators_by_fref(_GCLK_INIT_1ST);
#endif
  _oscctrl_init_referenced_generators();
  _gclk_init_generators_by_fref(_GCLK_INIT_LAST);

  // Update SystemCoreClock since it is hard coded with asf4 and not correct
  // Init 1ms tick timer (samd SystemCoreClock may not correct)
  SystemCoreClock = CONF_CPU_FREQUENCY;
  SysTick_Config(CONF_CPU_FREQUENCY / 1000);

  // Led init
  gpio_set_pin_direction(LED_PIN, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(LED_PIN, 0);

  // Button init
  gpio_set_pin_direction(BUTTON_PIN, GPIO_DIRECTION_IN);
  gpio_set_pin_pull_mode(BUTTON_PIN, GPIO_PULL_UP);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USB_1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USB_2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USB_3_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  /* USB Clock init
   * The USB module requires a GCLK_USB of 48 MHz ~ 0.25% clock
   * for low speed and full speed operation. */
  hri_gclk_write_PCHCTRL_reg(GCLK, USB_GCLK_ID, GCLK_PCHCTRL_GEN_GCLK1_Val | GCLK_PCHCTRL_CHEN);
  hri_mclk_set_AHBMASK_USB_bit(MCLK);
  hri_mclk_set_APBBMASK_USB_bit(MCLK);

  // USB Pin Init
  gpio_set_pin_direction(PIN_PA24, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(PIN_PA24, false);
  gpio_set_pin_pull_mode(PIN_PA24, GPIO_PULL_OFF);
  gpio_set_pin_direction(PIN_PA25, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(PIN_PA25, false);
  gpio_set_pin_pull_mode(PIN_PA25, GPIO_PULL_OFF);

  gpio_set_pin_function(PIN_PA24, PINMUX_PA24H_USB_DM);
  gpio_set_pin_function(PIN_PA25, PINMUX_PA25H_USB_DP);

#if CFG_TUH_ENABLED && defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421
  // CS pin
  gpio_set_pin_direction(MAX3421E_CS_PIN, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(MAX3421E_CS_PIN, 1);

  // SPI
  max3421_init();

  // INT pin with external interrupt
  gpio_set_pin_direction(MAX3241E_INTR_PIN, GPIO_DIRECTION_IN);
  gpio_set_pin_pull_mode(MAX3241E_INTR_PIN, GPIO_PULL_UP);

  // Enable the APB clock for EIC (External Interrupt Controller)
  MCLK->APBAMASK.reg |= MCLK_APBAMASK_EIC;

  // Configure GCLK for EIC
  GCLK->PCHCTRL[EIC_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);

  // Configure PA20 as an input
  PORT->Group[0].DIRCLR.reg = PORT_PA20;
  PORT->Group[0].PINCFG[20].reg = PORT_PINCFG_INEN | PORT_PINCFG_PULLEN;
  PORT->Group[0].OUTSET.reg = PORT_PA20;  // Enable pull-up

  // Configure PA20 to use EIC
  PORT->Group[0].PMUX[10].bit.PMUXE = MUX_PA20A_EIC_EXTINT4;
  PORT->Group[0].PINCFG[20].bit.PMUXEN = 1;

  // Disable EIC
  EIC->CTRLA.bit.ENABLE = 0;
  while (EIC->SYNCBUSY.bit.ENABLE);

  // Configure EXTINT4 (PA20) to trigger on falling edge
  EIC->CONFIG[0].reg |= EIC_CONFIG_SENSE4_FALL;

  // Enable EXTINT4
  EIC->INTENSET.reg = EIC_INTENSET_EXTINT(1 << 4);

  // Enable EIC
  EIC->CTRLA.bit.ENABLE = 1;
  while (EIC->SYNCBUSY.bit.ENABLE);

#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  gpio_set_pin_level(LED_PIN, state);
}

uint32_t board_button_read(void) {
  // button is active low
  return gpio_get_pin_level(BUTTON_PIN) ? 0 : 1;
}

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t board_millis(void) {
  return system_ticks;
}

//--------------------------------------------------------------------+
// API: SPI transfer with MAX3421E, must be implemented by application
//--------------------------------------------------------------------+
#if CFG_TUH_ENABLED && defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421

void EIC_4_Handler(void)
{
  // Clear the interrupt flag
  EIC->INTFLAG.reg = EIC_INTFLAG_EXTINT(1 << 4);

  // Call the TinyUSB interrupt handler
  tuh_int_handler(1);
}

void tuh_max3421e_int_api(uint8_t rhport, bool enabled) {
  (void) rhport;

  if (enabled) {
    NVIC_EnableIRQ(EIC_4_IRQn);
  } else {
    NVIC_DisableIRQ(EIC_4_IRQn);
  }
}

void tuh_max3421_spi_cs_api(uint8_t rhport, bool active) {
  (void) rhport;
  gpio_set_pin_level(MAX3421E_CS_PIN, active ? 0 : 1);
}

bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const *tx_buf, size_t tx_len, uint8_t *rx_buf, size_t rx_len) {
  (void) rhport;

  size_t count = 0;
  while (count < tx_len || count < rx_len) {
    // Wait for the transmit buffer to be empty
    while (!SERCOM2->SPI.INTFLAG.bit.DRE);

    // Write data to be transmitted
    uint8_t data = 0x00;
    if (count < tx_len) {
       data = tx_buf[count];
    }

    SERCOM2->SPI.DATA.reg = (uint32_t) data;

    // Wait for the receive buffer to be filled
    while (!SERCOM2->SPI.INTFLAG.bit.RXC);

    // Read received data
    data = (uint8_t) SERCOM2->SPI.DATA.reg;
    if (count < rx_len) {
      rx_buf[count] = data;
    }

    count++;
  }

  // wait for bus idle and clear flags
  while (!(SERCOM2->SPI.INTFLAG.reg & (SERCOM_SPI_INTFLAG_TXC | SERCOM_SPI_INTFLAG_DRE)));
  SERCOM2->SPI.INTFLAG.reg = SERCOM_SPI_INTFLAG_TXC | SERCOM_SPI_INTFLAG_DRE;

  return true;
}
#endif

#endif
