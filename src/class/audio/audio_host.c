/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 TinyUSB contributors
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

#if (CFG_TUH_ENABLED && CFG_TUH_AUDIO)

#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "audio_host.h"

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUH_AUDIO_LOG_LEVEL
  #define CFG_TUH_AUDIO_LOG_LEVEL   CFG_TUH_LOG_LEVEL
#endif

#define TU_LOG_DRV(...)   TU_LOG(CFG_TUH_AUDIO_LOG_LEVEL, __VA_ARGS__)

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tuh_audio_descriptor_cb(uint8_t idx, const tuh_audio_descriptor_cb_t *desc_cb_data) {
  (void) idx;
  (void) desc_cb_data;
}

TU_ATTR_WEAK void tuh_audio_mount_cb(uint8_t idx, const tuh_audio_mount_cb_t *mount_cb_data) {
  (void) idx;
  (void) mount_cb_data;
}

TU_ATTR_WEAK void tuh_audio_umount_cb(uint8_t idx) {
  (void) idx;
}

TU_ATTR_WEAK void tuh_audio_rx_cb(uint8_t idx, uint8_t ep_addr, uint16_t xferred_bytes) {
  (void) idx;
  (void) ep_addr;
  (void) xferred_bytes;
}

TU_ATTR_WEAK void tuh_audio_tx_cb(uint8_t idx, uint8_t ep_addr, uint16_t xferred_bytes) {
  (void) idx;
  (void) ep_addr;
  (void) xferred_bytes;
}

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Per-AS interface internal storage
typedef struct {
  uint8_t interface_num;
  uint8_t alt_setting;
  uint8_t ep_addr;
  uint16_t ep_size;
  uint8_t ep_dir;

  uint8_t format_type;
  uint8_t num_channels;
  uint8_t sub_frame_size;
  uint8_t bit_resolution;
  uint8_t sam_freq_type;
  uint32_t sam_freq[CFG_TUH_AUDIO_MAX_SAM_FREQ];
  uint32_t sam_freq_lower;
  uint32_t sam_freq_upper;
} audioh_as_t;

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber; // Audio Control interface number
  uint8_t iInterface;
  uint8_t itf_count;        // number of interfaces (AC + AS)

  // Audio Streaming Interface
  uint8_t as_interface_num;   // Audio Streaming interface number
  uint8_t alt_setting;      // current alt setting

  // Terminal info (from Audio Control Interface)
  uint16_t input_terminal_type;         // wTerminalType of Input Terminal
  uint8_t input_terminal_id;            // bTerminalID of Input Terminal
  uint8_t input_terminal_channels;      // bNrChannels of Input Terminal
  uint16_t output_terminal_type;        // wTerminalType of Output Terminal
  uint8_t output_terminal_id;           // bTerminalID of Output Terminal

  // Feature Unit info
  uint8_t feature_unit_id;              // bUnitID of Feature Unit (0 = none)
  uint8_t feature_unit_source_id;       // bSourceID of Feature Unit

  // Isochronous IN endpoint
  uint8_t ep_in;
  uint16_t ep_in_size;
  uint16_t ep_in_interval;

  // Isochronous OUT endpoint
  uint8_t ep_out;
  uint16_t ep_out_size;
  uint16_t ep_out_interval;

  // Multiple AS interfaces support
  uint8_t as_interfaces[CFG_TUH_AUDIO_MAX_AS];
  uint8_t as_alt_settings[CFG_TUH_AUDIO_MAX_AS];
  uint8_t as_count;
  uint8_t as_set_idx;

  // Per-AS interface independent storage (new)
  audioh_as_t as[CFG_TUH_AUDIO_MAX_AS];

  bool mounted;
} audioh_interface_t;

typedef struct {
  TUH_EPBUF_DEF(epin, CFG_TUH_AUDIO_EPIN_BUFSIZE);
  TUH_EPBUF_DEF(epout, CFG_TUH_AUDIO_EPOUT_BUFSIZE);
  TUH_EPBUF_DEF(ctrl, 8);  
} audioh_epbuf_t;

