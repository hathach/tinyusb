/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 HiFiPhile (Zixun LI)
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
 * This driver provides USB Host controller support for STM32 MCUs with "USB A"/"PCD"/"HCD" peripheral.
 * This covers these MCU families:
 *
 * C0                             2048 byte buffer; 32-bit bus; host mode
 * G0                             2048 byte buffer; 32-bit bus; host mode
 * H5                             2048 byte buffer; 32-bit bus; host mode
 * U535, U545                     2048 byte buffer; 32-bit bus; host mode
 *
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_FSDEV) && \
    TU_CHECK_MCU(OPT_MCU_STM32C0, OPT_MCU_STM32G0, OPT_MCU_STM32H5, OPT_MCU_STM32U5)

#include "host/hcd.h"
#include "host/usbh.h"

#if defined(TUP_USBIP_FSDEV_STM32)
  #include "fsdev_stm32.h"
#else
  #error "Unknown USB IP"
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Debug level for FSDEV
#define FSDEV_DEBUG 1

// Max number of endpoints application can open, can be larger than FSDEV_EP_COUNT
#ifndef CFG_TUH_FSDEV_ENDPOINT_MAX
  #define CFG_TUH_FSDEV_ENDPOINT_MAX 16u
#endif

TU_VERIFY_STATIC(CFG_TUH_FSDEV_ENDPOINT_MAX <= 255, "currently only use 8-bit for index");

enum {
  HCD_XFER_ERROR_MAX = 3
};

// Host driver struct for each opened endpoint
typedef struct {
  uint8_t *buffer;
  uint16_t buflen;
  uint16_t max_packet_size;
  uint8_t dev_addr;
  uint8_t ep_addr;
  uint8_t ep_type;
  uint8_t interval;
  bool low_speed;
  bool allocated;
  bool next_setup;
} hcd_endpoint_t;

// Additional info for each channel when it is active
typedef struct {
  hcd_endpoint_t* edpt[2]; // OUT/IN
  uint16_t queued_len[2];
  uint8_t dev_addr;
  uint8_t ep_num;
  uint8_t ep_type;
  bool allocated[2];
  uint8_t retry[2];
  uint8_t result;
} hcd_xfer_t;

// Root hub port state
static struct {
  bool connected;
} _hcd_port;

typedef struct {
  hcd_xfer_t xfer[FSDEV_EP_COUNT];
  hcd_endpoint_t edpt[CFG_TUH_FSDEV_ENDPOINT_MAX];
} hcd_data_t;

hcd_data_t _hcd_data;

//--------------------------------------------------------------------+
// Prototypes
//--------------------------------------------------------------------+

static uint8_t endpoint_alloc(void);
static uint8_t endpoint_find(uint8_t dev_addr, uint8_t ep_addr);
static uint32_t hcd_pma_alloc(uint8_t channel, tusb_dir_t dir, uint16_t len);
static uint8_t channel_alloc(uint8_t dev_addr, uint8_t ep_addr, uint8_t ep_type);
static bool hcd_write_packet_memory(uint16_t dst, const void *__restrict src, uint16_t nbytes);
static bool hcd_read_packet_memory(void *__restrict dst, uint16_t src, uint16_t nbytes);
static bool edpt_xfer_kickoff(uint8_t ep_id);
static bool channel_xfer_start(uint8_t ch_id, tusb_dir_t dir);
static void edpoint_close(uint8_t ep_id);
static void port_status_handler(uint8_t rhport, bool in_isr);
//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

static inline void endpoint_dealloc(hcd_endpoint_t* edpt) {
  edpt->allocated = false;
}

static inline void channel_dealloc(hcd_xfer_t* xfer, tusb_dir_t dir) {
  xfer->allocated[dir] = false;
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// Optional HCD configuration, called by tuh_configure()
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport;
  (void) cfg_id;
  (void) cfg_param;
  return false;
}

