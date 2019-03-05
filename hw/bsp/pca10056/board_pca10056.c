/**************************************************************************/
/*!
    @file     board_pca10056.c
    @author   hathach

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/
#ifdef BOARD_PCA10056

#include "bsp/board.h"
#include "nrfx/hal/nrf_gpio.h"
#include "nrfx/drivers/include/nrfx_power.h"
#include "nrfx/drivers/include/nrfx_qspi.h"

#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdm.h"
#include "nrf_soc.h"
#endif

#include "tusb.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
#define LED_PIN         13
#define LED_STATE_ON    0

uint8_t _button_pins[] = { 11, 12, 24, 25 };

#define BOARD_BUTTON_COUNT  sizeof(_button_pins)


/*------------------------------------------------------------------*/
/* TUSB HAL MILLISECOND
 *------------------------------------------------------------------*/
#if CFG_TUSB_OS  == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t tusb_hal_millis(void)
{
  return board_tick2ms(system_ticks);
}
#endif

/*------------------------------------------------------------------*/
/* BOARD API
 *------------------------------------------------------------------*/
enum {
  QSPI_CMD_RSTEN = 0x66,
  QSPI_CMD_RST = 0x99,
  QSPI_CMD_WRSR = 0x01,
  QSPI_CMD_READID = 0x90
};

/* tinyusb function that handles power event (detected, ready, removed)
 * We must call it within SD's SOC event handler, or set it as power event handler if SD is not enabled.
 */
extern void tusb_hal_nrf_power_event(uint32_t event);

void board_init(void)
{
  // Config clock source: XTAL or RC in sdk_config.h
  NRF_CLOCK->LFCLKSRC = (uint32_t)((CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos) & CLOCK_LFCLKSRC_SRC_Msk);
  NRF_CLOCK->TASKS_LFCLKSTART = 1UL;

  // LEDs
  nrf_gpio_cfg_output(LED_PIN);
  board_led_control(false);

  // Button
  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) nrf_gpio_cfg_input(_button_pins[i], NRF_GPIO_PIN_PULLUP);

#if CFG_TUSB_OS  == OPT_OS_NONE
  // Tick init
  SysTick_Config(SystemCoreClock/1000);
#endif

// 64 Mbit qspi flash
#if 0 // def BOARD_MSC_FLASH_QSPI
  nrfx_qspi_config_t qspi_cfg = {
    .xip_offset = 0,
    .pins = {
       .sck_pin = 19,
       .csn_pin = 17,
       .io0_pin = 20,
       .io1_pin = 21,
       .io2_pin = 22,
       .io3_pin = 23,
    },
    .prot_if = {
      .readoc    = NRF_QSPI_READOC_READ4IO,
      .writeoc = NRF_QSPI_WRITEOC_PP4IO,
      .addrmode  = NRF_QSPI_ADDRMODE_24BIT,
      .dpmconfig = false,  // deep power down
    },
    .phy_if = {
      .sck_freq = NRF_QSPI_FREQ_32MDIV1,
      .sck_delay = 1,
      .spi_mode  = NRF_QSPI_MODE_0,
      .dpmen     = false
    },
    .irq_priority   = 7,
  };

  // NULL callback for blocking API
  nrfx_qspi_init(&qspi_cfg, NULL, NULL);

  nrf_qspi_cinstr_conf_t cinstr_cfg = {
    .opcode    = 0,
    .length    = 0,
    .io2_level = true,
    .io3_level = true,
    .wipwait   = false,
    .wren      = false
  };

  // Send reset enable
  cinstr_cfg.opcode = QSPI_CMD_RSTEN;
  cinstr_cfg.length = NRF_QSPI_CINSTR_LEN_1B;
  nrfx_qspi_cinstr_xfer(&cinstr_cfg, NULL, NULL);

  // Send reset command
  cinstr_cfg.opcode = QSPI_CMD_RST;
  cinstr_cfg.length = NRF_QSPI_CINSTR_LEN_1B;
  nrfx_qspi_cinstr_xfer(&cinstr_cfg, NULL, NULL);

  NRFX_DELAY_US(100); // wait for flash reset

  // Send (Read ID + 3 dummy bytes) + Receive 2 bytes of Manufacture + Device ID
  uint8_t dummy[6] = { 0 };
  uint8_t id_resp[6] = { 0 };
  cinstr_cfg.opcode = QSPI_CMD_READID;
  cinstr_cfg.length = 6;

  // Bug with -nrf_qspi_cinstrdata_get() didn't combine data.
  // https://devzone.nordicsemi.com/f/nordic-q-a/38540/bug-nrf_qspi_cinstrdata_get-didn-t-collect-data-from-both-cinstrdat1-and-cinstrdat0
  nrfx_qspi_cinstr_xfer(&cinstr_cfg, dummy, id_resp);

  // Due to the bug, we collect data manually
  uint8_t dev_id = (uint8_t) NRF_QSPI->CINSTRDAT1;
  uint8_t mfgr_id = (uint8_t) ( NRF_QSPI->CINSTRDAT0 >> 24 );

  // Switch to quad mode
  uint16_t sr_quad_en = 0x40;
  cinstr_cfg.opcode = QSPI_CMD_WRSR;
  cinstr_cfg.length = 3;
  cinstr_cfg.wipwait = cinstr_cfg.wren = true;
  nrfx_qspi_cinstr_xfer(&cinstr_cfg, &sr_quad_en, NULL);
#endif

  NVIC_SetPriority(USBD_IRQn, 2);

  // USB power may already be ready at this time -> no event generated
  // We need to invoke the handler based on the status initially
  uint32_t usb_reg;

#ifdef SOFTDEVICE_PRESENT

// Enable to test enable SD before USB scenario
#if 1
  extern void nrf_error_cb(uint32_t id, uint32_t pc, uint32_t info);
  nrf_clock_lf_cfg_t clock_cfg =
  {
      // LFXO
      .source        = NRF_CLOCK_LF_SRC_XTAL,
      .rc_ctiv       = 0,
      .rc_temp_ctiv  = 0,
      .accuracy      = NRF_CLOCK_LF_ACCURACY_20_PPM
  };

  sd_softdevice_enable(&clock_cfg, nrf_error_cb);
  NVIC_EnableIRQ(SD_EVT_IRQn);
#endif

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
    const nrfx_power_usbevt_config_t config = { .handler = (nrfx_power_usb_event_handler_t) tusb_hal_nrf_power_event };
    nrfx_power_usbevt_init(&config);

    nrfx_power_usbevt_enable();

    usb_reg = NRF_POWER->USBREGSTATUS;
  }

  if ( usb_reg & POWER_USBREGSTATUS_VBUSDETECT_Msk ) {
    tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_DETECTED);
  }

  if ( usb_reg & POWER_USBREGSTATUS_OUTPUTRDY_Msk ) {
    tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_READY);
  }
}

void board_led_control(bool state)
{
  nrf_gpio_pin_write(LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_buttons(void)
{
  uint32_t ret = 0;

  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++)
  {
    // button is active LOW
    ret |= ( nrf_gpio_pin_read(_button_pins[i]) ? 0 : (1 << i));
  }

  return ret;
}

uint8_t board_uart_getchar(void)
{
  return 0;
}

void board_uart_putchar(uint8_t c)
{
  (void) c;
}

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

#endif
