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

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_IP3516)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/tusb_common.h"
#include "host/hcd.h"
#include "host/usbh.h"
#include "hcd_lpc_ip3516.h"

#if CFG_TUSB_MCU == OPT_MCU_LPC55
  #include "fsl_device_registers.h"
#else
  #error "Unsupported MCUs"
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

#define USBHSH_PORTSC1_W1C_MASK (USBHSH_PORTSC1_CSC_MASK | USBHSH_PORTSC1_PEDC_MASK | USBHSH_PORTSC1_OCC_MASK)

//--------------------------------------------------------------------+
// Proprietary Transfer Descriptor
//--------------------------------------------------------------------+

CFG_TUD_MEM_SECTION TU_ATTR_ALIGNED(1024) static ip3516_ptd_t _ptd;

static struct {
  uint32_t uframe_number;
  uint32_t uframe_length;
  bool attached; // Track attachment state to avoid duplicate events, sometimes high-speed disconnection detector is not reliable
} _hcd_data;

//--------------------------------------------------------------------+
// Helper Functions
//--------------------------------------------------------------------+

static inline bool is_ptd_free(const ptd_ctrl1_t ctrl1) {
  return ctrl1.mps == 0;
}

static inline bool is_xfer_asyc(tusb_xfer_type_t xfer_type) {
  return (xfer_type == TUSB_XFER_CONTROL || xfer_type == TUSB_XFER_BULK);
}

static inline void ptd_clear_state(ptd_state_t *state) {
  ptd_state_t local = {.value = 0};
  local.ep_type     = state->ep_type;     // preserve ep_type
  local.token       = state->token;       // preserve token
  local.data_toggle = state->data_toggle; // preserve data_toggle
  *state            = local;
}

static inline uint8_t ptd_find_free(tusb_xfer_type_t xfer_type) {
  uint8_t  max_count;
  intptr_t ptd_array;

  switch (xfer_type) {
    case TUSB_XFER_CONTROL:
    case TUSB_XFER_BULK:
      max_count = IP3516_ATL_NUM;
      ptd_array = (intptr_t)&_ptd.atl;
      break;

    case TUSB_XFER_INTERRUPT:
      max_count = IP3516_PTL_NUM;
      ptd_array = (intptr_t)&_ptd.intr;
      break;

    case TUSB_XFER_ISOCHRONOUS:
      max_count = IP3516_PTL_NUM;
      ptd_array = (intptr_t)&_ptd.iso;
      break;

    default:
      return TUSB_INDEX_INVALID_8;
  }

  for (uint8_t i = 0; i < max_count; i++) {
    // For ATL: stride is sizeof(ip3516_atl_t) = 16 bytes = 4 words
    // For PTL: stride is sizeof(ip3516_ptl_t) = 32 bytes = 8 words
    uint8_t      stride = is_xfer_asyc(xfer_type) ? sizeof(ip3516_atl_t) : sizeof(ip3516_ptl_t);
    ptd_ctrl1_t *ctrl1  = (ptd_ctrl1_t *)(ptd_array + i * stride);

    if (is_ptd_free(*ctrl1)) {
      return i;
    }
  }

  return TUSB_INDEX_INVALID_8; // No free PTD found
}