static audioh_interface_t _audioh_itf[CFG_TUH_AUDIO_MAX];
 static audioh_epbuf_t _audioh_epbuf[CFG_TUH_AUDIO_MAX];
//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline uint8_t find_new_audio_index(void) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    if (_audioh_itf[idx].daddr == 0) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

static inline uint8_t get_idx_by_ep_addr(uint8_t daddr, uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    const audioh_interface_t *p_audio = &_audioh_itf[idx];
    if ((p_audio->daddr == daddr) &&
        (ep_addr == p_audio->ep_in || ep_addr == p_audio->ep_out)) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

//--------------------------------------------------------------------+
// USBH API
//--------------------------------------------------------------------+
bool audioh_init(void) {
  tu_memclr(&_audioh_itf, sizeof(_audioh_itf));
  return true;
}

bool audioh_deinit(void) {
  return true;
}

void audioh_close(uint8_t daddr) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    audioh_interface_t *p_audio = &_audioh_itf[idx];
    if (p_audio->daddr == daddr) {
      TU_LOG_DRV("  AUDIO close addr = %u index = %u\r\n", daddr, idx);
      tuh_audio_umount_cb(idx);

      p_audio->bInterfaceNumber = 0;
      p_audio->as_interface_num = 0;
      p_audio->alt_setting = 0;
      p_audio->daddr = 0;
      p_audio->mounted = false;
      p_audio->ep_in = 0;
      p_audio->ep_out = 0;
      p_audio->as_count = 0;
      p_audio->as_set_idx = 0;
      tu_memclr(p_audio->as_interfaces, sizeof(p_audio->as_interfaces));
      tu_memclr(p_audio->as_alt_settings, sizeof(p_audio->as_alt_settings));
      tu_memclr(p_audio->as, sizeof(p_audio->as));
    }
  }
}

