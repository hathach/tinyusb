/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 TinyUSB contributors
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

#if CFG_TUH_ENABLED && defined(TUP_USBIP_WCH_CH58X) && \
    defined(CFG_TUH_WCH_USBIP_USBFS) && CFG_TUH_WCH_USBIP_USBFS

#include <string.h>

#include "host/hcd.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"

#include "ch58x_usbfs_reg.h"

//--------------------------------------------------------------------+
// Configuration
//--------------------------------------------------------------------+
#define USBFS_MAX_PACKET_SIZE  64

#define LOG_CH58X_HCD(...)  TU_LOG3(__VA_ARGS__)

//--------------------------------------------------------------------+
// RX/TX buffers (must be 4-byte aligned and in lower 64KB of RAM)
// Separate buffers for each USB port to avoid DMA conflicts
//--------------------------------------------------------------------+
TU_ATTR_ALIGNED(4) static uint8_t _rx_buf[2][USBFS_MAX_PACKET_SIZE];
TU_ATTR_ALIGNED(4) static uint8_t _tx_buf[2][USBFS_MAX_PACKET_SIZE];

//--------------------------------------------------------------------+
// USB base address selection by rhport
//--------------------------------------------------------------------+
static inline uint32_t _get_usb_base(uint8_t rhport) {
  return (rhport == 0) ? CH58X_USB_BASE : CH58X_USB2_BASE;
}

//--------------------------------------------------------------------+
// Endpoint record
//--------------------------------------------------------------------+
typedef struct {
  bool configured;
  uint8_t dev_addr;
  uint8_t ep_addr;
  uint8_t max_packet_size;
  uint8_t xfer_type;
  uint8_t data_toggle;   // 0=DATA0, 1=DATA1
  bool is_nak_pending;
  uint16_t buflen;
  uint8_t* buf;
} hcd_edpt_t;

static hcd_edpt_t _edpt_list[CFG_TUH_DEVICE_MAX * 6] = {};

//--------------------------------------------------------------------+
// Current transfer state (only one transfer at a time per root port)
//--------------------------------------------------------------------+
typedef struct {
  volatile bool is_busy;
  uint8_t rhport;
  uint8_t dev_addr;
  uint8_t ep_addr;
  uint32_t start_ms;
  uint8_t* buffer;
  uint16_t bufferlen;
  uint16_t xferred_len;
  bool nak_pending;
} hcd_xfer_t;

static volatile hcd_xfer_t _current_xfer = {};

//--------------------------------------------------------------------+
// Per-port state
//--------------------------------------------------------------------+
typedef struct {
  uint32_t usb_base;
  bool int_enabled;
} hcd_port_t;

static hcd_port_t _port_data[2] = {};

//--------------------------------------------------------------------+
// Endpoint record management
//--------------------------------------------------------------------+
static hcd_edpt_t* _get_edpt(uint8_t dev_addr, uint8_t ep_addr) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(_edpt_list); i++) {
    hcd_edpt_t* e = &_edpt_list[i];
    if (e->configured && e->dev_addr == dev_addr && e->ep_addr == ep_addr) {
      return e;
    }
  }
  return NULL;
}

static hcd_edpt_t* _alloc_edpt(void) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(_edpt_list); i++) {
    if (!_edpt_list[i].configured) {
      return &_edpt_list[i];
    }
  }
  return NULL;
}

static hcd_edpt_t* _add_edpt(uint8_t dev_addr, uint8_t ep_addr,
                              uint16_t max_packet_size, uint8_t xfer_type) {
  hcd_edpt_t* e = _alloc_edpt();
  TU_ASSERT(e != NULL, NULL);

  e->dev_addr = dev_addr;
  e->ep_addr = ep_addr;
  e->max_packet_size = (uint8_t) TU_MIN(max_packet_size, USBFS_MAX_PACKET_SIZE);
  e->xfer_type = xfer_type;
  e->data_toggle = 0;
  e->is_nak_pending = false;
  e->buflen = 0;
  e->buf = NULL;
  e->configured = true;

  return e;
}

