/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nathan Conrad
 *
 * Portions:
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2022 Simon Küppers (skuep)
 * Copyright (c) 2022 HiFiPhile
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
 *  - F070, F072, L053, F042F6
 *
 * It also should work with minimal changes for any ST MCU with an "USB A"/"PCD"/"HCD" peripheral. This
 *  covers:
 *
 * F04x, F072, F078, F070x6/B     1024 byte buffer
 * F102, F103                      512 byte buffer; no internal D+ pull-up (maybe many more changes?)
 * F302xB/C, F303xB/C, F373        512 byte buffer; no internal D+ pull-up
 * F302x6/8, F302xD/E2, F303xD/E  1024 byte buffer; no internal D+ pull-up
 * C0                             2048 byte buffer; 32-bit bus; host mode
 * G0                             2048 byte buffer; 32-bit bus; host mode
 * G4                             1024 byte buffer
 * H5                             2048 byte buffer; 32-bit bus; host mode
 * L0x2, L0x3                     1024 byte buffer
 * L1                              512 byte buffer
 * L4x2, L4x3                     1024 byte buffer
 * L5                             1024 byte buffer
 * U0                             1024 byte buffer; 32-bit bus
 * U535, U545                     2048 byte buffer; 32-bit bus; host mode
 * WB35, WB55                     1024 byte buffer
 *
 * To use this driver, you must:
 * - If you are using a device with crystal-less USB, set up the clock recovery system (CRS)
 * - Remap pins to be D+/D- on devices that they are shared (for example: F042Fx)
 *   - This is different to the normal "alternate function" GPIO interface, needs to go through SYSCFG->CFGRx register
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
 * - All EP BTABLE buffers are created based on max packet size of first EP opened with that address.
 * - Packet buffer memory is copied in the interrupt.
 *   - This is better for performance, but means interrupts are disabled for longer
 *   - DMA may be the best choice, but it could also be pushed to the USBD task.
 * - No double-buffering
 * - No DMA
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

#if CFG_TUD_ENABLED && defined(TUP_USBIP_FSDEV) && !(defined(TUP_USBIP_FSDEV_CH32) && CFG_TUD_WCH_USBIP_FSDEV == 0)

  #include "device/dcd.h"
  #include "fsdev_common.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// One of these for every EP IN & OUT, uses a bit of RAM....
typedef struct {
  uint8_t   *buffer;
  tu_fifo_t *ff;
  uint16_t   total_len;
  uint16_t   queued_len;
  uint16_t   max_packet_size;
  uint8_t    ep_idx;         // index for USB_EPnR register
  bool       iso_in_sending; // Workaround for ISO IN EP doesn't have interrupt mask
} xfer_ctl_t;

// EP allocator
typedef struct {
  uint8_t ep_num;
  uint8_t ep_type;
  bool    allocated[2];
} ep_alloc_t;

static xfer_ctl_t xfer_status[CFG_TUD_ENDPPOINT_MAX][2];
static ep_alloc_t ep_alloc_status[FSDEV_EP_COUNT];
static uint8_t    remoteWakeCountdown; // When wake is requested

//--------------------------------------------------------------------+
// Prototypes
//--------------------------------------------------------------------+

// into the stack.
static void handle_bus_reset(uint8_t rhport);
static void dcd_transmit_packet(xfer_ctl_t *xfer, uint16_t ep_ix);
static bool edpt_xfer(uint8_t rhport, uint8_t ep_num, tusb_dir_t dir);

// PMA allocation/access
static uint16_t ep_buf_ptr; ///< Points to first free memory location
static uint32_t dcd_pma_alloc(uint16_t len, bool dbuf);
static uint8_t  dcd_ep_alloc(uint8_t ep_addr, uint8_t ep_type);

static void edpt0_open(uint8_t rhport);

TU_ATTR_ALWAYS_INLINE static inline void edpt0_prepare_setup(void) {
  btable_set_rx_bufsize(0, BTABLE_BUF_RX, 8);
}