// Close all PTDs associated with a specific device address
static void close_ptds_by_device(uint8_t dev_addr, intptr_t ptd_array, uint8_t max_count, uint8_t stride,
                                 volatile uint32_t *skip_reg) {

  uint32_t skip_mask = 0;

  for (uint8_t i = 0; i < max_count; i++) {
    intptr_t     ptd_ptr   = ptd_array + i * stride;
    ptd_ctrl1_t *ptd_ctrl1 = (ptd_ctrl1_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl1));
    ptd_ctrl2_t *ptd_ctrl2 = (ptd_ctrl2_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl2));

    if (!is_ptd_free(*ptd_ctrl1) && ptd_ctrl2->dev_addr == dev_addr) {
      *skip_reg |= (1 << i);
      skip_mask |= (1 << i);
    }
  }

  if (skip_mask) {
    // Wait 1 uframe for PTDs to be inactive
    uint32_t start_uframe =
      (USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT;
    while (((USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT) ==
           start_uframe) {}

    // Clear PTDs
    for (uint8_t i = 0; i < max_count; i++) {
      if (skip_mask & (1 << i)) {
        intptr_t ptd_ptr = ptd_array + i * stride;
        tu_memclr((void *)ptd_ptr, stride);
      }
    }

    // Clear skip bits
    *skip_reg &= ~skip_mask;
  }
}

// Check if a PTD matches the given endpoint criteria
static bool ptd_matches(intptr_t ptd_ptr, uint8_t dev_addr, uint8_t ep_num, uint8_t ep_dir) {
  ptd_ctrl1_t *ptd_ctrl1 = (ptd_ctrl1_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl1));
  if (is_ptd_free(*ptd_ctrl1)) {
    return false;
  }

  ptd_ctrl2_t *ptd_ctrl2 = (ptd_ctrl2_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl2));
  if (ptd_ctrl2->dev_addr != dev_addr || ptd_ctrl2->ep_num != ep_num) {
    return false;
  }

  ptd_state_t *ptd_state  = (ptd_state_t *)(ptd_ptr + offsetof(ip3516_atl_t, state));
  bool         is_control = (ptd_state->ep_type == TUSB_XFER_CONTROL);

  // For control endpoint, match both IN and OUT directions
  if (is_control) {
    return true;
  }

  if (ep_dir == TUSB_DIR_IN && ptd_state->token == IP3516_PTD_TOKEN_IN) {
    return true;
  }

  if (ep_dir == TUSB_DIR_OUT && ptd_state->token == IP3516_PTD_TOKEN_OUT) {
    return true;
  }

  return false;
}

// Find and close a specific PTD
static bool find_and_close_ptd(uint8_t dev_addr, uint8_t ep_num, uint8_t ep_dir, intptr_t ptd_array, uint8_t max_count,
                               uint8_t stride, volatile uint32_t *skip_reg) {
  for (uint8_t i = 0; i < max_count; i++) {
    intptr_t ptd_ptr = ptd_array + i * stride;
    if (ptd_matches(ptd_ptr, dev_addr, ep_num, ep_dir)) {
      if (skip_reg) {
        *skip_reg |= (1 << i);

        // Wait 1 uframe for PTD to be inactive
        uint32_t start_uframe =
          (USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT;
        while (((USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT) ==
               start_uframe) {}

        // Just clear state
        ptd_ctrl1_t *ptd_ctrl1 = (ptd_ctrl1_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl1));
        ptd_state_t *ptd_state = (ptd_state_t *)(ptd_ptr + offsetof(ip3516_atl_t, state));
        ptd_clear_state(ptd_state);
        ptd_ctrl1->valid = 0;

        *skip_reg &= ~(1 << i);
      } else {
        // Clear PTD
        tu_memclr((void *)ptd_ptr, stride);
      }
      return true;
    }
  }
  return false;
}

// Find an opened PTD
static intptr_t find_opened_ptd(uint8_t dev_addr, uint8_t ep_addr) {
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);

  // Search in ATL
  for (uint8_t i = 0; i < IP3516_ATL_NUM; i++) {
    intptr_t ptd_ptr = (intptr_t)&_ptd.atl[i];
    if (ptd_matches(ptd_ptr, dev_addr, ep_num, ep_dir)) {
      return ptd_ptr;
    }
  }

  // Search in INT
  for (uint8_t i = 0; i < IP3516_PTL_NUM; i++) {
    intptr_t ptd_ptr = (intptr_t)&_ptd.intr[i];
    if (ptd_matches(ptd_ptr, dev_addr, ep_num, ep_dir)) {
      return ptd_ptr;
    }
  }

  // Search in ISO
  for (uint8_t i = 0; i < IP3516_PTL_NUM; i++) {
    intptr_t ptd_ptr = (intptr_t)&_ptd.iso[i];
    if (ptd_matches(ptd_ptr, dev_addr, ep_num, ep_dir)) {
      return ptd_ptr;
    }
  }

  return TUSB_INDEX_INVALID_8;
}