static hcd_edpt_t* _get_or_add_edpt(uint8_t dev_addr, uint8_t ep_addr,
                                     uint16_t max_packet_size, uint8_t xfer_type) {
  hcd_edpt_t* e = _get_edpt(dev_addr, ep_addr);
  if (e != NULL) return e;
  return _add_edpt(dev_addr, ep_addr, max_packet_size, xfer_type);
}

static void _remove_edpts_for_device(uint8_t dev_addr) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(_edpt_list); i++) {
    if (_edpt_list[i].configured && _edpt_list[i].dev_addr == dev_addr) {
      _edpt_list[i].configured = false;
    }
  }
}

//--------------------------------------------------------------------+
// Low-level hardware helpers
//--------------------------------------------------------------------+

// Busywait delay (approximate microseconds at ~60MHz)
TU_ATTR_ALWAYS_INLINE static inline void _delay_loops(uint32_t count) {
  volatile uint32_t c = count / 3;
  if (c == 0) return;
  while (c-- != 0) {}
}

static void _hw_init_host(uint8_t rhport, bool enabled) {
  uint32_t base = _port_data[rhport].usb_base;

  if (!enabled) {
    // Reset SIE when disabling
    CH58X_USB_CTRL(base) = CH58X_UC_RESET_SIE | CH58X_UC_CLR_ALL;
    _delay_loops(600); // ~10us at 60MHz
    CH58X_USB_CTRL(base) = 0;
    return;
  }

  // host mode, pull-down enabled, clear addr
  CH58X_USB_CTRL(base) = CH58X_UC_HOST_MODE;
  while (!(CH58X_USB_CTRL(base) & CH58X_UC_HOST_MODE)) {}
  CH58X_UHOST_CTRL(base) = 0;
  CH58X_USB_DEV_AD(base) = 0x00;

  // EP2 RX (IN), EP3 TX (OUT/SETUP)
  CH58X_UH_EP_MOD(base) = CH58X_UH_EP_TX_EN | CH58X_UH_EP_RX_EN;

  // DMA: 16-bit address, lower 64KB only
  CH58X_UH_RX_DMA(base) = (uint16_t)(uint32_t)_rx_buf[rhport];
  CH58X_UH_TX_DMA(base) = (uint16_t)(uint32_t)_tx_buf[rhport];

  CH58X_UH_RX_CTRL(base) = 0x00;
  CH58X_UH_TX_CTRL(base) = 0x00;

  CH58X_USB_CTRL(base) = CH58X_UC_HOST_MODE | CH58X_UC_INT_BUSY | CH58X_UC_DMA_EN;
  CH58X_UH_SETUP(base) = CH58X_UH_SOF_EN;

  CH58X_USB_INT_FG(base) = 0xFF; // clear all flags
  CH58X_USB_INT_EN(base) = CH58X_UIE_TRANSFER | CH58X_UIE_DETECT;
}

static bool _hw_start_xfer(uint8_t rhport, uint8_t pid, uint8_t ep_addr, uint8_t data_toggle) {
  uint32_t base = _port_data[rhport].usb_base;

  LOG_CH58X_HCD("_hw_start_xfer(pid=0x%02x, ep=0x%02x, tog=%d)\r\n", pid, ep_addr, data_toggle);

  // Workaround: small delay for low-speed devices
  bool is_lowspeed = tuh_speed_get(_current_xfer.dev_addr) == TUSB_SPEED_LOW;
  if (is_lowspeed) {
    _delay_loops(60000000 / 1000000 * 40); // ~40us at 60MHz
  }

  // Set toggle controls (same as SDK: R8_UH_RX_CTRL = R8_UH_TX_CTRL = tog)
  uint8_t tog_ctrl = (data_toggle != 0) ? CH58X_UH_T_TOG : 0;
  CH58X_UH_TX_CTRL(base) = tog_ctrl;
  CH58X_UH_RX_CTRL(base) = (data_toggle != 0) ? CH58X_UH_R_TOG : 0;

  uint8_t pid_endp = (pid << 4) | (tu_edpt_number(ep_addr) & 0x0F);

  // clear flag, enable int, then set PID to start transfer
  CH58X_USB_INT_FG(base) = CH58X_UIF_TRANSFER;
  CH58X_USB_INT_EN(base) |= CH58X_UIE_TRANSFER;
  CH58X_UH_EP_PID(base) = pid_endp;

  return true;
}

