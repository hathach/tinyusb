/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (tinyusb.org)
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

#if CFG_TUH_ENABLED && defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421

#include <stdatomic.h>
#include "host/hcd.h"

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Command format is
// Reg [7:3] | 0 [2] | Dir [1] | Ack [0]

enum {
  CMDBYTE_WRITE = 0x02,
};

enum {
  RCVVFIFO_ADDR = 1u  << 3, // 0x08
  SNDFIFO_ADDR  = 2u  << 3, // 0x10
  SUDFIFO_ADDR  = 4u  << 3, // 0x20
  RCVBC_ADDR    = 6u  << 3, // 0x30
  SNDBC_ADDR    = 7u  << 3, // 0x38
  USBIRQ_ADDR   = 13u << 3, // 0x68
  USBIEN_ADDR   = 14u << 3, // 0x70
  USBCTL_ADDR   = 15u << 3, // 0x78
  CPUCTL_ADDR   = 16u << 3, // 0x80
  PINCTL_ADDR   = 17u << 3, // 0x88
  REVISION_ADDR = 18u << 3, // 0x90
  HIRQ_ADDR     = 25u << 3, // 0xC8
  HIEN_ADDR     = 26u << 3, // 0xD0
  MODE_ADDR     = 27u << 3, // 0xD8
  PERADDR_ADDR  = 28u << 3, // 0xE0
  HCTL_ADDR     = 29u << 3, // 0xE8
  HXFR_ADDR     = 30u << 3, // 0xF0
  HRSL_ADDR     = 31u << 3, // 0xF8
};

enum {
  USBIRQ_OSCOK_IRQ  = 1u << 0,
  USBIRQ_NOVBUS_IRQ = 1u << 5,
  USBIRQ_VBUS_IRQ   = 1u << 6,
};

enum {
  USBCTL_PWRDOWN = 1u << 4,
  USBCTL_CHIPRES = 1u << 5,
};

enum {
  CPUCTL_IE        = 1u << 0,
  CPUCTL_PULSEWID0 = 1u << 6,
  CPUCTL_PULSEWID1 = 1u << 7,
};

enum {
  PINCTL_GPXA     = 1u << 0,
  PINCTL_GPXB     = 1u << 1,
  PINCTL_POSINT   = 1u << 2,
  PINCTL_INTLEVEL = 1u << 3,
  PINCTL_FDUPSPI  = 1u << 4,
};

enum {
  HIRQ_BUSEVENT_IRQ = 1u << 0,
  HIRQ_RWU_IRQ      = 1u << 1,
  HIRQ_RCVDAV_IRQ   = 1u << 2,
  HIRQ_SNDBAV_IRQ   = 1u << 3,
  HIRQ_SUSDN_IRQ    = 1u << 4,
  HIRQ_CONDET_IRQ   = 1u << 5,
  HIRQ_FRAME_IRQ    = 1u << 6,
  HIRQ_HXFRDN_IRQ   = 1u << 7,
};

enum {
  MODE_HOST      = 1u << 0,
  MODE_LOWSPEED  = 1u << 1,
  MODE_HUBPRE    = 1u << 2,
  MODE_SOFKAENAB = 1u << 3,
  MODE_SEPIRQ    = 1u << 4,
  MODE_DELAYISO  = 1u << 5,
  MODE_DMPULLDN  = 1u << 6,
  MODE_DPPULLDN  = 1u << 7,
};

enum {
  HCTL_BUSRST    = 1u << 0,
  HCTL_FRMRST    = 1u << 1,
  HCTL_SAMPLEBUS = 1u << 2,
  HCTL_SIGRSM    = 1u << 3,
  HCTL_RCVTOG0   = 1u << 4,
  HCTL_RCVTOG1   = 1u << 5,
  HCTL_SNDTOG0   = 1u << 6,
  HCTL_SNDTOG1   = 1u << 7,
};

enum {
  HXFR_EPNUM_MASK = 0x0f,
  HXFR_SETUP      = 1u << 4,
  HXFR_OUT_NIN    = 1u << 5,
  HXFR_ISO        = 1u << 6,
  HXFR_HS         = 1u << 7,
};

