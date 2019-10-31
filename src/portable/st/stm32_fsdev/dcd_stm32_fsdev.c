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
 *  - F070, F072, L053
 *
 * It also should work with minimal changes for any ST MCU with an "USB A"/"PCD"/"HCD" peripheral. This
 *  covers:
 *
 * F04x, F072, F078, 070x6/B      1024 byte buffer
 * F102, F103                      512 byte buffer; no internal D+ pull-up (maybe many more changes?)
 * F302xB/C, F303xB/C, F373        512 byte buffer; no internal D+ pull-up
 * F302x6/8, F302xD/E2, F303xD/E  1024 byte buffer; no internal D+ pull-up
 * L0x2, L0x3                     1024 byte buffer
 * L1                              512 byte buffer
 * L4x2, L4x3                     1024 byte buffer
 *
 * To use this driver, you must:
 * - Enable USB clock; Perhaps use __HAL_RCC_USB_CLK_ENABLE();
 * - (Optionally configure GPIO HAL to tell it the USB driver is using the USB pins)
 * - call tusb_init();
 * - periodically call tusb_task();
 *
 * Assumptions of the driver:
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
 *   - Perhaps error interrupts should be reported to the stack, or cause a device reset?
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

#if defined(STM32F102x6) || defined(STM32F102xB) || \
    defined(STM32F103x6) || defined(STM32F103xB) || \
    defined(STM32F103xE) || defined(STM32F103xG)
#define STM32F1_FSDEV
#endif

#if (TUSB_OPT_DEVICE_ENABLED) && ( \
      (CFG_TUSB_MCU == OPT_MCU_STM32F0                          ) || \
      (CFG_TUSB_MCU == OPT_MCU_STM32F1 && defined(STM32F1_FSDEV)) || \
      (CFG_TUSB_MCU == OPT_MCU_STM32F3                          ) || \
      (CFG_TUSB_MCU == OPT_MCU_STM32L0                          ) \
    )

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

TU_VERIFY_STATIC((MAX_EP_COUNT) <= STFSDEV_EP_COUNT, "Only 8 endpoints supported on the hardware");

TU_VERIFY_STATIC(((DCD_STM32_BTABLE_BASE) + (DCD_STM32_BTABLE_LENGTH))<=(PMA_LENGTH),
    "BTABLE does not fit in PMA RAM");

TU_VERIFY_STATIC(((DCD_STM32_BTABLE_BASE) % 8) == 0, "BTABLE base must be aligned to 8 bytes");

// One of these for every EP IN & OUT, uses a bit of RAM....
typedef struct
{
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_packet_size;
} xfer_ctl_t;

static xfer_ctl_t xfer_status[MAX_EP_COUNT][2];

static inline xfer_ctl_t* xfer_ctl_ptr(uint32_t epnum, uint32_t dir)
{
  return &xfer_status[epnum][dir];
}

static TU_ATTR_ALIGNED(4) uint32_t _setup_packet[6];

static uint8_t newDADDR; // Used to set the new device address during the CTR IRQ handler
static uint8_t remoteWakeCountdown; // When wake is requested

// EP Buffers assigned from end of memory location, to minimize their chance of crashing
// into the stack.
static uint16_t ep_buf_ptr;
static void dcd_handle_bus_reset(void);
static bool dcd_write_packet_memory(uint16_t dst, const void *__restrict src, size_t wNBytes);
static bool dcd_read_packet_memory(void *__restrict dst, uint16_t src, size_t wNBytes);
static void dcd_transmit_packet(xfer_ctl_t * xfer, uint16_t ep_ix);
static uint16_t dcd_ep_ctr_handler(void);


// Using a function due to better type checks
// This seems better than having to do type casts everywhere else
static inline void reg16_clear_bits(__IO uint16_t *reg, uint16_t mask) {
  *reg = (uint16_t)(*reg & ~mask);
}

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
  reg16_clear_bits(&USB->CNTR, USB_CNTR_PDWN);// Remove powerdown
  // Wait startup time, for F042 and F070, this is <= 1 us.
  for(uint32_t i = 0; i<200; i++) // should be a few us
  {
    asm("NOP");
  }
  USB->CNTR = 0; // Enable USB

  USB->BTABLE = DCD_STM32_BTABLE_BASE;

  reg16_clear_bits(&USB->ISTR, USB_ISTR_ALL_EVENTS); // Clear pending interrupts

  // Reset endpoints to disabled
  for(uint32_t i=0; i<STFSDEV_EP_COUNT; i++)
  {
    // This doesn't clear all bits since some bits are "toggle", but does set the type to DISABLED.
    pcd_set_endpoint(USB,i,0u);
  }

  // Initialize the BTABLE for EP0 at this point (though setting up the EP0R is unneeded)
  // This is actually not necessary, but helps debugging to start with a blank RAM area
  for(uint32_t i=0;i<(DCD_STM32_BTABLE_LENGTH>>1); i++)
  {
    pma[PMA_STRIDE*(DCD_STM32_BTABLE_BASE + i)] = 0u;
  }
  USB->CNTR |= USB_CNTR_RESETM | USB_CNTR_SOFM | USB_CNTR_ESOFM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
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
  // Member here forces write to RAM before allowing ISR to execute
  __DSB();
  __ISB();
