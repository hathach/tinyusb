/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Mitsumine Suzu (verylowfreq)
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

#if CFG_TUH_ENABLED && defined(TUP_USBIP_WCH_USBFS) && defined(CFG_TUH_WCH_USBIP_USBFS) && CFG_TUH_WCH_USBIP_USBFS


#include "host/hcd.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"

#include "bsp/board_api.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

#include "ch32v20x.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "ch32v20x_usb.h"

#define USBFS_RX_BUF_LEN 64
#define USBFS_TX_BUF_LEN 64
TU_ATTR_ALIGNED(4) static uint8_t USBFS_RX_Buf[USBFS_RX_BUF_LEN];
TU_ATTR_ALIGNED(4) static uint8_t USBFS_TX_Buf[USBFS_TX_BUF_LEN];


#define PANIC(...)                            \
  do {                                        \
    printf("%s() L%d: ", __func__, __LINE__); \
    printf("\r\n[PANIC] " __VA_ARGS__);       \
    while (true) {}                           \
  } while (false)

#define LOG_CH32_USBFSH(...) TU_LOG3(__VA_ARGS__)

// Busywait for delay microseconds/nanoseconds
TU_ATTR_ALWAYS_INLINE static inline void loopdelay(uint32_t count) {
  volatile uint32_t c = count / 3;
  if (c == 0) { return; }
  // while (c-- != 0);
  asm volatile(
    "1:                     \n" // loop label
    "    addi  %0, %0, -1   \n" // c--
    "    bne   %0, zero, 1b \n" // if (c != 0) goto loop
    : "+r"(c) // c is input/output operand
  );
}

// Endpoint status
typedef struct usb_edpt {
  // Is this a valid struct
  bool configured;

  uint8_t dev_addr;
  uint8_t ep_addr;
  uint8_t max_packet_size;

  uint8_t xfer_type;
  uint8_t interval;

  // Data toggle (0 or not 0) for DATA0/1
  uint8_t data_toggle;

  uint32_t xfer_start_ms;
  uint32_t next_retry_sof;
  bool is_nak_pending;

  uint16_t xferred_len;
  uint16_t buflen;
  uint8_t* buf;
} usb_edpt_t;


#define USB_EDPT_LIST_LENGTH (CFG_TUH_DEVICE_MAX * 6)
static usb_edpt_t usb_edpt_list[USB_EDPT_LIST_LENGTH] = {};

typedef struct usb_current_xfer_st {
  bool is_busy;
  uint8_t dev_addr;
  uint8_t ep_addr;
  usb_edpt_t* edpt_info;
} usb_current_xfer_t;

static uint8_t nak_retry_roundrobin = 0;

static volatile uint32_t sof_number = 0;

static volatile usb_current_xfer_t usb_current_xfer_info = {};

static usb_edpt_t *get_edpt_record(uint8_t dev_addr, uint8_t ep_addr) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    usb_edpt_t *cur = &usb_edpt_list[i];
    if (cur->configured && cur->dev_addr == dev_addr && cur->ep_addr == ep_addr) {
      return cur;
    }
  }
  return NULL;
}

static usb_edpt_t *get_empty_record_slot(void) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    if (!usb_edpt_list[i].configured) {
      return &usb_edpt_list[i];
    }
  }
  return NULL;
}

static usb_edpt_t *add_edpt_record(uint8_t dev_addr, uint8_t ep_addr, uint16_t max_packet_size, uint8_t xfer_type) {
  usb_edpt_t *slot = get_empty_record_slot();
  TU_ASSERT(slot != NULL, NULL);

  slot->dev_addr = dev_addr;
  slot->ep_addr = ep_addr;
  slot->max_packet_size = max_packet_size;
  slot->xfer_type = xfer_type;
  slot->interval = 0;
  slot->data_toggle = 0;
  slot->xfer_start_ms = 0;
  slot->next_retry_sof = 0;
  slot->is_nak_pending = false;
  slot->xferred_len = 0;
  slot->buflen = 0;
  slot->buf = NULL;

  slot->configured = true;

  return slot;
}

