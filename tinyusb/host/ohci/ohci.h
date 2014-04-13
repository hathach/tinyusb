/**************************************************************************/
/*!
    @file     ohci.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup Group_HCD
 * @{
 *  \defgroup OHCI
 *  \brief OHCI driver. All documents sources mentioned here (eg section 3.5) is referring to OHCI Specs unless state otherwise
 *  @{ */

#ifndef _TUSB_OHCI_H_
#define _TUSB_OHCI_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/common.h"

//--------------------------------------------------------------------+
// OHCI CONFIGURATION & CONSTANTS
//--------------------------------------------------------------------+
#define HOST_HCD_XFER_INTERRUPT // TODO interrupt is used widely, should always be enalbed
#define OHCI_PERIODIC_LIST (defined HOST_HCD_XFER_INTERRUPT || defined HOST_HCD_XFER_ISOCHRONOUS)

// TODO merge OHCI with EHCI
enum {
  OHCI_MAX_ITD = 4
};

enum {
  OHCI_PID_SETUP = 0,
  OHCI_PID_OUT,
  OHCI_PID_IN,
};

//--------------------------------------------------------------------+
// OHCI Data Structure
//--------------------------------------------------------------------+
typedef struct {
  uint32_t interrupt_table[32];
  volatile uint16_t frame_number;
  volatile uint16_t frame_pad;
  volatile uint32_t done_head;
  uint8_t reserved[116+4];  // TODO try to make use of this area if possible, extra 4 byte to make the whole struct size = 256
}ohci_hcca_t; // ATTR_ALIGNED(256)

STATIC_ASSERT( sizeof(ohci_hcca_t) == 256, "size is not correct" );

typedef struct {
  uint32_t reserved[2];
  volatile uint32_t next_td;
  uint32_t reserved2;
}ohci_td_item_t;


typedef struct ATTR_ALIGNED(16) {
	//------------- Word 0 -------------//
	uint32_t used                    : 1;
	uint32_t index                   : 4;  // endpoint index the td belongs to, or device address in case of control xfer
  uint32_t expected_bytes          : 13; // TODO available for hcd

  uint32_t buffer_rounding         : 1;
  uint32_t pid                     : 2;
  uint32_t delay_interrupt         : 3;
  volatile uint32_t data_toggle    : 2;
  volatile uint32_t error_count    : 2;
  volatile uint32_t condition_code : 4;
	/*---------- End Word 1 ----------*/

	//------------- Word 1 -------------//
	volatile uint8_t* current_buffer_pointer;

	//------------- Word 2 -------------//
	volatile uint32_t next_td;

	//------------- Word 3 -------------//
	uint8_t* buffer_end;
} ohci_gtd_t;

STATIC_ASSERT( sizeof(ohci_gtd_t) == 16, "size is not correct" );

typedef struct ATTR_ALIGNED(16) {
  //------------- Word 0 -------------//
	uint32_t device_address   : 7;
	uint32_t endpoint_number  : 4;
	uint32_t direction        : 2;
	uint32_t speed            : 1;
	uint32_t skip             : 1;
	uint32_t is_iso           : 1;
	uint32_t max_package_size : 11;

	uint32_t used              : 1; // HCD
	uint32_t is_interrupt_xfer : 1;
	uint32_t is_stalled        : 1;
	uint32_t                   : 2;


	//------------- Word 1 -------------//
	union {
	  uint32_t address; // 4 lsb bits are free to use
	  struct {
	    uint32_t class_code : 4; // FIXME refractor to use interface number instead
	    uint32_t : 28;
	  };
	}td_tail;

	//------------- Word 2 -------------//
	volatile union {
		uint32_t address;
		struct {
			uint32_t halted : 1;
			uint32_t toggle : 1;
			uint32_t : 30;
		};
	}td_head;

	//------------- Word 3 -------------//
	uint32_t next_ed; // 4 lsb bits are free to use
} ohci_ed_t;

STATIC_ASSERT( sizeof(ohci_ed_t) == 16, "size is not correct" );

typedef struct ATTR_ALIGNED(32) {
	/*---------- Word 1 ----------*/
  uint32_t starting_frame          : 16;
  uint32_t                         : 5; // can be used
  uint32_t delay_interrupt         : 3;
  uint32_t frame_count             : 3;
  uint32_t                         : 1; // can be used
  volatile uint32_t condition_code : 4;
	/*---------- End Word 1 ----------*/

	/*---------- Word 2 ----------*/
	uint32_t buffer_page0;	// 12 lsb bits can be used

	/*---------- Word 3 ----------*/
	volatile uint32_t next_td;

	/*---------- Word 4 ----------*/
	uint32_t buffer_end;

	/*---------- Word 5-8 ----------*/
	volatile uint16_t offset_packetstatus[8];
} ochi_itd_t;