enum {
  HRSL_RESULT_MASK = 0x0f,
  HRSL_RCVTOGRD    = 1u << 4,
  HRSL_SNDTOGRD    = 1u << 5,
  HRSL_KSTATUS     = 1u << 6,
  HRSL_JSTATUS     = 1u << 7,
};

enum {
  HRSL_SUCCESS = 0,
  HRSL_BUSY,
  HRSL_BAD_REQ,
  HRSL_UNDEF,
  HRSL_NAK,
  HRSL_STALL,
  HRSL_TOG_ERR,
  HRSL_WRONG_PID,
  HRSL_BAD_BYTECOUNT,
  HRSL_PID_ERR,
  HRSL_PKT_ERR,
  HRSL_CRC_ERR,
  HRSL_K_ERR,
  HRSL_J_ERR,
  HRSL_TIMEOUT,
  HRSL_BABBLE,
};

enum {
  DEFAULT_HIEN = HIRQ_CONDET_IRQ | HIRQ_FRAME_IRQ | HIRQ_HXFRDN_IRQ | HIRQ_RCVDAV_IRQ
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

typedef struct {
  uint16_t packet_size;

  struct TU_ATTR_PACKED {
    uint8_t is_iso        : 1;
    uint8_t data_toggle   : 1;
    uint8_t xfer_queued   : 1;
    uint8_t xfer_complete : 1;
  };

  uint16_t total_len;
  uint16_t xferred_len;
  uint8_t* buf;
} hcd_ep_t;

typedef struct {
  atomic_bool busy; // busy transferring

  // cached register
  uint8_t sndbc;
  uint8_t hirq;
  uint8_t hien;
  uint8_t mode;
  uint8_t peraddr;
  uint8_t hxfr;

  volatile uint16_t frame_count;

  hcd_ep_t ep[8][2];

  OSAL_MUTEX_DEF(spi_mutexdef);
#if OSAL_MUTEX_REQUIRED
  osal_mutex_t spi_mutex;
#endif
} max2341_data_t;

static max2341_data_t _hcd_data;

//--------------------------------------------------------------------+
// API: SPI transfer with MAX3421E, must be implemented by application
//--------------------------------------------------------------------+

void tuh_max3421_spi_cs_api(uint8_t rhport, bool active);
bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const * tx_buf, size_t tx_len, uint8_t * rx_buf, size_t rx_len);
void tuh_max3421e_int_api(uint8_t rhport, bool enabled);

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static void max3421_spi_lock(uint8_t rhport, bool in_isr) {
  // disable interrupt and mutex lock (for pre-emptive RTOS) if not in_isr
  if (!in_isr) {
    tuh_max3421e_int_api(rhport, false);
    (void) osal_mutex_lock(_hcd_data.spi_mutex, OSAL_TIMEOUT_WAIT_FOREVER);
  }

  // assert CS
  tuh_max3421_spi_cs_api(rhport, true);
}

static void max3421_spi_unlock(uint8_t rhport, bool in_isr) {
  // de-assert CS
  tuh_max3421_spi_cs_api(rhport, false);

  // mutex unlock and re-enable interrupt
  if (!in_isr) {
    tuh_max3421e_int_api(rhport, true);
    (void) osal_mutex_unlock(_hcd_data.spi_mutex);
  }
}

static void fifo_write(uint8_t rhport, uint8_t reg, uint8_t const * buffer, uint16_t len, bool in_isr) {
  uint8_t hirq;
  reg |= CMDBYTE_WRITE;

  max3421_spi_lock(rhport, in_isr);

  tuh_max3421_spi_xfer_api(rhport, &reg, 1, &hirq, 1);
  _hcd_data.hirq = hirq;
  tuh_max3421_spi_xfer_api(rhport, buffer, len, NULL, 0);

  max3421_spi_unlock(rhport, in_isr);

}

static void fifo_read(uint8_t rhport, uint8_t * buffer, uint16_t len, bool in_isr) {
  uint8_t hirq;
  uint8_t const reg = RCVVFIFO_ADDR;

  max3421_spi_lock(rhport, in_isr);

  tuh_max3421_spi_xfer_api(rhport, &reg, 1, &hirq, 0);
  _hcd_data.hirq = hirq;
  tuh_max3421_spi_xfer_api(rhport, NULL, 0, buffer, len);

  max3421_spi_unlock(rhport, in_isr);
}