static usb_edpt_t *get_or_add_edpt_record(uint8_t dev_addr, uint8_t ep_addr, uint16_t max_packet_size, uint8_t xfer_type) {
  usb_edpt_t *ret = get_edpt_record(dev_addr, ep_addr);
  if (ret != NULL) {
    return ret;
  } else {
    return add_edpt_record(dev_addr, ep_addr, max_packet_size, xfer_type);
  }
}

static void remove_edpt_record_for_device(uint8_t dev_addr) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    if (usb_edpt_list[i].configured && usb_edpt_list[i].dev_addr == dev_addr) {
      usb_edpt_list[i].configured = false;
    }
  }
}

// static void dump_edpt_record_list() {
//     for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
//         usb_edpt_t* cur = &usb_edpt_list[i];
//         if (cur->configured) {
//             printf("[%2d] Device 0x%02x Endpoint 0x%02x\r\n", i, cur->dev_addr, cur->ep_addr);
//         } else {
//             printf("[%2d] not configured\r\n", i);
//         }
//     }
// }

static bool interrupt_enabled = false;

/** Enable or disable USBFS Host function */
static void hardware_init_host(bool enabled) {
  // Reset USBOTG module
  USBOTG_H_FS->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;

  tusb_time_delay_ms_api(1);
  USBOTG_H_FS->BASE_CTRL = 0;

  if (!enabled) {
    // Disable all feature
    USBOTG_H_FS->BASE_CTRL = 0;
  } else {
    // Enable USB Host features
    // NVIC_DisableIRQ(USBFS_IRQn);
    hcd_int_disable(0);
    USBOTG_H_FS->BASE_CTRL = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBOTG_H_FS->HOST_EP_MOD = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;
    USBOTG_H_FS->HOST_RX_DMA = (uint32_t) USBFS_RX_Buf;
    USBOTG_H_FS->HOST_TX_DMA = (uint32_t) USBFS_TX_Buf;
    // USBOTG_H_FS->INT_EN = USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
    USBOTG_H_FS->INT_EN = USBFS_UIE_DETECT | USBFS_UIE_HST_SOF;
  }
}

static bool hardware_start_xfer(uint8_t pid, uint8_t ep_addr, uint8_t data_toggle) {
  LOG_CH32_USBFSH("hardware_start_xfer(pid=%s(0x%02x), ep_addr=0x%02x, toggle=%d)\r\n",
                  pid == USB_PID_IN ? "IN" : pid == USB_PID_OUT ? "OUT"
                                         : pid == USB_PID_SETUP ? "SETUP"
                                                                : "(other)",
                  pid, ep_addr, data_toggle);

  //WORKAROUND: For LowSpeed device, insert small delay
  bool is_lowspeed_device = tuh_speed_get(usb_current_xfer_info.dev_addr) == TUSB_SPEED_LOW;
  if (is_lowspeed_device) {
    //NOTE: worked -> SystemCoreClock / 1000000 * 50, 25
    //      NOT worked -> 20 and less  (at 144MHz internal clock)
    loopdelay(SystemCoreClock / 1000000 * 40);
  }

  uint8_t pid_edpt = (pid << 4) | (tu_edpt_number(ep_addr) & 0x0f);
  USBOTG_H_FS->HOST_TX_CTRL = (data_toggle != 0) ? USBFS_UH_T_TOG : 0;
  USBOTG_H_FS->HOST_RX_CTRL = (data_toggle != 0) ? USBFS_UH_R_TOG : 0;
  USBOTG_H_FS->HOST_EP_PID = pid_edpt;
  USBOTG_H_FS->INT_EN |= USBFS_UIE_TRANSFER;
  USBOTG_H_FS->INT_FG = USBFS_UIF_TRANSFER;
  return true;
}


/** Set device address to communicate */
static void hardware_update_device_address(uint8_t dev_addr) {
  // Keep the bit of GP_BIT. Other 7bits are actual device address.
  USBOTG_H_FS->DEV_ADDR = (USBOTG_H_FS->DEV_ADDR & USBFS_UDA_GP_BIT) | (dev_addr & USBFS_USB_ADDR_MASK);
}