//--------------------------------------------------------------------+
// Inline helper
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline xfer_ctl_t *xfer_ctl_ptr(uint8_t epnum, uint8_t dir) {
  return &xfer_status[epnum][dir];
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+
bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rh_init;

  fsdev_core_reset();

  FSDEV_REG->CNTR = 0; // Enable USB

  #if !defined(FSDEV_BUS_32BIT)
  // BTABLE register does not exist any more on 32-bit bus devices
  FSDEV_REG->BTABLE = FSDEV_BTABLE_BASE;
  #endif

  // Enable interrupts for device mode
  FSDEV_REG->CNTR |=
    USB_CNTR_RESETM | USB_CNTR_ESOFM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM | USB_CNTR_PMAOVRM;

  handle_bus_reset(rhport);

  // Enable pull-up if supported
  dcd_connect(rhport);

  return true;
}

bool dcd_deinit(uint8_t rhport) {
  (void)rhport;

  fsdev_deinit();

  return true;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void)rhport;

  if (en) {
    FSDEV_REG->CNTR |= USB_CNTR_SOFM;
  } else {
    FSDEV_REG->CNTR &= ~USB_CNTR_SOFM;
  }
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;

  // Respond with status
  dcd_edpt_xfer(rhport, TUSB_DIR_IN_MASK | 0x00, NULL, 0, false);

  // DCD can only set address after status for this request is complete.
  // do it at dcd_edpt0_status_complete()
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void)rhport;

  FSDEV_REG->CNTR |= USB_CNTR_RESUME;
  remoteWakeCountdown = 4u; // required to be 1 to 15 ms, ESOF should trigger every 1ms.
}

static void handle_bus_reset(uint8_t rhport) {
  FSDEV_REG->DADDR = 0u; // disable USB Function

  for (uint32_t i = 0; i < FSDEV_EP_COUNT; i++) {
    // Clear EP allocation status
    ep_alloc_status[i].ep_num       = 0xFF;
    ep_alloc_status[i].ep_type      = 0xFF;
    ep_alloc_status[i].allocated[0] = false;
    ep_alloc_status[i].allocated[1] = false;
  }

  // Reset PMA allocation
  ep_buf_ptr = FSDEV_BTABLE_BASE + 8 * FSDEV_EP_COUNT;

  edpt0_open(rhport);              // open control endpoint (both IN & OUT)

  FSDEV_REG->DADDR = USB_DADDR_EF; // Enable USB Function
}