bool audioh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) result;
  const uint8_t idx = get_idx_by_ep_addr(dev_addr, ep_addr);
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX);
  audioh_interface_t *p_audio = &_audioh_itf[idx];

  if (ep_addr == p_audio->ep_in) {
    tuh_audio_rx_cb(idx, ep_addr, (uint16_t) xferred_bytes);
  } else if (ep_addr == p_audio->ep_out) {
    tuh_audio_tx_cb(idx, ep_addr, (uint16_t) xferred_bytes);
  }

  return true;
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+
uint16_t audioh_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_interface_t *desc_itf, uint16_t max_len) {
  (void) rhport;

  TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass, 0);
  TU_VERIFY(AUDIO_SUBCLASS_CONTROL == desc_itf->bInterfaceSubClass, 0);

  const uint8_t *desc_start = (const uint8_t *)desc_itf;
  const uint8_t *p_desc = desc_start;
  const uint8_t *desc_end = desc_start + max_len;

  const uint8_t idx = find_new_audio_index();
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  p_audio->itf_count = 0;

  tuh_audio_descriptor_cb_t desc_cb = { 0 };

  // Parse Audio Control Interface
  TU_LOG_DRV("AUDIO opening AC Interface %u (addr = %u)\r\n", desc_itf->bInterfaceNumber, dev_addr);
  p_audio->bInterfaceNumber = desc_itf->bInterfaceNumber;
  p_audio->iInterface = desc_itf->iInterface;
  p_audio->itf_count = 1;
  desc_cb.desc_ac_interface = desc_itf;
  desc_cb.ac_interface_num = desc_itf->bInterfaceNumber;

  // Parse Audio Control interface descriptors (Input Terminal, Output Terminal, Feature Unit, etc.)
  p_desc = tu_desc_next(p_desc);
  while (tu_desc_in_bounds(p_desc, desc_end) && tu_desc_type(p_desc) != TUSB_DESC_INTERFACE) {
    if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE) {
      switch (tu_desc_subtype(p_desc)) {
        case AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL: {
          const audio10_desc_input_terminal_t *it = (const audio10_desc_input_terminal_t *)p_desc;
          p_audio->input_terminal_type = tu_le16toh(it->wTerminalType);
          p_audio->input_terminal_id = it->bTerminalID;
          p_audio->input_terminal_channels = it->bNrChannels;
          TU_LOG_DRV("    Input Terminal: ID=%u, Type=0x%04x, Channels=%u\r\n",
                     it->bTerminalID, tu_le16toh(it->wTerminalType), it->bNrChannels);
          break;
        }
        case AUDIO10_CS_AC_INTERFACE_OUTPUT_TERMINAL: {
          const audio10_desc_output_terminal_t *ot = (const audio10_desc_output_terminal_t *)p_desc;
          p_audio->output_terminal_type = tu_le16toh(ot->wTerminalType);
          p_audio->output_terminal_id = ot->bTerminalID;
          TU_LOG_DRV("    Output Terminal: ID=%u, Type=0x%04x\r\n",
                     ot->bTerminalID, tu_le16toh(ot->wTerminalType));
          break;
        }
        case AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT: {
          const uint8_t *fu = p_desc;
          p_audio->feature_unit_id = fu[3];  // bUnitID
          p_audio->feature_unit_source_id = fu[4];  // bSourceID
          TU_LOG_DRV("    Feature Unit: ID=%u, SourceID=%u\r\n", fu[3], fu[4]);
          break;
        }
        default:
          break;
      }
    }
    p_desc = tu_desc_next(p_desc);
  }

  // Parse all remaining descriptors in this configuration looking for Audio Streaming interfaces
  while (tu_desc_in_bounds(p_desc, desc_end)) {
    if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) {
      const tusb_desc_interface_t *itf = (const tusb_desc_interface_t *)p_desc;
       // Stop at the first non-Audio interface so we don't claim the rest of the configuration
       if (itf->bInterfaceClass != TUSB_CLASS_AUDIO) break;
       if (itf->bInterfaceSubClass == AUDIO_SUBCLASS_STREAMING) {
        // Found Audio Streaming Interface
        TU_LOG_DRV("  Found AS Interface %u (alt = %u)\r\n", itf->bInterfaceNumber, itf->bAlternateSetting);

        if (itf->bAlternateSetting == 0) {
          // Interface descriptor with alt setting 0 (no endpoints)
          // Add to AS interfaces array
          if (p_audio->as_count < CFG_TUH_AUDIO_MAX_AS) {
            p_audio->as_interface_num = itf->bInterfaceNumber;
            p_audio->as_interfaces[p_audio->as_count] = itf->bInterfaceNumber;
            desc_cb.desc_as_interface = itf;
            desc_cb.as_interface_num = itf->bInterfaceNumber;
            // Create new AS entry for per-interface storage
            p_audio->as[p_audio->as_count].interface_num = itf->bInterfaceNumber;
            p_audio->as[p_audio->as_count].alt_setting = 0;
            p_audio->as_count++;
          }
        } else if (itf->bNumEndpoints > 0) {
          // Interface descriptor with alt setting > 0 (has endpoints)
          // Find matching AS interface and set alt_setting
          uint8_t as_entry_idx = CFG_TUH_AUDIO_MAX_AS;
          for (uint8_t as_idx = 0; as_idx < p_audio->as_count; as_idx++) {
            if (p_audio->as_interfaces[as_idx] == itf->bInterfaceNumber) {
              p_audio->alt_setting = itf->bAlternateSetting;
              p_audio->as_alt_settings[as_idx] = itf->bAlternateSetting;
              desc_cb.alt_setting = itf->bAlternateSetting;
              desc_cb.desc_as_interface_alt = itf;
              break;
            }
          }
          // Find or create AS entry for per-interface storage
          for (uint8_t i = 0; i < p_audio->as_count; i++) {
            if (p_audio->as[i].interface_num == itf->bInterfaceNumber) {
              as_entry_idx = i;
              break;
            }
          }
          if (as_entry_idx >= CFG_TUH_AUDIO_MAX_AS && p_audio->as_count < CFG_TUH_AUDIO_MAX_AS) {
            as_entry_idx = p_audio->as_count;
            p_audio->as[as_entry_idx].interface_num = itf->bInterfaceNumber;
            p_audio->as_count++;
          }
          if (as_entry_idx < CFG_TUH_AUDIO_MAX_AS) {
            p_audio->as[as_entry_idx].alt_setting = itf->bAlternateSetting;
          }

          // Parse the interface's descriptors
          p_desc = tu_desc_next(p_desc);
          // Temporary variables to hold format info until endpoint direction is known
          uint8_t tmp_format_type = 0;
          uint8_t tmp_num_channels = 0;
          uint8_t tmp_sub_frame_size = 0;
          uint8_t tmp_bit_resolution = 0;
          uint8_t tmp_sam_freq_type = 0;
          uint32_t tmp_sam_freq[CFG_TUH_AUDIO_MAX_SAM_FREQ] = {0};
          uint32_t tmp_sam_freq_lower = 0;
          uint32_t tmp_sam_freq_upper = 0;
          while (tu_desc_in_bounds(p_desc, desc_end) && tu_desc_type(p_desc) != TUSB_DESC_INTERFACE) {
            switch (tu_desc_type(p_desc)) {
              case TUSB_DESC_CS_INTERFACE: {
                switch (tu_desc_subtype(p_desc)) {
                  case AUDIO10_CS_AS_INTERFACE_AS_GENERAL: {
                    TU_LOG_DRV("    AS General descriptor\r\n");
                    desc_cb.desc_cs_as_general = p_desc;
                    break;
                  }
                  case AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE: {
                    TU_LOG_DRV("    Format Type descriptor\r\n");
                    desc_cb.desc_format_type = p_desc;
                    // Parse UAC 1.0 Format Type I descriptor fields into temporary variables
                    tmp_format_type    = p_desc[3];  // bFormatType
                    tmp_num_channels   = p_desc[4];  // bNrChannels
                    tmp_sub_frame_size = p_desc[5];  // bSubFrameSize
                    tmp_bit_resolution = p_desc[6];  // bBitResolution

                    // Parse sampling frequencies
                    uint8_t bLength = p_desc[0];
                    if (bLength >= 8) {
                      tmp_sam_freq_type = p_desc[7];  // bSamFreqType
                      if (tmp_sam_freq_type == 0) {
                        // Continuous range: tLowerSamFreq, tUpperSamFreq (3 bytes each)
                        if (bLength >= 14) {
                          tmp_sam_freq_lower = ((uint32_t)p_desc[8]  | ((uint32_t)p_desc[9] << 8)  | ((uint32_t)p_desc[10] << 16));
                          tmp_sam_freq_upper = ((uint32_t)p_desc[11] | ((uint32_t)p_desc[12] << 8) | ((uint32_t)p_desc[13] << 16));
                        }
                      } else {
                        // Discrete sampling frequencies
                        uint8_t max_freqs = tmp_sam_freq_type < CFG_TUH_AUDIO_MAX_SAM_FREQ ? tmp_sam_freq_type : CFG_TUH_AUDIO_MAX_SAM_FREQ;
                        for (uint8_t i = 0; i < max_freqs && (8 + i * 3 + 2) < bLength; i++) {
                          tmp_sam_freq[i] = ((uint32_t)p_desc[8 + i * 3]     |
                                             ((uint32_t)p_desc[9 + i * 3] << 8) |
                                             ((uint32_t)p_desc[10 + i * 3] << 16));
                        }
                      }
                    }
                    break;
                  }
                  default:
                    break;
                }
                break;
              }
              case TUSB_DESC_ENDPOINT: {
                const tusb_desc_endpoint_t *p_ep = (const tusb_desc_endpoint_t *)p_desc;
                if (p_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) {
                  TU_LOG_DRV("    Isochronous EP %02x\r\n", p_ep->bEndpointAddress);
                  if (tu_edpt_dir(p_ep->bEndpointAddress) == TUSB_DIR_IN) {
                    p_audio->ep_in = p_ep->bEndpointAddress;
                    p_audio->ep_in_size = tu_edpt_packet_size(p_ep);
                    p_audio->ep_in_interval = p_ep->bInterval;
                    desc_cb.desc_ep_in = p_ep;
                    // Save to per-AS storage
                    if (as_entry_idx < CFG_TUH_AUDIO_MAX_AS) {
                      audioh_as_t *as = &p_audio->as[as_entry_idx];
                      as->ep_addr = p_ep->bEndpointAddress;
                      as->ep_size = tu_edpt_packet_size(p_ep);
                      as->ep_dir = TUSB_DIR_IN;
                      as->format_type = tmp_format_type;
                      as->num_channels = tmp_num_channels;
                      as->sub_frame_size = tmp_sub_frame_size;
                      as->bit_resolution = tmp_bit_resolution;
                      as->sam_freq_type = tmp_sam_freq_type;
                      as->sam_freq_lower = tmp_sam_freq_lower;
                      as->sam_freq_upper = tmp_sam_freq_upper;
                      for (uint8_t i = 0; i < CFG_TUH_AUDIO_MAX_SAM_FREQ; i++) {
                        as->sam_freq[i] = tmp_sam_freq[i];
                      }
                    }
                  } else {
                    p_audio->ep_out = p_ep->bEndpointAddress;
                    p_audio->ep_out_size = tu_edpt_packet_size(p_ep);
                    p_audio->ep_out_interval = p_ep->bInterval;
                    desc_cb.desc_ep_out = p_ep;
                    // Save to per-AS storage
                    if (as_entry_idx < CFG_TUH_AUDIO_MAX_AS) {
                      audioh_as_t *as = &p_audio->as[as_entry_idx];
                      as->ep_addr = p_ep->bEndpointAddress;
                      as->ep_size = tu_edpt_packet_size(p_ep);
                      as->ep_dir = TUSB_DIR_OUT;
                      as->format_type = tmp_format_type;
                      as->num_channels = tmp_num_channels;
                      as->sub_frame_size = tmp_sub_frame_size;
                      as->bit_resolution = tmp_bit_resolution;
                      as->sam_freq_type = tmp_sam_freq_type;
                      as->sam_freq_lower = tmp_sam_freq_lower;
                      as->sam_freq_upper = tmp_sam_freq_upper;
                      for (uint8_t i = 0; i < CFG_TUH_AUDIO_MAX_SAM_FREQ; i++) {
                        as->sam_freq[i] = tmp_sam_freq[i];
                      }
                    }
                  }
                  TU_ASSERT(tuh_edpt_open(dev_addr, p_ep), 0);
                }
                break;
              }
              default:
                break;
            }
            p_desc = tu_desc_next(p_desc);
          }
          // Continue to parse other AS interfaces (don't break, device may have both IN and OUT)
          // break; // Removed: allow parsing multiple AS interfaces (e.g. mic + speaker)
          continue;
        }
        p_audio->itf_count++;
      } else if (itf->bInterfaceClass == TUSB_CLASS_AUDIO && itf->bInterfaceSubClass == AUDIO_SUBCLASS_CONTROL) {
        // Another Audio Control interface (shouldn't happen in normal UAC 1.0)
        p_audio->itf_count++;
      }
    }
    p_desc = tu_desc_next(p_desc);
  }

  p_audio->daddr = dev_addr;
  tuh_audio_descriptor_cb(idx, &desc_cb);

  return (uint16_t)((uintptr_t)p_desc - (uintptr_t)desc_start);
}