STATIC_ASSERT( sizeof(ochi_itd_t) == 32, "size is not correct" );

// structure with member alignment required from large to small
typedef struct ATTR_ALIGNED(256) {
  ohci_hcca_t hcca;

  ohci_ed_t bulk_head_ed; // static bulk head (dummy)
  ohci_ed_t period_head_ed; // static periodic list head (dummy)

  // control endpoints has reserved resources
  struct {
    ohci_ed_t ed;
    ohci_gtd_t gtd[3]; // setup, data, status
  }control[TUSB_CFG_HOST_DEVICE_MAX+1];

  struct {
    //  ochi_itd_t itd[OHCI_MAX_ITD]; // itd requires alignment of 32
    ohci_ed_t ed[HCD_MAX_ENDPOINT];
    ohci_gtd_t gtd[HCD_MAX_XFER];
  }device[TUSB_CFG_HOST_DEVICE_MAX];

} ohci_data_t;

//--------------------------------------------------------------------+
// OHCI Operational Register
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// OHCI Data Organization
//--------------------------------------------------------------------+
typedef volatile struct
{
  uint32_t revision;

  union {
    uint32_t control;
    struct {
      uint32_t control_bulk_service_ratio : 2;
      uint32_t periodic_list_enable       : 1;
      uint32_t isochronous_enable         : 1;
      uint32_t control_list_enable        : 1;
      uint32_t bulk_list_enable           : 1;
      uint32_t hc_functional_state        : 2;
      uint32_t interrupt_routing          : 1;
      uint32_t remote_wakeup_connected    : 1;
      uint32_t remote_wakeup_enale        : 1;
      uint32_t : 0;
    }control_bit;
  };

  union {
    uint32_t command_status;
    struct {
      uint32_t controller_reset         : 1;
      uint32_t control_list_filled      : 1;
      uint32_t bulk_list_filled         : 1;
      uint32_t ownership_change_request : 1;
      uint32_t                          : 12;
      uint32_t scheduling_overrun_count : 2;
    }command_status_bit;
  };

  uint32_t interrupt_status;
  uint32_t interrupt_enable;
  uint32_t interrupt_disable;

  uint32_t hcca;
  uint32_t period_current_ed;
  uint32_t control_head_ed;
  uint32_t control_current_ed;
  uint32_t bulk_head_ed;
  uint32_t bulk_current_ed;
  uint32_t done_head;

  uint32_t frame_interval;
  uint32_t frame_remaining;
  uint32_t frame_number;
  uint32_t periodic_start;
  uint32_t lowspeed_threshold;

  uint32_t rh_descriptorA;
  uint32_t rh_descriptorB;

  union {
    uint32_t rh_status;
    struct {
      uint32_t local_power_status            : 1; // read Local Power Status; write: Clear Global Power
      uint32_t over_current_indicator        : 1;
      uint32_t                               : 13;
      uint32_t device_remote_wakeup_enable   : 1;
      uint32_t local_power_status_change     : 1;
      uint32_t over_current_indicator_change : 1;
      uint32_t                               : 13;
      uint32_t clear_remote_wakeup_enable    : 1;
    }rh_status_bit;
  };

  union {
    uint32_t rhport_status[2]; // TODO NXP OHCI controller only has 2 ports
    struct {
      uint32_t current_connect_status             : 1;
      uint32_t port_enable_status                 : 1;
      uint32_t port_suspend_status                : 1;
      uint32_t port_over_current_indicator        : 1;
      uint32_t port_reset_status                  : 1;
      uint32_t                                    : 3;
      uint32_t port_power_status                  : 1;
      uint32_t low_speed_device_attached          : 1;
      uint32_t                                    : 6;
      uint32_t connect_status_change              : 1;
      uint32_t port_enable_status_change          : 1;
      uint32_t port_suspend_status_change         : 1;
      uint32_t port_over_current_indicator_change : 1;
      uint32_t port_reset_status_change           : 1;
      uint32_t                                    : 0;
    }rhport_status_bit[2];
  };
}ohci_registers_t;

STATIC_ASSERT( sizeof(ohci_registers_t) == 0x5c, "size is not correct");

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OHCI_H_ */

/** @} */
/** @} */