// Handle CTR interrupt for the TX/IN direction
static void handle_ctr_tx(uint32_t ep_id) {
  uint32_t ep_reg = ep_read(ep_id) | USB_EP_CTR_TX | USB_EP_CTR_RX;

  const uint8_t ep_num = ep_reg & USB_EPADDR_FIELD;
  xfer_ctl_t   *xfer   = xfer_ctl_ptr(ep_num, TUSB_DIR_IN);

  if (ep_is_iso(ep_reg)) {
    // Ignore spurious interrupts that we don't schedule
    // host can send IN token while there is no data to send, since ISO does not have NAK
    // this will result to zero length packet --> trigger interrupt (which cannot be masked)
    if (!xfer->iso_in_sending) {
      return;
    }
    xfer->iso_in_sending = false;
  #if FSDEV_USE_SBUF_ISO == 0
    uint8_t buf_id = (ep_reg & USB_EP_DTOG_TX) ? 0 : 1;
  #else
    uint8_t buf_id = BTABLE_BUF_TX;
  #endif
    btable_set_count(ep_id, buf_id, 0);
  }

  if (xfer->total_len != xfer->queued_len) {
    dcd_transmit_packet(xfer, ep_id);
  } else {
    dcd_event_xfer_complete(0, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }
}

static void handle_ctr_setup(uint32_t ep_id) {
  uint16_t rx_count = btable_get_count(ep_id, BTABLE_BUF_RX);
  uint16_t rx_addr  = btable_get_addr(ep_id, BTABLE_BUF_RX);
  uint8_t  setup_packet[8] TU_ATTR_ALIGNED(4);

  tu_hwfifo_read(PMA_BUF_AT(rx_addr), setup_packet, rx_count, NULL);

  // Clear CTR RX if another setup packet arrived before this, it will be discarded
  ep_write_clear_ctr(ep_id, TUSB_DIR_OUT);

  // Setup packet should always be 8 bytes. If not, we probably missed the packet
  if (rx_count == 8) {
    dcd_event_setup_received(0, (uint8_t *)setup_packet, true);
    // Hardware should reset EP0 RX/TX to NAK and both toggle to 1
  } else {
    // Missed setup packet !!!
    TU_BREAKPOINT();
    edpt0_prepare_setup();
  }
}

// Handle CTR interrupt for the RX/OUT direction
static void handle_ctr_rx(uint32_t ep_id) {
  uint32_t      ep_reg = ep_read(ep_id) | USB_EP_CTR_TX | USB_EP_CTR_RX;
  const uint8_t ep_num = ep_reg & USB_EPADDR_FIELD;
  const bool    is_iso = ep_is_iso(ep_reg);
  xfer_ctl_t   *xfer   = xfer_ctl_ptr(ep_num, TUSB_DIR_OUT);

  uint8_t buf_id;
  #if FSDEV_USE_SBUF_ISO == 0
  bool const dbl_buf = is_iso;
  #else
  bool const dbl_buf = false;
  #endif
  if (dbl_buf) {
    buf_id = (ep_reg & USB_EP_DTOG_RX) ? 0 : 1;
  } else {
    buf_id = BTABLE_BUF_RX;
  }
  const uint16_t   rx_count = btable_get_count(ep_id, buf_id);
  uint16_t         pma_addr = (uint16_t)btable_get_addr(ep_id, buf_id);
  fsdev_pma_buf_t *pma_buf  = PMA_BUF_AT(pma_addr);

  if (xfer->ff) {
    tu_hwfifo_read_to_fifo(pma_buf, xfer->ff, rx_count, NULL);
  } else {
    tu_hwfifo_read(pma_buf, xfer->buffer + xfer->queued_len, rx_count, NULL);
  }
  xfer->queued_len += rx_count;

  if ((rx_count < xfer->max_packet_size) || (xfer->queued_len >= xfer->total_len)) {
    // all bytes received or short packet

    // For ch32v203: reset rx bufsize to mps to prevent race condition to cause PMAOVR (occurs with msc write10)
    btable_set_rx_bufsize(ep_id, BTABLE_BUF_RX, xfer->max_packet_size);

    dcd_event_xfer_complete(0, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);

    // ch32 seems to unconditionally accept ZLP on EP0 OUT, which can incorrectly use queued_len of previous
    // transfer. So reset total_len and queued_len to 0.
    xfer->total_len = xfer->queued_len = 0;
  } else {
    // Set endpoint active again for receiving more data. Note that isochronous endpoints stay active always
    if (!is_iso) {
      const uint16_t cnt = tu_min16(xfer->total_len - xfer->queued_len, xfer->max_packet_size);
      btable_set_rx_bufsize(ep_id, BTABLE_BUF_RX, cnt);
    }
    ep_reg &= USB_EPREG_MASK | EP_STAT_MASK(TUSB_DIR_OUT); // will change RX Status, reserved other toggle bits
    ep_change_status(&ep_reg, TUSB_DIR_OUT, EP_STAT_VALID);
    ep_write(ep_id, ep_reg, false);
  }
}

void dcd_int_handler(uint8_t rhport) {
  uint32_t int_status = FSDEV_REG->ISTR;

  /* Put SOF flag at the beginning of ISR in case to get least amount of jitter if it is used for timing purposes */
  if (int_status & USB_ISTR_SOF) {
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_SOF;
    dcd_event_sof(0, FSDEV_REG->FNR & USB_FNR_FN, true);
  }

  if (int_status & USB_ISTR_RESET) {
    // USBRST is start of reset.
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_RESET;
    handle_bus_reset(rhport);
    dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
    return; // Don't do the rest of the things here; perhaps they've been cleared?
  }

  if (int_status & USB_ISTR_WKUP) {
    FSDEV_REG->CNTR &= ~USB_CNTR_LPMODE;
    FSDEV_REG->CNTR &= ~USB_CNTR_FSUSP;

    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_WKUP;
    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }

  if (int_status & USB_ISTR_SUSP) {
    /* Suspend is asserted for both suspend and unplug events. without Vbus monitoring,
     * these events cannot be differentiated, so we only trigger suspend. */

    /* Force low-power mode in the macrocell */
    FSDEV_REG->CNTR |= USB_CNTR_FSUSP;
    FSDEV_REG->CNTR |= USB_CNTR_LPMODE;

    /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_SUSP;
    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }

  if (int_status & USB_ISTR_ESOF) {
    if (remoteWakeCountdown == 1u) {
      FSDEV_REG->CNTR &= ~USB_CNTR_RESUME;
    }
    if (remoteWakeCountdown > 0u) {
      remoteWakeCountdown--;
    }
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_ESOF;
  }

  // loop to handle all pending CTR interrupts
  while (FSDEV_REG->ISTR & USB_ISTR_CTR) {
    // skip DIR bit, and use CTR TX/RX instead, since there is chance we have both TX/RX completed in one interrupt
    const uint32_t ep_id  = FSDEV_REG->ISTR & USB_ISTR_EP_ID;
    const uint32_t ep_reg = ep_read(ep_id);

    if (ep_reg & USB_EP_CTR_RX) {
  #ifdef FSDEV_BUS_32BIT
      /* https://www.st.com/resource/en/errata_sheet/es0561-stm32h503cbebkbrb-device-errata-stmicroelectronics.pdf
       * https://www.st.com/resource/en/errata_sheet/es0587-stm32u535xx-and-stm32u545xx-device-errata-stmicroelectronics.pdf
       * From H503/U535 errata: Buffer description table update completes after CTR interrupt triggers
       * Description:
       * - During OUT transfers, the correct transfer interrupt (CTR) is triggered a little before the last USB SRAM
       * accesses have completed. If the software responds quickly to the interrupt, the full buffer contents may not be
       * correct. Workaround:
       * - Software should ensure that a small delay is included before accessing the SRAM contents. This delay
       * should be 800 ns in Full Speed mode and 6.4 μs in Low Speed mode
       * - Since H5 can run up to 250Mhz -> 1 cycle = 4ns. Per errata, we need to wait 200 cycles. Though executing code
       * also takes time, so we'll wait 60 cycles (count = 20).
       * - Since Low Speed mode is not supported/popular, we will ignore it for now.
       *
       * Note: this errata may also apply to G0, U5, H5 etc.
       */
      volatile uint32_t cycle_count = 20; // defined as PCD_RX_PMA_CNT in stm32 hal_driver
      while (cycle_count > 0U) {
        cycle_count--;                    // each count take 3 cycles (1 for sub, jump, and compare)
      }
  #endif

      if (ep_reg & USB_EP_SETUP) {
        handle_ctr_setup(ep_id); // CTR will be clear after copied setup packet
      } else {
        ep_write_clear_ctr(ep_id, TUSB_DIR_OUT);
        handle_ctr_rx(ep_id);
      }
    }

    if (ep_reg & USB_EP_CTR_TX) {
      ep_write_clear_ctr(ep_id, TUSB_DIR_IN);
      handle_ctr_tx(ep_id);
    }
  }

  if (int_status & USB_ISTR_PMAOVR) {
    TU_BREAKPOINT();
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_PMAOVR;
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Invoked when a control transfer's status stage is complete.
// May help DCD to prepare for next control transfer, this API is optional.
void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;

  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD && request->bRequest == TUSB_REQ_SET_ADDRESS) {
    const uint8_t dev_addr = (uint8_t)request->wValue;
    FSDEV_REG->DADDR       = (USB_DADDR_EF | dev_addr);
  }

  edpt0_prepare_setup();
}

/***
 * Allocate a section of PMA
 * In case of double buffering, high 16bit is the address of 2nd buffer
 * During failure, TU_ASSERT is used. If this happens, rework/reallocate memory manually.
 */
static uint32_t dcd_pma_alloc(uint16_t len, bool dbuf) {
  uint8_t  blsize, num_block;
  uint16_t aligned_len = pma_align_buffer_size(len, &blsize, &num_block);
  (void)blsize;
  (void)num_block;

  uint32_t addr = ep_buf_ptr;
  ep_buf_ptr    = (uint16_t)(ep_buf_ptr + aligned_len); // increment buffer pointer

  if (dbuf) {
    addr |= ((uint32_t)ep_buf_ptr) << 16;
    ep_buf_ptr = (uint16_t)(ep_buf_ptr + aligned_len); // increment buffer pointer
  }

  // Verify packet buffer is not overflowed
  TU_ASSERT(ep_buf_ptr <= CFG_TUSB_FSDEV_PMA_SIZE, 0xFFFF);

  return addr;
}

/***
 * Allocate hardware endpoint
 */
static uint8_t dcd_ep_alloc(uint8_t ep_addr, uint8_t ep_type) {
  const uint8_t epnum = tu_edpt_number(ep_addr);
  const uint8_t dir   = tu_edpt_dir(ep_addr);

  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    // Check if already allocated
    if (ep_alloc_status[i].allocated[dir] && ep_alloc_status[i].ep_type == ep_type &&
        ep_alloc_status[i].ep_num == epnum) {
      return i;
    }

  #if FSDEV_USE_SBUF_ISO == 0
    bool const dbl_buf = ep_type == TUSB_XFER_ISOCHRONOUS;
  #else
    bool const dbl_buf = false;
  #endif

    // If EP of current direction is not allocated
    // For double-buffered mode both directions needs to be free
    if (!ep_alloc_status[i].allocated[dir] && (!dbl_buf || !ep_alloc_status[i].allocated[dir ^ 1])) {
      // Check if EP number is the same
      if (ep_alloc_status[i].ep_num == 0xFF || ep_alloc_status[i].ep_num == epnum) {
        // One EP pair has to be the same type
        if (ep_alloc_status[i].ep_type == 0xFF || ep_alloc_status[i].ep_type == ep_type) {
          ep_alloc_status[i].ep_num         = epnum;
          ep_alloc_status[i].ep_type        = ep_type;
          ep_alloc_status[i].allocated[dir] = true;

          return i;
        }
      }
    }
  }

  // Allocation failed
  TU_ASSERT(0);
}