static void _hw_set_device_addr(uint8_t rhport, uint8_t dev_addr) {
  uint32_t base = _port_data[rhport].usb_base;
  CH58X_USB_DEV_AD(base) = (CH58X_USB_DEV_AD(base) & CH58X_UDA_GP_BIT) |
                            (dev_addr & CH58X_USB_ADDR_MASK);
}

static void _hw_set_speed(uint8_t rhport, tusb_speed_t speed) {
  uint32_t base = _port_data[rhport].usb_base;

  LOG_CH58X_HCD("_hw_set_speed(%s)\r\n",
                speed == TUSB_SPEED_FULL ? "Full" : "Low");

  if (speed == TUSB_SPEED_LOW) {
    CH58X_USB_CTRL(base) |= CH58X_UC_LOW_SPEED;
    CH58X_UHOST_CTRL(base) |= CH58X_UH_LOW_SPEED;
  } else {
    CH58X_USB_CTRL(base) &= ~CH58X_UC_LOW_SPEED;
    CH58X_UHOST_CTRL(base) &= ~CH58X_UH_LOW_SPEED;
    CH58X_UH_SETUP(base) &= ~CH58X_UH_PRE_PID_EN;
  }
}

static void _hw_set_addr_speed(uint8_t rhport, uint8_t dev_addr) {
  _hw_set_device_addr(rhport, dev_addr);

  tusb_speed_t rhport_speed = hcd_port_speed_get(rhport);
  tusb_speed_t dev_speed = tuh_speed_get(dev_addr);
  _hw_set_speed(rhport, dev_speed);

  // FS root + LS device: hub uses PRE PID, clear LS on host ctrl
  if (rhport_speed == TUSB_SPEED_FULL && dev_speed == TUSB_SPEED_LOW) {
    uint32_t base = _port_data[rhport].usb_base;
    CH58X_UHOST_CTRL(base) &= ~CH58X_UH_LOW_SPEED;
  }
}

static bool _hw_device_attached(uint8_t rhport) {
  uint32_t base = _port_data[rhport].usb_base;
  return (CH58X_USB_MIS_ST(base) & CH58X_UMS_DEV_ATTACH) != 0;
}

//--------------------------------------------------------------------+
// NAK retry callback
//--------------------------------------------------------------------+
static void _xfer_retry(void* param) {
  LOG_CH58X_HCD("_xfer_retry()\r\n");
  hcd_edpt_t* edpt = (hcd_edpt_t*)param;

  if (_current_xfer.nak_pending) {
    uint8_t rhport = _current_xfer.rhport;
    _current_xfer.nak_pending = false;
    edpt->is_nak_pending = false;

    uint8_t dev_addr = edpt->dev_addr;
    uint8_t ep_addr = edpt->ep_addr;
    uint16_t buflen = edpt->buflen;
    uint8_t* buf = edpt->buf;

    // Check if endpoint is still valid
    hcd_edpt_t* current = _get_edpt(dev_addr, ep_addr);
    if (current) {
      hcd_edpt_xfer(rhport, dev_addr, ep_addr, buf, buflen);
    }
  }
}

//--------------------------------------------------------------------+
// HCD API: Controller
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void)rh_init;

  _port_data[rhport].usb_base = _get_usb_base(rhport);
  _port_data[rhport].int_enabled = false;

  // Clear endpoint records
  tu_memclr(_edpt_list, sizeof(_edpt_list));
  tu_memclr((void*)&_current_xfer, sizeof(_current_xfer));

  _hw_init_host(rhport, true);

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  _hw_init_host(rhport, false);
  return true;
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void)rhport;
  return tusb_time_millis_api();
}

