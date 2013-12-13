/**************************************************************************/
/*!
    @file     ohci.c
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

#include "common/common.h"

#if MODE_HOST_SUPPORTED && (TUSB_CFG_MCU == MCU_LPC175X_6X)
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "hal/hal.h"
#include "osal/osal.h"
#include "common/timeout_timer.h"

#include "../hcd.h"
#include "../usbh_hcd.h"
#include "ohci.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define OHCI_REG               ((ohci_registers_t *) LPC_USB_BASE)

enum {
  OHCI_CONTROL_FUNCSTATE_RESET = 0,
  OHCI_CONTROL_FUNCSTATE_RESUME,
  OHCI_CONTROL_FUNCSTATE_OPERATIONAL,
  OHCI_CONTROL_FUNCSTATE_SUSPEND
};

enum {
  OHCI_CONTROL_CONTROL_BULK_RATIO           = 3,
  OHCI_CONTROL_LIST_PERIODIC_ENABLE_MASK    = BIT_(2),
  OHCI_CONTROL_LIST_ISOCHRONOUS_ENABLE_MASK = BIT_(3),
  OHCI_CONTROL_LIST_CONTROL_ENABLE_MASK     = BIT_(4),
  OHCI_CONTROL_LIST_BULK_ENABLE_MASK        = BIT_(5),
};

enum {
  OHCI_FMINTERVAL_FI    = 0x2EDF, // 7.3.1 nominal (reset) value
  OHCI_FMINTERVAL_FSMPS = (6*(OHCI_FMINTERVAL_FI-210)) / 7, // 5.4 calculated based on maximum overhead + bit stuffing
};

enum {
  OHCI_PERIODIC_START = 0x3E67
};

enum {
  OHCI_INT_SCHEDULING_OVERUN_MASK    = BIT_(0),
  OHCI_INT_WRITEBACK_DONEHEAD_MASK   = BIT_(1),
  OHCI_INT_SOF_MASK                  = BIT_(2),
  OHCI_INT_RESUME_DETECTED_MASK      = BIT_(3),
  OHCI_INT_UNRECOVERABLE_ERROR_MASK  = BIT_(4),
  OHCI_INT_FRAME_OVERFLOW_MASK       = BIT_(5),
  OHCI_INT_RHPORT_STATUS_CHANGE_MASK = BIT_(6),

  OHCI_INT_OWNERSHIP_CHANGE_MASK     = BIT_(30),
  OHCI_INT_MASTER_ENABLE_MASK        = BIT_(31),

  OHCI_INT_ALL_MASK = OHCI_INT_SCHEDULING_OVERUN_MASK | OHCI_INT_WRITEBACK_DONEHEAD_MASK | OHCI_INT_SOF_MASK |
    OHCI_INT_RESUME_DETECTED_MASK | OHCI_INT_UNRECOVERABLE_ERROR_MASK | OHCI_INT_FRAME_OVERFLOW_MASK |
    OHCI_INT_RHPORT_STATUS_CHANGE_MASK | OHCI_INT_OWNERSHIP_CHANGE_MASK | OHCI_INT_MASTER_ENABLE_MASK
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
ohci_data_t ohci_data TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+
// Initialization according to 5.1.1.4
tusb_error_t hcd_init(void)
{
  //------------- Data Structure init -------------//
  memclr_(&ohci_data, sizeof(ohci_data_t));
  ohci_data.control[0].ed.skip = 1;

  // reset controller
  OHCI_REG->command_status_bit.controller_reset = 1;
  while( OHCI_REG->command_status_bit.controller_reset ) {} // should not take longer than 10 us

  // TODO peridoic list build
  // TODO assign control, bulk head

  //------------- init ohci registers -------------//
  OHCI_REG->control_bit.hc_functional_state = OHCI_CONTROL_FUNCSTATE_OPERATIONAL; // move HC to operational state TODO use this to suspend (save power)

  OHCI_REG->frame_interval = (OHCI_FMINTERVAL_FSMPS << 16) | OHCI_FMINTERVAL_FI;
  OHCI_REG->periodic_start = (OHCI_FMINTERVAL_FI * 9) / 10; // Periodic start is 90% of frame interval

  OHCI_REG->control_head_ed = (uint32_t) &ohci_data.control[0].ed;
  OHCI_REG->hcca            = (uint32_t) &ohci_data.hcca;

  OHCI_REG->control |= OHCI_CONTROL_CONTROL_BULK_RATIO | OHCI_CONTROL_LIST_CONTROL_ENABLE_MASK |
       0 /*OHCI_CONTROL_LIST_BULK_ENABLE_MASK*/; // TODO periodic enable

  OHCI_REG->rh_status_bit.local_power_status_change = 1; // set global power for ports

  OHCI_REG->interrupt_disable = OHCI_INT_ALL_MASK;
  OHCI_REG->interrupt_status  = OHCI_REG->interrupt_status; // clear current set bits
  OHCI_REG->interrupt_enable  = OHCI_INT_WRITEBACK_DONEHEAD_MASK | OHCI_INT_RESUME_DETECTED_MASK |
      OHCI_INT_UNRECOVERABLE_ERROR_MASK | OHCI_INT_FRAME_OVERFLOW_MASK | OHCI_INT_RHPORT_STATUS_CHANGE_MASK |
      OHCI_INT_MASTER_ENABLE_MASK;

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PORT API
//--------------------------------------------------------------------+
void hcd_port_reset(uint8_t hostid)
{
  // TODO OHCI
}

