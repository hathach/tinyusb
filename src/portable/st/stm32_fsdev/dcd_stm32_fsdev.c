/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nathan Conrad
 *
 * Portions:
 * Copyright (c) 2016 STMicroelectronics
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

/**********************************************
 * This driver has been tested with the following MCUs:
 * 
 * 
 * STM32F070RB
 *
 *
 * It also should work with minimal changes for any ST MCU with an "USB A" peripheral. This
 * covers:
 *
 * F04x, F072, F078, 070x6/B      1024 byte buffer
 * F102, F103                      512 byte buffer; no internal D+ pull-up (maybe many more changes?)
 * F302xB/C, F303xB/C, F373        512 byte buffer; no internal D+ pull-up
 * F302x6/8, F302xD/E2, F303xD/E  1024 byte buffer; no internal D+ pull-up
 * L0x2, L0x3                     1024 byte buffer
 * L1                              512 byte buffer
 * 2L4x2, 2L4x3                   1024 byte buffer
 *
 * Assumptions of the driver:
 * - dcd_fs_irqHandler() is called by the USB interrupt handler
 * - USB clock enabled before usb_init() is called; Perhaps use __HAL_RCC_USB_CLK_ENABLE();
 * - You are not using CAN (it must share the packet buffer)
 * - APB clock is >= 10 MHz
 * - On some boards, series resistors are required, but not on others.
 * - On some boards, D+ pull up resistor (1.5kohm) is required, but not on others.
 * - You don't have long-running interrupts; some USB packets must be quickly responded to.
 * - You have the ST CMSIS library linked into the project. HAL is not used.
 *
 * Current driver limitations (i.e., a list of features for you to add):
 * - STALL handled, but not tested.
 *   - Does it work? No clue.
 * - Only tested on F070RB; other models will have an #error during compilation
 * - All EP BTABLE buffers are created as max 64 bytes.
 *   - Smaller can be requested, but it has to be an even number.
 * - No isochronous endpoints
 * - Endpoint index is the ID of the endpoint
 *   - This means that priority is given to endpoints with lower ID numbers
 *   - Code is mixing up EP IX with EP ID. Everywhere.
 * - No way to close endpoints; Can a device be reconfigured without a reset?
 * - Packet buffer memory is copied in the interrupt.
 *   - This is better for performance, but means interrupts are disabled for longer
 *   - DMA may be the best choice, but it could also be pushed to the USBD task.
 * - No double-buffering
 * - No DMA
 * - No provision to control the D+ pull-up using GPIO on devices without an internal pull-up.
 * - Minimal error handling
 *   - Perhaps error interrupts sholud be reported to the stack, or cause a device reset?
 * - Assumes a single USB peripheral; I think that no hardware has multiple so this is fine.
 * - Add a callback for enabling/disabling the D+ PU on devices without an internal PU.
 * - F3 models use three separate interrupts. I think we could only use the LP interrupt for
 *     everything?  However, the interrupts are configurable so the DisableInt and EnableInt
 *     below functions could be adjusting the wrong interrupts (if they had been reconfigured)
 * - LPM is not used correctly, or at all?
 *
 * USB documentation and Reference implementations
 * - STM32 Reference manuals
 * - STM32 USB Hardware Guidelines AN4879
 *
 * - STM32 HAL (much of this driver is based on this)
 * - libopencm3/lib/stm32/common/st_usbfs_core.c
 * - Keil USB Device http://www.keil.com/pack/doc/mw/USB/html/group__usbd.html
 *
 * - YouTube OpenTechLab 011; https://www.youtube.com/watch?v=4FOkJLp_PUw
 *
 * Advantages over HAL driver:
 * - Tiny (saves RAM, assumes a single USB peripheral)
 *
 * Notes:
 * - The buffer table is allocated as endpoints are opened. The allocation is only
 *   cleared when the device is reset. This may be bad if the USB device needs
 *   to be reconfigured.
 */

#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED) && ((CFG_TUSB_MCU) == (OPT_MCU_STM32_FSDEV))

// In order to reduce the dependance on HAL, we undefine this.
// Some definitions are copied to our private include file.
#undef USE_HAL_DRIVER