static bool edpt_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen, bool is_setup) {
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);

  intptr_t ptd_ptr = find_opened_ptd(dev_addr, ep_addr);
  TU_ASSERT(ptd_ptr != TUSB_INDEX_INVALID_8);

  ptd_ctrl1_t *ptd_ctrl1 = (ptd_ctrl1_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl1));
  ptd_ctrl2_t *ptd_ctrl2 = (ptd_ctrl2_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl2));
  ptd_data_t  *ptd_data  = (ptd_data_t *)(ptd_ptr + offsetof(ip3516_atl_t, data));
  ptd_state_t *ptd_state = (ptd_state_t *)(ptd_ptr + offsetof(ip3516_atl_t, state));

  // Setup data buffer and length
  ptd_data->data_addr = (uint32_t)(uintptr_t)buffer & IP3516_PTD_DATA_ADDR_MASK;
  ptd_data->xfer_len  = buflen;

  // Clear previous state
  ptd_clear_state(ptd_state);

  // Set token for EP0
  if (ep_num == 0) {
    if (is_setup) {
      ptd_state->token       = IP3516_PTD_TOKEN_SETUP;
      ptd_state->data_toggle = 0;
    } else {
      ptd_state->token       = (ep_dir == TUSB_DIR_IN) ? IP3516_PTD_TOKEN_IN : IP3516_PTD_TOKEN_OUT;
      ptd_state->data_toggle = 1;
    }
  }

  // Interrupt split transfer needs to be relauched manually if NAKed
  if (ptd_ctrl2->split && ptd_state->ep_type == TUSB_XFER_INTERRUPT) {
    ptd_ctrl2->reload  = 0x0f;
    ptd_state->nak_cnt = 0x0f;
  }

  // Activate PTD
  ptd_ctrl1->valid  = 1;
  ptd_state->active = 1;

  return true;
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// Initialize controller to host mode
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rh_init;
  (void)rhport;

  // Reset controller
  USBHSH->USBCMD |= USBHSH_USBCMD_HCRESET_MASK;
  while (USBHSH->USBCMD & USBHSH_USBCMD_HCRESET_MASK) {}

  USBHSH->PORTMODE = USBHSH_PORTMODE_SW_CTRL_PDCOM_MASK;

  tu_memclr(&_ptd, sizeof(_ptd));
  tu_varclr(&_hcd_data);

  // Set base addresses
  USBHSH->ATLPTD      = (uint32_t)&_ptd.atl & USBHSH_ATLPTD_ATL_BASE_MASK;
  USBHSH->INTPTD      = (uint32_t)&_ptd.intr & USBHSH_INTPTD_INT_BASE_MASK;
  USBHSH->ISOPTD      = (uint32_t)&_ptd.iso & USBHSH_ISOPTD_ISO_BASE_MASK;
  USBHSH->DATAPAYLOAD = (uint32_t)&_ptd & USBHSH_DATAPAYLOAD_DAT_BASE_MASK;

  // Turn on power switch
  if (USBHSH->HCSPARAMS & USBHSH_HCSPARAMS_PPC_MASK) {
    USBHSH->PORTSC1 |= USBHSH_PORTSC1_PP_MASK;
  }

  // Get frame list size
  uint32_t fls = (USBHSH->USBCMD & USBHSH_USBCMD_FLS_MASK) >> USBHSH_USBCMD_FLS_SHIFT;
  _hcd_data.uframe_length = 8192 >> fls;

  // Clear pending interrupts
  USBHSH->USBSTS = 0xFFFFFFFF;

  // Enable interrupts
  USBHSH->USBINTR = USBHSH_USBINTR_ATL_IRQ_E_MASK | USBHSH_USBINTR_INT_IRQ_E_MASK | USBHSH_USBINTR_ISO_IRQ_E_MASK |
                    USBHSH_USBINTR_PCDE_MASK | USBHSH_USBINTR_FLRE_MASK;


  // Enable all PTDs
  USBHSH->LASTPTD = USBHSH_LASTPTD_ATL_LAST(IP3516_ATL_NUM - 1) | USBHSH_LASTPTD_INT_LAST(IP3516_PTL_NUM - 1) |
                    USBHSH_LASTPTD_ISO_LAST(IP3516_PTL_NUM - 1);

  // Enable controller
  USBHSH->USBCMD = USBHSH_USBCMD_ATL_EN_MASK | USBHSH_USBCMD_INT_EN_MASK | USBHSH_USBCMD_ISO_EN_MASK | USBHSH_USBCMD_RS_MASK;

  return true;
}