// Initialize controller to host mode
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;

  // Follow the RM mentions to use a special ordering of PDWN and FRES
  for (volatile uint32_t i = 0; i < 200; i++) {
    asm("NOP");
  }

  // Perform USB peripheral reset
  FSDEV_REG->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
  for (volatile uint32_t i = 0; i < 200; i++) {
    asm("NOP");
  }

  FSDEV_REG->CNTR &= ~USB_CNTR_PDWN;

  // Wait startup time
  for (volatile uint32_t i = 0; i < 200; i++) {
    asm("NOP");
  }

  FSDEV_REG->CNTR = USB_CNTR_HOST; // Enable USB in Host mode

#if !defined(FSDEV_BUS_32BIT)
  // BTABLE register does not exist on 32-bit bus devices
  FSDEV_REG->BTABLE = FSDEV_BTABLE_BASE;
#endif

  FSDEV_REG->ISTR = 0; // Clear pending interrupts

  // Reset channels to disabled
  for (uint32_t i = 0; i < FSDEV_EP_COUNT; i++) {
    ch_write(i, 0u, false);
  }

  tu_memclr(&_hcd_data, sizeof(_hcd_data));

  // Enable interrupts for host mode
  FSDEV_REG->CNTR |= USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_SUSPM |
                     USB_CNTR_WKUPM | USB_CNTR_ERRM | USB_CNTR_PMAOVRM;

  // Initialize port state
  _hcd_port.connected = false;

  fsdev_connect(rhport);

  return true;
}

static void port_status_handler(uint8_t rhport, bool in_isr) {
  uint32_t const fnr_reg = FSDEV_REG->FNR;
  uint32_t const istr_reg = FSDEV_REG->ISTR;
  // SE0 detected USB Disconnected state
  if ((fnr_reg & (USB_FNR_RXDP | USB_FNR_RXDM)) == 0U) {
    _hcd_port.connected = false;
    hcd_event_device_remove(rhport, in_isr);
    return;
  }

  if (!_hcd_port.connected) {
    // J-state or K-state detected & LastState=Disconnected
    if (((fnr_reg & USB_FNR_RXDP) != 0U) || ((istr_reg & USB_ISTR_LS_DCONN) != 0U)) {
      _hcd_port.connected = true;
      hcd_event_device_attach(rhport, in_isr);
    }
  } else {
    // J-state or K-state detected & lastState=Connected: a Missed disconnection is detected
    if (((fnr_reg & USB_FNR_RXDP) != 0U) || ((istr_reg & USB_ISTR_LS_DCONN) != 0U)) {
      _hcd_port.connected = false;
      hcd_event_device_remove(rhport, in_isr);
    }
  }
}