void hcd_int_enable(uint8_t rhport) {
  // PFIC interrupt enable
  volatile uint32_t* pfic_ienr = (volatile uint32_t*)0xE000E100;
  uint8_t irqn = (rhport == 0) ? 22 : 23; // USB_IRQn or USB2_IRQn
  pfic_ienr[irqn / 32] = (1u << (irqn % 32));
  _port_data[rhport].int_enabled = true;
}

void hcd_int_disable(uint8_t rhport) {
  volatile uint32_t* pfic_irer = (volatile uint32_t*)0xE000E180;
  uint8_t irqn = (rhport == 0) ? 22 : 23;
  pfic_irer[irqn / 32] = (1u << (irqn % 32));
  __asm volatile("fence.i");
  _port_data[rhport].int_enabled = false;
}

//--------------------------------------------------------------------+
// HCD API: Port
//--------------------------------------------------------------------+
bool hcd_port_connect_status(uint8_t rhport) {
  return _hw_device_attached(rhport);
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  uint32_t base = _port_data[rhport].usb_base;
  // DM level high = low-speed device, low = full-speed
  if (CH58X_USB_MIS_ST(base) & CH58X_UMS_DM_LEVEL) {
    return TUSB_SPEED_LOW;
  }
  return TUSB_SPEED_FULL;
}

static bool _int_state_before_reset = false;

void hcd_port_reset(uint8_t rhport) {
  uint32_t base = _port_data[rhport].usb_base;

  LOG_CH58X_HCD("hcd_port_reset()\r\n");

  _int_state_before_reset = _port_data[rhport].int_enabled;
  hcd_int_disable(rhport);

  _hw_set_device_addr(rhport, 0x00);

  // Disable port and default to full-speed before reset (matches SDK ResetRootHubPort)
  CH58X_UHOST_CTRL(base) &= ~CH58X_UH_PORT_EN;
  _hw_set_speed(rhport, TUSB_SPEED_FULL);

  // Start bus reset (clear low-speed bit simultaneously)
  CH58X_UHOST_CTRL(base) = (CH58X_UHOST_CTRL(base) & ~CH58X_UH_LOW_SPEED) | CH58X_UH_BUS_RESET;
}

void hcd_port_reset_end(uint8_t rhport) {
  uint32_t base = _port_data[rhport].usb_base;

  LOG_CH58X_HCD("hcd_port_reset_end()\r\n");

  // End bus reset
  CH58X_UHOST_CTRL(base) &= ~CH58X_UH_BUS_RESET;
  tusb_time_delay_ms_api(2);

  // Detect speed and configure
  if ((CH58X_UHOST_CTRL(base) & CH58X_UH_PORT_EN) == 0) {
    if (hcd_port_speed_get(rhport) == TUSB_SPEED_LOW) {
      _hw_set_speed(rhport, TUSB_SPEED_LOW);
    }
  }

  // Enable port and SOF
  CH58X_UHOST_CTRL(base) |= CH58X_UH_PORT_EN;
  CH58X_UH_SETUP(base) |= CH58X_UH_SOF_EN;

  // Suppress stale detect event
  CH58X_USB_INT_FG(base) = CH58X_UIF_DETECT;

  if (_int_state_before_reset) {
    hcd_int_enable(rhport);
  }
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void)rhport;
  LOG_CH58X_HCD("hcd_device_close(dev=0x%02x)\r\n", dev_addr);
  _remove_edpts_for_device(dev_addr);
}

