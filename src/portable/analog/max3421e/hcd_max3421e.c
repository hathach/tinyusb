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

#if CFG_TUH_ENABLED && defined(CFG_TUH_MAX3421E) && CFG_TUH_MAX3421E

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
  HCTL_BUSRST    = 1u << 1,
  HCTL_FRMRST    = 1u << 2,
  HCTL_SAMPLEBUS = 1u << 3,
  HCTL_SIGRSM    = 1u << 4,
  HCTL_RCVTOG0   = 1u << 5,
  HCTL_RCVTOG1   = 1u << 6,
  HCTL_SNDTOG0   = 1u << 7,
  HCTL_SNDTOG1   = 1u << 8,
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


//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

typedef struct {
  // cached register
  uint8_t mode;
  uint8_t peraddr;
  uint8_t hxfr;

  volatile uint16_t frame_count;

  struct {
    uint16_t packet_size;
    uint16_t total_len;
    uint8_t xfer_type;
  }ep[8][2];
} max2341e_data_t;

static max2341e_data_t _hcd_data;

//--------------------------------------------------------------------+
// API: SPI transfer with MAX3421E, must be implemented by application
//--------------------------------------------------------------------+

bool tuh_max3421e_spi_xfer_api(uint8_t rhport, uint8_t const * tx_buf, size_t tx_len,
                               uint8_t * rx_buf, size_t rx_len, bool keep_cs);
//void tuh_max3421e_int_enable(uint8_t rhport, bool enabled);

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// return HIRQ register since we are in full-duplex mode
static uint8_t reg_write(uint8_t reg, uint8_t data) {
  uint8_t tx_buf[2] = {reg | CMDBYTE_WRITE, data};
  uint8_t rx_buf[2] = {0, 0};
  tuh_max3421e_spi_xfer_api(0, tx_buf, 2, rx_buf, 2, false);
  TU_LOG2("HIRQ: %02X\r\n", rx_buf[0]);
  return rx_buf[0];
}

static uint8_t reg_read(uint8_t reg) {
  uint8_t tx_buf[2] = {reg, 0};
  uint8_t rx_buf[2] = {0, 0};
  return tuh_max3421e_spi_xfer_api(0, tx_buf, 2, rx_buf, 2, false) ? rx_buf[1] : 0;
}

static inline uint8_t mode_write(uint8_t data) {
  _hcd_data.mode = data;
  return reg_write(MODE_ADDR, data);
}

static inline uint8_t peraddr_write(uint8_t data) {
  if ( _hcd_data.peraddr == data ) return 0; // no need to change address

  _hcd_data.peraddr = data;
  return reg_write(PERADDR_ADDR, data);
}

static inline uint8_t hxfr_write(uint8_t data) {
  _hcd_data.hxfr = data;
  return reg_write(HXFR_ADDR, data);
}

static void fifo_write(uint8_t reg, uint8_t const * buffer, uint16_t len) {
  uint8_t tx_buf[1] = {reg | CMDBYTE_WRITE};
  tuh_max3421e_spi_xfer_api(0, tx_buf, 1, NULL, 0, true);
  tuh_max3421e_spi_xfer_api(0, buffer, len, NULL, 0, false);
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
  (void) rhport;

  uint8_t const hrsl = reg_read(HRSL_ADDR);
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
      uint8_t const mode = reg_read(MODE_ADDR);
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

  mode_write(new_mode);
  TU_LOG2_INT(speed);
  return speed;
}

