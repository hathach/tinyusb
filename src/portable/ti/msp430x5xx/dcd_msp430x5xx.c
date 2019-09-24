/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 William D. Jones
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

#if TUSB_OPT_DEVICE_ENABLED && ( CFG_TUSB_MCU == OPT_MCU_MSP430x5xx )

#include "msp430.h"
#include "device/dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
#define USB_BUF_PTR(_x) (uint8_t *) ((uint16_t) _x)

// usbpllir_mirror and usbmaintl_mirror can be added later if needed.
static volatile uint16_t usbiepie_mirror = 0;
static volatile uint16_t usboepie_mirror = 0;
static volatile uint8_t usbie_mirror = 0;
static volatile uint16_t usbpwrctl_mirror = 0;
static bool in_isr = false;

uint8_t _setup_packet[8];

static void bus_reset(void)
{
  // Enable the control EP 0. Also enable Indication Enable- a guard flag
  // separate from the Interrupt Enable mask.
  USBOEPCNF_0 |= (UBME | USBIIE);
  USBIEPCNF_0 |= (UBME | USBIIE);

  // Enable interrupts for this endpoint.
  USBOEPIE |= BIT0;
  USBIEPIE |= BIT0;

  // Clear NAK so packets can be received.
  // Dedicated buffers in hardware for SETUP and EP0, no setup needed.
  USBOEPCNT_0 &= ~NAK;
  USBIEPCNT_0 &= ~NAK;

  // Now safe to respond to SETUP packets.
  USBIE |= SETUPIE;
}


/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
void dcd_init (uint8_t rhport)
{
  (void) rhport;

  USBKEYPID = USBKEY;

  // Enable the module (required to write config regs)!
  USBCNF |= USB_EN;

  // Reset used interrupts
  USBOEPIE = 0;
  USBIEPIE = 0;
  USBIE = 0;
  USBOEPIFG = 0;
  USBIEPIFG = 0;
  USBIFG = 0;
  USBPWRCTL &= ~(VUOVLIE | VBONIE | VBOFFIE | VUOVLIFG | VBONIFG | VBOFFIFG);
  USBVECINT = 0;

  // Enable reset and wait for it before continuing.
  USBIE |= RSTRIE;

  // Enable pullup.
  USBCNF |= PUR_EN;

  USBKEYPID = 0;
}

// There is no "USB peripheral interrupt disable" bit on MSP430, so we have
// to save the relevant registers individually.
// WARNING: Unlike the ARM/NVIC routines, these functions are _not_ idempotent
// if you modified the registers saved in between calls so they don't match
// the mirrors; mirrors will be updated to reflect most recent register
// contents.
void dcd_int_enable (uint8_t rhport)
{
  (void) rhport;

  __bic_SR_register(GIE); // Unlikely to be called in ISR, but let's be safe.
                          // Also, this cleanly disables all USB interrupts
                          // atomically from application's POV.

  // This guard is required because tinyusb can enable interrupts without
  // having disabled them first.
  if(in_isr)
  {
    USBOEPIE = usboepie_mirror;
    USBIEPIE = usbiepie_mirror;
    USBIE = usbie_mirror;
    USBPWRCTL |= usbpwrctl_mirror;
  }

  in_isr = false;
  __bis_SR_register(GIE);
}

void dcd_int_disable (uint8_t rhport)
{
  (void) rhport;

  __bic_SR_register(GIE);
  usboepie_mirror = USBOEPIE;
  usbiepie_mirror = USBIEPIE;
  usbie_mirror = USBIE;
  usbpwrctl_mirror = (USBPWRCTL & (VUOVLIE | VBONIE | VBOFFIE));
  USBOEPIE = 0;
  USBIEPIE = 0;
  USBIE = 0;
  USBPWRCTL &= ~(VUOVLIE | VBONIE | VBOFFIE);
  in_isr = true;
  __bis_SR_register(GIE);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
  (void) dev_addr;
}

void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  // Nothing to do
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;
  (void) desc_edpt;

  return false;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;
  (void) ep_addr;
  (void) buffer;
  (void) total_bytes;

  return false;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  (void) ep_addr;
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  (void) ep_addr;
}

/*------------------------------------------------------------------*/

static void receive_packet(void)
{

}

static void transmit_packet(void)
{

}

static void handle_setup_packet(void)
{
  volatile uint8_t * setup_buf = &USBSUBLK;

  for(int i = 0; i < 8; i++)
  {
    _setup_packet[i] = setup_buf[i];
  }

  dcd_event_setup_received(0, (uint8_t*) &_setup_packet[0], true);
}

void __attribute__ ((interrupt(USB_UBM_VECTOR))) USB_UBM_ISR(void)
{
  // Setup is special- reading USBVECINT to handle setup packets is done to
  // stop NAKs on EP0.
  uint8_t setup_status = USBIFG & SETUPIFG;

  if(setup_status)
  {
    handle_setup_packet();
  }

  uint16_t curr_vector = USBVECINT;

  switch(curr_vector)
  {
    case USBVECINT_RSTR:
      bus_reset();
      dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
      break;

    // Clear the NAK on EP 0 after a SETUP packet is received.
    case USBVECINT_SETUP_PACKET_RECEIVED:
      break;

    case USBVECINT_INPUT_ENDPOINT0:
      transmit_packet();
      break;

    case USBVECINT_OUTPUT_ENDPOINT0:
      receive_packet();
      break;

    default:
      break;
  }

}

#endif
