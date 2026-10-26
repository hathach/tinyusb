/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to do so, subject to the
 * following conditions:
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
   manufacturer: Microchip
*/

#include "bsp/board_api.h"
#include "sam.h"

#include "hal/include/hal_gpio.h"
#include "hal/include/hal_init.h"
#include "hal/include/hal_usart_async.h"
#include "hpl/pmc/hpl_pmc.h"
#include "hpl/usart/hpl_usart_base.h"
#include "peripheral_clk_config.h"

static inline void board_vbus_set(uint8_t rhport, bool state);
void _init(void);
#include "board.h"

#ifndef UART_BUFFER_SIZE
  #define UART_BUFFER_SIZE 64
#endif

#define LED_STATE_OFF (1 - LED_STATE_ON)

static struct usart_async_descriptor edbg_com;
static uint8_t edbg_com_buffer[UART_BUFFER_SIZE];
static volatile bool uart_busy = false;
static void tx_complete_cb(const struct usart_async_descriptor *const io_descr) {
  (void) io_descr;
  uart_busy = false;
}

void board_init(void) {
  init_mcu();

  /* Disable Watchdog */
  hri_wdt_set_MR_WDDIS_bit(WDT);

#if defined(TRACE_ETM)
  // same70_xplained J403 (Cortex Debug+ETM footprint, bottom side) carries
  // 4-bit trace: TRACECLK=PD8 (peripheral D), TRACED0-3=PD4-7 (peripheral C).
#ifdef TRACE_ETM_QUIET_ENET_PHY
  // The trace pins double as the Ethernet PHY's RMII receive lines
  // (PD4=CRS_DV, PD5/6=RXD0/1, PD7=RXER - PHY OUTPUTS): hold the KSZ8081 in
  // reset or it drives against the trace stream. The reset net is a BOARD
  // property (same70_xplained: PHY_RESET=PC10), hence the board.h gate.
  _pmc_enable_periph_clock(ID_PIOC);
  gpio_set_pin_level(GPIO(GPIO_PORTC, 10), false);
  gpio_set_pin_direction(GPIO(GPIO_PORTC, 10), GPIO_DIRECTION_OUT);
  gpio_set_pin_function(GPIO(GPIO_PORTC, 10), GPIO_PIN_FUNCTION_OFF);
#endif

  // The TPIU is clocked from PCK3 (datasheet 16.7.4) - run it from MCK.
  // skip if the debugger already started PCK3 (reprogramming glitches the
  // clock mid-stream and desyncs the decoder)
  uint32_t const pck3 = PMC_PCK_CSS_MCK | PMC_PCK_PRES(1); // MCK/2 = 75 MHz -> 37.5 MHz pin
  if (PMC->PMC_PCK[3] != pck3 || !(PMC->PMC_SR & PMC_SR_PCKRDY3)) {
    PMC->PMC_PCK[3] = pck3;
    PMC->PMC_SCER = PMC_SCER_PCK3;
    while (!(PMC->PMC_SR & PMC_SR_PCKRDY3)) {}
  }
  _pmc_enable_periph_clock(ID_PIOD);
  uint32_t const clk_pin = PIO_PD8D_TPIU_TRACECLK;
  uint32_t const dat_pin = PIO_PD4C_TPIU_TRACED0 | PIO_PD5C_TPIU_TRACED1 |
                           PIO_PD6C_TPIU_TRACED2 | PIO_PD7C_TPIU_TRACED3;
  PIOD->PIO_ABCDSR[0] = (PIOD->PIO_ABCDSR[0] | clk_pin) & ~dat_pin; // D=11, C=01
  PIOD->PIO_ABCDSR[1] |= clk_pin | dat_pin;
  PIOD->PIO_PDR = clk_pin | dat_pin; // hand the pins to the peripheral
#endif

#ifdef LED_PIN
  _pmc_enable_periph_clock(LED_PORT_CLOCK);
  gpio_set_pin_level(LED_PIN, LED_STATE_OFF);
  gpio_set_pin_direction(LED_PIN, GPIO_DIRECTION_OUT);
  gpio_set_pin_function(LED_PIN, GPIO_PIN_FUNCTION_OFF);
#endif

#ifdef BUTTON_PIN
  _pmc_enable_periph_clock(BUTTON_PORT_CLOCK);
  gpio_set_pin_direction(BUTTON_PIN, GPIO_DIRECTION_IN);
  gpio_set_pin_pull_mode(BUTTON_PIN, BUTTON_STATE_ACTIVE ? GPIO_PULL_DOWN : GPIO_PULL_UP);
  gpio_set_pin_function(BUTTON_PIN, GPIO_PIN_FUNCTION_OFF);
#endif

  _pmc_enable_periph_clock(UART_PORT_CLOCK);
  gpio_set_pin_function(UART_RX_PIN, UART_RX_FUNCTION);
  gpio_set_pin_function(UART_TX_PIN, UART_TX_FUNCTION);

  usart_async_init(&edbg_com, BOARD_USART, edbg_com_buffer, sizeof(edbg_com_buffer), _usart_get_usart_async());
  usart_async_set_baud_rate(&edbg_com, CFG_BOARD_UART_BAUDRATE);
  usart_async_register_callback(&edbg_com, USART_ASYNC_TXC_CB, tx_complete_cb);
  usart_async_enable(&edbg_com);

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer (SystemCoreClock may not be correct after init)
  SysTick_Config(CONF_CPU_FREQUENCY / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority((IRQn_Type) ID_USBHS, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  // Enable USB clock
  _pmc_enable_periph_clock(ID_USBHS);

#if CFG_TUH_ENABLED
  board_vbus_set(0, true);
#endif
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USBHS_Handler(void) {
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
#ifdef LED_PIN
  gpio_set_pin_level(LED_PIN, state ? LED_STATE_ON : LED_STATE_OFF);
#else
  (void) state;
#endif
}

uint32_t board_button_read(void) {
#ifdef BUTTON_PIN
  return BUTTON_STATE_ACTIVE == gpio_get_pin_level(BUTTON_PIN);
#else
  return 0;
#endif
}

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return -1;
}

int board_uart_write(void const *buf, int len) {
  while (uart_busy) {}
  uart_busy = true;

  io_write(&edbg_com.io, buf, len);
  return len;
}

// Read 128-bit unique ID via EFC STUI/SPUI commands
// Must run from RAM since STUI remaps flash to the unique ID
__attribute__((noinline)) TU_ATTR_SECTION(.ramfunc) static void read_unique_id(uint32_t uid[4]) {
  // Wait for flash to be ready
  while (!(EFC->EEFC_FSR & EEFC_FSR_FRDY)) {}

  // Issue Start Read Unique Identifier command
  EFC->EEFC_FCR = EEFC_FCR_FKEY_PASSWD | EEFC_FCR_FCMD_STUI;
  while (EFC->EEFC_FSR & EEFC_FSR_FRDY) {}

  // Read 128-bit unique ID from flash base address
  const volatile uint32_t *flash = (const volatile uint32_t *) IFLASH_ADDR;
  for (int i = 0; i < 4; i++) {
    uid[i] = flash[i];
  }

  // Issue Stop Read Unique Identifier command
  EFC->EEFC_FCR = EEFC_FCR_FKEY_PASSWD | EEFC_FCR_FCMD_SPUI;
  while (!(EFC->EEFC_FSR & EEFC_FSR_FRDY)) {}
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  const size_t uid_len = 16;
  if (max_len < uid_len) {
    return 0;
  }
  read_unique_id((uint32_t *)(uintptr_t) id);
  return uid_len;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

void _init(void) {
}
