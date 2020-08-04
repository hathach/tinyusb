/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 William D. Jones
 * Copyright (c) 2019-2020 Ha Thach (tinyusb.org)
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
// usbpllir_mirror and usbmaintl_mirror can be added later if needed.
static volatile uint16_t usbiepie_mirror = 0;
static volatile uint16_t usboepie_mirror = 0;
static volatile uint8_t usbie_mirror = 0;
static volatile uint16_t usbpwrctl_mirror = 0;
static bool in_isr = false;

uint8_t _setup_packet[8];

// Xfer control
typedef struct
{
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_size;
  bool short_packet;
} xfer_ctl_t;

xfer_ctl_t xfer_status[8][2];
#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]

// Accessing endpoint regs
typedef volatile uint8_t * ep_regs_t;

typedef enum
{
  CNF = 0,
  BBAX = 1,
  BCTX = 2,
  BBAY = 5,
  BCTY = 6,
  SIZXY = 7
} ep_regs_index_t;

#define EP_REGS(epnum, dir) ((ep_regs_t) ((uintptr_t)&USBOEPCNF_1 + 64*dir + 8*(epnum - 1)))

static void bus_reset(void)
{
  // Hardcoded into the USB core.
  xfer_status[0][TUSB_DIR_OUT].max_size = 8;
  xfer_status[0][TUSB_DIR_IN].max_size = 8;

  USBKEYPID = USBKEY;

  // Enable the control EP 0. Also enable Indication Enable- a guard flag
  // separate from the Interrupt Enable mask.
  USBOEPCNF_0 |= (UBME | USBIIE);
  USBIEPCNF_0 |= (UBME | USBIIE);

  // Enable interrupts for this endpoint.
  USBOEPIE |= BIT0;
  USBIEPIE |= BIT0;

  // Clear NAK until a setup packet is received.
  USBOEPCNT_0 &= ~NAK;
  USBIEPCNT_0 &= ~NAK;

  USBCTL |= FEN; // Enable responding to packets.

  // Dedicated buffers in hardware for SETUP and EP0, no setup needed.
  // Now safe to respond to SETUP packets.
  USBIE |= SETUPIE;

  USBKEYPID = 0;
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
  usboepie_mirror = 0;
  usbiepie_mirror = 0;
  usbie_mirror = 0;
  usbpwrctl_mirror = 0;

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

  USBFUNADR = dev_addr;

  // Response with status after changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

void dcd_connect(uint8_t rhport)
{
  dcd_int_disable(rhport);

  USBKEYPID = USBKEY;
  USBCNF |= PUR_EN; // Enable pullup.
  USBKEYPID = 0;

  dcd_int_enable(rhport);
}

void dcd_disconnect(uint8_t rhport)
{
  dcd_int_disable(rhport);

  USBKEYPID = USBKEY;
  USBCNF &= ~PUR_EN; // Disable pullup.
  USBKEYPID = 0;

  dcd_int_enable(rhport);
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);

  // Unsupported endpoint numbers/size or type (Iso not supported. Control
  // not supported on nonzero endpoints).
  if((desc_edpt->wMaxPacketSize.size > 64) || (epnum > 7) || \
      (desc_edpt->bmAttributes.xfer == 0) || \
      (desc_edpt->bmAttributes.xfer == 1)) {
    return false;
  }

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->max_size = desc_edpt->wMaxPacketSize.size;

  // Buffer allocation scheme:
  // For simplicity, only single buffer for now, since tinyusb currently waits
  // for an xfer to complete before scheduling another one. This means only
  // the X buffer is used.
  //
  // 1904 bytes are available, the max endpoint size supported on msp430 is
  // 64 bytes. This is enough RAM for all 14 endpoints enabled _with_ double
  // bufferring (64*14*2 = 1792 bytes). Extra RAM exists for triple and higher
  // order bufferring, which must be maintained in software.
  //
  // For simplicity, each endpoint gets a hardcoded 64 byte chunk (regardless
  // of actual wMaxPacketSize) whose start address is the following:
  // addr = 128 * (epnum - 1) + 64 * dir.
  //
  // Double buffering equation:
  // x_addr = 256 * (epnum - 1) + 128 * dir
  // y_addr = x_addr + 64
  // Address is right-shifted by 3 to fit into 8 bits.

  uint8_t buf_base = (128 * (epnum - 1) + 64 * dir) >> 3;

  // IN and OUT EP registers have the same structure.
  ep_regs_t ep_regs = EP_REGS(epnum, dir);

  // FIXME: I was able to get into a situation where OUT EP 3 would stall
  // while debugging, despite stall code never being called. It appears
  // these registers don't get cleared on reset, being part of RAM.
  // Investigate and see if I can duplicate.
  // Also, DBUF got set on OUT EP 2 while debugging. Only OUT EPs seem to be
  // affected at this time. USB RAM directly precedes main RAM; perhaps I'm
  // overwriting registers via buffer overflow w/ my debugging code?
  ep_regs[SIZXY] = desc_edpt->wMaxPacketSize.size;
  ep_regs[BCTX] |= NAK;
  ep_regs[BBAX] = buf_base;
  ep_regs[CNF] &= ~(TOGGLE | STALL | DBUF); // ISO xfers not supported on
                           // MSP430, so no need to gate DATA0/1 and frame
                           // behavior. Clear stall and double buffer bit as
                           // well- see above comment.
  ep_regs[CNF] |= (UBME | USBIIE);

  USBKEYPID = USBKEY;
  if(dir == TUSB_DIR_OUT)
  {
    USBOEPIE |= (1 << epnum);
  }
  else
  {
    USBIEPIE |= (1 << epnum);
  }
  USBKEYPID = 0;

  return true;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  xfer->short_packet = false;

  if(epnum == 0)
  {
    if(dir == TUSB_DIR_OUT)
    {
      // Interrupt will notify us when data was received.
      USBCTL &= ~DIR;
      USBOEPCNT_0 &= ~NAK;
    }
    else
    {
      // Kickstart the IN packet handler by queuing initial data and calling
      // the ISR to transmit the first packet.
      // Interrupt only fires on completed xfer.
      USBCTL |= DIR;
      USBIEPIFG |= BIT0;
    }
  }
  else
  {
    ep_regs_t ep_regs = EP_REGS(epnum, dir);

    if(dir == TUSB_DIR_OUT)
    {
      ep_regs[BCTX] &= ~NAK;
    }
    else
    {
      USBIEPIFG |= (1 << epnum);
    }
  }

  return true;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if(epnum == 0)
  {
    if(dir == TUSB_DIR_OUT)
    {
      USBOEPCNT_0 |= NAK;
      USBOEPCNF_0 |= STALL;
    }
    else
    {
      USBIEPCNT_0 |= NAK;
      USBIEPCNF_0 |= STALL;
    }
  }
  else
  {
    ep_regs_t ep_regs = EP_REGS(epnum, dir);
    ep_regs[CNF] |= STALL;
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if(epnum == 0)
  {
    if(dir == TUSB_DIR_OUT)
    {
      USBOEPCNF_0 &= ~STALL;
    }
    else
    {
      USBIEPCNF_0 &= ~STALL;
    }
  }
  else
  {
    ep_regs_t ep_regs = EP_REGS(epnum, dir);
    // Required by USB spec to reset DATA toggle bit to DATA0 on interrupt
    // and bulk endpoints.
    ep_regs[CNF] &= ~(STALL + TOGGLE);
  }
}

void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;
  (void) request;

  // FIXME: Per manual, we should be clearing the NAK bits of EP0 after the
  // Status Phase of a control xfer is done, in preparation of another possible
  // SETUP packet. However, from my own testing, SETUP packets _are_ correctly
  // handled by the USB core without clearing the NAKs.
  //
  // Right now, clearing NAKs in this callbacks causes a direction mismatch
  // between host and device on EP0. Figure out why and come back to this.
  // USBOEPCNT_0 &= ~NAK;
  // USBIEPCNT_0 &= ~NAK;
}

