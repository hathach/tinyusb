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
  // 19 is not used
  IOPINS1_ADDR  = 20u << 3, // 0xA0
  IOPINS2_ADDR  = 21u << 3, // 0xA8
  GPINIRQ_ADDR  = 22u << 3, // 0xB0
  GPINIEN_ADDR  = 23u << 3, // 0xB8
  GPINPOL_ADDR  = 24u << 3, // 0xC0
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
  struct TU_ATTR_PACKED {
    uint8_t ep_dir        : 1;
    uint8_t is_iso        : 1;
    uint8_t is_setup      : 1;
    uint8_t data_toggle   : 1;
    uint8_t xfer_pending  : 1;
    uint8_t xfer_complete : 1;
  };
  struct TU_ATTR_PACKED {
    uint8_t daddr : 4;
    uint8_t ep_num : 4;
  };

  uint16_t packet_size;
  uint16_t total_len;
  uint16_t xferred_len;
  uint8_t* buf;
} max3421_ep_t;

typedef struct {
  // cached register
  uint8_t sndbc;
  uint8_t hirq;
  uint8_t hien;
  uint8_t mode;
  uint8_t peraddr;
  uint8_t hxfr;

  atomic_flag busy; // busy transferring
  volatile uint16_t frame_count;

  max3421_ep_t ep[CFG_TUH_MAX3421_ENDPOINT_TOTAL]; // [0] is reserved for addr0

  OSAL_MUTEX_DEF(spi_mutexdef);
#if OSAL_MUTEX_REQUIRED
  osal_mutex_t spi_mutex;
#endif
} max3421_data_t;

static max3421_data_t _hcd_data;

//--------------------------------------------------------------------+
// API: SPI transfer with MAX3421E
// - spi_cs_api(), spi_xfer_api(), int_api(): must be implemented by application
// - reg_read(), reg_write(): is implemented by this driver, can be used by application
//--------------------------------------------------------------------+

// API to control MAX3421 SPI CS
extern void tuh_max3421_spi_cs_api(uint8_t rhport, bool active);

// API to transfer data with MAX3421 SPI
// Either tx_buf or rx_buf can be NULL, which means transfer is write or read only
extern bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const* tx_buf, uint8_t* rx_buf, size_t xfer_bytes);

// API to enable/disable MAX3421 INTR pin interrupt
extern void tuh_max3421_int_api(uint8_t rhport, bool enabled);

// API to read MAX3421's register. Implemented by TinyUSB
uint8_t tuh_max3421_reg_read(uint8_t rhport, uint8_t reg, bool in_isr);

// API to write MAX3421's register. Implemented by TinyUSB
bool tuh_max3421_reg_write(uint8_t rhport, uint8_t reg, uint8_t data, bool in_isr);

//--------------------------------------------------------------------+
// SPI Commands and Helper
//--------------------------------------------------------------------+

#define reg_read  tuh_max3421_reg_read
#define reg_write tuh_max3421_reg_write

static void max3421_spi_lock(uint8_t rhport, bool in_isr) {
  // disable interrupt and mutex lock (for pre-emptive RTOS) if not in_isr
  if (!in_isr) {
    (void) osal_mutex_lock(_hcd_data.spi_mutex, OSAL_TIMEOUT_WAIT_FOREVER);
    tuh_max3421_int_api(rhport, false);
  }

  // assert CS
  tuh_max3421_spi_cs_api(rhport, true);
}

static void max3421_spi_unlock(uint8_t rhport, bool in_isr) {
  // de-assert CS
  tuh_max3421_spi_cs_api(rhport, false);

  // mutex unlock and re-enable interrupt
  if (!in_isr) {
    tuh_max3421_int_api(rhport, true);
    (void) osal_mutex_unlock(_hcd_data.spi_mutex);
  }
}

uint8_t tuh_max3421_reg_read(uint8_t rhport, uint8_t reg, bool in_isr) {
  uint8_t tx_buf[2] = {reg, 0};
  uint8_t rx_buf[2] = {0, 0};

  max3421_spi_lock(rhport, in_isr);
  bool ret = tuh_max3421_spi_xfer_api(rhport, tx_buf, rx_buf, 2);
  max3421_spi_unlock(rhport, in_isr);

  _hcd_data.hirq = rx_buf[0];
  return ret ? rx_buf[1] : 0;
}