static void reg_write(uint8_t rhport, uint8_t reg, uint8_t data, bool in_isr) {
  uint8_t tx_buf[2] = {reg | CMDBYTE_WRITE, data};
  uint8_t rx_buf[2] = {0, 0};

  max3421_spi_lock(rhport, in_isr);

  tuh_max3421_spi_xfer_api(rhport, tx_buf, 2, rx_buf, 2);

  max3421_spi_unlock(rhport, in_isr);

  // HIRQ register since we are in full-duplex mode
  _hcd_data.hirq = rx_buf[0];
}

static uint8_t reg_read(uint8_t rhport, uint8_t reg, bool in_isr) {
  uint8_t tx_buf[2] = {reg, 0};
  uint8_t rx_buf[2] = {0, 0};

  max3421_spi_lock(rhport, in_isr);

  bool ret = tuh_max3421_spi_xfer_api(rhport, tx_buf, 2, rx_buf, 2);

  max3421_spi_unlock(rhport, in_isr);

  _hcd_data.hirq = rx_buf[0];
  return ret ? rx_buf[1] : 0;
}

static inline void hien_write(uint8_t rhport, uint8_t data, bool in_isr) {
  _hcd_data.hien = data;
  reg_write(rhport, HIEN_ADDR, data, in_isr);
}

static inline void mode_write(uint8_t rhport, uint8_t data, bool in_isr) {
  _hcd_data.mode = data;
  reg_write(rhport, MODE_ADDR, data, in_isr);
}

static inline void peraddr_write(uint8_t rhport, uint8_t data, bool in_isr) {
  if ( _hcd_data.peraddr == data ) return; // no need to change address

  _hcd_data.peraddr = data;
  reg_write(rhport, PERADDR_ADDR, data, in_isr);
}

static inline void hxfr_write(uint8_t rhport, uint8_t data, bool in_isr) {
  _hcd_data.hxfr = data;
  reg_write(rhport, HXFR_ADDR, data, in_isr);
}

static inline void sndbc_write(uint8_t rhport, uint8_t data, bool in_isr) {
  _hcd_data.sndbc = data;
  reg_write(rhport, SNDBC_ADDR, data, in_isr);
}


//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// optional hcd configuration, called by tuh_configure()
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport;
  (void) cfg_id;
  (void) cfg_param;

  return false;
}

tusb_speed_t handle_connect_irq(uint8_t rhport) {
  uint8_t const hrsl = reg_read(rhport, HRSL_ADDR, true);
  uint8_t const jk = hrsl & (HRSL_JSTATUS | HRSL_KSTATUS);

  tusb_speed_t speed;
  uint8_t new_mode = MODE_DPPULLDN | MODE_DMPULLDN | MODE_HOST;

  switch(jk) {
    case 0x00:
      // SEO is disconnected
      speed = TUSB_SPEED_INVALID;
      break;

    case (HRSL_JSTATUS | HRSL_KSTATUS):
      // SE1 is illegal
      speed = TUSB_SPEED_INVALID;
      break;

    default: {
      // Low speed if (LS = 1 and J-state) or (LS = 0 and K-State)
      uint8_t const mode = reg_read(rhport, MODE_ADDR, true);
      uint8_t const ls_bit = mode & MODE_LOWSPEED;

      if ( (ls_bit && (jk == HRSL_JSTATUS)) || (!ls_bit && (jk == HRSL_KSTATUS)) ) {
        speed = TUSB_SPEED_LOW;
        new_mode |= MODE_LOWSPEED;
      } else {
        speed =  TUSB_SPEED_FULL;
      }

      new_mode |= MODE_SOFKAENAB; // enable SOF since there is new device

      break;
    }
  }

  mode_write(rhport, new_mode, true);
  TU_LOG2_INT(speed);
  return speed;
}