void edpt0_open(uint8_t rhport) {
  (void)rhport;

  dcd_ep_alloc(0x0, TUSB_XFER_CONTROL);
  dcd_ep_alloc(0x80, TUSB_XFER_CONTROL);

  xfer_status[0][0].max_packet_size = CFG_TUD_ENDPOINT0_SIZE;
  xfer_status[0][0].ep_idx          = 0;

  xfer_status[0][1].max_packet_size = CFG_TUD_ENDPOINT0_SIZE;
  xfer_status[0][1].ep_idx          = 0;

  uint16_t pma_addr0 = dcd_pma_alloc(CFG_TUD_ENDPOINT0_SIZE, false);
  uint16_t pma_addr1 = dcd_pma_alloc(CFG_TUD_ENDPOINT0_SIZE, false);

  btable_set_addr(0, BTABLE_BUF_RX, pma_addr0);
  btable_set_addr(0, BTABLE_BUF_TX, pma_addr1);

  uint32_t ep_reg = ep_read(0) & ~USB_EPREG_MASK; // only get toggle bits
  ep_reg |= USB_EP_CONTROL;
  ep_change_status(&ep_reg, TUSB_DIR_IN, EP_STAT_NAK);
  ep_change_status(&ep_reg, TUSB_DIR_OUT, EP_STAT_NAK);
  // no need to explicitly set DTOG bits since we aren't masked DTOG bit

  edpt0_prepare_setup(); // prepare for setup packet
  ep_write(0, ep_reg, false);
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
  (void)rhport;
  const uint8_t    ep_addr     = desc_ep->bEndpointAddress;
  const uint8_t    ep_num      = tu_edpt_number(ep_addr);
  const tusb_dir_t dir         = tu_edpt_dir(ep_addr);
  const uint16_t   packet_size = tu_edpt_packet_size(desc_ep);
  const uint8_t    ep_idx      = dcd_ep_alloc(ep_addr, desc_ep->bmAttributes.xfer);
  TU_ASSERT(ep_idx < FSDEV_EP_COUNT);

  uint32_t ep_reg = ep_read(ep_idx) & ~USB_EPREG_MASK;
  ep_reg |= tu_edpt_number(ep_addr) | USB_EP_CTR_TX | USB_EP_CTR_RX;

  // Set type
  switch (desc_ep->bmAttributes.xfer) {
    case TUSB_XFER_BULK:
      ep_reg |= USB_EP_BULK;
      break;
    case TUSB_XFER_INTERRUPT:
      ep_reg |= USB_EP_INTERRUPT;
      break;

    default:
      // Note: ISO endpoint should use alloc / active functions
      TU_ASSERT(false);
  }

  /* Create a packet memory buffer area. */
  uint16_t pma_addr = dcd_pma_alloc(packet_size, false);
  btable_set_addr(ep_idx, dir == TUSB_DIR_IN ? BTABLE_BUF_TX : BTABLE_BUF_RX, pma_addr);

  xfer_ctl_t *xfer      = xfer_ctl_ptr(ep_num, dir);
  xfer->max_packet_size = packet_size;
  xfer->ep_idx          = ep_idx;

  ep_change_status(&ep_reg, dir, EP_STAT_NAK);
  ep_change_dtog(&ep_reg, dir, 0);

  // reserve other direction toggle bits
  if (dir == TUSB_DIR_IN) {
    ep_reg &= ~(USB_EPRX_STAT | USB_EP_DTOG_RX);
  } else {
    ep_reg &= ~(USB_EPTX_STAT | USB_EP_DTOG_TX);
  }

  ep_write(ep_idx, ep_reg, true);

  return true;
}