static void _audioh_mount(uint8_t dev_addr, uint8_t idx);

static void audioh_set_interface_complete(tuh_xfer_t* xfer) {
  uint8_t idx = (uint8_t) xfer->user_data;
  audioh_interface_t *p_audio = &_audioh_itf[idx];

  // Send SET_INTERFACE for next AS interface if any
  p_audio->as_set_idx++;
  if (p_audio->as_set_idx < p_audio->as_count) {
    uint8_t as_idx = p_audio->as_set_idx;
    uint8_t itf = p_audio->as_interfaces[as_idx];
    uint8_t alt = p_audio->as_alt_settings[as_idx];
    if (alt > 0) {
      TU_LOG_DRV("AUDIO Set Interface %u Alt %u (addr = %u)\r\n", itf, alt, xfer->daddr);
      tuh_interface_set(xfer->daddr, itf, alt, audioh_set_interface_complete, idx);
      return;
    }
  }

  // All SET_INTERFACE done, mount the device
  _audioh_mount(xfer->daddr, idx);
}

static void _audioh_mount(uint8_t dev_addr, uint8_t idx) {
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  p_audio->mounted = true;

  tuh_audio_mount_cb_t mount_cb_data = {
    .daddr = dev_addr,
    .bInterfaceNumber = p_audio->bInterfaceNumber,
    .bAltSetting = p_audio->alt_setting,
    .input_terminal_type = p_audio->input_terminal_type,
    .input_terminal_id = p_audio->input_terminal_id,
    .input_terminal_channels = p_audio->input_terminal_channels,
    .output_terminal_type = p_audio->output_terminal_type,
    .output_terminal_id = p_audio->output_terminal_id,
    .feature_unit_id = p_audio->feature_unit_id,
    .feature_unit_source_id = p_audio->feature_unit_source_id,
    .ep_in = p_audio->ep_in,
    .ep_out = p_audio->ep_out,
    .ep_in_size = p_audio->ep_in_size,
    .ep_out_size = p_audio->ep_out_size,
  };

  // Fill per-AS interface info
  mount_cb_data.as_count = p_audio->as_count;
  for (uint8_t i = 0; i < p_audio->as_count && i < CFG_TUH_AUDIO_MAX_AS; i++) {
    audioh_as_t *as = &p_audio->as[i];
    mount_cb_data.as_info[i].interface_num = as->interface_num;
    mount_cb_data.as_info[i].alt_setting = as->alt_setting;
    mount_cb_data.as_info[i].ep_addr = as->ep_addr;
    mount_cb_data.as_info[i].ep_size = as->ep_size;
    mount_cb_data.as_info[i].ep_dir = as->ep_dir;
    mount_cb_data.as_info[i].format_type = as->format_type;
    mount_cb_data.as_info[i].num_channels = as->num_channels;
    mount_cb_data.as_info[i].sub_frame_size = as->sub_frame_size;
    mount_cb_data.as_info[i].bit_resolution = as->bit_resolution;
    mount_cb_data.as_info[i].sam_freq_type = as->sam_freq_type;
    mount_cb_data.as_info[i].sam_freq_lower = as->sam_freq_lower;
    mount_cb_data.as_info[i].sam_freq_upper = as->sam_freq_upper;
    for (uint8_t j = 0; j < CFG_TUH_AUDIO_MAX_SAM_FREQ; j++) {
      mount_cb_data.as_info[i].sam_freq[j] = as->sam_freq[j];
    }
  }

  tuh_audio_mount_cb(idx, &mount_cb_data);

  usbh_driver_set_config_complete(dev_addr, p_audio->bInterfaceNumber);
}

