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

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_NRF5X

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_clock.h"
#include "nrf_usbd.h"
#include "nrf_drv_usbd_errata.h"

#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdm.h"
#include "nrf_soc.h"

// TODO fully move to nrfx
enum {
    NRFX_POWER_USB_EVT_DETECTED, /**< USB power detected on the connector (plugged in). */
    NRFX_POWER_USB_EVT_REMOVED,  /**< USB power removed from the connector. */
    NRFX_POWER_USB_EVT_READY     /**< USB power regulator ready. */
};
#else
#include "nrfx_power.h"
#endif

#include "tusb_hal.h"
#include "device/dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
#define USB_NVIC_PRIO   7

void tusb_hal_nrf_power_event(uint32_t event);

/*------------------------------------------------------------------*/
/* HFCLK helper
 *------------------------------------------------------------------*/

#ifdef SOFTDEVICE_PRESENT
// check if SD is present and enabled
static bool is_sd_enabled(void)
{
  uint8_t sd_en = false;
  (void) sd_softdevice_is_enabled(&sd_en);
  return sd_en;
}
#endif

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

/*------------------------------------------------------------------*/
/* Controller Start up Sequence (USBD 51.4 specs)
 *------------------------------------------------------------------*/
// tusb_hal_nrf_power_event must be called by SOC event handler
void tusb_hal_nrf_power_event (uint32_t event)
{
  switch ( event )
  {
    case NRFX_POWER_USB_EVT_DETECTED:
      if ( !NRF_USBD->ENABLE )
      {
        /* Prepare for READY event receiving */
        nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);

        /* Enable the peripheral */
        // ERRATA 171, 187, 166

        if ( nrf_drv_usbd_errata_187() )
        {
          // CRITICAL_REGION_ENTER();
          if ( *((volatile uint32_t *) (0x4006EC00)) == 0x00000000 )
          {
            *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
            *((volatile uint32_t *) (0x4006ED14)) = 0x00000003;
            *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
          }
          else
          {
            *((volatile uint32_t *) (0x4006ED14)) = 0x00000003;
          }
          // CRITICAL_REGION_EXIT();
        }

        if ( nrf_drv_usbd_errata_171() )
        {
          // CRITICAL_REGION_ENTER();
          if ( *((volatile uint32_t *) (0x4006EC00)) == 0x00000000 )
          {
            *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
            *((volatile uint32_t *) (0x4006EC14)) = 0x000000C0;
            *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
          }
          else
          {
            *((volatile uint32_t *) (0x4006EC14)) = 0x000000C0;
          }
          // CRITICAL_REGION_EXIT();
        }

        nrf_usbd_enable();

        // Enable HFCLK
        hfclk_enable();
      }
    break;

    case NRFX_POWER_USB_EVT_READY:
      /* Waiting for USBD peripheral enabled */
      while ( !(USBD_EVENTCAUSE_READY_Msk & NRF_USBD->EVENTCAUSE) ) { }

      nrf_usbd_eventcause_clear(USBD_EVENTCAUSE_READY_Msk);
      nrf_usbd_event_clear(USBD_EVENTCAUSE_READY_Msk);

      if ( nrf_drv_usbd_errata_171() )
      {
        // CRITICAL_REGION_ENTER();
        if ( *((volatile uint32_t *) (0x4006EC00)) == 0x00000000 )
        {
          *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
          *((volatile uint32_t *) (0x4006EC14)) = 0x00000000;
          *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
        }
        else
        {
          *((volatile uint32_t *) (0x4006EC14)) = 0x00000000;
        }

        // CRITICAL_REGION_EXIT();
      }

      if ( nrf_drv_usbd_errata_187() )
      {
        // CRITICAL_REGION_ENTER();
        if ( *((volatile uint32_t *) (0x4006EC00)) == 0x00000000 )
        {
          *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
          *((volatile uint32_t *) (0x4006ED14)) = 0x00000000;
          *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
        }
        else
        {
          *((volatile uint32_t *) (0x4006ED14)) = 0x00000000;
        }
        // CRITICAL_REGION_EXIT();
      }

      if ( nrf_drv_usbd_errata_166() )
      {
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x800)) = 0x7E3;
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x804)) = 0x40;

        __ISB();
        __DSB();
      }

      nrf_usbd_isosplit_set(USBD_ISOSPLIT_SPLIT_HalfIN);

      // Enable interrupt. SOF is used as CDC auto flush
      NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_EPDATA_Msk |
          USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk | USBD_INTEN_ENDEPOUT0_Msk;

      // Enable interrupt, Priorities 0,1,4,5 (nRF52) are reserved for SoftDevice
      NVIC_SetPriority(USBD_IRQn, USB_NVIC_PRIO);
      NVIC_ClearPendingIRQ(USBD_IRQn);
      NVIC_EnableIRQ(USBD_IRQn);

      // Wait for HFCLK
      while ( !hfclk_running() ) { }

      // Enable pull up
      nrf_usbd_pullup_enable();
    break;

    case NRFX_POWER_USB_EVT_REMOVED:
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

        dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, true);
      }
    break;

    default: break;
  }
}

#endif