void dcd_edpt_close_all(uint8_t rhport) {
  dcd_int_disable(rhport);

  for (uint32_t i = 1; i < FSDEV_EP_COUNT; i++) {
    // Reset endpoint
    ep_write(i, 0, false);
    // Clear EP allocation status
    ep_alloc_status[i].ep_num       = 0xFF;
    ep_alloc_status[i].ep_type      = 0xFF;
    ep_alloc_status[i].allocated[0] = false;
    ep_alloc_status[i].allocated[1] = false;
  }

  dcd_int_enable(rhport);

  // Reset PMA allocation
  ep_buf_ptr = FSDEV_BTABLE_BASE + 8 * CFG_TUD_ENDPPOINT_MAX + 2 * CFG_TUD_ENDPOINT0_SIZE;
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  (void)rhport;

  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t dir    = tu_edpt_dir(ep_addr);
  const uint8_t ep_idx = dcd_ep_alloc(ep_addr, TUSB_XFER_ISOCHRONOUS);

  #if CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP != 0
  uint32_t pma_addr  = dcd_pma_alloc(largest_packet_size, true);
  uint16_t pma_addr2 = pma_addr >> 16;
  #else
  uint32_t pma_addr  = dcd_pma_alloc(largest_packet_size, false);
  uint16_t pma_addr2 = pma_addr;
  #endif

  #if FSDEV_USE_SBUF_ISO == 0
  btable_set_addr(ep_idx, 0, pma_addr);
  btable_set_addr(ep_idx, 1, pma_addr2);
  #else
  btable_set_addr(ep_idx, dir == TUSB_DIR_IN ? BTABLE_BUF_TX : BTABLE_BUF_RX, pma_addr);
  (void)pma_addr2;
  #endif

  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_num, dir);
  xfer->ep_idx     = ep_idx;

  return true;
}