/*------------------------------------------------------------------*/

static void receive_packet(uint8_t ep_num)
{
  xfer_ctl_t * xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
  ep_regs_t ep_regs = EP_REGS(ep_num, TUSB_DIR_OUT);
  uint8_t xfer_size;

  if(ep_num == 0)
  {
    xfer_size = USBOEPCNT_0 & 0x0F;
  }
  else
  {
    xfer_size = ep_regs[BCTX] & 0x7F;
  }

  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t to_recv_size;

  if(remaining <= xfer->max_size) {
    // Avoid buffer overflow.
    to_recv_size = (xfer_size > remaining) ? remaining : xfer_size;
  } else {
    // Room for full packet, choose recv_size based on what the microcontroller
    // claims.
    to_recv_size = (xfer_size > xfer->max_size) ? xfer->max_size : xfer_size;
  }

  uint8_t * base = (xfer->buffer + xfer->queued_len);

  if(ep_num == 0)
  {
    volatile uint8_t * ep0out_buf = &USBOEP0BUF;
    for(uint16_t i = 0; i < to_recv_size; i++)
    {
      base[i] = ep0out_buf[i];
    }
  }
  else
  {
    volatile uint8_t * ep_buf = &USBSTABUFF + (ep_regs[BBAX] << 3);
    for(uint16_t i = 0; i < to_recv_size ; i++)
    {
      base[i] = ep_buf[i];
    }
  }

  xfer->queued_len += xfer_size;

  xfer->short_packet = (xfer_size < xfer->max_size);
  if((xfer->total_len == xfer->queued_len) || xfer->short_packet)
  {
    dcd_event_xfer_complete(0, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }
  else
  {
    // Schedule to receive another packet.
    if(ep_num == 0)
    {
      USBOEPCNT_0 &= ~NAK;
    }
    else
    {
      ep_regs[BCTX] &= ~NAK;
    }
  }
}

static void transmit_packet(uint8_t ep_num)
{
  xfer_ctl_t * xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);

  // First, determine whether we should even send a packet or finish
  // up the xfer.
  bool zlp = (xfer->total_len == 0); // By necessity, xfer->total_len will
                                     // equal xfer->queued_len for ZLPs.
                                     // Of course a ZLP is a short packet.
  if((!zlp && (xfer->total_len == xfer->queued_len)) || xfer->short_packet)
  {
    dcd_event_xfer_complete(0, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
    return;
  }

  // Then actually commit to transmit a packet.
  uint8_t * base = (xfer->buffer + xfer->queued_len);
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint8_t xfer_size = (xfer->max_size < xfer->total_len) ? xfer->max_size : remaining;

  xfer->queued_len += xfer_size;
  if(xfer_size < xfer->max_size)
  {
    // Next "xfer complete interrupt", the transfer will end.
    xfer->short_packet = true;
  }

  if(ep_num == 0)
  {
    volatile uint8_t * ep0in_buf = &USBIEP0BUF;
    for(uint16_t i = 0; i < xfer_size; i++)
    {
      ep0in_buf[i] = base[i];
    }

    USBIEPCNT_0 = (USBIEPCNT_0 & 0xF0) + xfer_size;
    USBIEPCNT_0 &= ~NAK;
  }
  else
  {
    ep_regs_t ep_regs = EP_REGS(ep_num, TUSB_DIR_IN);
    volatile uint8_t * ep_buf = &USBSTABUFF + (ep_regs[BBAX] << 3);

    for(int i = 0; i < xfer_size; i++)
    {
      ep_buf[i] = base[i];
    }

    ep_regs[BCTX] = (ep_regs[BCTX] & 0x80) + (xfer_size & 0x7F);
    ep_regs[BCTX] &= ~NAK;
  }
}

static void handle_setup_packet(void)
{
  volatile uint8_t * setup_buf = &USBSUBLK;

  for(int i = 0; i < 8; i++)
  {
    _setup_packet[i] = setup_buf[i];
  }

  // Clearing SETUPIFG by reading USBVECINT does not set NAK, so now that we
  // have a SETUP packet, force NAKs until tinyusb can handle the SETUP
  // packet and prepare for a new xfer.
  USBIEPCNT_0 |= NAK;
  USBOEPCNT_0 |= NAK;
  dcd_event_setup_received(0, (uint8_t*) &_setup_packet[0], true);
}

void dcd_int_handler(uint8_t rhport)
{
  (void) rhport;

  // Setup is special- reading USBVECINT to handle setup packets is done to
  // stop hardware-generated NAKs on EP0.
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

    // Clear the (hardware-enforced) NAK on EP 0 after a SETUP packet
    // is received. At this point, even though the hardware is no longer
    // forcing NAKs, the EP0 NAK bits should still be set to avoid
    // sending/receiving data before tinyusb is ready.
    //
    // Furthermore, it's possible for the hardware to STALL in the middle of
    // a control xfer if the EP0 NAK bits aren't set properly.
    // See: https://e2e.ti.com/support/microcontrollers/msp430/f/166/t/845259
    // From my testing, if all of the following hold:
    // * OUT EP0 NAK is cleared.
    // * IN EP0 NAK is set.
    // * DIR bit in USBCTL is clear.
    // and an IN packet is received on EP0, the USB core will STALL. Setting
    // both EP0 NAKs manually when a SETUP packet is received, as is done
    // in handle_setup_packet(), avoids meeting STALL conditions.
    //
    // TODO: Figure out/explain why the STALL condition can be reached in the
    // first place. When I first noticed the STALL, the only two places I
    // touched the NAK bits were in dcd_edpt_xfer() and to _set_ (sic) them in
    // bus_reset(). SETUP packet handling should've been unaffected.
    case USBVECINT_SETUP_PACKET_RECEIVED:
      break;

    case USBVECINT_INPUT_ENDPOINT0:
      transmit_packet(0);
      break;

    case USBVECINT_OUTPUT_ENDPOINT0:
      receive_packet(0);
      break;

    case USBVECINT_INPUT_ENDPOINT1:
    case USBVECINT_INPUT_ENDPOINT2:
    case USBVECINT_INPUT_ENDPOINT3:
    case USBVECINT_INPUT_ENDPOINT4:
    case USBVECINT_INPUT_ENDPOINT5:
    case USBVECINT_INPUT_ENDPOINT6:
    case USBVECINT_INPUT_ENDPOINT7:
    {
      uint8_t ep = ((curr_vector - USBVECINT_INPUT_ENDPOINT1) >> 1) + 1;
      transmit_packet(ep);
    }
    break;

    case USBVECINT_OUTPUT_ENDPOINT1:
    case USBVECINT_OUTPUT_ENDPOINT2:
    case USBVECINT_OUTPUT_ENDPOINT3:
    case USBVECINT_OUTPUT_ENDPOINT4:
    case USBVECINT_OUTPUT_ENDPOINT5:
    case USBVECINT_OUTPUT_ENDPOINT6:
    case USBVECINT_OUTPUT_ENDPOINT7:
    {
      uint8_t ep = ((curr_vector - USBVECINT_OUTPUT_ENDPOINT1) >> 1) + 1;
      receive_packet(ep);
    }
    break;

    default:
      while(true);
      break;
  }

}

#endif