bool tuh_max3421_reg_write(uint8_t rhport, uint8_t reg, uint8_t data, bool in_isr) {
  uint8_t tx_buf[2] = {reg | CMDBYTE_WRITE, data};
  uint8_t rx_buf[2] = {0, 0};

  max3421_spi_lock(rhport, in_isr);
  bool ret = tuh_max3421_spi_xfer_api(rhport, tx_buf, rx_buf, 2);
  max3421_spi_unlock(rhport, in_isr);

  // HIRQ register since we are in full-duplex mode
  _hcd_data.hirq = rx_buf[0];

  return ret;
}

static void fifo_write(uint8_t rhport, uint8_t reg, uint8_t const * buffer, uint16_t len, bool in_isr) {
  uint8_t hirq;
  reg |= CMDBYTE_WRITE;

  max3421_spi_lock(rhport, in_isr);

  tuh_max3421_spi_xfer_api(rhport, &reg, &hirq, 1);
  _hcd_data.hirq = hirq;
  tuh_max3421_spi_xfer_api(rhport, buffer, NULL, len);

  max3421_spi_unlock(rhport, in_isr);

}

static void fifo_read(uint8_t rhport, uint8_t * buffer, uint16_t len, bool in_isr) {
  uint8_t hirq;
  uint8_t const reg = RCVVFIFO_ADDR;

  max3421_spi_lock(rhport, in_isr);

  tuh_max3421_spi_xfer_api(rhport, &reg, &hirq, 1);
  _hcd_data.hirq = hirq;
  tuh_max3421_spi_xfer_api(rhport, NULL, buffer, len);

  max3421_spi_unlock(rhport, in_isr);
}