bool audioh_set_config(uint8_t dev_addr, uint8_t itf_num) {
  uint8_t idx = tuh_audio_itf_get_index(dev_addr, itf_num);

  // If not found, check if this is an AS interface that belongs to a known AC interface
  if (idx >= CFG_TUH_AUDIO_MAX) {
    for (uint8_t i = 0; i < CFG_TUH_AUDIO_MAX; i++) {
      if (_audioh_itf[i].daddr == dev_addr && _audioh_itf[i].as_interface_num == itf_num) {
         // AS interface: configuration is driven by the AC interface, so just pass through
         usbh_driver_set_config_complete(dev_addr, itf_num);		
        return true;
      }
    }
     // Not an Audio interface we own; pass through so enumeration can continue
     usbh_driver_set_config_complete(dev_addr, itf_num);
     return true;
  }

  audioh_interface_t *p_audio = &_audioh_itf[idx];

  // Send SET_INTERFACE for all AS interfaces with alt_setting > 0
  if (p_audio->as_count > 0) {
    p_audio->as_set_idx = 0;
    uint8_t itf = p_audio->as_interfaces[0];
    uint8_t alt = p_audio->as_alt_settings[0];
    if (alt > 0) {
      TU_LOG_DRV("AUDIO Set Interface %u Alt %u (addr = %u)\r\n", itf, alt, dev_addr);
      tuh_interface_set(dev_addr, itf, alt, audioh_set_interface_complete, idx);
      return true;
    }
  }

  _audioh_mount(dev_addr, idx);
  return true;
}

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+
bool tuh_audio_mounted(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  return p_audio->mounted;
}