/** Set port speed */
static void hardware_update_port_speed(tusb_speed_t speed) {
  LOG_CH32_USBFSH("hardware_update_port_speed(%s)\r\n", speed == TUSB_SPEED_FULL ? "Full" : speed == TUSB_SPEED_LOW ? "Low"
                                                                                                                    : "(invalid)");
  switch (speed) {
    case TUSB_SPEED_LOW:
      USBOTG_H_FS->BASE_CTRL |= USBFS_UC_LOW_SPEED;
      USBOTG_H_FS->HOST_CTRL |= USBFS_UH_LOW_SPEED;
      // USBOTG_H_FS->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
      return;
    case TUSB_SPEED_FULL:
      USBOTG_H_FS->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
      USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
      USBOTG_H_FS->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
      return;
    default:
      PANIC("hardware_update_port_speed(%d)\r\n", speed);
  }
}

static void hardware_set_port_address_speed(uint8_t dev_addr) {
  hardware_update_device_address(dev_addr);
  tusb_speed_t rhport_speed = hcd_port_speed_get(0);
  tusb_speed_t dev_speed = tuh_speed_get(dev_addr);
  hardware_update_port_speed(dev_speed);
  if (rhport_speed == TUSB_SPEED_FULL && dev_speed == TUSB_SPEED_LOW) {
    USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
    USBOTG_H_FS->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
  } else {
    USBOTG_H_FS->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
  }
}

static bool hardware_device_attached(void) {
  return USBOTG_H_FS->MIS_ST & USBFS_UMS_DEV_ATTACH;
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void) rhport;
  (void) rh_init;

  memset(usb_edpt_list, 0x00, sizeof(usb_edpt_list));

  hardware_init_host(true);

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void) rhport;
  hardware_init_host(false);

  return true;
}

static bool int_state_for_portreset = false;

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
  LOG_CH32_USBFSH("hcd_port_reset()\r\n");
  int_state_for_portreset = interrupt_enabled;
  // NVIC_DisableIRQ(USBFS_IRQn);
  hcd_int_disable(rhport);
  hardware_update_device_address(0x00);

  // USBOTG_H_FS->HOST_SETUP = 0x00;

  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_BUS_RESET;

  return;
}

void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
  LOG_CH32_USBFSH("hcd_port_reset_end()\r\n");

  USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
  tusb_time_delay_ms_api(2);

  // if ((USBOTG_H_FS->HOST_CTRL & USBFS_UH_PORT_EN) == 0) {
    if (hcd_port_speed_get(0) == TUSB_SPEED_LOW) {
      hardware_update_port_speed(TUSB_SPEED_LOW);
    }
  // }

  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
  USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;

  // Suppress the attached event
  USBOTG_H_FS->INT_FG |= USBFS_UIF_DETECT;

  if (int_state_for_portreset) {
    hcd_int_enable(rhport);
  }
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;

  return hardware_device_attached();
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  if (USBOTG_H_FS->MIS_ST & USBFS_UMS_DM_LEVEL) {
    return TUSB_SPEED_LOW;
  } else {
    return TUSB_SPEED_FULL;
  }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;
  LOG_CH32_USBFSH("hcd_device_close(%d, 0x%02x)\r\n", rhport, dev_addr);
  remove_edpt_record_for_device(dev_addr);
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;

  return sof_number;
}

void hcd_int_enable(uint8_t rhport) {
  (void) rhport;
  NVIC_EnableIRQ(USBFS_IRQn);
  interrupt_enabled = true;
}

void hcd_int_disable(uint8_t rhport) {
  (void) rhport;
  NVIC_DisableIRQ(USBFS_IRQn);
  interrupt_enabled = false;
}


static void xfer_retry(void* _params) {
  LOG_CH32_USBFSH("xfer_retry()\r\n");
  usb_edpt_t* edpt_info = (usb_edpt_t*)_params;

  edpt_info->is_nak_pending = false;

  uint8_t dev_addr = edpt_info->dev_addr;
  uint8_t ep_addr = edpt_info->ep_addr;
  uint16_t buflen = edpt_info->buflen;
  uint8_t* buf = edpt_info->buf;

  // Check connectivity
  usb_edpt_t* edpt_info_current = get_edpt_record(dev_addr, ep_addr);
  if (edpt_info_current) {
    while (usb_current_xfer_info.is_busy) { }

    usb_current_xfer_info.is_busy = true;
    usb_current_xfer_info.dev_addr = dev_addr;
    usb_current_xfer_info.ep_addr = ep_addr;
    usb_current_xfer_info.edpt_info = edpt_info_current;

    edpt_info->xfer_start_ms = tusb_time_millis_api();

    hardware_set_port_address_speed(dev_addr);

    if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
      hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info->data_toggle);
    } else {
      uint16_t copylen = TU_MIN(edpt_info->max_packet_size, buflen);
      USBOTG_H_FS->HOST_TX_LEN = copylen;
      memcpy(USBFS_TX_Buf, buf, copylen);
      hardware_start_xfer(USB_PID_OUT, ep_addr, edpt_info->data_toggle);
    }
  }
}

