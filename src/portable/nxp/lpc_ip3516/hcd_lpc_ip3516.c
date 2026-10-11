/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 HiFiPhile (Zixun LI)
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
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
#if CFG_TUH_HUB
#include "host/hub.h"
#endif
#include "hcd_lpc_ip3516.h"

#if TU_CHECK_MCU(OPT_MCU_LPC55, OPT_MCU_LPC54)
  #include "fsl_device_registers.h"
#else
  #error "Unsupported MCUs"
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

#if TU_CHECK_MCU(OPT_MCU_LPC54)
  #define ATLPTD ATL_PTD_BASE_ADDR
  #define INTPTD INT_PTD_BASE_ADDR
  #define ISOPTD ISO_PTD_BASE_ADDR
  #define ATLPTDD ATL_PTD_DONE_MAP
  #define INTPTDD INT_PTD_DONE_MAP
  #define ISOPTDD ISO_PTD_DONE_MAP
  #define ATLPTDS ATL_PTD_SKIP_MAP
  #define INTPTDS INT_PTD_SKIP_MAP
  #define ISOPTDS ISO_PTD_SKIP_MAP
  #define DATAPAYLOAD DATA_PAYLOAD_BASE_ADDR
  #define LASTPTD LAST_PTD_INUSE

  #define USBHSH_ATLPTD_ATL_BASE_MASK USBHSH_ATL_PTD_BASE_ADDR_ATL_BASE_MASK
  #define USBHSH_INTPTD_INT_BASE_MASK USBHSH_INT_PTD_BASE_ADDR_INT_BASE_MASK
  #define USBHSH_ISOPTD_ISO_BASE_MASK USBHSH_ISO_PTD_BASE_ADDR_ISO_BASE_MASK

  #define USBHSH_DATAPAYLOAD_DAT_BASE_MASK USBHSH_DATA_PAYLOAD_BASE_ADDR_DAT_BASE_MASK

  #define USBHSH_LASTPTD_ATL_LAST USBHSH_LAST_PTD_INUSE_ATL_LAST
  #define USBHSH_LASTPTD_INT_LAST USBHSH_LAST_PTD_INUSE_INT_LAST
  #define USBHSH_LASTPTD_ISO_LAST USBHSH_LAST_PTD_INUSE_ISO_LAST
#endif

#define USBHSH_PORTSC1_W1C_MASK (USBHSH_PORTSC1_CSC_MASK | USBHSH_PORTSC1_PEDC_MASK | USBHSH_PORTSC1_OCC_MASK)

#define IP3516_PSPD_LOW   0
#define IP3516_PSPD_FULL  1
#define IP3516_PSPD_HIGH  2

//--------------------------------------------------------------------+
// Proprietary Transfer Descriptor
//--------------------------------------------------------------------+

CFG_TUH_MEM_SECTION TU_ATTR_ALIGNED(1024) static ip3516_ptd_t _ptd;

// ISO scheduling fields are consumed by hardware and must be restored for each transfer.
static struct {
  uint16_t interval;
  uint16_t max_packet_size;
  uint8_t uframe_active;
  uint8_t uframe_complete;
  uint8_t split_slot;
} _iso_ep[IP3516_PTL_NUM];

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

static inline bool is_xfer_async(tusb_xfer_type_t xfer_type) {
  return (xfer_type == TUSB_XFER_CONTROL || xfer_type == TUSB_XFER_BULK);
}