// Enable USB interrupt
void hcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USB1_IRQn);
}

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USB1_IRQn);
}

bool hcd_deinit(uint8_t rhport) {
  (void)rhport;

  // Disable interrupts
  USBHSH->USBINTR = 0;
  USBHSH->USBSTS = 0xFFFFFFFF;

  // Disable controller
  USBHSH->USBCMD &= ~(USBHSH_USBCMD_ATL_EN_MASK | USBHSH_USBCMD_INT_EN_MASK | USBHSH_USBCMD_ISO_EN_MASK | USBHSH_USBCMD_RS_MASK);

  // Turn off power switch
  if (USBHSH->HCSPARAMS & USBHSH_HCSPARAMS_PPC_MASK) {
    USBHSH->PORTSC1 &= ~USBHSH_PORTSC1_PP_MASK;
  }

  // Connect PHY to device mode
  USBHSH->PORTMODE = USBHSH_PORTMODE_SW_CTRL_PDCOM_MASK | USBHSH_PORTMODE_DEV_ENABLE_MASK;

  return true;
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Reset USB bus on the port. Return immediately, bus reset sequence may not be complete.
// Some port would require hcd_port_reset_end() to be invoked after 10ms to complete the reset sequence.
void hcd_port_reset(uint8_t rhport) {
  (void)rhport;
  uint32_t status = USBHSH->PORTSC1 & ~USBHSH_PORTSC1_W1C_MASK;
  USBHSH->PORTSC1 = status | USBHSH_PORTSC1_PR_MASK;
}

// Complete bus reset sequence, may be required by some controllers
void hcd_port_reset_end(uint8_t rhport) {
  (void)rhport;
  uint32_t status = USBHSH->PORTSC1 & ~USBHSH_PORTSC1_W1C_MASK;
  USBHSH->PORTSC1 = status & ~USBHSH_PORTSC1_PR_MASK;
  while (USBHSH->PORTSC1 & USBHSH_PORTSC1_PR_MASK) {}
#if ((defined FSL_FEATURE_SOC_USBPHY_COUNT) && (FSL_FEATURE_SOC_USBPHY_COUNT > 0U))
  uint32_t pspd = (USBHSH->PORTSC1 & USBHSH_PORTSC1_PSPD_MASK) >> USBHSH_PORTSC1_PSPD_SHIFT;
  if (pspd == 2) {
    // enable phy disconnection for high speed
    USBPHY->CTRL |= USBPHY_CTRL_ENHOSTDISCONDETECT_MASK;
  }
#endif
}

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport) {
  (void)rhport;
  return (USBHSH->PORTSC1 & USBHSH_PORTSC1_CCS_MASK) ? true : false;
}

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void)rhport;
  uint32_t pspd = (USBHSH->PORTSC1 & USBHSH_PORTSC1_PSPD_MASK) >> USBHSH_PORTSC1_PSPD_SHIFT;
  switch (pspd) {
    case 0:
      return TUSB_SPEED_LOW;
    case 1:
      return TUSB_SPEED_FULL;
    case 2:
      return TUSB_SPEED_HIGH;
    default:
      return TUSB_SPEED_INVALID;
  }
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  (void)rhport;
  uint32_t uframe = (USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT;

  return ((uframe & (_hcd_data.uframe_length - 1) + _hcd_data.uframe_number)) >> 3;
}