bool dcd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
  (void)rhport;
  const uint8_t    ep_addr = desc_ep->bEndpointAddress;
  const uint8_t    ep_num  = tu_edpt_number(ep_addr);
  const tusb_dir_t dir     = tu_edpt_dir(ep_addr);
  xfer_ctl_t      *xfer    = xfer_ctl_ptr(ep_num, dir);

  const uint8_t ep_idx = xfer->ep_idx;

  xfer->max_packet_size = tu_edpt_packet_size(desc_ep);

  uint32_t ep_reg = ep_read(ep_idx) & ~USB_EPREG_MASK;
  ep_reg |= tu_edpt_number(ep_addr) | USB_EP_ISOCHRONOUS | USB_EP_CTR_TX | USB_EP_CTR_RX;
  #if FSDEV_USE_SBUF_ISO != 0
  ep_reg |= USB_EP_KIND;

  ep_change_status(&ep_reg, dir, EP_STAT_DISABLED);
  ep_change_dtog(&ep_reg, dir, 0);

  if (dir == TUSB_DIR_IN) {
    ep_reg &= ~(USB_EPRX_STAT | USB_EP_DTOG_RX);
  } else {
    ep_reg &= ~(USB_EPTX_STAT | USB_EP_DTOG_TX);
  }
  #else
  ep_change_status(&ep_reg, TUSB_DIR_IN, EP_STAT_DISABLED);
  ep_change_status(&ep_reg, TUSB_DIR_OUT, EP_STAT_DISABLED);
  ep_change_dtog(&ep_reg, dir, 0);
  ep_change_dtog(&ep_reg, (tusb_dir_t)(1 - dir), 1);
  #endif

  ep_write(ep_idx, ep_reg, true);

  return true;
}