// Initialize controller to host mode
bool hcd_init(uint8_t rhport) {
  (void) rhport;

  hcd_int_disable(rhport);

  TU_LOG2_INT(sizeof(hcd_ep_t));
  TU_LOG2_INT(sizeof(max2341_data_t));

  tu_memclr(&_hcd_data, sizeof(_hcd_data));

#if OSAL_MUTEX_REQUIRED
  _hcd_data.spi_mutex = osal_mutex_create(&_hcd_data.spi_mutexdef);
#endif

  // full duplex, interrupt negative edge
  reg_write(rhport, PINCTL_ADDR, PINCTL_FDUPSPI, false);

  // reset
  reg_write(rhport, USBCTL_ADDR, USBCTL_CHIPRES, false);
  reg_write(rhport, USBCTL_ADDR, 0, false);
  while( !(reg_read(rhport, USBIRQ_ADDR, false) & USBIRQ_OSCOK_IRQ) ) {
    // wait for oscillator to stabilize
  }

  // Mode: Host and DP/DM pull down
  mode_write(rhport, MODE_DPPULLDN | MODE_DMPULLDN | MODE_HOST, false);

  // frame reset & bus reset, this will trigger CONDET IRQ if device is already connected
  reg_write(rhport, HCTL_ADDR, HCTL_BUSRST | HCTL_FRMRST, false);

  // clear all previously pending IRQ
  reg_write(rhport, HIRQ_ADDR, 0xff, false);

  // Enable IRQ
  hien_write(rhport, DEFAULT_HIEN, false);

  // Enable Interrupt pin
  reg_write(rhport, CPUCTL_ADDR, CPUCTL_IE, false);

  return true;
}

// Enable USB interrupt
void hcd_int_enable (uint8_t rhport) {
  tuh_max3421e_int_api(rhport, true);
}

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport) {
  tuh_max3421e_int_api(rhport, false);
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;
  return (uint32_t ) _hcd_data.frame_count;
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return (_hcd_data.mode & MODE_SOFKAENAB) ? true : false;
}

// Reset USB bus on the port. Return immediately, bus reset sequence may not be complete.
// Some port would require hcd_port_reset_end() to be invoked after 10ms to complete the reset sequence.
void hcd_port_reset(uint8_t rhport) {
  // Bus reset will also trigger CONDET IRQ, disable it
  uint8_t const hien = DEFAULT_HIEN & ~HIRQ_CONDET_IRQ;
  hien_write(rhport, hien, false);

  reg_write(rhport, HCTL_ADDR, HCTL_BUSRST, false);
}

// Complete bus reset sequence, may be required by some controllers
void hcd_port_reset_end(uint8_t rhport) {
  reg_write(rhport, HCTL_ADDR, 0, false);

  // Bus reset will also trigger CONDET IRQ, clear and re-enable it after reset
  reg_write(rhport, HIRQ_ADDR, HIRQ_CONDET_IRQ, false);
  hien_write(rhport, DEFAULT_HIEN, false);
}

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  return (_hcd_data.mode & MODE_LOWSPEED) ? TUSB_SPEED_LOW : TUSB_SPEED_FULL;
}

// HCD closes all opened endpoints belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;
  (void) dev_addr;
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

// Open an endpoint
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc) {
  (void) rhport;
  (void) dev_addr;

  uint8_t ep_num = tu_edpt_number(ep_desc->bEndpointAddress);
  uint8_t ep_dir = tu_edpt_dir(ep_desc->bEndpointAddress);

  hcd_ep_t * ep = &_hcd_data.ep[ep_num][ep_dir];

  ep->packet_size = tu_edpt_packet_size(ep_desc);

  if (ep_desc->bEndpointAddress == 0) {
    _hcd_data.ep[0][1].packet_size = ep->packet_size;
  }

  ep->is_iso = (TUSB_XFER_ISOCHRONOUS == ep_desc->bmAttributes.xfer) ? 1 : 0;

  return true;
}