// Handle CTR interrupt for the TX/OUT direction
static void handle_ctr_tx(uint32_t ch_id) {
  uint32_t ch_reg = ch_read(ch_id) | USB_EP_CTR_TX | USB_EP_CTR_RX;

  uint8_t const ep_num = ch_reg & USB_EPADDR_FIELD;
  uint8_t const daddr = (ch_reg & USB_CHEP_DEVADDR_Msk) >> USB_CHEP_DEVADDR_Pos;

  uint8_t ep_id = endpoint_find(daddr, ep_num);
  TU_VERIFY(ep_id != TUSB_INDEX_INVALID_8, );

  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  TU_VERIFY(xfer->allocated[TUSB_DIR_OUT],);

  // Manage Correct Transaction
  if ((ch_reg & USB_CH_ERRTX) == 0U) {
    // Acked
    if ((ch_reg & USB_CH_TX_STTX) == USB_CH_TX_ACK_SBUF) {
      if (edpt->buflen != xfer->queued_len[TUSB_DIR_OUT]) {
        uint16_t const len = tu_min16(edpt->buflen - xfer->queued_len[TUSB_DIR_OUT], edpt->max_packet_size);
        uint16_t pma_addr = (uint16_t) btable_get_addr(ch_id, BTABLE_BUF_TX);
        hcd_write_packet_memory(pma_addr, &(edpt->buffer[xfer->queued_len[TUSB_DIR_OUT]]), len);
        btable_set_count(ch_id, BTABLE_BUF_TX, len);
        xfer->queued_len[TUSB_DIR_OUT] += len;

        ch_change_status(&ch_reg, TUSB_DIR_OUT, EP_STAT_VALID);
        ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_OUT); // only change TX Status, reserve other toggle bits
        ch_write(ch_id, ch_reg, false);
      } else {
        channel_dealloc(xfer, TUSB_DIR_OUT);
        hcd_event_xfer_complete(daddr, ep_num, xfer->queued_len[TUSB_DIR_OUT], XFER_RESULT_SUCCESS, true);
      }
    } else if ((ch_reg & USB_CH_TX_STTX) == USB_CH_TX_NAK) {
      // NAKed
      if (edpt->ep_type != TUSB_XFER_INTERRUPT) {
        ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_OUT); // will change TX Status, reserved other toggle bits
        ch_change_status(&ch_reg, TUSB_DIR_OUT, EP_STAT_VALID);
        ch_write(ch_id, ch_reg, false);
      }
    } else if ((ch_reg & USB_CH_TX_STTX) == USB_CH_TX_STALL) {
      // STALLed
      channel_dealloc(xfer, TUSB_DIR_OUT);
      ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_OUT); // will change TX Status, reserved other toggle bits
      ch_change_status(&ch_reg, TUSB_DIR_OUT, EP_STAT_DISABLED);
      ch_write(ch_id,  ch_reg, false);
      hcd_event_xfer_complete(daddr, ep_num, xfer->queued_len[TUSB_DIR_OUT], XFER_RESULT_STALLED, true);
    }
  } else {
    // Error
    TU_LOG(FSDEV_DEBUG, "handle_ctr_tx error epreg=0x%08X ch=%u ep=0x%02X daddr=%u queued=%u/%u\r\n",
             ch_reg, ch_id, ep_num, daddr, xfer->queued_len[TUSB_DIR_OUT], edpt->buflen);
    ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_OUT); // will change TX Status, reserved other toggle bits
    ch_reg &=~USB_CH_ERRTX;
    if (xfer->retry[TUSB_DIR_OUT] < HCD_XFER_ERROR_MAX) {
      // Retry
      xfer->retry[TUSB_DIR_OUT]++;
    } else {
      // Failed after retries
      channel_dealloc(xfer, TUSB_DIR_OUT);
      ch_change_status(&ch_reg, TUSB_DIR_OUT, EP_STAT_DISABLED);
      hcd_event_xfer_complete(daddr, ep_num, xfer->queued_len[TUSB_DIR_OUT], XFER_RESULT_FAILED, true);
    }
    ch_write(ch_id, ch_reg, false);
  }
}