uint8_t tuh_audio_itf_get_index(uint8_t daddr, uint8_t itf_num) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    const audioh_interface_t *p_audio = &_audioh_itf[idx];
    if (p_audio->daddr == daddr && p_audio->bInterfaceNumber == itf_num) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

bool tuh_audio_itf_get_info(uint8_t idx, tuh_itf_info_t *info) {
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio && info);

  info->daddr = p_audio->daddr;

  // re-construct descriptor
  tusb_desc_interface_t *desc = &info->desc;
  desc->bLength = sizeof(tusb_desc_interface_t);
  desc->bDescriptorType = TUSB_DESC_INTERFACE;

  desc->bInterfaceNumber = p_audio->bInterfaceNumber;
  desc->bAlternateSetting = 0;
  desc->bNumEndpoints = (uint8_t)((p_audio->ep_in ? 1u : 0u) + (p_audio->ep_out ? 1u : 0u));
  desc->bInterfaceClass = TUSB_CLASS_AUDIO;
  desc->bInterfaceSubClass = AUDIO_SUBCLASS_CONTROL;
  desc->bInterfaceProtocol = 0;
  desc->iInterface = p_audio->iInterface;

  return true;
}

//--------------------------------------------------------------------+
// Control Endpoint API
//--------------------------------------------------------------------+
bool tuh_audio_set_sampling_freq(uint8_t daddr, uint8_t ep_addr, uint32_t sampling_freq,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  static uint8_t freq_buf[3] = {0};
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_ENDPOINT,
      .type = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = AUDIO10_CS_REQ_SET_CUR,
    .wValue = tu_u16(AUDIO10_EP_CTRL_SAMPLING_FREQ, 0),  // Control Selector = Sampling Freq, Channel = 0
    .wIndex = tu_u16_low(ep_addr),
    .wLength = 3
  };

  // UAC 1.0 sampling frequency is 3 bytes little-endian
  // uint8_t freq_buf[3] = {
  //   (uint8_t)(sampling_freq & 0xFF),
  //   (uint8_t)((sampling_freq >> 8) & 0xFF),
  //   (uint8_t)((sampling_freq >> 16) & 0xFF)
  // };
  freq_buf[0] = (uint8_t)(sampling_freq & 0xFF);
  freq_buf[1] = (uint8_t)((sampling_freq >> 8) & 0xFF);
  freq_buf[2] = (uint8_t)((sampling_freq >> 16) & 0xFF);
  tuh_xfer_t xfer = {
    .daddr = daddr,
    .ep_addr = 0,
    .setup = &request,
    .buffer = freq_buf,
    .complete_cb = complete_cb,
    .user_data = user_data
  };

  return tuh_control_xfer(&xfer);
}