// HCD closes all opened endpoints belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void)rhport;

  close_ptds_by_device(dev_addr, (intptr_t)&_ptd.atl, IP3516_ATL_NUM, sizeof(ip3516_atl_t), &USBHSH->ATLPTDS);
  close_ptds_by_device(dev_addr, (intptr_t)&_ptd.intr, IP3516_PTL_NUM, sizeof(ip3516_ptl_t), &USBHSH->INTPTDS);
  close_ptds_by_device(dev_addr, (intptr_t)&_ptd.iso, IP3516_PTL_NUM, sizeof(ip3516_ptl_t), &USBHSH->ISOPTDS);
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

static inline intptr_t get_ptd_from_index(tusb_xfer_type_t xfer_type, uint8_t ptd_index) {
  if (is_xfer_asyc(xfer_type)) {
    return (intptr_t)&_ptd.atl[ptd_index];
  } else {
    if (xfer_type == TUSB_XFER_INTERRUPT) {
      return (intptr_t)&_ptd.intr[ptd_index];
    } else {
      return (intptr_t)&_ptd.iso[ptd_index];
    }
  }
}


// Open an endpoint
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t *ep_desc) {
  (void)rhport;

  const uint8_t          ep_num    = tu_edpt_number(ep_desc->bEndpointAddress);
  const tusb_xfer_type_t xfer_type = (tusb_xfer_type_t)ep_desc->bmAttributes.xfer;

  tuh_bus_info_t bus_info;
  tuh_bus_info_get(dev_addr, &bus_info);

  // Find a free PTD
  uint8_t ptd_index = ptd_find_free(xfer_type);
  TU_ASSERT(ptd_index != TUSB_INDEX_INVALID_8);

  // Configure PTD
  intptr_t              ptd_ptr = get_ptd_from_index(xfer_type, ptd_index);
  volatile ptd_ctrl1_t *ctrl1   = (volatile ptd_ctrl1_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl1));
  volatile ptd_ctrl2_t *ctrl2   = (volatile ptd_ctrl2_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl2));
  volatile ptd_data_t  *data    = (volatile ptd_data_t *)(ptd_ptr + offsetof(ip3516_atl_t, data));
  volatile ptd_state_t *state   = (volatile ptd_state_t *)(ptd_ptr + offsetof(ip3516_atl_t, state));

  // Initialize PTD fields
  ctrl1->mps  = ep_desc->wMaxPacketSize;
  ctrl1->mult = 1;

  ctrl2->dev_addr = dev_addr;
  ctrl2->ep_num   = ep_num;
  ctrl2->speed    = bus_info.speed == TUSB_SPEED_LOW ? 2 : 0;
  ctrl2->hub_addr = bus_info.hub_addr;
  ctrl2->hub_port = bus_info.hub_port;
  ctrl2->split    = (hcd_port_speed_get(rhport) == TUSB_SPEED_HIGH) && (bus_info.speed != TUSB_SPEED_HIGH) ? 1 : 0;

  data->intr = 1;

  state->ep_type = (uint32_t)xfer_type;
  state->token   = tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN ? IP3516_PTD_TOKEN_IN : IP3516_PTD_TOKEN_OUT;

  if (!is_xfer_asyc(xfer_type)) {
    ip3516_ptl_t *ptd = (ip3516_ptl_t *)ptd_ptr;

    uint32_t uframe_interval;
    if (bus_info.speed == TUSB_SPEED_HIGH) {
      uframe_interval = 1 << (ep_desc->bInterval - 1);
    } else {
      uframe_interval = ep_desc->bInterval << 3;
      // round down to nearest power of 2
      uframe_interval = 1 << tu_log2(uframe_interval);
    }
    uframe_interval = tu_min32(uframe_interval, IP3516_MAX_UFRAME);

    // uframe_active is an 8-bit mask, where each bit corresponds to a micro-frame within a 1ms frame.
    // A '1' indicates the endpoint should be polled in that micro-frame.
    // For example:
    // Interval 1 (poll every u-frame) -> mask is 0b11111111 (0xFF)
    // Interval 2 (poll every 2nd u-frame, e.g., 0, 2, 4, 6) -> mask is 0b10101010 (0xAA)
    // Interval 4 (poll every 4th u-frame, e.g., 0, 4) -> mask is 0b10001000 (0x88)
    // Interval 8 (poll every 8th u-frame, e.g., 0) -> mask is 0b10000000 (0x80)
    switch (uframe_interval) {
      case 1:
        ptd->status.uframe_active = 0xFF;
        break;
      case 2:
        ptd->status.uframe_active = 0xAA;
        break;
      case 4:
        ptd->status.uframe_active = 0x11;
        break;
      case 8:
        ptd->status.uframe_active = 0x01;
        break;
      default:
        // For intervals > 8, we poll once per frame (every 8 u-frames) and use ctrl1.uframe to skip frames.
        ptd->status.uframe_active = 0x01;
        if (uframe_interval >= 16) {
          ctrl1->uframe = tu_log2(uframe_interval) - 3;
        }
        break;
    }

    if (ctrl2->split) {
      // 11.18.1 Best Case Full-Speed Budget
      //
      // A microframe of time allows at most 187.5 raw bytes of signaling on a full-speed bus.
      // The best case full-speed budget assumes that 188 full-speed bytes occur in each microframe.
      //
      // A 1 ms frame subdivided into microframes of budget time:
      //
      // Microframes            Y_0   Y_1   Y_2   Y_3   Y_4   Y_5   Y_6   Y_7
      // Max wire time          187.5 187.5 187.5 187.5 187.5 187.5 32
      // Best case wire budget  188   188   188   188   188   188   29
      //
      // 11.18.4  Host Split Transaction Scheduling Requirements
      //
      // 1. The host must never schedule a start-split in microframe Y_6.
      // 2. For isochronous OUT full-speed transactions, for each microframe in which the transaction is
      // budgeted, the host must schedule a 188 (or the remaining data size) data byte start-split transaction.
      // For isochronous IN and interrupt IN/OUT full-/low-speed transactions, a single start-split must be
      // scheduled in the microframe before the transaction is budgeted to start on the full-/low-speed bus.
      // 3. For isochronous OUT full-speed transactions, the host must never schedule a complete-split. The
      // TT response to a complete-split for an isochronous OUT is undefined.
      //    For interrupt IN/OUT full-/low-speed transactions, the host must schedule a complete-split
      // transaction in each of the two microframes following the first microframe in which the full-/low-
      // speed transaction is budgeted.  An additional complete-split must also be scheduled in the third
      // following microframe unless the full-/low-speed transaction was budgeted to start in microframe Y_6
      //    For isochronous IN full-speed transactions, for each microframe in which the full-speed transaction
      // is budgeted, a complete-split must be scheduled for each following microframe.
      // Also, determine the last microframe in which a complete-split is scheduled, call it L.
      // If L is less than Y_6, schedule additional complete-splits in microframe L+1 and L+2.
      // If L is equal to Y_6, schedule one complete-split in microframe Y_7.
      //
      // TODO: Implement budget check scheduling
      // Otherwise, it may cause bus contention with other split transfers
      // Here we simply start interrupt transfers for Y_0 and Y1 and isochronous transfers for Y_2
      if (xfer_type == TUSB_XFER_ISOCHRONOUS) {
        const uint8_t    ss_slot = 2; // Start-split slot
        const uint8_t    slots   = (ep_desc->wMaxPacketSize + 187) / 188;
        const tusb_dir_t ep_dir  = tu_edpt_dir(ep_desc->bEndpointAddress);
        if (ep_dir == TUSB_DIR_IN) {
          if (ep_desc->wMaxPacketSize > 192) {
            ctrl1->mps = 192;
          }
          ptd->status.uframe_active = 1 << ss_slot;
          for (uint8_t i = 0; i < slots; i++) {
            ptd->iso_in_0.uframe_complete |= 1 << (2 + ss_slot + i);
          }
          // Schedule additional complete-splits if needed
          uint8_t last_complete = ss_slot + slots + 1;
          if (last_complete < 6) {
            ptd->iso_in_0.uframe_complete |= 1 << (ss_slot + last_complete + 1);
            ptd->iso_in_0.uframe_complete |= 1 << (ss_slot + last_complete + 2);
          } else if (last_complete == 6) {
            ptd->iso_in_0.uframe_complete |= 1 << 7;
          }
        } else {
          if (ep_desc->wMaxPacketSize > 188) {
            ctrl1->mps = 188;
          }
          for (uint8_t i = 0; i < slots; i++) {
            ptd->status.uframe_active |= 1 << (ss_slot + i);
          }
        }
      } else {
        // Start-split slot, jigging to avoid bus contention: EP odd -> Y_1, EP even -> Y_0
        const uint8_t ss_slot     = ep_num & 0x01;
        ptd->status.uframe_active = 1 << ss_slot;
        // Complete-split slots: next 3 u-frames
        ptd->iso_in_0.uframe_complete = 0x1c << ss_slot;
      }
    }
  }

  return true;
}