//------------- register write helper -------------//
static inline void hirq_write(uint8_t rhport, uint8_t data, bool in_isr) {
  reg_write(rhport, HIRQ_ADDR, data, in_isr);
  // HIRQ write 1 is clear
  _hcd_data.hirq &= ~data;
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
// Endpoint helper
//--------------------------------------------------------------------+

static max3421_ep_t* find_ep_not_addr0(uint8_t daddr, uint8_t ep_num, uint8_t ep_dir) {
  for(size_t i=1; i<CFG_TUH_MAX3421_ENDPOINT_TOTAL; i++) {
    max3421_ep_t* ep = &_hcd_data.ep[i];
    // for control endpoint, skip direction check
    if (daddr == ep->daddr && ep_num == ep->ep_num && (ep_dir == ep->ep_dir || ep_num == 0)) {
      return ep;
    }
  }

  return NULL;
}

// daddr = 0 and ep_num = 0 means find a free (allocate) endpoint
TU_ATTR_ALWAYS_INLINE static inline max3421_ep_t * allocate_ep(void) {
  return find_ep_not_addr0(0, 0, 0);
}

TU_ATTR_ALWAYS_INLINE static inline max3421_ep_t * find_opened_ep(uint8_t daddr, uint8_t ep_num, uint8_t ep_dir) {
  if (daddr == 0 && ep_num == 0) {
    return &_hcd_data.ep[0];
  }else{
    return find_ep_not_addr0(daddr, ep_num, ep_dir);
  }
}

// free all endpoints belong to device address
static void free_ep(uint8_t daddr) {
  for (size_t i=1; i<CFG_TUH_MAX3421_ENDPOINT_TOTAL; i++) {
    max3421_ep_t* ep = &_hcd_data.ep[i];
    if (ep->daddr == daddr) {
      tu_memclr(ep, sizeof(max3421_ep_t));
    }
  }
}

static max3421_ep_t * find_next_pending_ep(max3421_ep_t * cur_ep) {
  size_t const idx = cur_ep - _hcd_data.ep;

  // starting from next endpoint
  for (size_t i = idx + 1; i < CFG_TUH_MAX3421_ENDPOINT_TOTAL; i++) {
    max3421_ep_t* ep = &_hcd_data.ep[i];
    if (ep->xfer_pending && ep->packet_size) {
//      TU_LOG3("next pending i = %u\r\n", i);
      return ep;
    }
  }

  // wrap around including current endpoint
  for (size_t i = 0; i <= idx; i++) {
    max3421_ep_t* ep = &_hcd_data.ep[i];
    if (ep->xfer_pending && ep->packet_size) {
//      TU_LOG3("next pending i = %u\r\n", i);
      return ep;
    }
  }

  return NULL;
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

// Initialize controller to host mode
bool hcd_init(uint8_t rhport) {
  (void) rhport;

  tuh_max3421_int_api(rhport, false);

  TU_LOG2_INT(sizeof(max3421_ep_t));
  TU_LOG2_INT(sizeof(max3421_data_t));

  tu_memclr(&_hcd_data, sizeof(_hcd_data));
  _hcd_data.peraddr = 0xff; // invalid

#if OSAL_MUTEX_REQUIRED
  _hcd_data.spi_mutex = osal_mutex_create(&_hcd_data.spi_mutexdef);
#endif

  // full duplex, interrupt negative edge
  reg_write(rhport, PINCTL_ADDR, PINCTL_FDUPSPI, false);

  // V1 is 0x01, V2 is 0x12, V3 is 0x13
  uint8_t const revision = reg_read(rhport, REVISION_ADDR, false);
  TU_ASSERT(revision == 0x01 || revision == 0x12 || revision == 0x13, false);
  TU_LOG2_HEX(revision);

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
  hirq_write(rhport, 0xff, false);

  // Enable IRQ
  hien_write(rhport, DEFAULT_HIEN, false);

  tuh_max3421_int_api(rhport, true);

  // Enable Interrupt pin
  reg_write(rhport, CPUCTL_ADDR, CPUCTL_IE, false);

  return true;
}

// Enable USB interrupt
// Not actually enable GPIO interrupt, just set variable to prevent handler to process
void hcd_int_enable (uint8_t rhport) {
  tuh_max3421_int_api(rhport, true);
}

// Disable USB interrupt
// Not actually disable GPIO interrupt, just set variable to prevent handler to process
void hcd_int_disable(uint8_t rhport) {
  tuh_max3421_int_api(rhport, false);
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
  reg_write(rhport, HCTL_ADDR, HCTL_BUSRST, false);
}

// Complete bus reset sequence, may be required by some controllers
void hcd_port_reset_end(uint8_t rhport) {
  reg_write(rhport, HCTL_ADDR, 0, false);
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
bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, tusb_desc_endpoint_t const * ep_desc) {
  (void) rhport;
  (void) daddr;

  uint8_t ep_num = tu_edpt_number(ep_desc->bEndpointAddress);
  uint8_t ep_dir = tu_edpt_dir(ep_desc->bEndpointAddress);

  max3421_ep_t * ep;
  if (daddr == 0 && ep_num == 0) {
    ep = &_hcd_data.ep[0];
  }else {
    ep = allocate_ep();
    TU_ASSERT(ep);
    ep->daddr = daddr;
    ep->ep_num = ep_num;
    ep->ep_dir = ep_dir;
  }

  if ( TUSB_XFER_ISOCHRONOUS == ep_desc->bmAttributes.xfer ) {
    ep->is_iso = 1;
  }

  ep->packet_size = tu_edpt_packet_size(ep_desc);

  return true;
}

void xact_out(uint8_t rhport, max3421_ep_t *ep, bool switch_ep, bool in_isr) {
  // Page 12: Programming BULK-OUT Transfers
  // TODO double buffered
  if (switch_ep) {
    peraddr_write(rhport, ep->daddr, in_isr);

    uint8_t const hctl = (ep->data_toggle ? HCTL_SNDTOG1 : HCTL_SNDTOG0);
    reg_write(rhport, HCTL_ADDR, hctl, in_isr);
  }

  uint8_t const xact_len = (uint8_t) tu_min16(ep->total_len - ep->xferred_len, ep->packet_size);
  TU_ASSERT(_hcd_data.hirq & HIRQ_SNDBAV_IRQ,);
  if (xact_len) {
    fifo_write(rhport, SNDFIFO_ADDR, ep->buf, xact_len, in_isr);
  }
  sndbc_write(rhport, xact_len, in_isr);

  uint8_t hxfr = ep->ep_num | HXFR_OUT_NIN | (ep->is_iso ? HXFR_ISO : 0);
  hxfr_write(rhport, hxfr, in_isr);
}

void xact_in(uint8_t rhport, max3421_ep_t *ep, bool switch_ep, bool in_isr) {
  // Page 13: Programming BULK-IN Transfers
  if (switch_ep) {
    peraddr_write(rhport, ep->daddr, in_isr);

    uint8_t const hctl = (ep->data_toggle ? HCTL_RCVTOG1 : HCTL_RCVTOG0);
    reg_write(rhport, HCTL_ADDR, hctl, in_isr);
  }

  uint8_t hxfr = ep->ep_num | (ep->is_iso ? HXFR_ISO : 0);
  hxfr_write(rhport, hxfr, in_isr);
}

TU_ATTR_ALWAYS_INLINE static inline void xact_inout(uint8_t rhport, max3421_ep_t *ep, bool switch_ep, bool in_isr) {
  if (ep->ep_num == 0 ) {
    // setup
    if (ep->is_setup) {
      peraddr_write(rhport, ep->daddr, in_isr);
      fifo_write(rhport, SUDFIFO_ADDR, ep->buf, 8, in_isr);
      hxfr_write(rhport, HXFR_SETUP, in_isr);
      return;
    }

    // status
    if (ep->buf == NULL || ep->total_len == 0) {
      uint8_t const hxfr = HXFR_HS | (ep->ep_dir ? 0 : HXFR_OUT_NIN);
      peraddr_write(rhport, ep->daddr, in_isr);
      hxfr_write(rhport, hxfr, in_isr);
      return;
    }
  }

  if (ep->ep_dir) {
    xact_in(rhport, ep, switch_ep, in_isr);
  }else {
    xact_out(rhport, ep, switch_ep, in_isr);
  }
}

// Submit a transfer, when complete hcd_event_xfer_complete() must be invoked
bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  uint8_t const ep_num = tu_edpt_number(ep_addr);
  uint8_t const ep_dir = tu_edpt_dir(ep_addr);

  max3421_ep_t* ep = find_opened_ep(daddr, ep_num, ep_dir);
  TU_VERIFY(ep);

  // control transfer can switch direction
  ep->ep_dir = ep_dir;

  ep->buf = buffer;
  ep->total_len = buflen;
  ep->xferred_len = 0;
  ep->xfer_complete = 0;
  ep->xfer_pending = 1;

  if ( ep_num == 0 ) {
    ep->is_setup = 0;
    ep->data_toggle = 1;
  }

  // carry out transfer if not busy
  if ( !atomic_flag_test_and_set(&_hcd_data.busy) ) {
    xact_inout(rhport, ep, true, false);
  } else {
    return true;
  }

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

  max3421_ep_t* ep = find_opened_ep(daddr, 0, 0);
  TU_ASSERT(ep);

  ep->ep_dir = 0;
  ep->is_setup = 1;
  ep->buf = (uint8_t*)(uintptr_t) setup_packet;
  ep->total_len = 8;
  ep->xferred_len = 0;
  ep->xfer_complete = 0;
  ep->xfer_pending = 1;

  // carry out transfer if not busy
  if ( !atomic_flag_test_and_set(&_hcd_data.busy) ) {
    xact_inout(rhport, ep, true, false);
  }

  return true;
}

// clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return false;
}