#if CFG_TUSB_MCU == OPT_MCU_STM32F0 || CFG_TUSB_MCU == OPT_MCU_STM32L0
  NVIC_EnableIRQ(USB_IRQn);
#elif CFG_TUSB_MCU == OPT_MCU_STM32F3
  NVIC_EnableIRQ(USB_HP_CAN_TX_IRQn);
  NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);
  NVIC_EnableIRQ(USBWakeUp_IRQn);
#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
  NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  NVIC_EnableIRQ(USBWakeUp_IRQn);
#else
  #error Unknown arch in USB driver
#endif
}

// Disable device interrupt
void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;

#if CFG_TUSB_MCU == OPT_MCU_STM32F0 || CFG_TUSB_MCU == OPT_MCU_STM32L0
  NVIC_DisableIRQ(USB_IRQn);
#elif CFG_TUSB_MCU == OPT_MCU_STM32F3
  NVIC_DisableIRQ(USB_HP_CAN_TX_IRQn);
  NVIC_DisableIRQ(USB_LP_CAN_RX0_IRQn);
  NVIC_DisableIRQ(USBWakeUp_IRQn);
#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  NVIC_DisableIRQ(USB_HP_CAN1_TX_IRQn);
  NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  NVIC_DisableIRQ(USBWakeUp_IRQn);
#else
  #error Unknown arch in USB driver
#endif

  // CMSIS has a membar after disabling interrupts
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

  USB->CNTR |= (uint16_t) USB_CNTR_RESUME;
  remoteWakeCountdown = 4u; // required to be 1 to 15 ms, ESOF should trigger every 1ms.
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

#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic pop
#endif

static void dcd_handle_bus_reset(void)
{
  //__IO uint16_t * const epreg = &(EPREG(0));
  USB->DADDR = 0u; // disable USB peripheral by clearing the EF flag

  // Clear all EPREG (or maybe this is automatic? I'm not sure)
  for(uint32_t i=0; i<STFSDEV_EP_COUNT; i++)
  {
    pcd_set_endpoint(USB,i,0u);
  }

  ep_buf_ptr = DCD_STM32_BTABLE_BASE + 8*MAX_EP_COUNT; // 8 bytes per endpoint (two TX and two RX words, each)
  dcd_edpt_open (0, &ep0OUT_desc);
  dcd_edpt_open (0, &ep0IN_desc);
  newDADDR = 0u;
  USB->DADDR = USB_DADDR_EF; // Set enable flag, and leaving the device address as zero.
}