bool tuh_audio_get_sampling_freq(uint8_t daddr, uint8_t ep_addr, uint32_t *sampling_freq,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(sampling_freq, false);
  *sampling_freq = 0;

  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_ENDPOINT,
      .type = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_IN
    },
    .bRequest = AUDIO10_CS_REQ_GET_CUR,
    .wValue = tu_u16(AUDIO10_EP_CTRL_SAMPLING_FREQ, 0),  // Control Selector = Sampling Freq, Channel = 0
    .wIndex = tu_u16_low(ep_addr),
    .wLength = 3
  };

  // Application needs to parse 3-byte little-endian sampling frequency from buffer
  tuh_xfer_t xfer = {
    .daddr = daddr,
    .ep_addr = 0,
    .setup = &request,
    .buffer = (uint8_t *)sampling_freq,
    .complete_cb = complete_cb,
    .user_data = user_data
  };

  return tuh_control_xfer(&xfer);
}

bool tuh_audio_feature_unit_set(uint8_t daddr, uint8_t itf_num, uint8_t unit_id,
                                 uint8_t control_selector, uint8_t channel,
                                 uint16_t value, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = AUDIO10_CS_REQ_SET_CUR,
    .wValue = tu_u16(control_selector, channel),
    .wIndex = tu_u16(itf_num, unit_id),
    .wLength = 2
  };

  uint8_t const idx = tuh_audio_itf_get_index(daddr, itf_num);
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);

  uint8_t* val_buf = _audioh_epbuf[idx].ctrl;
  val_buf[0] = (uint8_t)(value & 0xFF);
  val_buf[1] = (uint8_t)((value >> 8) & 0xFF);

  tuh_xfer_t xfer = {
    .daddr = daddr,
    .ep_addr = 0,
    .setup = &request,
    .buffer = val_buf,
    .complete_cb = complete_cb,
    .user_data = user_data
  };

  return tuh_control_xfer(&xfer);
}