//--------------------------------------------------------------------+
// Interrupt Handler
//--------------------------------------------------------------------+

static void handle_connect_irq(uint8_t rhport, bool in_isr) {
  uint8_t const hrsl = reg_read(rhport, HRSL_ADDR, in_isr);
  uint8_t const jk = hrsl & (HRSL_JSTATUS | HRSL_KSTATUS);

  uint8_t new_mode = MODE_DPPULLDN | MODE_DMPULLDN | MODE_HOST;
  TU_LOG2_HEX(jk);

  switch(jk) {
    case 0x00:                          // SEO is disconnected
    case (HRSL_JSTATUS | HRSL_KSTATUS): // SE1 is illegal
      mode_write(rhport, new_mode, in_isr);

      // port reset anyway, this will help to stable bus signal for next connection
      reg_write(rhport, HCTL_ADDR, HCTL_BUSRST, in_isr);
      hcd_event_device_remove(rhport, in_isr);
      reg_write(rhport, HCTL_ADDR, 0, in_isr);
      break;

    default: {
      // Bus Reset also cause CONDET IRQ, skip if we are already connected and doing bus reset
      if ((_hcd_data.hirq & HIRQ_BUSEVENT_IRQ) && (_hcd_data.mode & MODE_SOFKAENAB)) {
        break;
      }

      // Low speed if (LS = 1 and J-state) or (LS = 0 and K-State)
      // However, since we are always in full speed mode, we can just check J-state
      if (jk == HRSL_KSTATUS) {
        new_mode |= MODE_LOWSPEED;
        TU_LOG3("Low speed\r\n");
      }else {
        TU_LOG3("Full speed\r\n");
      }
      new_mode |= MODE_SOFKAENAB;
      mode_write(rhport, new_mode, in_isr);

      // FIXME multiple MAX3421 rootdevice address is not 1
      uint8_t const daddr = 1;
      free_ep(daddr);

      hcd_event_device_attach(rhport, in_isr);
      break;
    }
  }
}

