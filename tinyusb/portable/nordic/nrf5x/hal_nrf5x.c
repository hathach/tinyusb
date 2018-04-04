/**************************************************************************/
/*!
    @file     hal_nrf5x.c
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
*/
/**************************************************************************/

#ifdef NRF52840_XXAA

#include <stdbool.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_clock.h"
#include "nrf_drv_power.h"
#include "nrf_usbd.h"
#include "nrf_drv_usbd_errata.h"

#include "tusb_hal.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/

/*------------------------------------------------------------------*/
/* VARIABLE DECLARATION
 *------------------------------------------------------------------*/
static void power_usb_event_handler(uint32_t event);

/*------------------------------------------------------------------*/
/* FUNCTION DECLARATION
 *------------------------------------------------------------------*/
bool tusb_hal_init(void)
{
#ifndef SOFTDEVICE_PRESENT
  // USB Power detection
  const nrf_drv_power_usbevt_config_t config =
  {
      .handler = power_usb_event_handler
  };
  return ( NRF_SUCCESS == nrf_drv_power_usbevt_init(&config) );
#else

#endif
}

void tusb_hal_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USBD_IRQn);
}

void tusb_hal_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USBD_IRQn);
}

/*------------------------------------------------------------------*/
/* Controller Start up Sequence
 *------------------------------------------------------------------*/

/*------------- HFCLK helper  -------------*/
static bool is_sd_enabled(void)
{
#ifdef SOFTDEVICE_PRESENT
  uint8_t sd_en = 0;
  (void) sd_softdevice_is_enabled(&sd_en);

  return sd_en;
#else
  return false;
#endif
}

static bool hfclk_running(void)
{
#ifdef SOFTDEVICE_PRESENT
  if ( is_sd_enabled() )
  {
    uint32_t is_running;
    (void) sd_clock_hfclk_is_running(&is_running);
    return (is_running ? true : false);
  }
#endif

  return nrf_clock_hf_is_running(NRF_CLOCK_HFCLK_HIGH_ACCURACY);
}

static void hfclk_enable(void)
{
  // already running, nothing to do
  if ( hfclk_running() ) return;

#ifdef SOFTDEVICE_PRESENT
  if ( is_sd_enabled() )
  {
    (void)sd_clock_hfclk_request();
    return;
  }
#endif

  nrf_clock_event_clear(NRF_CLOCK_EVENT_HFCLKSTARTED);
  nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTART);
}

static void hfclk_disable(void)
{
#ifdef SOFTDEVICE_PRESENT
  if ( is_sd_enabled() )
  {
    (void)sd_clock_hfclk_release();
    return;
  }
#endif

  nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTOP);
}

/*------------- 51.4 specs USBD start-up sequene -------------*/
static void power_usb_event_handler(uint32_t event)
{
  switch ( event )
  {
    case NRF_DRV_POWER_USB_EVT_DETECTED:
      if ( !NRF_USBD->ENABLE )
      {
        /* Prepare for READY event receiving */
        nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);

        /* Enable the peripheral */
        nrf_usbd_enable();

        // Enable HFCLK
        hfclk_enable();

        /* Waiting for peripheral to enable, this should take a few us */
        while ( !(NRF_USBD_EVENTCAUSE_READY_MASK & NRF_USBD->EVENTCAUSE) ) { }
        nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);
        nrf_usbd_event_clear(NRF_USBD_EVENT_USBEVENT);
      }
    break;

    case NRF_DRV_POWER_USB_EVT_READY:
      // Wait for HFCLK
      while ( !hfclk_running() ) {}

      if ( nrf_drv_usbd_errata_166() )
      {
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x800)) = 0x7E3;
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x804)) = 0x40;

        __ISB(); __DSB();
      }

      nrf_usbd_isosplit_set(NRF_USBD_ISOSPLIT_Half);

      // Enable interrupt. SOF is used as CDC auto flush
      NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_ACCESSFAULT_Msk |
                           USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk |  USBD_INTEN_ENDEPOUT0_Msk |
                           USBD_INTEN_EPDATA_Msk   | USBD_INTEN_SOF_Msk;

      //  if (enable_sof || nrf_drv_usbd_errata_104())
      //  {
      //    ints_to_enable |= NRF_USBD_INT_SOF_MASK;
      //  }

      // Enable interrupt, Priorities 0,1,4,5 (nRF52) are reserved for SoftDevice
      NVIC_SetPriority(USBD_IRQn, 7);
      NVIC_ClearPendingIRQ(USBD_IRQn);
      NVIC_EnableIRQ(USBD_IRQn);

      // Enable pull up
      nrf_usbd_pullup_enable();
    break;

    case NRF_DRV_POWER_USB_EVT_REMOVED:
      if ( NRF_USBD->ENABLE )
      {
        // Abort all transfers

        // Disable pull up
        nrf_usbd_pullup_disable();

        // Disable Interrupt
        NVIC_DisableIRQ(USBD_IRQn);

        // disable all interrupt
        NRF_USBD->INTENCLR = NRF_USBD->INTEN;

        nrf_usbd_disable();
        hfclk_disable();
      }
    break;

    default: break;
  }
}

#endif