// Handle CTR interrupt for the RX/IN direction
static void handle_ctr_rx(uint32_t ch_id) {
  uint32_t ch_reg = ch_read(ch_id) | USB_EP_CTR_TX | USB_EP_CTR_RX;
  uint8_t const ep_num = ch_reg & USB_EPADDR_FIELD;

  uint8_t const daddr = (ch_reg & USB_CHEP_DEVADDR_Msk) >> USB_CHEP_DEVADDR_Pos;

  uint8_t ep_id = endpoint_find(daddr, ep_num | TUSB_DIR_IN_MASK);
  TU_VERIFY(ep_id != TUSB_INDEX_INVALID_8, );

  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
    hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  TU_VERIFY(xfer->allocated[TUSB_DIR_IN],);

  // Manage Correct Transaction
  if ((ch_reg & USB_CH_ERRRX) == 0U) {
    // Acked
    if ((ch_reg & USB_CH_RX_STRX) == USB_CH_RX_ACK_SBUF) {
      uint16_t const rx_count = btable_get_count(ch_id, BTABLE_BUF_RX);
      uint16_t pma_addr = (uint16_t) btable_get_addr(ch_id, BTABLE_BUF_RX);

      hcd_read_packet_memory(edpt->buffer + xfer->queued_len[TUSB_DIR_IN], pma_addr, rx_count);
      xfer->queued_len[TUSB_DIR_IN] += rx_count;

      if ((rx_count < edpt->max_packet_size) || (xfer->queued_len[TUSB_DIR_IN] >= edpt->buflen)) {
        // all bytes received or short packet
        channel_dealloc(xfer, TUSB_DIR_IN);
        hcd_event_xfer_complete(daddr, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len[TUSB_DIR_IN], XFER_RESULT_SUCCESS, true);
      } else {
        // Set endpoint active again for receiving more data. Note that isochronous endpoints stay active always
        uint16_t const cnt = tu_min16(edpt->buflen - xfer->queued_len[TUSB_DIR_IN], edpt->max_packet_size);
        btable_set_rx_bufsize(ch_id, BTABLE_BUF_RX, cnt);
        ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_IN); // will change RX Status, reserved other toggle bits
        ch_change_status(&ch_reg, TUSB_DIR_IN, EP_STAT_VALID);
        ch_write(ch_id, ch_reg, false);
      }
    } else if ((ch_reg & USB_CH_RX_STRX) == USB_CH_RX_NAK) {
      // NAKed
      if (edpt->ep_type != TUSB_XFER_INTERRUPT) {
        ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_IN); // will change TX Status, reserved other toggle bits
        ch_change_status(&ch_reg, TUSB_DIR_IN, EP_STAT_VALID);
        ch_write(ch_id, ch_reg, false);
      }
    } else {
      // STALLed
      channel_dealloc(xfer, TUSB_DIR_IN);
      ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_IN); // will change TX Status, reserved other toggle bits
      ch_change_status(&ch_reg, TUSB_DIR_IN, EP_STAT_DISABLED);
      ch_write(ch_id, ch_reg, false);
      hcd_event_xfer_complete(daddr, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len[TUSB_DIR_IN], XFER_RESULT_STALLED, true);
    }
  } else {
    // Error
    TU_LOG(FSDEV_DEBUG, "handle_ctr_tx error epreg=0x%08X ch=%u ep=0x%02X daddr=%u queued=%u/%u\r\n",
             ch_reg, ch_id, ep_num, daddr, xfer->queued_len[TUSB_DIR_IN], edpt->buflen);
    ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_IN); // will change RX Status, reserved other toggle bits
    ch_reg &=~USB_CH_ERRRX;
    if (xfer->retry[TUSB_DIR_IN] < HCD_XFER_ERROR_MAX) {
      // Retry
      xfer->retry[TUSB_DIR_IN]++;
    } else {
      // Failed after retries
      channel_dealloc(xfer, TUSB_DIR_IN);
      ch_change_status(&ch_reg, TUSB_DIR_IN, EP_STAT_DISABLED);
      hcd_event_xfer_complete(daddr, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len[TUSB_DIR_IN], XFER_RESULT_FAILED, true);
    }
    ch_write(ch_id, ch_reg, false);
  }
}