static void xfer_complete_isr(uint8_t rhport, max3421_ep_t *ep, xfer_result_t result, uint8_t hrsl, bool in_isr) {
  uint8_t const ep_addr = tu_edpt_addr(ep->ep_num, ep->ep_dir);

  // save data toggle
  if (ep->ep_dir) {
    ep->data_toggle = (hrsl & HRSL_RCVTOGRD) ? 1 : 0;
  }else {
    ep->data_toggle = (hrsl & HRSL_SNDTOGRD) ? 1 : 0;
  }

  ep->xfer_pending = 0;
  hcd_event_xfer_complete(ep->daddr, ep_addr, ep->xferred_len, result, in_isr);

  // Find next pending endpoint
  max3421_ep_t *next_ep = find_next_pending_ep(ep);
  if (next_ep) {
    xact_inout(rhport, next_ep, true, in_isr);
  }else {
    // no more pending
    atomic_flag_clear(&_hcd_data.busy);
  }
}

static void handle_xfer_done(uint8_t rhport, bool in_isr) {
  uint8_t const hrsl = reg_read(rhport, HRSL_ADDR, in_isr);
  uint8_t const hresult = hrsl & HRSL_RESULT_MASK;

  uint8_t const ep_num = _hcd_data.hxfr & HXFR_EPNUM_MASK;
  uint8_t const hxfr_type = _hcd_data.hxfr & 0xf0;
  uint8_t const ep_dir = ((hxfr_type & HXFR_SETUP) || (hxfr_type & HXFR_OUT_NIN)) ? 0 : 1;

  max3421_ep_t *ep = find_opened_ep(_hcd_data.peraddr, ep_num, ep_dir);
  TU_VERIFY(ep, );

  xfer_result_t xfer_result;
  switch(hresult) {
    case HRSL_SUCCESS:
      xfer_result = XFER_RESULT_SUCCESS;
      break;

    case HRSL_STALL:
      xfer_result = XFER_RESULT_STALLED;
      break;

    case HRSL_NAK:
      if (ep_num == 0) {
        // NAK on control, retry immediately
        hxfr_write(rhport, _hcd_data.hxfr, in_isr);
      }else {
        // NAK on non-control, find next pending to switch
        max3421_ep_t *next_ep = find_next_pending_ep(ep);

        if (ep == next_ep) {
          // this endpoint is only one pending, retry immediately
          hxfr_write(rhport, _hcd_data.hxfr, in_isr);
        }else if (next_ep) {
          // switch to next pending TODO could have issue with double buffered if not clear previously out data
          xact_inout(rhport, next_ep, true, in_isr);
        }else {
          TU_ASSERT(false,);
        }
      }
      return;

    case HRSL_BAD_REQ:
      // occurred when initialized without any pending transfer. Skip for now
      return;

    default:
      TU_LOG3("HRSL: %02X\r\n", hrsl);
      xfer_result = XFER_RESULT_FAILED;
      break;
  }

  if (xfer_result != XFER_RESULT_SUCCESS) {
    xfer_complete_isr(rhport, ep, xfer_result, hrsl, in_isr);
    return;
  }

  if (ep_dir) {
    // IN transfer: fifo data is already received in RCVDAV IRQ
    if ( hxfr_type & HXFR_HS ) {
      ep->xfer_complete = 1;
    }

    // short packet or all bytes transferred
    if ( ep->xfer_complete ) {
      xfer_complete_isr(rhport, ep, xfer_result, hrsl, in_isr);
    }else {
      // more to transfer
      hxfr_write(rhport, _hcd_data.hxfr, in_isr);
    }
  } else {
    // SETUP or OUT transfer
    uint8_t xact_len;

    if (hxfr_type & HXFR_SETUP) {
      xact_len = 8;
    } else if (hxfr_type & HXFR_HS) {
      xact_len = 0;
    } else {
      xact_len = _hcd_data.sndbc;
    }

    ep->xferred_len += xact_len;
    ep->buf += xact_len;

    if (xact_len < ep->packet_size || ep->xferred_len >= ep->total_len) {
      xfer_complete_isr(rhport, ep, xfer_result, hrsl, in_isr);
    } else {
      // more to transfer
      xact_out(rhport, ep, false, in_isr);
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

// Interrupt handler
void hcd_int_handler(uint8_t rhport, bool in_isr) {
  uint8_t hirq = reg_read(rhport, HIRQ_ADDR, in_isr) & _hcd_data.hien;
  if (!hirq) return;
//  print_hirq(hirq);

  if (hirq & HIRQ_FRAME_IRQ) {
    _hcd_data.frame_count++;
  }

  if (hirq & HIRQ_CONDET_IRQ) {
    handle_connect_irq(rhport, in_isr);
  }

  // queue more transfer in handle_xfer_done() can cause hirq to be set again while external IRQ may not catch and/or
  // not call this handler again. So we need to loop until all IRQ are cleared
  while ( hirq & (HIRQ_RCVDAV_IRQ | HIRQ_HXFRDN_IRQ) ) {
    if ( hirq & HIRQ_RCVDAV_IRQ ) {
      uint8_t const ep_num = _hcd_data.hxfr & HXFR_EPNUM_MASK;
      max3421_ep_t *ep = find_opened_ep(_hcd_data.peraddr, ep_num, 1);
      uint8_t xact_len = 0;

      // RCVDAV_IRQ can trigger 2 times (dual buffered)
      while ( hirq & HIRQ_RCVDAV_IRQ ) {
        uint8_t rcvbc = reg_read(rhport, RCVBC_ADDR, in_isr);
        xact_len = (uint8_t) tu_min16(rcvbc, ep->total_len - ep->xferred_len);
        if ( xact_len ) {
          fifo_read(rhport, ep->buf, xact_len, in_isr);
          ep->buf += xact_len;
          ep->xferred_len += xact_len;
        }

        // ack RCVDVAV IRQ
        hirq_write(rhport, HIRQ_RCVDAV_IRQ, in_isr);
        hirq = reg_read(rhport, HIRQ_ADDR, in_isr);
      }

      if ( xact_len < ep->packet_size || ep->xferred_len >= ep->total_len ) {
        ep->xfer_complete = 1;
      }
    }

    if ( hirq & HIRQ_HXFRDN_IRQ ) {
      hirq_write(rhport, HIRQ_HXFRDN_IRQ, in_isr);
      handle_xfer_done(rhport, in_isr);
    }

    hirq = reg_read(rhport, HIRQ_ADDR, in_isr);
  }

  // clear all interrupt except SNDBAV_IRQ (never clear by us). Note RCVDAV_IRQ, HXFRDN_IRQ already clear while processing
  hirq &= ~HIRQ_SNDBAV_IRQ;
  if ( hirq ) {
    hirq_write(rhport, hirq, in_isr);
  }
}

#endif