// Close an opened endpoint
bool hcd_edpt_close(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);

  // Search in ATL
  if (find_and_close_ptd(dev_addr, ep_num, ep_dir, (intptr_t)&_ptd.atl, IP3516_ATL_NUM, sizeof(ip3516_atl_t), NULL)) {
    return true;
  }

  // Search in INT
  if (find_and_close_ptd(dev_addr, ep_num, ep_dir, (intptr_t)&_ptd.intr, IP3516_PTL_NUM, sizeof(ip3516_ptl_t), NULL)) {
    return true;
  }

  // Search in ISO
  if (find_and_close_ptd(dev_addr, ep_num, ep_dir, (intptr_t)&_ptd.iso, IP3516_PTL_NUM, sizeof(ip3516_ptl_t), NULL)) {
    return true;
  }

  return false;
}

// Submit a transfer on an endpoint
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void)rhport;

  return edpt_xfer(dev_addr, ep_addr, buffer, buflen, false);
}

// Abort a queued transfer. Note: it can only abort transfer that has not been started
// Return true if a queued transfer is aborted, false if there is no transfer to abort
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);

  // Search in ATL
  if (find_and_close_ptd(dev_addr, ep_num, ep_dir, (intptr_t)&_ptd.atl, IP3516_ATL_NUM, sizeof(ip3516_atl_t),
                         &USBHSH->ATLPTDS)) {
    return true;
  }

  // Search in INT
  if (find_and_close_ptd(dev_addr, ep_num, ep_dir, (intptr_t)&_ptd.intr, IP3516_PTL_NUM, sizeof(ip3516_ptl_t),
                         &USBHSH->INTPTDS)) {
    return true;
  }

  // Search in ISO
  if (find_and_close_ptd(dev_addr, ep_num, ep_dir, (intptr_t)&_ptd.iso, IP3516_PTL_NUM, sizeof(ip3516_ptl_t),
                         &USBHSH->ISOPTDS)) {
    return true;
  }

  return false;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
  (void)rhport;

  return edpt_xfer(dev_addr, 0x00, (uint8_t *)(uintptr_t)setup_packet, 8, true);
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;

  intptr_t ptd_ptr = find_opened_ptd(dev_addr, ep_addr);
  TU_ASSERT(ptd_ptr != TUSB_INDEX_INVALID_8);

  ptd_state_t *ptd_state = (ptd_state_t *)(ptd_ptr + offsetof(ip3516_atl_t, state));
  ptd_clear_state(ptd_state);
  ptd_state->data_toggle = 0; // reset data toggle to DATA0

  return true;
}