#include "device/dcd.h"
#include "portable/st/stm32_fsdev/dcd_stm32_fsdev_pvt_st.h"


/*****************************************************
 * Configuration
 *****************************************************/

// HW supports max of 8 endpoints, but this can be reduced to save RAM
#ifndef MAX_EP_COUNT
#  define MAX_EP_COUNT 8u
#endif

// If sharing with CAN, one can set this to be non-zero to give CAN space where it wants it
// Both of these MUST be a multiple of 2, and are in byte units.
#ifndef DCD_STM32_BTABLE_BASE
#  define DCD_STM32_BTABLE_BASE 0u
#endif

#ifndef DCD_STM32_BTABLE_LENGTH
#  define DCD_STM32_BTABLE_LENGTH (PMA_LENGTH - DCD_STM32_BTABLE_BASE)
#endif

/***************************************************
 * Checks, structs, defines, function definitions, etc.
 */

#if ((MAX_EP_COUNT) > 8)
#  error Only 8 endpoints supported on the hardware
#endif

#if (((DCD_STM32_BTABLE_BASE) + (DCD_STM32_BTABLE_LENGTH))>(PMA_LENGTH))
#  error BTABLE does not fit in PMA RAM
#endif

#if (((DCD_STM32_BTABLE_BASE) % 8) != 0)
// per STM32F3 reference manual
#error BTABLE must be aligned to 8 bytes
#endif

// Max size of a USB FS packet is 64...
#define MAX_PACKET_SIZE 64


// One of these for every EP IN & OUT, uses a bit of RAM....
typedef struct
{
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t queued_len;
} xfer_ctl_t;

static xfer_ctl_t  xfer_status[MAX_EP_COUNT][2];
#define XFER_CTL_BASE(_epnum, _dir) &xfer_status[_epnum][_dir]

static TU_ATTR_ALIGNED(4) uint32_t _setup_packet[6];

static uint8_t newDADDR; // Used to set the new device address during the CTR IRQ handler

// EP Buffers assigned from end of memory location, to minimize their chance of crashing
// into the stack.
static uint16_t ep_buf_ptr;
static void dcd_handle_bus_reset(void);
static void dcd_write_packet_memory(uint16_t dst, const void *__restrict src, size_t wNBytes);
static void dcd_read_packet_memory(void *__restrict dst, uint16_t src, size_t wNBytes);
static void dcd_transmit_packet(xfer_ctl_t * xfer, uint16_t ep_ix);
static uint16_t dcd_ep_ctr_handler(void);

void dcd_init (uint8_t rhport)
{
  (void)rhport;
  /* Clocks should already be enabled */
  /* Use __HAL_RCC_USB_CLK_ENABLE(); to enable the clocks before calling this function */

  /* The RM mentions to use a special ordering of PDWN and FRES, but this isn't done in HAL.
   * Here, the RM is followed. */

  for(uint32_t i = 0; i<200; i++) // should be a few us
  {
    asm("NOP");
  }
	// Perform USB peripheral reset
  USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
  for(uint32_t i = 0; i<200; i++) // should be a few us
  {
    asm("NOP");
  }
  USB->CNTR &= ~(USB_CNTR_PDWN);// Remove powerdown
  // Wait startup time, for F042 and F070, this is <= 1 us.
  for(uint32_t i = 0; i<200; i++) // should be a few us
  {
    asm("NOP");
  }
  USB->CNTR = 0; // Enable USB

  USB->BTABLE = DCD_STM32_BTABLE_BASE;

  USB->ISTR &= ~(USB_ISTR_ALL_EVENTS); // Clear pending interrupts

  // Clear all EPREG
  for(uint16_t i=0; i<8; i++)
  {
    EPREG(0) = 0u;
  }

  // Initialize the BTABLE for EP0 at this point (though setting up the EP0R is unneeded)
  // This is actually not necessary, but helps debugging to start with a blank RAM area
  for(uint16_t i=0;i<(DCD_STM32_BTABLE_LENGTH>>1); i++)
  {
    pma[PMA_STRIDE*(DCD_STM32_BTABLE_BASE + i)] = 0u;
  }
  USB->CNTR |= USB_CNTR_RESETM | USB_CNTR_SOFM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
  dcd_handle_bus_reset();

  // And finally enable pull-up, which may trigger the RESET IRQ if the host is connected.
  // (if this MCU has an internal pullup)
#if defined(USB_BCDR_DPPU)
  USB->BCDR |= USB_BCDR_DPPU;
#else
  // FIXME: callback to the user to ask them to twiddle a GPIO to disable/enable D+???
#endif

}