// Interrupt Handler
void hcd_int_handler(uint8_t rhport, bool in_isr) {
  uint32_t int_status = FSDEV_REG->ISTR;

  /* Port Change Detected (Connection/Disconnection) */
  if (int_status & USB_ISTR_DCON) {
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_DCON;
    port_status_handler(rhport, in_isr);
  }

  // Handle transfer complete (CTR)
  while (FSDEV_REG->ISTR & USB_ISTR_CTR) {
    uint32_t const ch_id = FSDEV_REG->ISTR & USB_ISTR_EP_ID;
    uint32_t const ch_reg = ch_read(ch_id);

    if (ch_reg & USB_EP_CTR_RX) {
      #ifdef FSDEV_BUS_32BIT
      /* https://www.st.com/resource/en/errata_sheet/es0561-stm32h503cbebkbrb-device-errata-stmicroelectronics.pdf
       * https://www.st.com/resource/en/errata_sheet/es0587-stm32u535xx-and-stm32u545xx-device-errata-stmicroelectronics.pdf
       * From H503/U535 errata: Buffer description table update completes after CTR interrupt triggers
       * Description:
       * - During OUT transfers, the correct transfer interrupt (CTR) is triggered a little before the last USB SRAM accesses
       * have completed. If the software responds quickly to the interrupt, the full buffer contents may not be correct.
       * Workaround:
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
        cycle_count--; // each count take 3 cycles (1 for sub, jump, and compare)
      }
      #endif

      ch_write_clear_ctr(ch_id, TUSB_DIR_IN);
      handle_ctr_rx(ch_id);
    }

    if (ch_reg & USB_EP_CTR_TX) {
      ch_write_clear_ctr(ch_id, TUSB_DIR_OUT);
      handle_ctr_tx(ch_id);
    }
  }

  if (int_status & USB_ISTR_ERR) {
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_ERR;
    // TODO: Handle error
  }

  if (int_status & USB_ISTR_PMAOVR) {
    TU_BREAKPOINT();
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_PMAOVR;
  }
}

// Enable USB interrupt
void hcd_int_enable(uint8_t rhport) {
  fsdev_int_enable(rhport);
}

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport) {
  fsdev_int_disable(rhport);
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;
  return FSDEV_REG->FNR & USB_FNR_FN;
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return _hcd_port.connected;
}

// Reset USB bus on the port
void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
  FSDEV_REG->CNTR |= USB_CNTR_FRES;
}

// Complete bus reset sequence
void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
  FSDEV_REG->CNTR &= ~USB_CNTR_FRES;
}

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  if ((FSDEV_REG->ISTR & USB_ISTR_LS_DCONN) != 0U) {
    return TUSB_SPEED_LOW;
  } else {
    return TUSB_SPEED_FULL;
  }
}

// HCD closes all opened endpoints belonging to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;

  // Close all endpoints for this device
  for(uint32_t i = 0; i < CFG_TUH_FSDEV_ENDPOINT_MAX; i++) {
    hcd_endpoint_t* edpt = &_hcd_data.edpt[i];
    if (edpt->allocated && edpt->dev_addr == dev_addr) {
      edpoint_close(i);
    }
  }

}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

// Open an endpoint
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const *ep_desc) {
  (void) rhport;

  uint8_t const ep_addr = ep_desc->bEndpointAddress;
  uint16_t const packet_size = tu_edpt_packet_size(ep_desc);
  uint8_t const ep_type = ep_desc->bmAttributes.xfer;

  uint8_t const ep_id = endpoint_alloc();
  TU_ASSERT(ep_id < CFG_TUH_FSDEV_ENDPOINT_MAX);

  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  edpt->dev_addr = dev_addr;
  edpt->ep_addr = ep_addr;
  edpt->ep_type = ep_type;
  edpt->max_packet_size = packet_size;
  edpt->interval = ep_desc->bInterval;
  edpt->low_speed = (hcd_port_speed_get(rhport) == TUSB_SPEED_FULL && tuh_speed_get(dev_addr) == TUSB_SPEED_LOW);

  // EP0 is bi-directional, so we need to open both OUT and IN channels
  if (ep_addr == 0) {
    uint8_t const ep_id_in = endpoint_alloc();
    TU_ASSERT(ep_id_in < CFG_TUH_FSDEV_ENDPOINT_MAX);

    _hcd_data.edpt[ep_id_in] = *edpt; // copy from OUT endpoint
    _hcd_data.edpt[ep_id_in].ep_addr = 0 | TUSB_DIR_IN_MASK;
  }

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;

  uint8_t const ep_id = endpoint_find(dev_addr, ep_addr);
  TU_ASSERT(ep_id < CFG_TUH_FSDEV_ENDPOINT_MAX);

  edpoint_close(ep_id);

  if (ep_addr == 0) {
    uint8_t const ep_id_in = endpoint_find(dev_addr, 0 | TUSB_DIR_IN_MASK);
    TU_ASSERT(ep_id_in < CFG_TUH_FSDEV_ENDPOINT_MAX);

    edpoint_close(ep_id_in);
  }

  return false;
}

// Submit a transfer
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void) rhport;

  TU_LOG(FSDEV_DEBUG, "hcd_edpt_xfer addr=%u ep=0x%02X len=%u\r\n", dev_addr, ep_addr, buflen);

  uint8_t const ep_id = endpoint_find(dev_addr, ep_addr);
  TU_ASSERT(ep_id < CFG_TUH_FSDEV_ENDPOINT_MAX);

  hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];

  edpt->buffer = buffer;
  edpt->buflen = buflen;

  return edpt_xfer_kickoff(ep_id);
}

// Abort a queued transfer
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;

  uint8_t const ep_id = endpoint_find(dev_addr, ep_addr);
  TU_ASSERT(ep_id < CFG_TUH_FSDEV_ENDPOINT_MAX);
  tusb_dir_t const dir = tu_edpt_dir(ep_addr);

  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    hcd_xfer_t* xfer = &_hcd_data.xfer[i];

    if (xfer->allocated[dir] &&
        xfer->dev_addr == dev_addr &&
        xfer->ep_num == tu_edpt_number(ep_addr)) {

      channel_dealloc(xfer, dir);

      uint32_t ch_reg = ch_read(i) | USB_EP_CTR_TX | USB_EP_CTR_RX; // reserve CTR bits
      ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(dir); // will change Status, reserved other toggle bits
      ch_change_status(&ch_reg, dir, EP_STAT_DISABLED);
      ch_write(i, ch_reg, true);

    }
  }

  return true;
}

// Submit a special transfer to send 8-byte Setup Packet
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  (void) rhport;

  uint8_t const ep_id = endpoint_find(dev_addr, 0);
  TU_ASSERT(ep_id < CFG_TUH_FSDEV_ENDPOINT_MAX);

  hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];
  edpt->next_setup = true;

  return hcd_edpt_xfer(rhport, dev_addr, 0, (uint8_t*)(uintptr_t) setup_packet, 8);
}

// Clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return true;
}

//--------------------------------------------------------------------+
// Helper Functions
//--------------------------------------------------------------------+

static uint8_t endpoint_alloc(void) {
  for (uint32_t i = 0; i < CFG_TUH_FSDEV_ENDPOINT_MAX; i++) {
    hcd_endpoint_t* edpt = &_hcd_data.edpt[i];
    if (!edpt->allocated) {
      edpt->allocated = true;
      return i;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

static uint8_t endpoint_find(uint8_t dev_addr, uint8_t ep_addr) {
  for (uint32_t i = 0; i < (uint32_t)CFG_TUH_FSDEV_ENDPOINT_MAX; i++) {
    hcd_endpoint_t* edpt = &_hcd_data.edpt[i];
    if (edpt->allocated && edpt->dev_addr == dev_addr && edpt->ep_addr == ep_addr) {
      return i;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

// close an opened endpoint
static void edpoint_close(uint8_t ep_id) {
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  endpoint_dealloc(edpt);

  // disable active channel belong to this endpoint
  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    hcd_xfer_t* xfer = &_hcd_data.xfer[i];

    if (xfer->allocated[TUSB_DIR_OUT] && xfer->edpt[TUSB_DIR_OUT] == edpt) {
      channel_dealloc(xfer, TUSB_DIR_OUT);

      uint32_t ch_reg = ch_read(i) | USB_EP_CTR_TX | USB_EP_CTR_RX; // reserve CTR bits
      ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_OUT); // will change RX Status, reserved other toggle bits
      ch_change_status(&ch_reg, TUSB_DIR_OUT, EP_STAT_DISABLED);
      ch_write(i, ch_reg, true);

    }
    if (xfer->allocated[TUSB_DIR_IN] && xfer->edpt[TUSB_DIR_IN] == edpt) {
      channel_dealloc(xfer, TUSB_DIR_IN);

      uint32_t ch_reg = ch_read(i) | USB_EP_CTR_TX | USB_EP_CTR_RX; // reserve CTR bits
      ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_IN); // will change TX Status, reserved other toggle bits
      ch_change_status(&ch_reg, TUSB_DIR_IN, EP_STAT_DISABLED);
      ch_write(i, ch_reg, true);

    }
  }
}

// Allocate PMA buffer
static uint32_t hcd_pma_alloc(uint8_t channel, tusb_dir_t dir, uint16_t len) {
  (void) len;
  // Simple static allocation as we are unlikely to handle ISO endpoints in host mode
  // We just give each channel a buffer of max packet size (64 bytes)

  uint16_t addr = FSDEV_BTABLE_BASE + 8 * FSDEV_EP_COUNT;
  addr += channel * 64 * 2 + (dir == TUSB_DIR_IN ? 64 : 0);

  TU_ASSERT(addr <= FSDEV_PMA_SIZE, 0xFFFF);

  return addr;
}

// Allocate hardware channel
static uint8_t channel_alloc(uint8_t dev_addr, uint8_t ep_addr, uint8_t ep_type) {
  uint8_t const ep_num = tu_edpt_number(ep_addr);
  tusb_dir_t const dir = tu_edpt_dir(ep_addr);

  // Find channel allocate for same ep_num but other direction
  tusb_dir_t const other_dir = (dir == TUSB_DIR_IN) ? TUSB_DIR_OUT : TUSB_DIR_IN;
  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    if (!_hcd_data.xfer[i].allocated[dir] &&
        _hcd_data.xfer[i].allocated[other_dir] &&
        _hcd_data.xfer[i].dev_addr == dev_addr &&
        _hcd_data.xfer[i].ep_num == ep_num &&
        _hcd_data.xfer[i].ep_type == ep_type) {
        _hcd_data.xfer[i].allocated[dir] = true;
        _hcd_data.xfer[i].queued_len[dir] = 0;
        _hcd_data.xfer[i].retry[dir] = 0;
      return i;
    }
  }

  // Find free channel
  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    if (!_hcd_data.xfer[i].allocated[0] && !_hcd_data.xfer[i].allocated[1]) {
      _hcd_data.xfer[i].dev_addr = dev_addr;
      _hcd_data.xfer[i].ep_num = ep_num;
      _hcd_data.xfer[i].ep_type = ep_type;
      _hcd_data.xfer[i].allocated[dir] = true;
      _hcd_data.xfer[i].queued_len[dir] = 0;
      _hcd_data.xfer[i].retry[dir] = 0;
      return i;
    }
  }

  // Allocation failed
  return TUSB_INDEX_INVALID_8;
}

// kick-off transfer with an endpoint
static bool edpt_xfer_kickoff(uint8_t ep_id) {
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  uint8_t ch_id = channel_alloc(edpt->dev_addr, edpt->ep_addr, edpt->ep_type);
  TU_ASSERT(ch_id != TUSB_INDEX_INVALID_8); // all channel are in used

  tusb_dir_t const dir = tu_edpt_dir(edpt->ep_addr);

  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  xfer->edpt[dir] = edpt;

  return channel_xfer_start(ch_id, dir);
}

static bool channel_xfer_start(uint8_t ch_id, tusb_dir_t dir) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt = xfer->edpt[dir];

  uint32_t ch_reg = ch_read(ch_id) & ~USB_EPREG_MASK;
  ch_reg |= tu_edpt_number(edpt->ep_addr) | edpt->dev_addr << USB_CHEP_DEVADDR_Pos |
          USB_EP_CTR_TX | USB_EP_CTR_RX;

  // Set type
  switch (edpt->ep_type) {
    case TUSB_XFER_BULK:
      ch_reg |= USB_EP_BULK;
      break;
    case TUSB_XFER_INTERRUPT:
      ch_reg |= USB_EP_INTERRUPT;
      break;

    case TUSB_XFER_CONTROL:
      ch_reg |= USB_EP_CONTROL;
      break;

    default:
      // Note: ISO endpoint is unsupported
      TU_ASSERT(false);
  }

  /* Create a packet memory buffer area. */
  uint16_t pma_addr = hcd_pma_alloc(ch_id, dir, edpt->max_packet_size);
  btable_set_addr(ch_id, dir == TUSB_DIR_OUT ? BTABLE_BUF_TX : BTABLE_BUF_RX, pma_addr);

  if (dir == TUSB_DIR_OUT) {
    uint16_t const len = tu_min16(edpt->buflen - xfer->queued_len[TUSB_DIR_OUT], edpt->max_packet_size);

    hcd_write_packet_memory(pma_addr, &(edpt->buffer[xfer->queued_len[TUSB_DIR_OUT]]), len);
    btable_set_count(ch_id, BTABLE_BUF_TX, len);

    xfer->queued_len[TUSB_DIR_OUT] += len;
    if (edpt->next_setup) {
      // Setup packet uses IN token
      edpt->next_setup = false;
      ch_reg |= USB_EP_SETUP;
    }

    ch_change_status(&ch_reg, TUSB_DIR_OUT, EP_STAT_VALID);
    ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_OUT); // only change TX Status, reserve other toggle bits

  } else {
    btable_set_rx_bufsize(ch_id, BTABLE_BUF_RX, edpt->max_packet_size);
    ch_change_status(&ch_reg, TUSB_DIR_IN, EP_STAT_VALID);
    ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(TUSB_DIR_IN); // will change RX Status, reserved other toggle bits
  }

  ch_write(ch_id, ch_reg, true);

  return true;
}

//--------------------------------------------------------------------+
// PMA read/write
//--------------------------------------------------------------------+

// Write to packet memory area (PMA) from user memory
static bool hcd_write_packet_memory(uint16_t dst, const void *__restrict src, uint16_t nbytes) {
  if (nbytes == 0) return true;
  uint32_t n_write = nbytes / FSDEV_BUS_SIZE;

  fsdev_pma_buf_t *pma_buf = PMA_BUF_AT(dst);
  const uint8_t *src8 = src;

  while (n_write--) {
    pma_buf->value = fsdevbus_unaligned_read(src8);
    src8 += FSDEV_BUS_SIZE;
    pma_buf++;
  }

  // Handle odd bytes
  uint16_t odd = nbytes & (FSDEV_BUS_SIZE - 1);
  if (odd) {
    fsdev_bus_t temp = 0;
    for (uint16_t i = 0; i < odd; i++) {
      temp |= *src8++ << (i * 8);
    }
    pma_buf->value = temp;
  }

  return true;
}

// Read from packet memory area (PMA) to user memory
static bool hcd_read_packet_memory(void *__restrict dst, uint16_t src, uint16_t nbytes) {
  if (nbytes == 0) return true;
  uint32_t n_read = nbytes / FSDEV_BUS_SIZE;

  fsdev_pma_buf_t *pma_buf = PMA_BUF_AT(src);
  uint8_t *dst8 = (uint8_t *)dst;

  while (n_read--) {
    fsdevbus_unaligned_write(dst8, (fsdev_bus_t) pma_buf->value);
    dst8 += FSDEV_BUS_SIZE;
    pma_buf++;
  }

  // Handle odd bytes
  uint16_t odd = nbytes & (FSDEV_BUS_SIZE - 1);
  if (odd) {
    fsdev_bus_t temp = pma_buf->value;
    while (odd--) {
      *dst8++ = (uint8_t)(temp & 0xfful);
      temp >>= 8;
    }
  }

  return true;
}

#endif