bool hcd_port_connect_status(uint8_t hostid)
{
  // TODO OHCI
}

tusb_speed_t hcd_port_speed_get(uint8_t hostid)
{
  // TODO OHCI
}

// TODO refractor abtract later
void hcd_port_unplug(uint8_t hostid)
{
  // TODO OHCI
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
tusb_error_t  hcd_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  // TODO OHCI return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_control_xfer(uint8_t dev_addr, tusb_control_request_t const * p_request, uint8_t data[])
{
  // TODO OHCI return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_control_close(uint8_t dev_addr)
{
  // TODO OHCI return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// BULK/INT/ISO PIPE API
//--------------------------------------------------------------------+
pipe_handle_t hcd_pipe_open(uint8_t dev_addr, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code)
{
  // TODO OHCI return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_queue_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes)
{
  // TODO OHCI return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete)
{
  // TODO OHCI return TUSB_ERROR_NONE;
}

/// pipe_close should only be called as a part of unmount/safe-remove process
tusb_error_t  hcd_pipe_close(pipe_handle_t pipe_hdl)
{
  // TODO OHCI return TUSB_ERROR_NONE;
}

bool hcd_pipe_is_busy(pipe_handle_t pipe_hdl)
{
  // TODO OHCI
}

bool hcd_pipe_is_error(pipe_handle_t pipe_hdl)
{
  // TODO OHCI
}

bool hcd_pipe_is_stalled(pipe_handle_t pipe_hdl)
{
  // TODO OHCI
}

uint8_t hcd_pipe_get_endpoint_addr(pipe_handle_t pipe_hdl)
{
  // TODO OHCI
}

tusb_error_t hcd_pipe_clear_stall(pipe_handle_t pipe_hdl)
{
  // TODO OHCI return TUSB_ERROR_NONE;
}


//--------------------------------------------------------------------+
// OHCI Interrupt Handler
//--------------------------------------------------------------------+
void hcd_isr(uint8_t hostid)
{
  uint32_t int_status = OHCI_REG->interrupt_status & OHCI_REG->interrupt_enable;
  OHCI_REG->interrupt_status = int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;

  if ( int_status & OHCI_INT_RHPORT_STATUS_CHANGE_MASK )
  {
    // TODO dual port is not yet supported
    if ( OHCI_REG->rhport_status_bit[0].connect_status_change )
    {
      if ( OHCI_REG->rhport_status_bit[0].current_connect_status )
      {
        usbh_hcd_rhport_plugged_isr(0);
      }else
      {
        usbh_hcd_rhport_unplugged_isr(0);
      }
    }
  }
}
//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+


#endif