bool tuh_audio_feature_unit_get(uint8_t daddr, uint8_t itf_num, uint8_t unit_id,
                                 uint8_t control_selector, uint8_t channel,
                                 void *buffer, uint8_t len,
                                 tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_IN
    },
    .bRequest = AUDIO10_CS_REQ_GET_CUR,
    .wValue = tu_u16(control_selector, channel),
    .wIndex = tu_u16(itf_num, unit_id),
    .wLength = len
  };

  tuh_xfer_t xfer = {
    .daddr = daddr,
    .ep_addr = 0,
    .setup = &request,
    .buffer = buffer,
    .complete_cb = complete_cb,
    .user_data = user_data
  };

  return tuh_control_xfer(&xfer);
}

//--------------------------------------------------------------------+
// Multi-AS interface API
//--------------------------------------------------------------------+
uint8_t tuh_audio_as_get_count(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, 0);
  return _audioh_itf[idx].as_count;
}

bool tuh_audio_as_get_info(uint8_t idx, uint8_t as_idx, tuh_audio_as_info_t *info) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);
  TU_VERIFY(as_idx < _audioh_itf[idx].as_count, false);
  TU_VERIFY(info, false);

  audioh_as_t *as = &_audioh_itf[idx].as[as_idx];
  info->interface_num = as->interface_num;
  info->alt_setting = as->alt_setting;
  info->ep_addr = as->ep_addr;
  info->ep_size = as->ep_size;
  info->ep_dir = as->ep_dir;
  info->format_type = as->format_type;
  info->num_channels = as->num_channels;
  info->sub_frame_size = as->sub_frame_size;
  info->bit_resolution = as->bit_resolution;
  info->sam_freq_type = as->sam_freq_type;
  info->sam_freq_lower = as->sam_freq_lower;
  info->sam_freq_upper = as->sam_freq_upper;
  memcpy(info->sam_freq, as->sam_freq, sizeof(info->sam_freq));
  return true;
}

//--------------------------------------------------------------------+
// Isochronous Endpoint API
//--------------------------------------------------------------------+
bool tuh_audio_receive(uint8_t daddr, uint8_t idx, uint8_t *buffer, uint16_t len) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->daddr == daddr);
  TU_VERIFY(p_audio->ep_in != 0);

  return usbh_edpt_xfer(daddr, p_audio->ep_in, buffer, len);
}

bool tuh_audio_send(uint8_t daddr, uint8_t idx, uint8_t *buffer, uint16_t len) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->daddr == daddr);
  TU_VERIFY(p_audio->ep_out != 0);

  return usbh_edpt_xfer(daddr, p_audio->ep_out, (uint8_t *)buffer, len);
}

//--------------------------------------------------------------------+
// Set Interface
//--------------------------------------------------------------------+
bool tuh_audio_set_interface(uint8_t daddr, uint8_t itf_num, uint8_t alt_setting,
                              tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = TUSB_REQ_SET_INTERFACE,
    .wValue = alt_setting,
    .wIndex = itf_num,
    .wLength = 0
  };

  tuh_xfer_t xfer = {
    .daddr = daddr,
    .ep_addr = 0,
    .setup = &request,
    .buffer = NULL,
    .complete_cb = complete_cb,
    .user_data = user_data
  };

  return tuh_control_xfer(&xfer);
}

#endif