// Initialize controller to host mode
bool hcd_init(uint8_t rhport) {
  (void) rhport;

  tu_memclr(&_hcd_data, sizeof(_hcd_data));

  // full duplex, interrupt level (should be configurable)
  reg_write(PINCTL_ADDR, PINCTL_FDUPSPI | PINCTL_INTLEVEL);

  // reset
  reg_write(USBCTL_ADDR, USBCTL_CHIPRES);
  reg_write(USBCTL_ADDR, 0);
  while( !(reg_read(USBIRQ_ADDR) & USBIRQ_OSCOK_IRQ) ) {
    // wait for oscillator to stabilize
  }

  // Mode: Host and DP/DM pull down
  mode_write(MODE_DPPULLDN | MODE_DMPULLDN | MODE_HOST);

  // Enable Connection IRQ
  reg_write(HIEN_ADDR, HIRQ_CONDET_IRQ | HIRQ_FRAME_IRQ | HIRQ_HXFRDN_IRQ);

  #if 0
  // Note: if device is already connected, CONDET IRQ may not be triggered.
  // We need to detect it by sampling bus signal. FIXME not working
  reg_write(HCTL_ADDR, HCTL_SAMPLEBUS);
  while ( reg_read(HCTL_ADDR) & HCTL_SAMPLEBUS ) {}

  if ( TUSB_SPEED_INVALID != handle_connect_irq(rhport) ) {
    reg_write(HIRQ_ADDR, HIRQ_CONDET_IRQ); // clear connect irq
  }
  #endif

  // Enable Interrupt pin
  reg_write(CPUCTL_ADDR, CPUCTL_IE);

  return true;
}

// Enable USB interrupt
void hcd_int_enable (uint8_t rhport) {
  (void) rhport;
}

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport) {
  (void) rhport;
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
  (void) rhport;
  reg_write(HCTL_ADDR, HCTL_BUSRST);
}

// Complete bus reset sequence, may be required by some controllers
void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
  reg_write(HCTL_ADDR, 0);
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
  (void) ep_desc;

  uint8_t ep_num = tu_edpt_number(ep_desc->bEndpointAddress);
  uint8_t ep_dir = tu_edpt_dir(ep_desc->bEndpointAddress);

  _hcd_data.ep[ep_num][ep_dir].packet_size = tu_edpt_packet_size(ep_desc);

  return true;
}

// Submit a transfer, when complete hcd_event_xfer_complete() must be invoked
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;
  (void) buffer;
  (void) buflen;

  return false;
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
  (void) daddr;
  (void) setup_packet;

  _hcd_data.ep[0][0].total_len = 8;

  peraddr_write(daddr);
  fifo_write(SUDFIFO_ADDR, setup_packet, 8);
  hxfr_write(HXFR_SETUP);

  return true;
}

// clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return false;
}

// Interrupt Handler
void hcd_int_handler(uint8_t rhport) {
  uint8_t hirq = reg_read(HIRQ_ADDR);
  TU_LOG3_HEX(hirq);

  if (hirq & HIRQ_CONDET_IRQ) {
    tusb_speed_t speed = handle_connect_irq(rhport);

    if (speed == TUSB_SPEED_INVALID) {
      hcd_event_device_remove(rhport, true);
    }else {
      hcd_event_device_attach(rhport, true);
    }
  }

  if (hirq & HIRQ_FRAME_IRQ) {
    _hcd_data.frame_count++;
  }

  if (hirq & HIRQ_HXFRDN_IRQ) {
    uint8_t const hrsl = reg_read(HRSL_ADDR);
    uint8_t const result = hrsl & HRSL_RESULT_MASK;
    uint8_t xfer_result;

    TU_LOG3("HRSL: %02X\r\n", hrsl);
    switch(result) {
      case HRSL_SUCCESS:
        xfer_result = XFER_RESULT_SUCCESS;
        break;

      case HRSL_STALL:
        xfer_result = XFER_RESULT_STALLED;
        break;

      default:
        xfer_result = XFER_RESULT_FAILED;
        break;
    }

    uint8_t ep_dir = 0;
    uint8_t ep_num = _hcd_data.hxfr & HXFR_EPNUM_MASK;
    uint8_t const xfer_type = _hcd_data.hxfr & 0xf0;

    if ( xfer_type & HXFR_SETUP ) {
      // SETUP transfer
      ep_dir = 0;
    }else if ( !(xfer_type & HXFR_OUT_NIN) ) {
      // IN transfer
      ep_dir = 1;
    }

    uint8_t const ep_addr = tu_edpt_addr(ep_num, ep_dir);
    uint16_t xferred_len = _hcd_data.ep[ep_num][ep_dir].total_len;

    hcd_event_xfer_complete(_hcd_data.peraddr, ep_addr, xferred_len, xfer_result, true);
  }

  // clear all interrupt
  if ( hirq ) {
    reg_write(HIRQ_ADDR, hirq);
  }
}

#endif