// Currently, single-buffered, and only 64 bytes at a time (max)
static void dcd_transmit_packet(xfer_ctl_t *xfer, uint16_t ep_ix) {
  uint16_t len    = tu_min16(xfer->total_len - xfer->queued_len, xfer->max_packet_size);
  uint32_t ep_reg = ep_read(ep_ix) | USB_EP_CTR_TX | USB_EP_CTR_RX; // reserve CTR

  const bool is_iso = ep_is_iso(ep_reg);

  uint8_t buf_id;
  #if FSDEV_USE_SBUF_ISO == 0
  bool const dbl_buf = is_iso;
  #else
  bool const dbl_buf = false;
  #endif
  if (dbl_buf) {
    buf_id = (ep_reg & USB_EP_DTOG_TX) ? 1 : 0;
  } else {
    buf_id = BTABLE_BUF_TX;
  }
  uint16_t         addr_ptr = (uint16_t)btable_get_addr(ep_ix, buf_id);
  fsdev_pma_buf_t *pma_buf  = PMA_BUF_AT(addr_ptr);

  if (xfer->ff) {
    tu_hwfifo_write_from_fifo(pma_buf, xfer->ff, len, NULL);
  } else {
    tu_hwfifo_write(pma_buf, &(xfer->buffer[xfer->queued_len]), len, NULL);
  }
  xfer->queued_len += len;

  btable_set_count(ep_ix, buf_id, len);
  ep_change_status(&ep_reg, TUSB_DIR_IN, EP_STAT_VALID);

  if (is_iso) {
    xfer->iso_in_sending = true;
  }
  ep_reg &= USB_EPREG_MASK | EP_STAT_MASK(TUSB_DIR_IN); // only change TX Status, reserve other toggle bits
  ep_write(ep_ix, ep_reg, true);
}