// Reserve coarse ISO payload windows on the downstream bus. Assume MTT hubs:
// each (hub address, downstream port) has an independent schedule.
// OUT grows from slot 0, IN from the end, leaving two extra complete attempts
// before the frame ends. Windows use maximum packet sizes, not current data.
// This is best effort: no full bus-time admission or interrupt reservations.
#if CFG_TUH_HUB
static bool iso_split_slot(uint8_t hub_addr, uint8_t hub_port, uint16_t packet_size, uint8_t dir, uint8_t *slot) {
  uint8_t used = 0;
  for (uint8_t i = 0; i < IP3516_PTL_NUM; i++) {
    const ip3516_ptl_t *ptd = &_ptd.iso[i];
    if (!is_ptd_free(ptd->ctrl1) && ptd->ctrl2.split &&
        ptd->ctrl2.hub_addr == hub_addr && ptd->ctrl2.hub_port == hub_port) {
      const uint8_t count = (uint8_t)((_iso_ep[i].max_packet_size + 187u) / 188u);
      used |= ((1u << count) - 1u) << _iso_ep[i].split_slot;
    }
  }

  return hub_iso_split_slot(used, packet_size, dir, slot);
}
#endif

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
    uint8_t      stride = is_xfer_async(xfer_type) ? sizeof(ip3516_atl_t) : sizeof(ip3516_ptl_t);
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
    // Wait 1 uframe for PTDs to be inactive (with timeout)
    uint32_t start_uframe =
      (USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT;
    uint32_t timeout = 10000;
    while (((USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT) ==
           start_uframe && timeout > 0) {
      timeout--;
    }

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

        // Wait 1 uframe for PTD to be inactive (with timeout)
        uint32_t start_uframe =
          (USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT;
        uint32_t timeout = 10000;
        while (((USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT) ==
               start_uframe && timeout > 0) {
          timeout--;
        }

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

  return 0;
}

static bool edpt_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen, bool is_setup) {
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);

  intptr_t ptd_ptr = find_opened_ptd(dev_addr, ep_addr);
  TU_ASSERT(ptd_ptr != 0);

  ptd_ctrl1_t *ptd_ctrl1 = (ptd_ctrl1_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl1));
  ptd_ctrl2_t *ptd_ctrl2 = (ptd_ctrl2_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl2));
  ptd_data_t  *ptd_data  = (ptd_data_t *)(ptd_ptr + offsetof(ip3516_atl_t, data));
  ptd_state_t *ptd_state = (ptd_state_t *)(ptd_ptr + offsetof(ip3516_atl_t, state));

  if (ptd_state->ep_type == TUSB_XFER_ISOCHRONOUS) {
    ip3516_ptl_t *ptd = (ip3516_ptl_t *)ptd_ptr;
    const uint8_t index = (uint8_t)(ptd - _ptd.iso);
    // Each submission covers at most one endpoint packet, including splits.
    TU_VERIFY(buflen <= _iso_ep[index].max_packet_size);
    const uint32_t interval = _iso_ep[index].interval;
    const uint32_t now = (USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >>
                         USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT;
    const uint32_t next = (now + interval) & ~(interval - 1u);

    // UM11126, Table 814: ISO uFrame[7:3] is a frame number; bits [2:0]
    // do not encode a polling interval. Select one service slot per transfer.
    ptd_ctrl1->uframe = next & 0xf8u;
    ptd->status.value = 0;
    ptd->status.uframe_active = ptd_ctrl2->split ? _iso_ep[index].uframe_active : (1u << (next & 7u));
    if (ptd_ctrl2->split && ep_dir == TUSB_DIR_OUT) {
      const uint8_t slots = (uint8_t)tu_max32(1, (buflen + 187u) / 188u);
      ptd->status.uframe_active = ((1u << slots) - 1u) << _iso_ep[index].split_slot;
    }
    ptd->iso_in_0.value = _iso_ep[index].uframe_complete;
    ptd->iso_in_1 = 0;
    ptd->iso_in_2 = 0;
  }

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

  // Interrupt split transfer needs to be relaunched manually if NAKed
  if (ptd_ctrl2->split && ptd_state->ep_type == TUSB_XFER_INTERRUPT) {
    ptd_ctrl2->reload  = 0x0f;
    ptd_state->nak_cnt = 0x0f;
  }

  // Activate only after the ISO scheduling fields have been restored.
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
  tu_memclr(_iso_ep, sizeof(_iso_ep));
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
  if (pspd == IP3516_PSPD_HIGH) {
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
    case IP3516_PSPD_LOW:
      return TUSB_SPEED_LOW;
    case IP3516_PSPD_FULL:
      return TUSB_SPEED_FULL;
    case IP3516_PSPD_HIGH:
      return TUSB_SPEED_HIGH;
    default:
      return TUSB_SPEED_INVALID;
  }
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  (void)rhport;
  uint32_t uframe = (USBHSH->FLADJ_FRINDEX & USBHSH_FLADJ_FRINDEX_FRINDEX_MASK) >> USBHSH_FLADJ_FRINDEX_FRINDEX_SHIFT;
  uframe &= (_hcd_data.uframe_length - 1);
  return (uframe + _hcd_data.uframe_number) >> 3;
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
  if (is_xfer_async(xfer_type)) {
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
  const uint8_t          ep_num    = tu_edpt_number(ep_desc->bEndpointAddress);
  const tusb_dir_t       ep_dir    = tu_edpt_dir(ep_desc->bEndpointAddress);
  const tusb_xfer_type_t xfer_type = (tusb_xfer_type_t)ep_desc->bmAttributes.xfer;

  tuh_bus_info_t bus_info;
  tuh_bus_info_get(dev_addr, &bus_info);

  const bool high_speed = bus_info.speed == TUSB_SPEED_HIGH;
  const bool split = hcd_port_speed_get(rhport) == TUSB_SPEED_HIGH && !high_speed;
  if (split) {
    // A full-speed hub may sit between this device and its high-speed TT.
    while (bus_info.hub_addr != 0) {
      tuh_bus_info_t hub;
      TU_VERIFY(tuh_bus_info_get(bus_info.hub_addr, &hub));
      if (hub.speed == TUSB_SPEED_HIGH) {
        break;
      }
      bus_info.hub_addr = hub.hub_addr;
      bus_info.hub_port = hub.hub_port;
    }
    TU_VERIFY(bus_info.hub_addr != 0);
  }

  const uint8_t ptd_index = ptd_find_free(xfer_type);
  TU_VERIFY(ptd_index != TUSB_INDEX_INVALID_8);

  uint16_t mps = ep_desc->wMaxPacketSize;
  uint8_t uframe = 0;
  uint8_t uframe_active = 0;
  uint8_t uframe_complete = 0;

  switch (xfer_type) {
    case TUSB_XFER_ISOCHRONOUS: {
      // ISO compares five frame bits, allowing at most 32 ms at HS. FS/split
      // must target a different frame, so its power-of-two limit is 16 ms.
      const uint8_t max_binterval = high_speed ? 9 : 5;
      TU_VERIFY(ep_desc->bInterval > 0 && ep_desc->bInterval <= max_binterval);
      const uint16_t interval = (uint16_t)(1u << (ep_desc->bInterval - 1u + (high_speed ? 0u : 3u)));
      uint8_t split_slot = 0;
      uframe_active = 1; // Non-split ISO gets its actual frame/slot at submission.
      #if CFG_TUH_HUB
      if (split) {
        // Reserve before publishing this endpoint. Existing windows stay put.
        TU_VERIFY(bus_info.speed == TUSB_SPEED_FULL);
        TU_VERIFY(mps > 0 && mps <= (ep_dir == TUSB_DIR_IN ? 564 : 1023));
        TU_VERIFY(iso_split_slot(bus_info.hub_addr, bus_info.hub_port, mps, ep_dir, &split_slot));
        uframe_active = 1u << split_slot;
        if (ep_dir == TUSB_DIR_IN) {
          const uint8_t slots = (uint8_t)((mps + 187u) / 188u);
          uframe_complete = ((1u << (slots + 2u)) - 1u) << (split_slot + 2u);
          mps = tu_min16(mps, 192);
        } else {
          mps = tu_min16(mps, 188);
        }
      }
      #endif
      _iso_ep[ptd_index].interval = interval;
      _iso_ep[ptd_index].max_packet_size = ep_desc->wMaxPacketSize;
      _iso_ep[ptd_index].split_slot = split_slot;
      _iso_ep[ptd_index].uframe_active = uframe_active;
      _iso_ep[ptd_index].uframe_complete = uframe_complete;
      break;
    }

    case TUSB_XFER_INTERRUPT: {
      TU_VERIFY(ep_desc->bInterval > 0);
      uint32_t interval;
      if (high_speed) {
        TU_VERIFY(ep_desc->bInterval <= 16);
        interval = 1u << (ep_desc->bInterval - 1u);
      } else {
        // Full-/low-speed interrupt intervals are linear frame counts.
        interval = 1u << tu_log2((uint32_t)ep_desc->bInterval << 3);
      }
      interval = tu_min32(interval, IP3516_MAX_UFRAME);
      switch (interval) {
        case 1: uframe_active = 0xff; break;
        case 2: uframe_active = 0xaa; break;
        case 4: uframe_active = 0x11; break;
        default:
          uframe_active = 0x01;
          uframe = (uint8_t)(tu_log2(interval) - 3);
          break;
      }
      if (split) {
        // Spread starts between slots 0/1, with three complete attempts.
        const uint8_t slot = ep_num & 1u;
        uframe_active = 1u << slot;
        uframe_complete = 0x1c << slot;
      }
      break;
    }

    default:
      break;
  }

  // All validation is complete; publish the endpoint configuration.
  const intptr_t ptd_ptr = get_ptd_from_index(xfer_type, ptd_index);
  volatile ptd_ctrl1_t *ctrl1 = (volatile ptd_ctrl1_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl1));
  volatile ptd_ctrl2_t *ctrl2 = (volatile ptd_ctrl2_t *)(ptd_ptr + offsetof(ip3516_atl_t, ctrl2));
  volatile ptd_data_t  *data  = (volatile ptd_data_t *)(ptd_ptr + offsetof(ip3516_atl_t, data));
  volatile ptd_state_t *state = (volatile ptd_state_t *)(ptd_ptr + offsetof(ip3516_atl_t, state));

  ctrl1->mps    = mps;
  ctrl1->mult   = 1;
  ctrl1->uframe = uframe;
  ctrl2->dev_addr = dev_addr;
  ctrl2->ep_num   = ep_num;
  ctrl2->speed    = bus_info.speed == TUSB_SPEED_LOW ? 2 : 0;
  ctrl2->hub_addr = bus_info.hub_addr;
  ctrl2->hub_port = bus_info.hub_port;
  ctrl2->split    = split;
  data->intr = 1;
  state->ep_type = (uint32_t)xfer_type;
  state->token   = ep_dir == TUSB_DIR_IN ? IP3516_PTD_TOKEN_IN : IP3516_PTD_TOKEN_OUT;
  if (!is_xfer_async(xfer_type)) {
    ip3516_ptl_t *ptd = (ip3516_ptl_t *)ptd_ptr;
    ptd->status.uframe_active = uframe_active;
    ptd->iso_in_0.uframe_complete = uframe_complete;
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
  TU_ASSERT(ptd_ptr != 0);

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