// Enable device interrupt
void dcd_int_enable (uint8_t rhport)
{
  (void)rhport;
#if defined(STM32F0)
  NVIC_SetPriority(USB_IRQn, 0);
  NVIC_EnableIRQ(USB_IRQn);
#elif defined(STM32F3)
  NVIC_SetPriority(USB_HP_CAN_TX_IRQn, 0);
  NVIC_SetPriority(USB_LP_CAN_RX0_IRQn, 0);
  NVIC_SetPriority(USBWakeUp_IRQn, 0);
  NVIC_EnableIRQ(USB_HP_CAN_TX_IRQn);
  NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);
  NVIC_EnableIRQ(USBWakeUp_IRQn);
#endif
}

// Disable device interrupt
void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;
#if defined(STM32F0)
  NVIC_DisableIRQ(USB_IRQn);
#elif defined(STM32F3)
  NVIC_DisableIRQ(USB_HP_CAN_TX_IRQn);
  NVIC_DisableIRQ(USB_LP_CAN_RX0_IRQn);
  NVIC_DisableIRQ(USBWakeUp_IRQn);
#else
#error Unknown arch in USB driver
#endif
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;
  // We cannot immediatly change it; it must be queued to change after the STATUS packet is sent.
  // (CTR handler will actually change the address once it sees that the transmission is complete)
  newDADDR = dev_addr;

  // Respond with status
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);

}

// Receive Set Config request
void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  // Nothing to do? Handled by stack.
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

// I'm getting a weird warning about missing braces here that I don't
// know how to fix.
#if defined(__GNUC__) && (__GNUC__ >= 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wmissing-braces"
#endif
static const tusb_desc_endpoint_t ep0OUT_desc =
{
    .wMaxPacketSize = CFG_TUD_ENDPOINT0_SIZE,
    .bDescriptorType = TUSB_XFER_CONTROL,
    .bEndpointAddress = 0x00
};

static const tusb_desc_endpoint_t ep0IN_desc =
{
    .wMaxPacketSize = CFG_TUD_ENDPOINT0_SIZE,
    .bDescriptorType = TUSB_XFER_CONTROL,
    .bEndpointAddress = 0x80
};

#pragma GCC diagnostic pop

static void dcd_handle_bus_reset(void)
{
  //__IO uint16_t * const epreg = &(EPREG(0));
  USB->DADDR = 0u; // disable USB peripheral by clearing the EF flag

  // Clear all EPREG (or maybe this is automatic? I'm not sure)
  for(uint16_t i=0; i<8; i++)
  {
    EPREG(0) = 0u;
  }

  ep_buf_ptr = DCD_STM32_BTABLE_BASE + 8*MAX_EP_COUNT; // 8 bytes per endpoint (two TX and two RX words, each)
  dcd_edpt_open (0, &ep0OUT_desc);
  dcd_edpt_open (0, &ep0IN_desc);
  newDADDR = 0;
  USB->DADDR = USB_DADDR_EF; // Set enable flag, and leaving the device address as zero.
  PCD_SET_EP_RX_STATUS(USB, 0, USB_EP_RX_VALID); // And start accepting SETUP on EP0
}