// Submit a transfer, when complete hcd_event_xfer_complete() must be invoked
bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  (void) rhport;

  uint8_t const ep_num = tu_edpt_number(ep_addr);
  uint8_t const ep_dir = tu_edpt_dir(ep_addr);

  hcd_ep_t* ep = &_hcd_data.ep[ep_num][ep_dir];

  ep->buf = buffer;
  ep->total_len = buflen;
  ep->xferred_len = 0;
  ep->xfer_complete = 0;
  ep->xfer_queued = 1;

  peraddr_write(rhport, daddr, false);

  uint8_t hctl = 0;
  uint8_t hxfr = ep_num;

  if ( ep->is_iso ) {
    hxfr |= HXFR_ISO;
  } else if ( ep_num == 0 ) {
    ep->data_toggle = 1;
    if ( buffer == NULL || buflen == 0 ) {
      // ZLP for ACK stage, use HS
      hxfr |= HXFR_HS;
      hxfr |= (ep_dir ? 0 : HXFR_OUT_NIN);
      hxfr_write(rhport, hxfr, false);
      return true;
    }
  }

  if ( 0 == ep_dir ) {
    // Page 12: Programming BULK-OUT Transfers
    TU_ASSERT(_hcd_data.hirq & HIRQ_SNDBAV_IRQ);

    uint8_t const xact_len = (uint8_t) tu_min16(buflen, ep->packet_size);
    fifo_write(rhport, SNDFIFO_ADDR, buffer, xact_len, false);
    sndbc_write(rhport, xact_len, false);

    hctl = (ep->data_toggle ? HCTL_SNDTOG1 : HCTL_SNDTOG0);
    hxfr |= HXFR_OUT_NIN;
  } else {
    // Page 13: Programming BULK-IN Transfers
    hctl = (ep->data_toggle ? HCTL_RCVTOG1 : HCTL_RCVTOG0);
  }

  reg_write(rhport, HCTL_ADDR, hctl, false);
  hxfr_write(rhport, hxfr, false);

  return true;
}

// Abort a queued transfer. Note: it can only abort transfer that has not been started
// Return true if a queued transfer is aborted, false if there is no transfer to abort
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return false;
}

// Submit a special transfer to send 8-byte Setup Packet, when complete hcd_event_xfer_complete() must be invoked
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, uint8_t const setup_packet[8]) {
  (void) rhport;

  hcd_ep_t* ep = &_hcd_data.ep[0][0];
  ep->total_len  = 8;
  ep->xferred_len = 0;

  peraddr_write(rhport, daddr, false);
  fifo_write(rhport, SUDFIFO_ADDR, setup_packet, 8, false);
  hxfr_write(rhport, HXFR_SETUP, false);

  return true;
}

// clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return false;
}

static void handle_xfer_done(uint8_t rhport) {
  uint8_t const hrsl = reg_read(rhport, HRSL_ADDR, true);
  uint8_t const hresult = hrsl & HRSL_RESULT_MASK;
  uint8_t ep_num = _hcd_data.hxfr & HXFR_EPNUM_MASK;

  xfer_result_t xfer_result;

  //TU_LOG3("HRSL: %02X\r\n", hrsl);
  switch(hresult) {
    case HRSL_SUCCESS:
      xfer_result = XFER_RESULT_SUCCESS;
      break;

    case HRSL_STALL:
      xfer_result = XFER_RESULT_STALLED;
      break;

    case HRSL_BAD_REQ:
      // occurred when initialized without any pending transfer. Skip for now
      return;

    case HRSL_NAK:
      // NAK on control, retry immediately
      //if (ep_num == 0)
      {
        hxfr_write(rhport, _hcd_data.hxfr, true);
      }
      return;

    default:
      xfer_result = XFER_RESULT_FAILED;
      break;
  }

  uint8_t const hxfr_type = _hcd_data.hxfr & 0xf0;

  if ( (hxfr_type & HXFR_SETUP) || (hxfr_type & HXFR_OUT_NIN) ) {
    // SETUP or OUT transfer
    hcd_ep_t *ep = &_hcd_data.ep[ep_num][0];
    uint8_t xact_len;

    if ( hxfr_type & HXFR_SETUP) {
      xact_len = 8;
    } else if ( hxfr_type & HXFR_HS ) {
      xact_len = 0;
    } else {
      xact_len = _hcd_data.sndbc;
    }

    ep->xferred_len += xact_len;
    ep->buf += xact_len;

    if ( xact_len < ep->packet_size || ep->xferred_len >= ep->total_len ) {
      // save data toggle
      ep->data_toggle = (hrsl & HRSL_SNDTOGRD) ? 1 : 0;
      hcd_event_xfer_complete(_hcd_data.peraddr, ep_num, ep->xferred_len, xfer_result, true);
    }else {
      // more to transfer
    }
  } else {
    // IN transfer: fifo data is already received in RCVDAV IRQ
    hcd_ep_t *ep = &_hcd_data.ep[ep_num][1];

    if ( hxfr_type & HXFR_HS ) {
      ep->xfer_complete = 1;
    }

    // short packet or all bytes transferred
    if ( ep->xfer_complete ) {
      // save data toggle
      ep->data_toggle = (hrsl & HRSL_RCVTOGRD) ? 1 : 0;
      hcd_event_xfer_complete(_hcd_data.peraddr, TUSB_DIR_IN_MASK | ep_num, ep->xferred_len, xfer_result, true);
    }else {
      // more to transfer
      hxfr_write(rhport, _hcd_data.hxfr, true);
    }
  }
}