// FIXME: Defined to return uint16 so that ASSERT can be used, even though a return value is not needed.
static uint16_t dcd_ep_ctr_handler(void)
{
  uint32_t count=0U;
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
        pcd_clear_tx_ep_ctr(USB, 0);

        xfer_ctl_t * xfer = xfer_ctl_ptr(EPindex,TUSB_DIR_IN);

        if((xfer->total_len == xfer->queued_len))
        {
          dcd_event_xfer_complete(0u, (uint8_t)(0x80 + EPindex), xfer->total_len, XFER_RESULT_SUCCESS, true);
          if((newDADDR != 0) && ( xfer->total_len == 0U))
          {
            // Delayed setting of the DADDR after the 0-len DATA packet acking the request is sent.
            reg16_clear_bits(&USB->DADDR, USB_DADDR_ADD);
            USB->DADDR = (uint16_t)(USB->DADDR | newDADDR); // leave the enable bit set
            newDADDR = 0;
          }
          if(xfer->total_len == 0) // Probably a status message?
          {
            pcd_clear_rx_dtog(USB,EPindex);
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

        xfer_ctl_t *xfer = xfer_ctl_ptr(EPindex,TUSB_DIR_OUT);

        //ep = &hpcd->OUT_ep[0];
        wEPVal = pcd_get_endpoint(USB, EPindex);

        if ((wEPVal & USB_EP_SETUP) != 0U) // SETUP
        {
          // The setup_received function uses memcpy, so this must first copy the setup data into
          // user memory, to allow for the 32-bit access that memcpy performs.
          uint8_t userMemBuf[8];
          /* Get SETUP Packet*/
          count = pcd_get_ep_rx_cnt(USB, EPindex);
          if(count == 8) // Setup packet should always be 8 bytes. If not, ignore it, and try again.
          {
            // Must reset EP to NAK (in case it had been stalling) (though, maybe too late here)
            pcd_set_ep_rx_status(USB,0u,USB_EP_RX_NAK);
            pcd_set_ep_tx_status(USB,0u,USB_EP_TX_NAK);
            dcd_read_packet_memory(userMemBuf, *pcd_ep_rx_address_ptr(USB,EPindex), 8);
            dcd_event_setup_received(0, (uint8_t*)userMemBuf, true);
          }
          /* SETUP bit kept frozen while CTR_RX = 1*/
          pcd_clear_rx_ep_ctr(USB, EPindex);
        }
        else if ((wEPVal & USB_EP_CTR_RX) != 0U) // OUT
        {

          pcd_clear_rx_ep_ctr(USB, EPindex);

          /* Get Control Data OUT Packet */
          count = pcd_get_ep_rx_cnt(USB,EPindex);

          if (count != 0U)
          {
            dcd_read_packet_memory(xfer->buffer, *pcd_ep_rx_address_ptr(USB,EPindex), count);
            xfer->queued_len = (uint16_t)(xfer->queued_len + count);
          }

          /* Process Control Data OUT status Packet*/
          dcd_event_xfer_complete(0, EPindex, xfer->total_len, XFER_RESULT_SUCCESS, true);

          pcd_set_ep_rx_cnt(USB, EPindex, CFG_TUD_ENDPOINT0_SIZE);
          if(EPindex == 0u && xfer->total_len == 0u)
          {
            pcd_set_ep_rx_status(USB, EPindex, USB_EP_RX_VALID);// Await next SETUP
          }
        }
      }
    }
    else /* Decode and service non control endpoints interrupt  */
    {
      /* process related endpoint register */
      wEPVal = pcd_get_endpoint(USB, EPindex);
      if ((wEPVal & USB_EP_CTR_RX) != 0U) // OUT
      {
        /* clear int flag */
        pcd_clear_rx_ep_ctr(USB, EPindex);

        xfer_ctl_t * xfer = xfer_ctl_ptr(EPindex,TUSB_DIR_OUT);

        //ep = &hpcd->OUT_ep[EPindex];

        count = pcd_get_ep_rx_cnt(USB, EPindex);
        if (count != 0U)
        {
          dcd_read_packet_memory(&(xfer->buffer[xfer->queued_len]),
              *pcd_ep_rx_address_ptr(USB,EPindex), count);
        }

        /*multi-packet on the NON control OUT endpoint */
        xfer->queued_len = (uint16_t)(xfer->queued_len + count);

        if ((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len))
        {
          /* RX COMPLETE */
          dcd_event_xfer_complete(0, EPindex, xfer->queued_len, XFER_RESULT_SUCCESS, true);
          // Though the host could still send, we don't know.
          // Does the bulk pipe need to be reset to valid to allow for a ZLP?
        }
        else
        {
          uint32_t remaining = (uint32_t)xfer->total_len - (uint32_t)xfer->queued_len;
          if(remaining >= xfer->max_packet_size) {
            pcd_set_ep_rx_cnt(USB, EPindex,xfer->max_packet_size);
          } else {
            pcd_set_ep_rx_cnt(USB, EPindex,remaining);
          }

          pcd_set_ep_rx_status(USB, EPindex, USB_EP_RX_VALID);
        }

      } /* if((wEPVal & EP_CTR_RX) */

      if ((wEPVal & USB_EP_CTR_TX) != 0U) // IN
      {
        /* clear int flag */
        pcd_clear_tx_ep_ctr(USB, EPindex);

        xfer_ctl_t * xfer = xfer_ctl_ptr(EPindex,TUSB_DIR_IN);

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

static void dcd_fs_irqHandler(void) {

  uint32_t int_status = USB->ISTR;
  //const uint32_t handled_ints = USB_ISTR_CTR | USB_ISTR_RESET | USB_ISTR_WKUP
  //    | USB_ISTR_SUSP | USB_ISTR_SOF | USB_ISTR_ESOF;
  // unused IRQs: (USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_L1REQ )

  // The ST driver loops here on the CTR bit, but that loop has been moved into the
  // dcd_ep_ctr_handler(), so less need to loop here. The other interrupts shouldn't
  // be triggered repeatedly.

  if(int_status & USB_ISTR_RESET) {
    // USBRST is start of reset.
    reg16_clear_bits(&USB->ISTR, USB_ISTR_RESET);
    dcd_handle_bus_reset();
    dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
    return; // Don't do the rest of the things here; perhaps they've been cleared?
  }

  if (int_status & USB_ISTR_CTR)
  {
    /* servicing of the endpoint correct transfer interrupt */
    /* clear of the CTR flag into the sub */
    dcd_ep_ctr_handler();
    reg16_clear_bits(&USB->ISTR, USB_ISTR_CTR);
  }

  if (int_status & USB_ISTR_WKUP)
  {
    reg16_clear_bits(&USB->CNTR, USB_CNTR_LPMODE);
    reg16_clear_bits(&USB->CNTR, USB_CNTR_FSUSP);
    reg16_clear_bits(&USB->ISTR, USB_ISTR_WKUP);
    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }

  if (int_status & USB_ISTR_SUSP)
  {
    /* Suspend is asserted for both suspend and unplug events. without Vbus monitoring,
     * these events cannot be differentiated, so we only trigger suspend. */

    /* Force low-power mode in the macrocell */
    USB->CNTR |= USB_CNTR_FSUSP;
    USB->CNTR |= USB_CNTR_LPMODE;

    /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
    reg16_clear_bits(&USB->ISTR, USB_ISTR_SUSP);
    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }

  if(int_status & USB_ISTR_SOF) {
    reg16_clear_bits(&USB->ISTR, USB_ISTR_SOF);
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }

  if(int_status & USB_ISTR_ESOF) {
    if(remoteWakeCountdown == 1u)
    {
      USB->CNTR &= (uint16_t)(~USB_CNTR_RESUME);
    }
    if(remoteWakeCountdown > 0u)
    {
      remoteWakeCountdown--;
    }
    reg16_clear_bits(&USB->ISTR, USB_ISTR_ESOF);
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
  const uint16_t epMaxPktSize = p_endpoint_desc->wMaxPacketSize.size;
  // Isochronous not supported (yet), and some other driver assumptions.

  TU_ASSERT(p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);
  TU_ASSERT(epnum < MAX_EP_COUNT);

  // Set type
  switch(p_endpoint_desc->bmAttributes.xfer) {
  case TUSB_XFER_CONTROL:
    pcd_set_eptype(USB, epnum, USB_EP_CONTROL);
    break;
#if (0)
  case TUSB_XFER_ISOCHRONOUS: // FIXME: Not yet supported
    pcd_set_eptype(USB, epnum, USB_EP_ISOCHRONOUS); break;
    break;
#endif

  case TUSB_XFER_BULK:
    pcd_set_eptype(USB, epnum, USB_EP_BULK);
    break;

  case TUSB_XFER_INTERRUPT:
    pcd_set_eptype(USB, epnum, USB_EP_INTERRUPT);
    break;

  default:
    TU_ASSERT(false);
    return false;
  }

  pcd_set_ep_address(USB, epnum, epnum);
  // Be normal, for now, instead of only accepting zero-byte packets (on control endpoint)
  // or being double-buffered (bulk endpoints)
  pcd_clear_ep_kind(USB,0);

  if(dir == TUSB_DIR_IN)
  {
    *pcd_ep_tx_address_ptr(USB, epnum) = ep_buf_ptr;
    pcd_set_ep_tx_cnt(USB, epnum, p_endpoint_desc->wMaxPacketSize.size);
    pcd_clear_tx_dtog(USB, epnum);
    pcd_set_ep_tx_status(USB,epnum,USB_EP_TX_NAK);
  }
  else
  {
    *pcd_ep_rx_address_ptr(USB, epnum) = ep_buf_ptr;
    pcd_set_ep_rx_cnt(USB, epnum, p_endpoint_desc->wMaxPacketSize.size);
    pcd_clear_rx_dtog(USB, epnum);
    pcd_set_ep_rx_status(USB, epnum, USB_EP_RX_NAK);
  }

  xfer_ctl_ptr(epnum, dir)->max_packet_size = epMaxPktSize;
  ep_buf_ptr = (uint16_t)(ep_buf_ptr + p_endpoint_desc->wMaxPacketSize.size); // increment buffer pointer

  return true;
}

// Currently, single-buffered, and only 64 bytes at a time (max)

static void dcd_transmit_packet(xfer_ctl_t * xfer, uint16_t ep_ix)
{
  uint16_t len = (uint16_t)(xfer->total_len - xfer->queued_len);

  if(len > xfer->max_packet_size) // max packet size for FS transfer
  {
    len = xfer->max_packet_size;
  }
  uint16_t oldAddr = *pcd_ep_tx_address_ptr(USB,ep_ix);
  dcd_write_packet_memory(oldAddr, &(xfer->buffer[xfer->queued_len]), len);
  xfer->queued_len = (uint16_t)(xfer->queued_len + len);

  pcd_set_ep_tx_cnt(USB,ep_ix,len);
  pcd_set_ep_tx_status(USB, ep_ix, USB_EP_TX_VALID);
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = xfer_ctl_ptr(epnum,dir);

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
    }
    if(total_bytes > xfer->max_packet_size)
    {
      pcd_set_ep_rx_cnt(USB,epnum,xfer->max_packet_size);
    } else {
      pcd_set_ep_rx_cnt(USB,epnum,total_bytes);
    }
    pcd_set_ep_rx_status(USB, epnum, USB_EP_RX_VALID);
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

  if (ep_addr & 0x80)
  { // IN
    pcd_set_ep_tx_status(USB, ep_addr & 0x7F, USB_EP_TX_STALL);
  }
  else
  { // OUT
    pcd_set_ep_rx_status(USB, ep_addr, USB_EP_RX_STALL);
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;

  if (ep_addr & 0x80)
  { // IN
    ep_addr &= 0x7F;

    pcd_set_ep_tx_status(USB,ep_addr, USB_EP_TX_NAK);

    /* Reset to DATA0 if clearing stall condition. */
    pcd_clear_tx_dtog(USB,ep_addr);
  }
  else
  { // OUT
    /* Reset to DATA0 if clearing stall condition. */
    pcd_clear_rx_dtog(USB,ep_addr);

    pcd_set_ep_rx_status(USB,ep_addr, USB_EP_RX_NAK);
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
static bool dcd_write_packet_memory(uint16_t dst, const void *__restrict src, size_t wNBytes)
{
  uint32_t n =  ((uint32_t)wNBytes + 1U) >> 1U;
  uint32_t i;
  uint16_t temp1, temp2;
  const uint8_t * srcVal;

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
  return true;
}

/**
  * @brief Copy a buffer from user memory area to packet memory area (PMA).
  *        Uses byte-access of system memory and 16-bit access of packet memory
  * @param   wNBytes no. of bytes to be copied.
  * @retval None
  */
static bool dcd_read_packet_memory(void *__restrict dst, uint16_t src, size_t wNBytes)
{
  uint32_t n = (uint32_t)wNBytes >> 1U;
  uint32_t i;
  // The GCC optimizer will combine access to 32-bit sizes if we let it. Force
  // it volatile so that it won't do that.
  __IO const uint16_t *pdwVal;
  uint32_t temp;

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
  return true;
}


// Interrupt handlers
#if CFG_TUSB_MCU == OPT_MCU_STM32F0 || CFG_TUSB_MCU == OPT_MCU_STM32L0
void USB_IRQHandler(void)
{
  dcd_fs_irqHandler();
}

#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
void USB_HP_IRQHandler(void)
{
  dcd_fs_irqHandler();
}
void USB_LP_IRQHandler(void)
{
  dcd_fs_irqHandler();
}
void USBWakeUp_IRQHandler(void)
{
  dcd_fs_irqHandler();
}

#elif (CFG_TUSB_MCU) == (OPT_MCU_STM32F3)
// USB defaults to using interrupts 19, 20, and 42 (based on SYSCFG_CFGR1.USB_IT_RMP)
// FIXME: Do all three need to be handled, or just the LP one?
// USB high-priority interrupt (Channel 19): Triggered only by a correct
// transfer event for isochronous and double-buffer bulk transfer to reach
// the highest possible transfer rate.
void USB_HP_CAN_TX_IRQHandler(void)
{
  dcd_fs_irqHandler();
}

// USB low-priority interrupt (Channel 20): Triggered by all USB events
// (Correct transfer, USB reset, etc.). The firmware has to check the
// interrupt source before serving the interrupt.
void USB_LP_CAN_RX0_IRQHandler(void)
{
  dcd_fs_irqHandler();
}

// USB wakeup interrupt (Channel 42): Triggered by the wakeup event from the USB
// Suspend mode.
void USBWakeUp_IRQHandler(void)
{
  dcd_fs_irqHandler();
}

#else
  #error Which IRQ handler do you need?
#endif

#endif

