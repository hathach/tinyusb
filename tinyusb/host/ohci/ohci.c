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

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
ohci_data_t ohci_data TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+
tusb_error_t hcd_init(void)
{
  //------------- Data Structure init -------------//
  memclr_(&ohci_data, sizeof(ohci_data_t));



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

}
//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+


#endif