//--------------------------------------------------------------------+
// HCD API: Endpoint
//--------------------------------------------------------------------+
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const* ep_desc) {
  uint8_t ep_addr = ep_desc->bEndpointAddress;
  uint8_t ep_num = tu_edpt_number(ep_addr);
  uint16_t max_packet_size = ep_desc->wMaxPacketSize;
  uint8_t xfer_type = ep_desc->bmAttributes.xfer;
  uint32_t base = _port_data[rhport].usb_base;

  LOG_CH58X_HCD("hcd_edpt_open(dev=0x%02x, ep=0x%02x, mps=%d, type=%d)\r\n",
                dev_addr, ep_addr, max_packet_size, xfer_type);

  // Wait for any pending transfer
  uint32_t t0 = tusb_time_millis_api();
  while (_current_xfer.is_busy) {
    if (tusb_time_millis_api() - t0 > 200) {
      _current_xfer.is_busy = false;
      break;
    }
  }

  if (ep_num == 0) {
    TU_ASSERT(_get_or_add_edpt(dev_addr, 0x00, max_packet_size, xfer_type) != NULL, false);
    TU_ASSERT(_get_or_add_edpt(dev_addr, 0x80, max_packet_size, xfer_type) != NULL, false);
  } else {
    TU_ASSERT(_get_or_add_edpt(dev_addr, ep_addr, max_packet_size, xfer_type) != NULL, false);
  }

  // Ensure port is enabled with SOF
  CH58X_UHOST_CTRL(base) |= CH58X_UH_PORT_EN;
  CH58X_UH_SETUP(base) |= CH58X_UH_SOF_EN;

  _hw_set_addr_speed(rhport, dev_addr);

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr,
                   uint8_t* buffer, uint16_t buflen) {
  uint32_t base = _port_data[rhport].usb_base;

  LOG_CH58X_HCD("hcd_edpt_xfer(dev=0x%02x, ep=0x%02x, len=%d)\r\n",
                dev_addr, ep_addr, buflen);

  // Wait for any pending transfer (with 200ms timeout to avoid deadlock on disconnect)
  uint32_t t0 = tusb_time_millis_api();
  while (_current_xfer.is_busy) {
    if (tusb_time_millis_api() - t0 > 200) {
      _current_xfer.is_busy = false;
      return false;
    }
  }
  _current_xfer.is_busy = true;

  hcd_edpt_t* edpt = _get_edpt(dev_addr, ep_addr);
  TU_ASSERT(edpt != NULL);

  _hw_set_addr_speed(rhport, dev_addr);

  _current_xfer.rhport = rhport;
  _current_xfer.dev_addr = dev_addr;
  _current_xfer.ep_addr = ep_addr;
  _current_xfer.buffer = buffer;
  _current_xfer.bufferlen = buflen;
  _current_xfer.start_ms = tusb_time_millis_api();
  _current_xfer.xferred_len = 0;
  _current_xfer.nak_pending = false;

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    // IN transfer: host receives data
    return _hw_start_xfer(rhport, CH58X_USB_PID_IN, ep_addr, edpt->data_toggle);
  } else {
    // OUT transfer: host sends data
    uint16_t copylen = TU_MIN(edpt->max_packet_size, buflen);
    CH58X_UH_TX_LEN(base) = (uint8_t)copylen;
    memcpy(_tx_buf[rhport], buffer, copylen);
    return _hw_start_xfer(rhport, CH58X_USB_PID_OUT, ep_addr, edpt->data_toggle);
  }
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  (void)dev_addr;
  (void)ep_addr;
  return false;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  uint32_t base = _port_data[rhport].usb_base;

  LOG_CH58X_HCD("hcd_setup_send(dev=0x%02x)\r\n", dev_addr);

  // Wait for any pending transfer
  uint32_t t0 = tusb_time_millis_api();
  while (_current_xfer.is_busy) {
    if (tusb_time_millis_api() - t0 > 200) {
      _current_xfer.is_busy = false;
      return false;
    }
  }
  _current_xfer.is_busy = true;

  _hw_set_addr_speed(rhport, dev_addr);

  hcd_edpt_t* edpt_tx = _get_edpt(dev_addr, 0x00);
  hcd_edpt_t* edpt_rx = _get_edpt(dev_addr, 0x80);
  TU_ASSERT(edpt_tx != NULL, false);
  TU_ASSERT(edpt_rx != NULL, false);

  // SETUP always starts with DATA0; after SETUP, IN data starts with DATA1
  edpt_tx->data_toggle = 0;
  edpt_rx->data_toggle = 1;

  memcpy(_tx_buf[rhport], setup_packet, 8);
  CH58X_UH_TX_LEN(base) = 8;

  _current_xfer.rhport = rhport;
  _current_xfer.dev_addr = dev_addr;
  _current_xfer.ep_addr = 0x00;  // SETUP always targets EP0 OUT
  _current_xfer.start_ms = tusb_time_millis_api();
  _current_xfer.buffer = _tx_buf[rhport];
  _current_xfer.bufferlen = 8;
  _current_xfer.xferred_len = 0;
  _current_xfer.nak_pending = false;

  _hw_start_xfer(rhport, CH58X_USB_PID_SETUP, 0, 0);

  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;

  LOG_CH58X_HCD("hcd_edpt_clear_stall(dev=0x%02x, ep=0x%02x)\r\n", dev_addr, ep_addr);
  hcd_edpt_t* edpt = _get_edpt(dev_addr, ep_addr);
  if (edpt != NULL) {
    edpt->data_toggle = 0;
  }

  return true;
}