#if CFG_TUSB_DEBUG >= 3
void print_hirq(uint8_t hirq) {
  TU_LOG3_HEX(hirq);

  if (hirq & HIRQ_HXFRDN_IRQ) TU_LOG3(" HXFRDN");
  if (hirq & HIRQ_FRAME_IRQ)  TU_LOG3(" FRAME");
  if (hirq & HIRQ_CONDET_IRQ) TU_LOG3(" CONDET");
  if (hirq & HIRQ_SUSDN_IRQ)  TU_LOG3(" SUSDN");
  if (hirq & HIRQ_SNDBAV_IRQ) TU_LOG3(" SNDBAV");
  if (hirq & HIRQ_RCVDAV_IRQ) TU_LOG3(" RCVDAV");
  if (hirq & HIRQ_RWU_IRQ)    TU_LOG3(" RWU");
  if (hirq & HIRQ_BUSEVENT_IRQ) TU_LOG3(" BUSEVENT");

  TU_LOG3("\r\n");
}

#else
  #define print_hirq(hirq)
#endif

// Interrupt Handler
void hcd_int_handler(uint8_t rhport) {
  uint8_t hirq = reg_read(rhport, HIRQ_ADDR, true) & _hcd_data.hien;
  if (!hirq) return;

//  print_hirq(hirq);

  if (hirq & HIRQ_FRAME_IRQ) {
    _hcd_data.frame_count++;
  }

  if (hirq & HIRQ_CONDET_IRQ) {
    tusb_speed_t speed = handle_connect_irq(rhport);

    if (speed == TUSB_SPEED_INVALID) {
      hcd_event_device_remove(rhport, true);
    }else {
      hcd_event_device_attach(rhport, true);
    }
  }

  // queue more transfer in handle_xfer_done() can cause hirq to be set again while external IRQ may not catch and/or
  // not call this handler again. So we need to loop until all IRQ are cleared
  while ( hirq & (HIRQ_RCVDAV_IRQ | HIRQ_HXFRDN_IRQ) ) {
    if ( hirq & HIRQ_RCVDAV_IRQ ) {
      uint8_t ep_num = _hcd_data.hxfr & HXFR_EPNUM_MASK;
      hcd_ep_t *ep = &_hcd_data.ep[ep_num][1];
      uint8_t xact_len;

      // RCVDAV_IRQ can trigger 2 times (dual buffered)
      while ( hirq & HIRQ_RCVDAV_IRQ ) {
        uint8_t rcvbc = reg_read(rhport, RCVBC_ADDR, true);
        xact_len = (uint8_t) tu_min16(rcvbc, ep->total_len - ep->xferred_len);
        if ( xact_len ) {
          fifo_read(rhport, ep->buf, xact_len, true);
          ep->buf += xact_len;
          ep->xferred_len += xact_len;
        }

        // ack RCVDVAV IRQ
        reg_write(rhport, HIRQ_ADDR, HIRQ_RCVDAV_IRQ, true);
        hirq = reg_read(rhport, HIRQ_ADDR, true);
      }

      if ( xact_len < ep->packet_size || ep->xferred_len >= ep->total_len ) {
        ep->xfer_complete = 1;
      }
    }

    if ( hirq & HIRQ_HXFRDN_IRQ ) {
      reg_write(rhport, HIRQ_ADDR, HIRQ_HXFRDN_IRQ, true);
      handle_xfer_done(rhport);
    }

    hirq = reg_read(rhport, HIRQ_ADDR, true);
  }

  // clear all interrupt except SNDBAV_IRQ (never clear by us). Note RCVDAV_IRQ, HXFRDN_IRQ already clear while processing
  hirq &= ~HIRQ_SNDBAV_IRQ;
  if ( hirq ) {
    reg_write(rhport, HIRQ_ADDR, hirq, true);
  }
}

#endif