// FIXME: Defined to return uint16 so that ASSERT can be used, even though a return value is not needed.
static uint16_t dcd_ep_ctr_handler(void)
{
  uint16_t count=0U;
  uint8_t EPindex;
  __IO uint16_t wIstr;
  __IO uint16_t wEPVal = 0U;

  // stack variables to pass to USBD

  /* stay in loop while pending interrupts */
  while (((wIstr = USB->ISTR) & USB_ISTR_CTR) != 0U)
  {
    /* extract highest priority endpoint index */
    EPindex = (uint8_t)(wIstr & USB_ISTR_EP_ID);

    if (EPindex == 0U)
    {
      /* Decode and service control endpoint interrupt */

      /* DIR bit = origin of the interrupt */
      if ((wIstr & USB_ISTR_DIR) == 0U)
      {
        /* DIR = 0  => IN  int */
        /* DIR = 0 implies that (EP_CTR_TX = 1) always  */
        PCD_CLEAR_TX_EP_CTR(USB, 0);

        xfer_ctl_t * xfer = XFER_CTL_BASE(EPindex,TUSB_DIR_IN);

        if((xfer->total_len == xfer->queued_len))
        {
          dcd_event_xfer_complete(0u, (uint8_t)(0x80 + EPindex), xfer->total_len, XFER_RESULT_SUCCESS, true);
          if((newDADDR != 0) && ( xfer->total_len == 0U))
          {
            // Delayed setting of the DADDR after the 0-len DATA packet acking the request is sent.
            USB->DADDR &= ~USB_DADDR_ADD;
            USB->DADDR |= newDADDR;
            newDADDR = 0;
          }
          if(xfer->total_len == 0) // Probably a status message?
          {
            PCD_CLEAR_RX_DTOG(USB,EPindex);
          }
        }
        else
        {
          dcd_transmit_packet(xfer,EPindex);
        }
      }
      else
      {
        /* DIR = 1 & CTR_RX       => SETUP or OUT int */
        /* DIR = 1 & (CTR_TX | CTR_RX) => 2 int pending */

        xfer_ctl_t *xfer = XFER_CTL_BASE(EPindex,TUSB_DIR_OUT);

        //ep = &hpcd->OUT_ep[0];
        wEPVal = PCD_GET_ENDPOINT(USB, EPindex);

        if ((wEPVal & USB_EP_SETUP) != 0U) // SETUP
        {
          // The setup_received function uses memcpy, so this must first copy the setup data into
          // user memory, to allow for the 32-bit access that memcpy performs.
          uint8_t userMemBuf[8];
          /* Get SETUP Packet*/
          count = PCD_GET_EP_RX_CNT(USB, EPindex);
          //TU_ASSERT_ERR(count == 8);
          dcd_read_packet_memory(userMemBuf, *PCD_EP_RX_ADDRESS_PTR(USB,EPindex), 8);
          /* SETUP bit kept frozen while CTR_RX = 1*/
          dcd_event_setup_received(0, (uint8_t*)userMemBuf, true);
          PCD_CLEAR_RX_EP_CTR(USB, EPindex);
        }
        else if ((wEPVal & USB_EP_CTR_RX) != 0U) // OUT
        {

          PCD_CLEAR_RX_EP_CTR(USB, EPindex);

          /* Get Control Data OUT Packet */
          count = PCD_GET_EP_RX_CNT(USB,EPindex);

          if (count != 0U)
          {
            dcd_read_packet_memory(xfer->buffer, *PCD_EP_RX_ADDRESS_PTR(USB,EPindex), count);
            xfer->queued_len = (uint16_t)(xfer->queued_len + count);
          }

          /* Process Control Data OUT status Packet*/
          if(EPindex == 0 && xfer->total_len == 0)
          {
             PCD_CLEAR_EP_KIND(USB,0); // Good, so allow non-zero length packets now.
          }
          dcd_event_xfer_complete(0, EPindex, xfer->total_len, XFER_RESULT_SUCCESS, true);

          PCD_SET_EP_RX_CNT(USB, EPindex, CFG_TUD_ENDPOINT0_SIZE);
          if(EPindex == 0 && xfer->total_len == 0)
          {
            PCD_SET_EP_RX_STATUS(USB, EPindex, USB_EP_RX_VALID);// Await next SETUP
          }

        }

      }
    }
    else /* Decode and service non control endpoints interrupt  */
    {

      /* process related endpoint register */
      wEPVal = PCD_GET_ENDPOINT(USB, EPindex);
      if ((wEPVal & USB_EP_CTR_RX) != 0U) // OUT
      {
        /* clear int flag */
        PCD_CLEAR_RX_EP_CTR(USB, EPindex);

        xfer_ctl_t * xfer = XFER_CTL_BASE(EPindex,TUSB_DIR_OUT);

        //ep = &hpcd->OUT_ep[EPindex];

        count = PCD_GET_EP_RX_CNT(USB, EPindex);
        if (count != 0U)
        {
          dcd_read_packet_memory(&(xfer->buffer[xfer->queued_len]),
              *PCD_EP_RX_ADDRESS_PTR(USB,EPindex), count);
        }

        /*multi-packet on the NON control OUT endpoint */
        xfer->queued_len = (uint16_t)(xfer->queued_len + count);

        if ((count < 64) || (xfer->queued_len == xfer->total_len))
        {
          /* RX COMPLETE */
          dcd_event_xfer_complete(0, EPindex, xfer->queued_len, XFER_RESULT_SUCCESS, true);
          // Though the host could still send, we don't know.
          // Does the bulk pipe need to be reset to valid to allow for a ZLP?
        }
        else
        {
          uint16_t remaining = (uint16_t)(xfer->total_len - xfer->queued_len);
          if(remaining >=64) {
            PCD_SET_EP_RX_CNT(USB, EPindex,64);
          } else {
            PCD_SET_EP_RX_CNT(USB, EPindex,remaining);
          }

          PCD_SET_EP_RX_STATUS(USB, EPindex, USB_EP_RX_VALID);
        }

      } /* if((wEPVal & EP_CTR_RX) */

      if ((wEPVal & USB_EP_CTR_TX) != 0U) // IN
      {
        /* clear int flag */
        PCD_CLEAR_TX_EP_CTR(USB, EPindex);

        xfer_ctl_t * xfer = XFER_CTL_BASE(EPindex,TUSB_DIR_IN);

        if (xfer->queued_len  != xfer->total_len) // data remaining in transfer?
        {
          dcd_transmit_packet(xfer, EPindex);
        } else {
          dcd_event_xfer_complete(0, (uint8_t)(0x80 + EPindex), xfer->total_len, XFER_RESULT_SUCCESS, true);
        }
      }
    }
  }
  return 0;
}