//--------------------------------------------------------------------+
// Interrupt Handler
//--------------------------------------------------------------------+
void hcd_int_handler(uint8_t rhport, bool in_isr) {
  uint32_t base = _port_data[rhport].usb_base;

  //-- Device attach/detach detection --
  if (CH58X_USB_INT_FG(base) & CH58X_UIF_DETECT) {
    CH58X_USB_INT_FG(base) = CH58X_UIF_DETECT;

    bool attached = _hw_device_attached(rhport);
    LOG_CH58X_HCD("hcd_int: detect, attached=%d\r\n", attached);

    if (attached) {
      hcd_event_device_attach(rhport, in_isr);
    } else {
      // Stop any ongoing hardware transfer before reporting removal
      CH58X_UH_EP_PID(base) = 0x00;
      _current_xfer.is_busy = false;
      _current_xfer.nak_pending = false;
      hcd_event_device_remove(rhport, in_isr);
    }
    return;
  }

  //-- Transfer complete --
  if (CH58X_USB_INT_FG(base) & CH58X_UIF_TRANSFER) {
    // Read PID/endpoint before stopping (must read first!)
    uint8_t pid_endp = CH58X_UH_EP_PID(base);
    uint8_t int_st = CH58X_USB_INT_ST(base);
    uint8_t dev_addr = CH58X_USB_DEV_AD(base) & CH58X_USB_ADDR_MASK;

    // Stop USB transaction immediately (SDK: R8_UH_EP_PID = 0x00)
    CH58X_UH_EP_PID(base) = 0x00;

    // Disable transfer interrupt (re-enabled when next transfer starts)
    CH58X_USB_INT_EN(base) &= ~CH58X_UIE_TRANSFER;

    uint8_t request_pid = pid_endp >> 4;
    uint8_t response_pid = int_st & CH58X_UIS_H_RES_MASK;
    uint8_t ep_addr = pid_endp & 0x0F;
    if (request_pid == CH58X_USB_PID_IN) {
      ep_addr |= 0x80;
    }

    LOG_CH58X_HCD("hcd_int: xfer pid=0x%02x ep=0x%02x resp=0x%02x\r\n",
                  request_pid, ep_addr, response_pid);

    hcd_edpt_t* edpt = _get_edpt(dev_addr, ep_addr);
    if (edpt == NULL) {
      // Unknown endpoint, discard
      LOG_CH58X_HCD("hcd_int: unknown edpt dev=0x%02x ep=0x%02x\r\n", dev_addr, ep_addr);
      _current_xfer.is_busy = false;
      CH58X_USB_INT_FG(base) = CH58X_UIF_TRANSFER;
      return;
    }

    // Check toggle match - SDK uses R8_USB_INT_ST & RB_UIS_TOG_OK
    if (int_st & CH58X_UIS_TOG_OK) {
      edpt->data_toggle ^= 0x01;

      switch (request_pid) {
        case CH58X_USB_PID_SETUP:
        case CH58X_USB_PID_OUT: {
          uint8_t tx_len = CH58X_UH_TX_LEN(base);
          _current_xfer.bufferlen -= tx_len;
          _current_xfer.xferred_len += tx_len;

          if (_current_xfer.bufferlen == 0) {
            LOG_CH58X_HCD("OUT/SETUP complete, %d bytes\r\n", _current_xfer.xferred_len);
            _current_xfer.is_busy = false;
            hcd_event_xfer_complete(dev_addr, ep_addr,
                                    _current_xfer.xferred_len, XFER_RESULT_SUCCESS, in_isr);
          } else {
            // Multi-packet OUT: send next chunk
            _current_xfer.buffer += tx_len;
            uint16_t copylen = TU_MIN(edpt->max_packet_size, _current_xfer.bufferlen);
            memcpy(_tx_buf[rhport], _current_xfer.buffer, copylen);
            CH58X_UH_TX_LEN(base) = (uint8_t)copylen;
            _hw_start_xfer(rhport, CH58X_USB_PID_OUT, ep_addr, edpt->data_toggle);
          }
          break;
        }

        case CH58X_USB_PID_IN: {
          uint8_t rx_len = CH58X_USB_RX_LEN(base);
          _current_xfer.xferred_len += rx_len;
          uint16_t xferred = _current_xfer.xferred_len;

          if (rx_len > 0 && _current_xfer.buffer != NULL) {
            memcpy(_current_xfer.buffer, _rx_buf[rhport], rx_len);
            _current_xfer.buffer += rx_len;
          }

          if ((rx_len < edpt->max_packet_size) || (xferred == _current_xfer.bufferlen)) {
            // Short packet or transfer complete
            LOG_CH58X_HCD("IN complete, %d bytes\r\n", xferred);
            _current_xfer.is_busy = false;
            hcd_event_xfer_complete(dev_addr, ep_addr, xferred,
                                    XFER_RESULT_SUCCESS, in_isr);
          } else {
            // More data expected
            _hw_start_xfer(rhport, CH58X_USB_PID_IN, ep_addr, edpt->data_toggle);
          }
          break;
        }

        default: {
          LOG_CH58X_HCD("unexpected PID 0x%02x\r\n", request_pid);
          _current_xfer.is_busy = false;
          hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
          break;
        }
      }
    } else {
      // Toggle mismatch, check response PID
      if (response_pid == CH58X_USB_PID_STALL) {
        LOG_CH58X_HCD("STALL response\r\n");
        edpt->data_toggle = 0;
        _current_xfer.is_busy = false;
        hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_STALLED, in_isr);
      } else if (response_pid == CH58X_USB_PID_NAK) {
        LOG_CH58X_HCD("NAK response\r\n");
        // NAK: schedule retry via deferred callback for all endpoint types
        // This avoids tight polling loops and allows other tasks to run
        _current_xfer.is_busy = false;
        _current_xfer.nak_pending = true;

        edpt->is_nak_pending = true;
        edpt->buflen = _current_xfer.bufferlen;
        edpt->buf = _current_xfer.buffer;

        hcd_event_t event = {
          .rhport = rhport,
          .dev_addr = dev_addr,
          .event_id = USBH_EVENT_FUNC_CALL,
          .func_call = {
            .func = _xfer_retry,
            .param = edpt
          }
        };
        hcd_event_handler(&event, in_isr);
      } else if (response_pid == CH58X_USB_PID_DATA0 || response_pid == CH58X_USB_PID_DATA1) {
        LOG_CH58X_HCD("toggle mismatch, DATA0/1 rx_len=%d\r\n", CH58X_USB_RX_LEN(base));
        _current_xfer.is_busy = false;
        hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
      } else {
        LOG_CH58X_HCD("unexpected response 0x%02x\r\n", response_pid);
        _current_xfer.is_busy = false;
        hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
      }
    }

    // Clear transfer flag
    CH58X_USB_INT_FG(base) = CH58X_UIF_TRANSFER;
  }
}

#endif /* CFG_TUH_ENABLED && TUP_USBIP_WCH_CH58X */
