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

#include "bsp/board.h"

#include "nrfx.h"
#include "nrfx/hal/nrf_gpio.h"
#include "nrfx/drivers/include/nrfx_power.h"
#include "nrfx/drivers/include/nrfx_uarte.h"

#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdm.h"
#include "nrf_soc.h"
#endif

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USBD_IRQHandler(void)
{
  tud_irq_handler(0);
}

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
#define LED_PIN               13
#define LED_STATE_ON          0

#define BUTTON_PIN            11
#define BUTTON_STATE_ACTIVE   0

#define UART_RX_PIN           8
#define UART_TX_PIN           6

static nrfx_uarte_t _uart_id = NRFX_UARTE_INSTANCE(0);
//static void uart_handler(nrfx_uart_event_t const * p_event, void* p_context);

// tinyusb function that handles power event (detected, ready, removed)
// We must call it within SD's SOC event handler, or set it as power event handler if SD is not enabled.
extern void tusb_hal_nrf_power_event(uint32_t event);

void board_init(void)
{
  // stop LF clock just in case we jump from application without reset
  NRF_CLOCK->TASKS_LFCLKSTOP = 1UL;

  // Use Internal OSC to compatible with all boards
  NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_RC;
  NRF_CLOCK->TASKS_LFCLKSTART = 1UL;

  // LED
  nrf_gpio_cfg_output(LED_PIN);
  board_led_write(false);

  // Button
  nrf_gpio_cfg_input(BUTTON_PIN, NRF_GPIO_PIN_PULLUP);

  // 1ms tick timer
  SysTick_Config(SystemCoreClock/1000);

  // UART
  nrfx_uarte_config_t uart_cfg =
  {
    .pseltxd   = UART_TX_PIN,
    .pselrxd   = UART_RX_PIN,
    .pselcts   = NRF_UARTE_PSEL_DISCONNECTED,
    .pselrts   = NRF_UARTE_PSEL_DISCONNECTED,
    .p_context = NULL,
    .baudrate  = NRF_UARTE_BAUDRATE_115200, // CFG_BOARD_UART_BAUDRATE
    .interrupt_priority = 7,
    .hal_cfg = {
      .hwfc      = NRF_UARTE_HWFC_DISABLED,
      .parity    = NRF_UARTE_PARITY_EXCLUDED,
    }
  };

  nrfx_uarte_init(&_uart_id, &uart_cfg, NULL); //uart_handler);

#if TUSB_OPT_DEVICE_ENABLED
  // Priorities 0, 1, 4 (nRF52) are reserved for SoftDevice
  // 2 is highest for application
  NVIC_SetPriority(USBD_IRQn, 2);

  // USB power may already be ready at this time -> no event generated
  // We need to invoke the handler based on the status initially
  uint32_t usb_reg;

#ifdef SOFTDEVICE_PRESENT
  uint8_t sd_en = false;
  sd_softdevice_is_enabled(&sd_en);

  if ( sd_en ) {
    sd_power_usbdetected_enable(true);
    sd_power_usbpwrrdy_enable(true);
    sd_power_usbremoved_enable(true);

    sd_power_usbregstatus_get(&usb_reg);
  }else
#endif
  {
    // Power module init
    const nrfx_power_config_t pwr_cfg = { 0 };
    nrfx_power_init(&pwr_cfg);

    // Register tusb function as USB power handler
    // cause cast-function-type warning
    const nrfx_power_usbevt_config_t config = { .handler = ((nrfx_power_usb_event_handler_t) tusb_hal_nrf_power_event) };
    nrfx_power_usbevt_init(&config);

    nrfx_power_usbevt_enable();

    usb_reg = NRF_POWER->USBREGSTATUS;
  }

  if ( usb_reg & POWER_USBREGSTATUS_VBUSDETECT_Msk ) tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_DETECTED);
  if ( usb_reg & POWER_USBREGSTATUS_OUTPUTRDY_Msk  ) tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_READY);
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  nrf_gpio_pin_write(LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void)
{
  return BUTTON_STATE_ACTIVE == nrf_gpio_pin_read(BUTTON_PIN);
}

//static void uart_handler(nrfx_uart_event_t const * p_event, void* p_context)
//{
//
//}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
//  return NRFX_SUCCESS == nrfx_uart_rx(&_uart_id, buf, (size_t) len) ? len : 0;
}

int board_uart_write(void const * buf, int len)
{
  return (NRFX_SUCCESS == nrfx_uarte_tx(&_uart_id, (uint8_t const*) buf, (size_t) len)) ? len : 0;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif

#ifdef SOFTDEVICE_PRESENT
// process SOC event from SD
uint32_t proc_soc(void)
{
  uint32_t soc_evt;
  uint32_t err = sd_evt_get(&soc_evt);

  if (NRF_SUCCESS == err)
  {
    /*------------- usb power event handler -------------*/
    int32_t usbevt = (soc_evt == NRF_EVT_POWER_USB_DETECTED   ) ? NRFX_POWER_USB_EVT_DETECTED:
                     (soc_evt == NRF_EVT_POWER_USB_POWER_READY) ? NRFX_POWER_USB_EVT_READY   :
                     (soc_evt == NRF_EVT_POWER_USB_REMOVED    ) ? NRFX_POWER_USB_EVT_REMOVED : -1;

    if ( usbevt >= 0) tusb_hal_nrf_power_event(usbevt);
  }

  return err;
}

uint32_t proc_ble(void)
{
  // do nothing with ble
  return NRF_ERROR_NOT_FOUND;
}

void SD_EVT_IRQHandler(void)
{
  // process BLE and SOC until there is no more events
  while( (NRF_ERROR_NOT_FOUND != proc_ble()) || (NRF_ERROR_NOT_FOUND != proc_soc()) )
  {

  }
}

void nrf_error_cb(uint32_t id, uint32_t pc, uint32_t info)
{
  (void) id;
  (void) pc;
  (void) info;
}
#endif