static bool edpt_xfer(uint8_t rhport, uint8_t ep_num, tusb_dir_t dir) {
  (void)rhport;

  xfer_ctl_t   *xfer   = xfer_ctl_ptr(ep_num, dir);
  const uint8_t ep_idx = xfer->ep_idx;

  if (dir == TUSB_DIR_IN) {
    dcd_transmit_packet(xfer, ep_idx);
  } else {
    uint32_t ep_reg = ep_read(ep_idx) | USB_EP_CTR_TX | USB_EP_CTR_RX; // reserve CTR
    ep_reg &= USB_EPREG_MASK | EP_STAT_MASK(dir);

    uint16_t cnt = tu_min16(xfer->total_len, xfer->max_packet_size);

  #if FSDEV_USE_SBUF_ISO == 0
    bool const dbl_buf = ep_is_iso(ep_reg);
  #else
    bool const dbl_buf = false;
  #endif
    if (dbl_buf) {
      btable_set_rx_bufsize(ep_idx, 0, cnt);
      btable_set_rx_bufsize(ep_idx, 1, cnt);
    } else {
      btable_set_rx_bufsize(ep_idx, BTABLE_BUF_RX, cnt);
    }

    ep_change_status(&ep_reg, dir, EP_STAT_VALID);
    ep_write(ep_idx, ep_reg, true);
  }

  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);
  xfer_ctl_t      *xfer   = xfer_ctl_ptr(ep_num, dir);

  xfer->buffer     = buffer;
  xfer->ff         = NULL;
  xfer->total_len  = total_bytes;
  xfer->queued_len = 0;

  return edpt_xfer(rhport, ep_num, dir);
}

bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t *ff, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);
  xfer_ctl_t      *xfer   = xfer_ctl_ptr(ep_num, dir);

  xfer->buffer     = NULL;
  xfer->ff         = ff;
  xfer->total_len  = total_bytes;
  xfer->queued_len = 0;

  return edpt_xfer(rhport, ep_num, dir);
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);
  xfer_ctl_t      *xfer   = xfer_ctl_ptr(ep_num, dir);
  const uint8_t    ep_idx = xfer->ep_idx;

  uint32_t ep_reg = ep_read(ep_idx) | USB_EP_CTR_TX | USB_EP_CTR_RX; // reserve CTR bits
  ep_reg &= USB_EPREG_MASK | EP_STAT_MASK(dir);
  ep_change_status(&ep_reg, dir, EP_STAT_STALL);

  ep_write(ep_idx, ep_reg, true);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);
  xfer_ctl_t      *xfer   = xfer_ctl_ptr(ep_num, dir);
  const uint8_t    ep_idx = xfer->ep_idx;

  uint32_t ep_reg = ep_read(ep_idx) | USB_EP_CTR_TX | USB_EP_CTR_RX; // reserve CTR bits
  ep_reg &= USB_EPREG_MASK | EP_STAT_MASK(dir) | EP_DTOG_MASK(dir);

  if (!ep_is_iso(ep_reg)) {
    ep_change_status(&ep_reg, dir, EP_STAT_NAK);
  }
  ep_change_dtog(&ep_reg, dir, 0); // Reset to DATA0
  ep_write(ep_idx, ep_reg, true);
}

void dcd_int_enable(uint8_t rhport) {
  fsdev_int_enable(rhport);
}

void dcd_int_disable(uint8_t rhport) {
  fsdev_int_disable(rhport);
}

  #if defined(USB_BCDR_DPPU) || defined(SYSCFG_PMC_USB_PU) || defined(EXTEN_USBD_PU_EN)
void dcd_connect(uint8_t rhport) {
  fsdev_connect(rhport);
}

void dcd_disconnect(uint8_t rhport) {
  fsdev_disconnect(rhport);
}
  #endif

#endif