void dcd_fs_irqHandler(void) {

  uint16_t int_status = USB->ISTR;
 // unused IRQs: (USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_WKUP | USB_ISTR_SUSP | USB_ISTR_ESOF | USB_ISTR_L1REQ )

  if (int_status & USB_ISTR_CTR)
  {
    /* servicing of the endpoint correct transfer interrupt */
    /* clear of the CTR flag into the sub */
    dcd_ep_ctr_handler();
    USB->ISTR &= ~USB_ISTR_CTR;
  }
  if(int_status & USB_ISTR_RESET) {
    // USBRST is start of reset.
    USB->ISTR &= ~USB_ISTR_RESET;
    dcd_handle_bus_reset();
    dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
  }
  if (int_status & USB_ISTR_WKUP)
  {

    USB->CNTR &= ~USB_CNTR_LPMODE;
    USB->CNTR &= ~USB_CNTR_FSUSP;
    USB->ISTR &= ~USB_ISTR_WKUP;
  }

  if (int_status & USB_ISTR_SUSP)
  {
    /* Force low-power mode in the macrocell */
    USB->CNTR |= USB_CNTR_FSUSP;
    USB->CNTR |= USB_CNTR_LPMODE;

    /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
    USB->ISTR &= ~USB_ISTR_SUSP;
  }

  if(int_status & USB_ISTR_SOF) {
    USB->ISTR &= ~USB_ISTR_SOF;
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// The STM32F0 doesn't seem to like |= or &= to manipulate the EP#R registers,
// so I'm using the #define from HAL here, instead.

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  (void)rhport;
  uint8_t const epnum = tu_edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(p_endpoint_desc->bEndpointAddress);

  // Isochronous not supported (yet), and some other driver assumptions.
  TU_ASSERT(p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);
  TU_ASSERT(p_endpoint_desc->wMaxPacketSize.size <= MAX_PACKET_SIZE);
  TU_ASSERT(epnum < MAX_EP_COUNT);
  TU_ASSERT((p_endpoint_desc->wMaxPacketSize.size %2) == 0);

 // __IO uint16_t * const epreg = &(EPREG(epnum));

  // Set type
  switch(p_endpoint_desc->bmAttributes.xfer) {
  case TUSB_XFER_CONTROL:
    PCD_SET_EPTYPE(USB, epnum, USB_EP_CONTROL); break;
  case TUSB_XFER_ISOCHRONOUS:
    PCD_SET_EPTYPE(USB, epnum, USB_EP_ISOCHRONOUS); break;
  case TUSB_XFER_BULK:
    PCD_SET_EPTYPE(USB, epnum, USB_EP_BULK); break;
  case TUSB_XFER_INTERRUPT:
    PCD_SET_EPTYPE(USB, epnum, USB_EP_INTERRUPT); break;
  default:
    TU_ASSERT(false);
  }

  PCD_SET_EP_ADDRESS(USB, epnum, epnum);
  PCD_CLEAR_EP_KIND(USB,0); // Be normal, for now, instead of only accepting zero-byte packets

  if(dir == TUSB_DIR_IN)
  {
    *PCD_EP_TX_ADDRESS_PTR(USB, epnum) = ep_buf_ptr;
    PCD_SET_EP_RX_CNT(USB, epnum, p_endpoint_desc->wMaxPacketSize.size);
    PCD_CLEAR_TX_DTOG(USB, epnum);
    PCD_SET_EP_TX_STATUS(USB,epnum,USB_EP_TX_NAK);
  }
  else
  {
    *PCD_EP_RX_ADDRESS_PTR(USB, epnum) = ep_buf_ptr;
    PCD_SET_EP_RX_CNT(USB, epnum, p_endpoint_desc->wMaxPacketSize.size);
    PCD_CLEAR_RX_DTOG(USB, epnum);
    PCD_SET_EP_RX_STATUS(USB, epnum, USB_EP_RX_NAK);
  }

  ep_buf_ptr = (uint16_t)(ep_buf_ptr + p_endpoint_desc->wMaxPacketSize.size); // increment buffer pointer

  return true;
}

// Currently, single-buffered, and only 64 bytes at a time (max)

static void dcd_transmit_packet(xfer_ctl_t * xfer, uint16_t ep_ix)
{
  uint16_t len = (uint16_t)(xfer->total_len - xfer->queued_len);

  if(len > 64u) // max packet size for FS transfer
  {
    len = 64u;
  }
  dcd_write_packet_memory(*PCD_EP_TX_ADDRESS_PTR(USB,ep_ix), &(xfer->buffer[xfer->queued_len]), len);
  xfer->queued_len = (uint16_t)(xfer->queued_len + len);

  PCD_SET_EP_TX_CNT(USB,ep_ix,len);
  PCD_SET_EP_TX_STATUS(USB, ep_ix, USB_EP_TX_VALID);
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum,dir);

  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;

  if ( dir == TUSB_DIR_OUT )
  {
    // A setup token can occur immediately after an OUT STATUS packet so make sure we have a valid
    // buffer for the control endpoint.
    if (epnum == 0 && buffer == NULL)
    {
        xfer->buffer = (uint8_t*)_setup_packet;
        PCD_SET_EP_KIND(USB,0); // Expect a zero-byte INPUT
    }
    if(total_bytes > 64)
    {
      PCD_SET_EP_RX_CNT(USB,epnum,64);
    } else {
      PCD_SET_EP_RX_CNT(USB,epnum,total_bytes);
    }
    PCD_SET_EP_RX_STATUS(USB, epnum, USB_EP_RX_VALID);
  }
  else // IN
  {
    dcd_transmit_packet(xfer,epnum);
  }
  return true;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;

  if (ep_addr == 0) { // CTRL EP0 (OUT for setup)
    PCD_SET_EP_TX_STATUS(USB,ep_addr, USB_EP_TX_STALL);
  }

  if (ep_addr & 0x80) { // IN
    ep_addr &= 0x7F;
    PCD_SET_EP_TX_STATUS(USB,ep_addr, USB_EP_TX_STALL);
  } else { // OUT
    PCD_SET_EP_RX_STATUS(USB,ep_addr, USB_EP_RX_STALL);
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;
  if (ep_addr == 0)
  {
    PCD_SET_EP_TX_STATUS(USB,ep_addr, USB_EP_TX_NAK);
  }

  if (ep_addr & 0x80)
  { // IN
    ep_addr &= 0x7F;

    PCD_SET_EP_TX_STATUS(USB,ep_addr, USB_EP_TX_NAK);

    /* Reset to DATA0 if clearing stall condition. */
    PCD_CLEAR_TX_DTOG(USB,ep_addr);
  }
  else
  { // OUT
    /* Reset to DATA0 if clearing stall condition. */
    PCD_CLEAR_RX_DTOG(USB,ep_addr);

    PCD_SET_EP_RX_STATUS(USB,ep_addr, USB_EP_RX_VALID);
  }
}

// Packet buffer access can only be 8- or 16-bit.
/**
  * @brief Copy a buffer from user memory area to packet memory area (PMA).
  *        This uses byte-access for user memory (so support non-aligned buffers)
  *        and 16-bit access for packet memory.
  * @param   dst, byte address in PMA; must be 16-bit aligned
  * @param   src pointer to user memory area.
  * @param   wPMABufAddr address into PMA.
  * @param   wNBytes no. of bytes to be copied.
  * @retval None
  */
static void dcd_write_packet_memory(uint16_t dst, const void *__restrict src, size_t wNBytes)
{
  uint32_t n =  ((uint32_t)((uint32_t)wNBytes + 1U)) >> 1U;
  uint32_t i;
  uint16_t temp1, temp2;
  const uint8_t * srcVal;

#ifdef DEBUG
  if(((dst%2) != 0) ||
      (dst < DCD_STM32_BTABLE_BASE) ||
      dst >= (DCD_STM32_BTABLE_BASE + DCD_STM32_BTABLE_LENGTH))
    while(1) TU_BREAKPOINT();
#endif
  // The GCC optimizer will combine access to 32-bit sizes if we let it. Force
  // it volatile so that it won't do that.
  __IO uint16_t *pdwVal;

  srcVal = src;
  pdwVal = &pma[PMA_STRIDE*(dst>>1)];

  for (i = n; i != 0; i--)
  {
    temp1 = (uint16_t) *srcVal;
    srcVal++;
    temp2 = temp1 | ((uint16_t)((uint16_t) ((*srcVal) << 8U))) ;
    *pdwVal = temp2;
    pdwVal += PMA_STRIDE;
    srcVal++;
  }
}

/**
  * @brief Copy a buffer from user memory area to packet memory area (PMA).
  *        Uses byte-access of system memory and 16-bit access of packet memory
  * @param   wNBytes no. of bytes to be copied.
  * @retval None
  */
static void dcd_read_packet_memory(void *__restrict dst, uint16_t src, size_t wNBytes)
{
  uint32_t n = (uint32_t)wNBytes >> 1U;
  uint32_t i;
  // The GCC optimizer will combine access to 32-bit sizes if we let it. Force
  // it volatile so that it won't do that.
  __IO const uint16_t *pdwVal;
  uint32_t temp;

#ifdef DEBUG
  if((src%2) != 0 ||
      (src < DCD_STM32_BTABLE_BASE) ||
      src >= (DCD_STM32_BTABLE_BASE + DCD_STM32_BTABLE_LENGTH))
    while(1) TU_BREAKPOINT();
#endif

  pdwVal = &pma[PMA_STRIDE*(src>>1)];
  uint8_t *dstVal = (uint8_t*)dst;

  for (i = n; i != 0U; i--)
  {
    temp = *pdwVal;
    pdwVal += PMA_STRIDE;
    *dstVal++ = ((temp >> 0) & 0xFF);
    *dstVal++ = ((temp >> 8) & 0xFF);
  }

  if (wNBytes % 2)
  {
    temp = *pdwVal;
    pdwVal += PMA_STRIDE;
    *dstVal++ = ((temp >> 0) & 0xFF);
  }
}

#endif