// Pick up next NAK pending endpoint
static void xfer_retry_next(void) {
  for (uint8_t i = 0; i < USB_EDPT_LIST_LENGTH; i++) {
    uint8_t index = (nak_retry_roundrobin + i) % USB_EDPT_LIST_LENGTH;
    usb_edpt_t* edpt = &usb_edpt_list[index];
    if (!edpt->configured || !edpt->is_nak_pending) {
      continue;
    }
    if (edpt->interval != 0) {
      if ((int32_t) (sof_number - edpt->next_retry_sof) < 0) {
        continue;
      }
    } else {
      // fallthrough
    }
    LOG_CH32_USBFSH("NAK pending at DEV %d EP %02x\r\n", edpt->dev_addr, edpt->ep_addr);
    xfer_retry(edpt);
    break;
  }
  nak_retry_roundrobin = (nak_retry_roundrobin + 1) % USB_EDPT_LIST_LENGTH;
}


void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void) rhport;
  (void) in_isr;

  if (USBOTG_H_FS->INT_FG & USBFS_UIF_HST_SOF) {
    USBOTG_H_FS->INT_FG |= USBFS_UIF_HST_SOF;
    sof_number += 1;
    if (!(USBOTG_H_FS->INT_FG & USBFS_UIF_TRANSFER)) {
      if (!usb_current_xfer_info.is_busy) {
        xfer_retry_next();
        return;
      }
    }
  }

  if (USBOTG_H_FS->INT_FG & USBFS_UIF_DETECT) {
    // Clear the flag
    USBOTG_H_FS->INT_FG = USBFS_UIF_DETECT;
    // Read the detection state
    bool attached = hardware_device_attached();
    LOG_CH32_USBFSH("hcd_int_handler() attached = %d\r\n", attached ? 1 : 0);
    if (attached) {
      hcd_event_device_attach(rhport, true);
    } else {
      hcd_event_device_remove(rhport, true);
    }
  }

  if (USBOTG_H_FS->INT_FG & USBFS_UIF_TRANSFER) {
    // Disable transfer interrupt
    USBOTG_H_FS->INT_EN &= ~USBFS_UIE_TRANSFER;
    // Clear the flag
    // NOTE: This flags will be cleared by hardware.
    // USBOTG_H_FS->INT_FG |= USBFS_UIF_TRANSFER;
    // Copy PID and Endpoint
    const uint8_t pid_edpt = USBOTG_H_FS->HOST_EP_PID;
    const uint8_t status = USBOTG_H_FS->INT_ST;
    const uint8_t dev_addr = USBOTG_H_FS->DEV_ADDR & USBFS_USB_ADDR_MASK;
    // Clear register to stop transfer
    USBOTG_H_FS->HOST_EP_PID = 0x00;

    LOG_CH32_USBFSH("hcd_int_handler() pid_edpt=0x%02x\r\n", pid_edpt);

    const uint8_t request_pid = pid_edpt >> 4;
    const uint8_t response_pid = status & USBFS_UIS_H_RES_MASK;
    const uint8_t ep_addr = (pid_edpt & 0x0f) | ((request_pid == USB_PID_IN) ? 0x80 : 0);

    usb_edpt_t *edpt_info = get_edpt_record(dev_addr, ep_addr);
    if (edpt_info == NULL) {
      PANIC("\r\nget_edpt_record(0x%02x, 0x%02x) returned NULL in USBHD_IRQHandler\r\n", dev_addr, ep_addr);
    }

    if (!usb_current_xfer_info.is_busy) {
      LOG_CH32_USBFSH("Unexpected hcd_int_handler() execution.\r\n");
      return;
    }

    if (status & USBFS_UIS_TOG_OK) {
      edpt_info->data_toggle ^= 0x01;
      edpt_info->is_nak_pending = false;

      switch (request_pid) {
        case USB_PID_SETUP:
        case USB_PID_OUT: {
          uint16_t tx_len = USBOTG_H_FS->HOST_TX_LEN;
          LOG_CH32_USBFSH("hcd_int_hander() SETUP or OUT: buflen=%d,tx_len=%d\r\n", edpt_info->buflen, tx_len);
          edpt_info->buf += tx_len;
          edpt_info->buflen -= tx_len;
          edpt_info->xferred_len += tx_len;
          if (edpt_info->buflen == 0) {
            LOG_CH32_USBFSH("USB_PID_%s completed %d bytes\r\n", request_pid == USB_PID_OUT ? "OUT" : "SETUP", edpt_info->xferred_len);
            usb_current_xfer_info.is_busy = false;
            hcd_event_xfer_complete(dev_addr, ep_addr, edpt_info->xferred_len, XFER_RESULT_SUCCESS, in_isr);
            return;
          } else {
            LOG_CH32_USBFSH("USB_PID_OUT continue... remains %d bytes\r\n", edpt_info->buflen);
            uint16_t copylen = TU_MIN(edpt_info->max_packet_size, edpt_info->buflen);
            USBOTG_H_FS->HOST_TX_LEN = copylen;
            memcpy(USBFS_TX_Buf, edpt_info->buf, copylen);
            hardware_start_xfer(USB_PID_OUT, ep_addr, edpt_info->data_toggle);
            return;
          }
        }
        case USB_PID_IN: {
          uint16_t received_len = USBOTG_H_FS->RX_LEN;
          edpt_info->xferred_len += received_len;
          uint16_t xferred_len = edpt_info->xferred_len;
          LOG_CH32_USBFSH("Read %d bytes\r\n", received_len);
          // if (received_len > 0 && (usb_current_xfer_info.buffer == NULL || usb_current_xfer_info.bufferlen == 0)) {
          //     PANIC("Data received but buffer not set\r\n");
          // }
          memcpy(edpt_info->buf, USBFS_RX_Buf, received_len);
          edpt_info->buf += received_len;
          edpt_info->buflen -= received_len;
          if ((received_len < edpt_info->max_packet_size) || (edpt_info->buflen == 0)) {
            // USB device sent all data.
            LOG_CH32_USBFSH("USB_PID_IN completed\r\n");
            usb_current_xfer_info.is_busy = false;
            hcd_event_xfer_complete(dev_addr, ep_addr, xferred_len, XFER_RESULT_SUCCESS, in_isr);
            return;
          } else {
            // USB device may send more data.
            LOG_CH32_USBFSH("Read more data\r\n");
            hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info->data_toggle);
            return;
          }
        }
        default: {
          LOG_CH32_USBFSH("hcd_int_handler() L%d: unexpected response PID: 0x%02x\r\n", __LINE__, response_pid);
          usb_current_xfer_info.is_busy = false;
          hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
          return;
        }
      }
    } else {
      if (response_pid == USB_PID_STALL) {
        LOG_CH32_USBFSH("STALL response 0x%02x, HOST_SETUP=0x%04x,HOST_CTRL=0x%04x,BASE_CTRL=0x%04x\r\n", response_pid, USBOTG_H_FS->HOST_SETUP, USBOTG_H_FS->HOST_CTRL, USBOTG_H_FS->BASE_CTRL);

        // hcd_edpt_clear_stall(0, dev_addr, ep_addr);
        // edpt_info->data_toggle ^= 1;
        // hardware_start_xfer(request_pid, ep_addr, edpt_info->data_toggle);
        usb_current_xfer_info.is_busy = false;
        hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_STALLED, in_isr);
        return;

      } else if (response_pid == USB_PID_NAK) {
        LOG_CH32_USBFSH("NAK reposense. dev_addr=%d, ep_addr=0x%02x\r\n", dev_addr, ep_addr);

        edpt_info->is_nak_pending = true;
        if (edpt_info->xfer_type == TUSB_XFER_INTERRUPT && edpt_info->interval != 0) {
          edpt_info->next_retry_sof = sof_number + edpt_info->interval;
        }
        usb_current_xfer_info.is_busy = false;

        return;

      } else if (response_pid == USB_PID_DATA0 || response_pid == USB_PID_DATA1) {
        LOG_CH32_USBFSH("Data toggle mismatched and DATA0/1 (not STALL). RX_LEN=%d\r\n", USBOTG_H_FS->RX_LEN);
        usb_current_xfer_info.is_busy = false;
        hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
        return;

      } else {
        LOG_CH32_USBFSH("hcd_int_handler() L%d: unexpected response dev_addr=%d,ep_addr=0x%02x, PID: 0x%02x, HOST_SETUP=0x%04x,HOST_CTRL=0x%04x,BASE_CTRL=0x%04x\r\n",
                        __LINE__, dev_addr, ep_addr, response_pid,
                        USBOTG_H_FS->HOST_SETUP, USBOTG_H_FS->HOST_CTRL, USBOTG_H_FS->BASE_CTRL);
        usb_current_xfer_info.is_busy = false;
        hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
        return;
      }
    }
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const *ep_desc) {
  (void) rhport;
  uint8_t ep_addr = ep_desc->bEndpointAddress;
  uint8_t ep_num = tu_edpt_number(ep_addr);
  uint16_t max_packet_size = ep_desc->wMaxPacketSize;
  uint8_t xfer_type = ep_desc->bmAttributes.xfer;
  LOG_CH32_USBFSH("hcd_edpt_open(rhport=%d, dev_addr=0x%02x, %p) EndpointAdderss=0x%02x,maxPacketSize=%d,xfer_type=%d\r\n", rhport, dev_addr, ep_desc, ep_addr, max_packet_size, xfer_type);

  while (usb_current_xfer_info.is_busy) { }

  if (ep_num == 0x00) {
    usb_edpt_t* edpt_out = get_or_add_edpt_record(dev_addr, 0x00, max_packet_size, xfer_type);
    usb_edpt_t* edpt_in = get_or_add_edpt_record(dev_addr, 0x80, max_packet_size, xfer_type);
    TU_ASSERT(edpt_out != NULL, false);
    TU_ASSERT(edpt_in != NULL, false);
    edpt_out->interval = 0;
    edpt_in->interval = 0;
  } else {
    usb_edpt_t* edpt = get_or_add_edpt_record(dev_addr, ep_addr, max_packet_size, xfer_type);
    TU_ASSERT(edpt != NULL, false);
    edpt->interval = (xfer_type == TUSB_XFER_INTERRUPT) ? ep_desc->bInterval : 0;
  }

  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
  USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;

  hardware_set_port_address_speed(dev_addr);

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void) rhport;

  LOG_CH32_USBFSH("hcd_edpt_xfer(%d, 0x%02x, 0x%02x, ...)\r\n", rhport, dev_addr, ep_addr);

  while (usb_current_xfer_info.is_busy) {}
  usb_current_xfer_info.is_busy = true;

  usb_edpt_t *edpt_info = get_edpt_record(dev_addr, ep_addr);
  TU_ASSERT(edpt_info != NULL);

  hardware_set_port_address_speed(dev_addr);

  usb_current_xfer_info.dev_addr = dev_addr;
  usb_current_xfer_info.ep_addr = ep_addr;
  usb_current_xfer_info.edpt_info = edpt_info;

  edpt_info->buf = buffer;
  edpt_info->buflen = buflen;
  edpt_info->xfer_start_ms = tusb_time_millis_api();
  edpt_info->xferred_len = 0;
  edpt_info->is_nak_pending = false;

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    LOG_CH32_USBFSH("hcd_edpt_xfer(): READ, dev_addr=0x%02x, ep_addr=0x%02x, len=%d\r\n", dev_addr, ep_addr, buflen);
    return hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info->data_toggle);
  } else {
    LOG_CH32_USBFSH("hcd_edpt_xfer(): WRITE, dev_addr=0x%02x, ep_addr=0x%02x, len=%d\r\n", dev_addr, ep_addr, buflen);
    uint16_t copylen = TU_MIN(edpt_info->max_packet_size, buflen);
    USBOTG_H_FS->HOST_TX_LEN = copylen;
    memcpy(USBFS_TX_Buf, buffer, copylen);
    return hardware_start_xfer(USB_PID_OUT, ep_addr, edpt_info->data_toggle);
  }
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  LOG_CH32_USBFSH("hcd_edpt_abort_xfer(dev=%d,ep=0x%02x)\r\n", dev_addr, ep_addr);
  usb_edpt_t* edpt = get_edpt_record(dev_addr, ep_addr);
  if (edpt) {
    edpt->is_nak_pending = false;
    return true;
  } else {
    return false;
  }
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  (void) rhport;

  while (usb_current_xfer_info.is_busy) {}

  usb_current_xfer_info.is_busy = true;

  LOG_CH32_USBFSH("hcd_setup_send(rhport=%d, dev_addr=0x%02x, %p)\r\n", rhport, dev_addr, setup_packet);

  hardware_set_port_address_speed(dev_addr);

  usb_edpt_t *edpt_info_tx = get_edpt_record(dev_addr, 0x00);
  usb_edpt_t *edpt_info_rx = get_edpt_record(dev_addr, 0x80);
  TU_ASSERT(edpt_info_tx != NULL, false);
  TU_ASSERT(edpt_info_rx != NULL, false);

  // Initialize data toggle (SETUP always starts with DATA0)
  // Data toggle for OUT is toggled in hcd_int_handler()
  edpt_info_tx->data_toggle = 0;
  // Data toggle for IN must be set 0x01 manually.
  edpt_info_rx->data_toggle = 0x01;
  const uint16_t setup_packet_datalen = 8;
  memcpy(USBFS_TX_Buf, setup_packet, setup_packet_datalen);
  USBOTG_H_FS->HOST_TX_LEN = setup_packet_datalen;
  usb_edpt_t* edpt_info = edpt_info_tx;
  uint8_t ep_addr = 0x00;
  usb_current_xfer_info.dev_addr = dev_addr;
  usb_current_xfer_info.ep_addr = ep_addr;
  usb_current_xfer_info.edpt_info = edpt_info;
  edpt_info->xfer_start_ms = tusb_time_millis_api();
  edpt_info->buf = (uint8_t*)(uintptr_t)setup_packet;
  edpt_info->buflen = setup_packet_datalen;
  edpt_info->xferred_len = 0;
  edpt_info->is_nak_pending = false;

  hardware_start_xfer(USB_PID_SETUP, 0, 0);

  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;

  LOG_CH32_USBFSH("hcd_edpt_clear_stall(rhport=%d, dev_addr=0x%02x, ep_addr=0x%02x)\r\n", rhport, dev_addr, ep_addr);
  if (tu_edpt_number(ep_addr) == 0x00) {
    // On EP 0, CLEAR_FEATURE should not be executed.
    return true;
  }
  // uint8_t edpt_num = tu_edpt_number(ep_addr);
  uint8_t edpt_num = ep_addr;
  uint8_t setup_request_clear_stall[8] = {
      0x02, 0x01, 0x00, 0x00, edpt_num, 0x00, 0x00, 0x00
  };
  memcpy(USBFS_TX_Buf, setup_request_clear_stall, 8);
  USBOTG_H_FS->HOST_TX_LEN = 8;

  bool prev_int_state = interrupt_enabled;
  hcd_int_disable(0);

  USBOTG_H_FS->HOST_EP_PID = (USB_PID_SETUP << 4) | 0x00;
  USBOTG_H_FS->INT_FG |= USBFS_UIF_TRANSFER;
  while ((USBOTG_H_FS->INT_FG & USBFS_UIF_TRANSFER) == 0) {}
  USBOTG_H_FS->HOST_EP_PID = 0;
  uint8_t response_pid = USBOTG_H_FS->INT_ST & USBFS_UIS_H_RES_MASK;
  (void) response_pid;
  LOG_CH32_USBFSH("hcd_edpt_clear_stall() response pid=0x%02x\r\n", response_pid);

  if (prev_int_state) {
    hcd_int_enable(0);
  }

  return true;
}

#endif
