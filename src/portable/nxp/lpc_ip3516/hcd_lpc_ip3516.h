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

#ifndef TUSB_HCD_IP3516_H_
#define TUSB_HCD_IP3516_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// IP3516 CONFIGURATION & CONSTANTS
//--------------------------------------------------------------------+

#define IP3516_ATL_NUM 32
#define IP3516_PTL_NUM 32

#define IP3516_PTD_TOKEN_OUT 0x00U
#define IP3516_PTD_TOKEN_IN 0x01U
#define IP3516_PTD_TOKEN_SETUP 0x02U

#define IP3516_PTD_EPTYPE_OUT 0x00U
#define IP3516_PTD_EPTYPE_IN 0x01U
#define IP3516_PTD_EPTYPE_SETUP 0x02U

#define IP3516_PTD_MAX_TRANSFER_LENGTH 0x7FFFU

#define IP3516_PTD_DATA_ADDR_MASK 0xFFFFU

#define IP3516_MAX_UFRAME (1UL << 8)

#define IP3516_PERIODIC_TRANSFER_GAP (3U)
#define IP3516_ISO_MULTIPLE_TRANSFER (8U)


//--------------------------------------------------------------------+
// OHCI Data Structure
//--------------------------------------------------------------------+

// Control Word 1
typedef union {
  uint32_t value;
  struct {
    uint32_t valid    : 1;
    uint32_t next_ptd : 5;
    uint32_t          : 1;
    uint32_t jump     : 1;
    uint32_t uframe   : 8;
    uint32_t mps      : 11;
    uint32_t          : 1;
    uint32_t mult     : 2;
    uint32_t          : 2;
  };
} ptd_ctrl1_t;

TU_VERIFY_STATIC(sizeof(ptd_ctrl1_t) == 4, "size is not correct");

// Control Word 2
typedef union {
  uint32_t value;
  struct {
    uint32_t ep_num   : 4;
    uint32_t dev_addr : 7;
    uint32_t split    : 1;
    uint32_t reload   : 4;
    uint32_t speed    : 2;
    uint32_t hub_port : 7;
    uint32_t hub_addr : 7;
  };
} ptd_ctrl2_t;

TU_VERIFY_STATIC(sizeof(ptd_ctrl2_t) == 4, "size is not correct");

// Data Word
typedef union {
  uint32_t value;
  struct {
    uint32_t xfer_len  : 15;
    uint32_t intr      : 1;
    uint32_t data_addr : 16;
  };
} ptd_data_t;

TU_VERIFY_STATIC(sizeof(ptd_data_t) == 4, "size is not correct");

// State Word
typedef union {
  uint32_t value;
  struct {
    uint32_t xferred_len    : 15;
    uint32_t token          : 2;
    uint32_t ep_type        : 2;
    uint32_t nak_cnt        : 4;
    uint32_t err_cnt        : 2;
    uint32_t data_toggle    : 1;
    uint32_t ping           : 1;
    uint32_t start_complete : 1;
    uint32_t error          : 1;
    uint32_t babble         : 1;
    uint32_t halt           : 1;
    uint32_t active         : 1;
  };
} ptd_state_t;

TU_VERIFY_STATIC(sizeof(ptd_state_t) == 4, "size is not correct");

// Status Word
typedef union {
  uint32_t value;
  struct {
    uint32_t uframe_active : 8;
    uint32_t iso_status0   : 3;
    uint32_t iso_status1   : 3;
    uint32_t iso_status2   : 3;
    uint32_t iso_status3   : 3;
    uint32_t iso_status4   : 3;
    uint32_t iso_status5   : 3;
    uint32_t iso_status6   : 3;
    uint32_t iso_status7   : 3;
  };
} ptd_status_t;

TU_VERIFY_STATIC(sizeof(ptd_status_t) == 4, "size is not correct");

// ATL (Asynchronous Transfer List) structure
typedef volatile struct {
  ptd_ctrl1_t ctrl1;
  ptd_ctrl2_t ctrl2;
  ptd_data_t  data;
  ptd_state_t state;
} ip3516_atl_t;

TU_VERIFY_STATIC(sizeof(ip3516_atl_t) == 16, "size is not correct");

// PTL (Periodic Transfer List) structure
typedef volatile struct {
  ptd_ctrl1_t  ctrl1;
  ptd_ctrl2_t  ctrl2;
  ptd_data_t   data;
  ptd_state_t  state;
  ptd_status_t status;
  union {
    uint32_t value;
    struct {
      uint32_t uframe_complete : 8;
      uint32_t spl_iso_in_0    : 24;
    };
  } iso_in_0;
  uint32_t iso_in_1;
  uint32_t iso_in_2;
} ip3516_ptl_t;

TU_VERIFY_STATIC(sizeof(ip3516_ptl_t) == 32, "size is not correct");

// Proprietary Transfer Descriptor
typedef struct {
  ip3516_ptl_t intr[IP3516_PTL_NUM];
  ip3516_ptl_t iso[IP3516_PTL_NUM];
  ip3516_atl_t atl[IP3516_ATL_NUM];
} ip3516_ptd_t;

#ifdef __cplusplus
}
#endif
#endif /* TUSB_HCD_IP3516_H_ */