//--------------------------------------------------------------------+
// Interrupt Handler
//--------------------------------------------------------------------+

// Handle port status change event
static inline void handle_port_status_change(uint8_t rhport) {
  const uint32_t status = USBHSH->PORTSC1;

  if (status & USBHSH_PORTSC1_CSC_MASK) {
    if (status & USBHSH_PORTSC1_CCS_MASK && !_hcd_data.attached) {
      _hcd_data.attached = true;
      hcd_event_device_attach(rhport, true);
    } else {
      _hcd_data.attached = false;
      hcd_event_device_remove(rhport, true);
  #if ((defined FSL_FEATURE_SOC_USBPHY_COUNT) && (FSL_FEATURE_SOC_USBPHY_COUNT > 0U))
      // disable phy disconnection for high speed
      USBPHY->CTRL &= ~USBPHY_CTRL_ENHOSTDISCONDETECT_MASK;
  #endif
    }
  }

  USBHSH->PORTSC1 |= status & USBHSH_PORTSC1_W1C_MASK;
}

// Handle PTD done interrupt
static inline void handle_ptd_done(uint32_t done_status, intptr_t ptd_array, bool is_async) {
  uint8_t max_count = is_async ? IP3516_ATL_NUM : IP3516_PTL_NUM;
  uint8_t stride    = is_async ? sizeof(ip3516_atl_t) : sizeof(ip3516_ptl_t);

  for (uint8_t i = 0; i < max_count; i++) {
    if (done_status & (1 << i)) {
      intptr_t     ptd_ptr   = ptd_array + i * stride;
      ptd_ctrl2_t *ptd_ctrl2 = (ptd_ctrl2_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl2));
      ptd_state_t *ptd_state = (ptd_state_t *)(ptd_ptr + offsetof(ip3516_atl_t, state));

      xfer_result_t result;
      if (ptd_state->halt) {
        result = XFER_RESULT_STALLED;
      } else if (ptd_state->error || ptd_state->babble) {
        result = XFER_RESULT_FAILED;
      } else {
        result = XFER_RESULT_SUCCESS;
      }

      uint8_t ep_addr = ptd_ctrl2->ep_num | (ptd_state->token == IP3516_PTD_TOKEN_IN ? 0x80 : 0x00);

      hcd_event_xfer_complete(ptd_ctrl2->dev_addr, ep_addr, ptd_state->xferred_len, result, true);
    }
  }
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void)in_isr;

  uint32_t int_status = USBHSH->USBSTS;
  USBHSH->USBSTS      = int_status; // clear interrupt status

  // Port Change Detect
  if (int_status & USBHSH_USBSTS_PCD_MASK) {
    handle_port_status_change(rhport);
  }

  // Frame List Rollover
  if (int_status & USBHSH_USBSTS_FLR_MASK) {
    _hcd_data.uframe_number += _hcd_data.uframe_length;
  }

  // ATL done
  if (int_status & USBHSH_USBSTS_ATL_IRQ_MASK) {
    uint32_t done_status = USBHSH->ATLPTDD;
    handle_ptd_done(done_status, (intptr_t)&_ptd.atl, true);
    USBHSH->ATLPTDD = done_status;
  }

  // INT done
  if (int_status & USBHSH_USBSTS_INT_IRQ_MASK) {
    uint32_t done_status = USBHSH->INTPTDD;
    handle_ptd_done(done_status, (intptr_t)&_ptd.intr, false);
    USBHSH->INTPTDD = done_status;
  }

  // ISO done
  if (int_status & USBHSH_USBSTS_ISO_IRQ_MASK) {
    uint32_t done_status = USBHSH->ISOPTDD;
    handle_ptd_done(done_status, (intptr_t)&_ptd.iso, false);
    USBHSH->ISOPTDD = done_status;
  }
}

#endif
