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

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_NRF5X

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_clock.h"
#include "nrf_usbd.h"
#include "nrfx_usbd_errata.h"

#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdm.h"
#include "nrf_soc.h"
#endif

#include "nrfx_power.h"
#include "device/dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
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

        if ( nrfx_usbd_errata_187() )
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

        if ( nrfx_usbd_errata_171() )
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

      if ( nrfx_usbd_errata_171() )
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

      if ( nrfx_usbd_errata_187() )
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

      if ( nrfx_usbd_errata_166() )
      {
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x800)) = 0x7E3;
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x804)) = 0x40;

        __ISB();
        __DSB();
      }

      nrf_usbd_isosplit_set(USBD_ISOSPLIT_SPLIT_HalfIN);

      // Enable interrupt
      NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_EPDATA_Msk |
          USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk | USBD_INTEN_ENDEPOUT0_Msk;

      // Enable interrupt, priorities should be set by application
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
