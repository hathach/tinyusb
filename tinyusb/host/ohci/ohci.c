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
  OHCI_CONTROL_CONTROL_BULK_RATIO           = 3, ///< This specifies the service ratio between Control and Bulk EDs. 0 = 1:1, 3 = 4:1
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

#ifdef __CC_ARM
#pragma diag_suppress 66 // Suppress Keil warnings #66-D: enumeration value is out of "int" range
#endif

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
};

#ifdef __CC_ARM
#pragma diag_default 66 // return Keil 66 to normal severity
#endif

enum {
  OHCI_RHPORT_CURRENT_CONNECT_STATUS_MASK      = BIT_(0),
  OHCI_RHPORT_PORT_ENABLE_STATUS_MASK          = BIT_(1),
  OHCI_RHPORT_PORT_SUSPEND_STATUS_MASK         = BIT_(2),
  OHCI_RHPORT_PORT_OVER_CURRENT_INDICATOR_MASK = BIT_(3),
  OHCI_RHPORT_PORT_RESET_STATUS_MASK           = BIT_(4), ///< write '1' to reset port

  OHCI_RHPORT_PORT_POWER_STATUS_MASK           = BIT_(8),
  OHCI_RHPORT_LOW_SPEED_DEVICE_ATTACHED_MASK   = BIT_(9),

  OHCI_RHPORT_CONNECT_STATUS_CHANGE_MASK       = BIT_(16),
  OHCI_RHPORT_PORT_ENABLE_CHANGE_MASK          = BIT_(17),
  OHCI_RHPORT_PORT_SUSPEND_CHANGE_MASK         = BIT_(18),
  OHCI_RHPORT_OVER_CURRENT_CHANGE_MASK         = BIT_(19),
  OHCI_RHPORT_PORT_RESET_CHANGE_MASK           = BIT_(20),

  OHCI_RHPORT_ALL_CHANGE_MASK = OHCI_RHPORT_CONNECT_STATUS_CHANGE_MASK | OHCI_RHPORT_PORT_ENABLE_CHANGE_MASK |
    OHCI_RHPORT_PORT_SUSPEND_CHANGE_MASK | OHCI_RHPORT_OVER_CURRENT_CHANGE_MASK | OHCI_RHPORT_PORT_RESET_CHANGE_MASK
};

enum {
  OHCI_CCODE_NO_ERROR              = 0,
  OHCI_CCODE_CRC                   = 1,
	OHCI_CCODE_BIT_STUFFING          = 2,
	OHCI_CCODE_DATA_TOGGLE_MISMATCH  = 3,
	OHCI_CCODE_STALL                 = 4,
	OHCI_CCODE_DEVICE_NOT_RESPONDING = 5,
	OHCI_CCODE_PID_CHECK_FAILURE     = 6,
	OHCI_CCODE_UNEXPECTED_PID        = 7,
	OHCI_CCODE_DATA_OVERRUN          = 8,
	OHCI_CCODE_DATA_UNDERRUN         = 9,
	OHCI_CCODE_BUFFER_OVERRUN        = 12,
	OHCI_CCODE_BUFFER_UNDERRUN       = 13,
	OHCI_CCODE_NOT_ACCESSED          = 14,
};

enum {
  OHCI_INT_ON_COMPLETE_YES = 0,
  OHCI_INT_ON_COMPLETE_NO  = BIN8(111)
};
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(256) STATIC_VAR ohci_data_t ohci_data;

static ohci_ed_t * const p_ed_head[] =
{
    [TUSB_XFER_CONTROL]     = &ohci_data.control[0].ed,
    [TUSB_XFER_BULK   ]     = &ohci_data.bulk_head_ed,
    [TUSB_XFER_INTERRUPT]   = &ohci_data.period_head_ed,
    [TUSB_XFER_ISOCHRONOUS] = NULL // TODO Isochronous
};

static void ed_list_insert(ohci_ed_t * p_pre, ohci_ed_t * p_ed);
static void ed_list_remove(ohci_ed_t * p_head, ohci_ed_t * p_ed);

static ohci_ed_t * ed_list_find_previous(ohci_ed_t const * p_head, ohci_ed_t const * p_ed);

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+
// Initialization according to 5.1.1.4
tusb_error_t hcd_init(void)
{
  //------------- Data Structure init -------------//
  memclr_(&ohci_data, sizeof(ohci_data_t));
  for(uint8_t i=0; i<32; i++)
  { // assign all interrupt pointes to period head ed
    ohci_data.hcca.interrupt_table[i] = (uint32_t) &ohci_data.period_head_ed;
  }

  ohci_data.control[0].ed.skip  = 1;
  ohci_data.bulk_head_ed.skip   = 1;
  ohci_data.period_head_ed.skip = 1;

  // reset controller
  OHCI_REG->command_status_bit.controller_reset = 1;
  while( OHCI_REG->command_status_bit.controller_reset ) {} // should not take longer than 10 us

  //------------- init ohci registers -------------//
  OHCI_REG->control_head_ed = (uint32_t) &ohci_data.control[0].ed;
  OHCI_REG->bulk_head_ed    = (uint32_t) &ohci_data.bulk_head_ed;
  OHCI_REG->hcca            = (uint32_t) &ohci_data.hcca;

  OHCI_REG->interrupt_disable = OHCI_REG->interrupt_enable; // disable all interrupts
  OHCI_REG->interrupt_status  = OHCI_REG->interrupt_status; // clear current set bits
  OHCI_REG->interrupt_enable  = OHCI_INT_WRITEBACK_DONEHEAD_MASK | OHCI_INT_RESUME_DETECTED_MASK |
      OHCI_INT_UNRECOVERABLE_ERROR_MASK | /*OHCI_INT_FRAME_OVERFLOW_MASK |*/ OHCI_INT_RHPORT_STATUS_CHANGE_MASK |
      OHCI_INT_MASTER_ENABLE_MASK;

  OHCI_REG->control |= OHCI_CONTROL_CONTROL_BULK_RATIO | OHCI_CONTROL_LIST_CONTROL_ENABLE_MASK |
       OHCI_CONTROL_LIST_BULK_ENABLE_MASK | OHCI_CONTROL_LIST_PERIODIC_ENABLE_MASK; // TODO Isochronous

  OHCI_REG->frame_interval = (OHCI_FMINTERVAL_FSMPS << 16) | OHCI_FMINTERVAL_FI;
  OHCI_REG->periodic_start = (OHCI_FMINTERVAL_FI * 9) / 10; // Periodic start is 90% of frame interval

  OHCI_REG->control_bit.hc_functional_state = OHCI_CONTROL_FUNCSTATE_OPERATIONAL; // make HC's state to operational state TODO use this to suspend (save power)
  OHCI_REG->rh_status_bit.local_power_status_change = 1; // set global power for ports

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PORT API
//--------------------------------------------------------------------+
void hcd_port_reset(uint8_t hostid)
{
  OHCI_REG->rhport_status[0] = OHCI_RHPORT_PORT_RESET_STATUS_MASK;
}

bool hcd_port_connect_status(uint8_t hostid)
{
  return OHCI_REG->rhport_status_bit[0].current_connect_status;
}

tusb_speed_t hcd_port_speed_get(uint8_t hostid)
{
  return OHCI_REG->rhport_status_bit[0].low_speed_device_attached ? TUSB_SPEED_LOW : TUSB_SPEED_FULL;
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
static inline tusb_xfer_type_t ed_get_xfer_type(ohci_ed_t const * const p_ed) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline tusb_xfer_type_t ed_get_xfer_type(ohci_ed_t const * const p_ed)
{
  return (p_ed->endpoint_number == 0 ) ? TUSB_XFER_CONTROL     :
         (p_ed->is_iso               ) ? TUSB_XFER_ISOCHRONOUS :
         (p_ed->is_interrupt_xfer    ) ? TUSB_XFER_INTERRUPT   : TUSB_XFER_BULK;
}

static void ed_init(ohci_ed_t *p_ed, uint8_t dev_addr, uint16_t max_packet_size, uint8_t endpoint_addr, uint8_t xfer_type, uint8_t interval)
{
  // address 0 is used as async head, which always on the list --> cannot be cleared
  if (dev_addr != 0)
  {
    memclr_(p_ed, sizeof(ohci_ed_t));
  }

  p_ed->device_address    = dev_addr;
  p_ed->endpoint_number   = endpoint_addr & 0x0F;
  p_ed->direction         = (xfer_type == TUSB_XFER_CONTROL) ? OHCI_PID_SETUP : ( (endpoint_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? OHCI_PID_IN : OHCI_PID_OUT );
  p_ed->speed             = usbh_devices[dev_addr].speed;
  p_ed->is_iso            = (xfer_type == TUSB_XFER_ISOCHRONOUS) ? 1 : 0;
  p_ed->max_package_size  = max_packet_size;

  p_ed->used              = 1;
  p_ed->is_interrupt_xfer = (xfer_type == TUSB_XFER_INTERRUPT ? 1 : 0);
}

static void gtd_init(ohci_gtd_t* p_td, void* data_ptr, uint16_t total_bytes)
{
  memclr_(p_td, sizeof(ohci_gtd_t));

  p_td->used                   = 1;
  p_td->expected_bytes         = total_bytes;

  p_td->buffer_rounding        = 1; // less than queued length is not a error
  p_td->delay_interrupt        = OHCI_INT_ON_COMPLETE_NO;
  p_td->condition_code         = OHCI_CCODE_NOT_ACCESSED;

  p_td->current_buffer_pointer = data_ptr;
  p_td->buffer_end             = total_bytes ? (((uint8_t*) data_ptr) + total_bytes-1) : NULL;
}

tusb_error_t  hcd_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  ohci_ed_t* const p_ed = &ohci_data.control[dev_addr].ed;

  ed_init(p_ed, dev_addr, max_packet_size, 0, TUSB_XFER_CONTROL, 0); // TODO binterval of control is ignored

  if ( dev_addr != 0 )
  { // insert to control head
    ed_list_insert( p_ed_head[TUSB_XFER_CONTROL], p_ed);
  }else
  {
    p_ed->skip = 0; // addr0 is used as static control head --> only need to clear skip bit
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_control_xfer(uint8_t dev_addr, tusb_control_request_t const * p_request, uint8_t data[])
{
  ohci_ed_t* const p_ed = &ohci_data.control[dev_addr].ed;

  ohci_gtd_t *p_setup      = &ohci_data.control[dev_addr].gtd[0];
  ohci_gtd_t *p_data       = p_setup + 1;
  ohci_gtd_t *p_status     = p_setup + 2;

  //------------- SETUP Phase -------------//
  gtd_init(p_setup, (void*) p_request, 8);
  p_setup->index       = dev_addr;
  p_setup->pid         = OHCI_PID_SETUP;
  p_setup->data_toggle = BIN8(10); // DATA0
  p_setup->next_td     = (uint32_t) p_data;

  //------------- DATA Phase -------------//
  if (p_request->wLength > 0)
  {
    gtd_init(p_data, data, p_request->wLength);
    p_data->index       = dev_addr;
    p_data->pid         = p_request->bmRequestType_bit.direction ? OHCI_PID_IN : OHCI_PID_OUT;
    p_data->data_toggle = BIN8(11); // DATA1
  }else
  {
    p_data = p_setup;
  }
  p_data->next_td = (uint32_t) p_status;

  //------------- STATUS Phase -------------//
  gtd_init(p_status, NULL, 0); // zero-length data
  p_status->index           = dev_addr;
  p_status->pid             = p_request->bmRequestType_bit.direction ? OHCI_PID_OUT : OHCI_PID_IN; // reverse direction of data phase
  p_status->data_toggle     = BIN8(11); // DATA1
  p_status->delay_interrupt = OHCI_INT_ON_COMPLETE_YES;

  //------------- Attach TDs list to Control Endpoint -------------//
  p_ed->td_head.address = (uint32_t) p_setup;

  OHCI_REG->command_status_bit.control_list_filled = 1;

  return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_control_close(uint8_t dev_addr)
{
  ohci_ed_t* const p_ed = &ohci_data.control[dev_addr].ed;

  if ( dev_addr == 0 )
  { // addr0 serves as static head --> only set skip bitx
    p_ed->skip = 1;
  }else
  {
    ed_list_remove( p_ed_head[ ed_get_xfer_type(p_ed)], p_ed );

    // TODO refractor to be USBH
    usbh_devices[dev_addr].state = TUSB_DEVICE_STATE_UNPLUG;
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// BULK/INT/ISO PIPE API
//--------------------------------------------------------------------+
static inline uint8_t ed_get_index(ohci_ed_t const * const p_ed) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline uint8_t ed_get_index(ohci_ed_t const * const p_ed)
{
  return p_ed - ohci_data.device[p_ed->device_address-1].ed;
}

static inline ohci_ed_t * ed_from_pipe_handle(pipe_handle_t pipe_hdl) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline ohci_ed_t * ed_from_pipe_handle(pipe_handle_t pipe_hdl)
{
  return &ohci_data.device[pipe_hdl.dev_addr-1].ed[pipe_hdl.index];
}

static inline ohci_ed_t * ed_find_free(uint8_t dev_addr) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline ohci_ed_t * ed_find_free(uint8_t dev_addr)
{
  for(uint8_t i = 0; i < HCD_MAX_ENDPOINT; i++)
  {
    if ( !ohci_data.device[dev_addr-1].ed[i].used )
    {
      return &ohci_data.device[dev_addr-1].ed[i];
    }
  }

  return NULL;
}

static ohci_ed_t * ed_list_find_previous(ohci_ed_t const * p_head, ohci_ed_t const * p_ed)
{
  uint32_t max_loop = HCD_MAX_ENDPOINT*TUSB_CFG_HOST_DEVICE_MAX;

  ohci_ed_t const * p_prev = p_head;

  ASSERT_PTR(p_prev, NULL);

  while ( align16(p_prev->next_ed) != 0               && /* not reach null */
          align16(p_prev->next_ed) != (uint32_t) p_ed && /* not found yet */
          max_loop > 0)
  {
    p_prev = (ohci_ed_t const *) align16(p_prev->next_ed);
    max_loop--;
  }

  return ( align16(p_prev->next_ed) == (uint32_t) p_ed ) ? (ohci_ed_t*) p_prev : NULL;
}

static void ed_list_insert(ohci_ed_t * p_pre, ohci_ed_t * p_ed)
{
  p_ed->next_ed |= p_pre->next_ed; // to reserve 4 lsb bits
  p_pre->next_ed = (p_pre->next_ed & 0x0FUL)  | ((uint32_t) p_ed);
}

static void ed_list_remove(ohci_ed_t * p_head, ohci_ed_t * p_ed)
{
  ohci_ed_t * const p_prev  = ed_list_find_previous(p_head, p_ed);

  p_prev->next_ed = (p_prev->next_ed & 0x0fUL) | align16(p_ed->next_ed);
  // point the removed ED's next pointer to list head to make sure HC can always safely move away from this ED
  p_ed->next_ed   = (uint32_t) p_head;
  p_ed->used      = 0; // free ED
}

pipe_handle_t hcd_pipe_open(uint8_t dev_addr, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code)
{
  pipe_handle_t const null_handle = { .dev_addr = 0, .xfer_type = 0, .index = 0 };

  // TODO iso support
  ASSERT(p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS, null_handle );

  //------------- Prepare Queue Head -------------//
  ohci_ed_t * const p_ed = ed_find_free(dev_addr);
  ASSERT_PTR(p_ed, null_handle);

  ed_init( p_ed, dev_addr, p_endpoint_desc->wMaxPacketSize.size, p_endpoint_desc->bEndpointAddress,
            p_endpoint_desc->bmAttributes.xfer, p_endpoint_desc->bInterval );
  p_ed->td_tail.class_code = class_code;

  ed_list_insert( p_ed_head[p_endpoint_desc->bmAttributes.xfer], p_ed );

  return (pipe_handle_t)
  {
    .dev_addr  = dev_addr,
    .xfer_type = p_endpoint_desc->bmAttributes.xfer,
    .index     = ed_get_index(p_ed)
  };
}

static ohci_gtd_t * gtd_find_free(uint8_t dev_addr)
{
  for(uint8_t i=0; i < HCD_MAX_XFER; i++)
  {
    if (!ohci_data.device[dev_addr-1].gtd[i].used)
    {
      return &ohci_data.device[dev_addr-1].gtd[i];
    }
  }

  return NULL;
}

static void td_insert_to_ed(ohci_ed_t* p_ed, ohci_gtd_t * p_gtd)
{
  // tail is always NULL
  if ( align16(p_ed->td_head.address) == 0 )
  { // TD queue is empty --> head = TD
    p_ed->td_head.address |= (uint32_t) p_gtd;
  }
  else
  { // TODO currently only support queue up to 2 TD each endpoint at a time
    ((ohci_gtd_t*) align16(p_ed->td_head.address))->next_td = (uint32_t) p_gtd;
  }
}

static tusb_error_t  pipe_queue_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete)
{
  ohci_ed_t* const p_ed = ed_from_pipe_handle(pipe_hdl);

  if ( !p_ed->is_iso )
  {
    ohci_gtd_t * const p_gtd = gtd_find_free(pipe_hdl.dev_addr);
    ASSERT_PTR(p_gtd, TUSB_ERROR_EHCI_NOT_ENOUGH_QTD); // TODO refractor error code

    gtd_init(p_gtd, buffer, total_bytes);
    p_gtd->index           = pipe_hdl.index;
    if ( int_on_complete )  p_gtd->delay_interrupt = OHCI_INT_ON_COMPLETE_YES;

    td_insert_to_ed(p_ed, p_gtd);
  }else
  {
    ASSERT_STATUS(TUSB_ERROR_NOT_SUPPORTED_YET);
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_queue_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes)
{
  return pipe_queue_xfer(pipe_hdl, buffer, total_bytes, false);
}

tusb_error_t  hcd_pipe_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete)
{
  ASSERT_STATUS( pipe_queue_xfer(pipe_hdl, buffer, total_bytes, true) );

  tusb_xfer_type_t xfer_type = ed_get_xfer_type( ed_from_pipe_handle(pipe_hdl) );

  if (TUSB_XFER_BULK == xfer_type) OHCI_REG->command_status_bit.bulk_list_filled = 1;

  return TUSB_ERROR_NONE;
}

/// pipe_close should only be called as a part of unmount/safe-remove process
// endpoints are tied to an address, which only reclaim after a long delay when enumerating
// thus there is no need to make sure ED is not in HC's cahed as it will not for sure
tusb_error_t  hcd_pipe_close(pipe_handle_t pipe_hdl)
{
  ohci_ed_t * const p_ed = ed_from_pipe_handle(pipe_hdl);

  ed_list_remove( p_ed_head[ ed_get_xfer_type(p_ed)], p_ed );

  return TUSB_ERROR_FAILED;
}

bool hcd_pipe_is_busy(pipe_handle_t pipe_hdl)
{
  ohci_ed_t const * const p_ed = ed_from_pipe_handle(pipe_hdl);
  return align16(p_ed->td_head.address) != align16(p_ed->td_tail.address);
}

bool hcd_pipe_is_error(pipe_handle_t pipe_hdl)
{
  ohci_ed_t const * const p_ed = ed_from_pipe_handle(pipe_hdl);
  return p_ed->td_head.halted;
}

bool hcd_pipe_is_stalled(pipe_handle_t pipe_hdl)
{
  ohci_ed_t const * const p_ed = ed_from_pipe_handle(pipe_hdl);
  return p_ed->td_head.halted && p_ed->is_stalled;
}

uint8_t hcd_pipe_get_endpoint_addr(pipe_handle_t pipe_hdl)
{
  ohci_ed_t const * const p_ed = ed_from_pipe_handle(pipe_hdl);
  return p_ed->endpoint_number | (p_ed->direction == OHCI_PID_IN ? TUSB_DIR_DEV_TO_HOST_MASK : 0 );
}

tusb_error_t hcd_pipe_clear_stall(pipe_handle_t pipe_hdl)
{
  ohci_ed_t * const p_ed = ed_from_pipe_handle(pipe_hdl);

  p_ed->is_stalled        = 0;
  p_ed->td_tail.address  &= 0x0Ful; // set tail pointer back to NULL

  p_ed->td_head.toggle            = 0; // reset data toggle
  p_ed->td_head.halted            = 0;

  if ( TUSB_XFER_BULK == ed_get_xfer_type(p_ed) ) OHCI_REG->command_status_bit.bulk_list_filled = 1;

  return TUSB_ERROR_NONE;
}


//--------------------------------------------------------------------+
// OHCI Interrupt Handler
//--------------------------------------------------------------------+
static ohci_td_item_t* list_reverse(ohci_td_item_t* td_head)
{
  ohci_td_item_t* td_reverse_head = NULL;

  while(td_head != NULL)
  {
    uint32_t next = td_head->next_td;

    // make current's item become reverse's first item
    td_head->next_td = (uint32_t) td_reverse_head;
    td_reverse_head  = td_head;

    td_head = (ohci_td_item_t*) next; // advance to next item
  }

  return td_reverse_head;
}

static inline bool gtd_is_control(ohci_gtd_t const * const p_qtd) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline bool gtd_is_control(ohci_gtd_t const * const p_qtd)
{
  return ((uint32_t) p_qtd) < ((uint32_t) ohci_data.device); // check ohci_data_t for memory layout
}

static inline ohci_ed_t* gtd_get_ed(ohci_gtd_t const * const p_qtd) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline ohci_ed_t* gtd_get_ed(ohci_gtd_t const * const p_qtd)
{
  if ( gtd_is_control(p_qtd) )
  {
    return &ohci_data.control[p_qtd->index].ed;
  }else
  {
    uint8_t dev_addr_idx = (((uint32_t)p_qtd) - ((uint32_t)ohci_data.device)) / sizeof(ohci_data.device[0]);

    return &ohci_data.device[dev_addr_idx].ed[p_qtd->index];
  }
}

static inline uint32_t gtd_xfer_byte_left(uint32_t buffer_end, uint32_t current_buffer) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint32_t gtd_xfer_byte_left(uint32_t buffer_end, uint32_t current_buffer)
{ // 5.2.9 OHCI sample code
  return (align4k(buffer_end ^ current_buffer) ? 0x1000 : 0) +
      offset4k(buffer_end) - offset4k(current_buffer) + 1;
}

static void done_queue_isr(uint8_t hostid)
{
  uint8_t max_loop = (TUSB_CFG_HOST_DEVICE_MAX+1)*(HCD_MAX_XFER+OHCI_MAX_ITD);

  // done head is written in reversed order of completion --> need to reverse the done queue first
  ohci_td_item_t* td_head = list_reverse ( (ohci_td_item_t*) align16(ohci_data.hcca.done_head) );

  while( td_head != NULL && max_loop > 0)
  {
    // TODO check if td_head is iso td
    //------------- Non ISO transfer -------------//
    ohci_gtd_t * const p_qtd = (ohci_gtd_t *) td_head;
    tusb_event_t const event = (p_qtd->condition_code == OHCI_CCODE_NO_ERROR) ? TUSB_EVENT_XFER_COMPLETE :
                               (p_qtd->condition_code == OHCI_CCODE_STALL) ? TUSB_EVENT_XFER_STALLED : TUSB_EVENT_XFER_ERROR;

    p_qtd->used = 0; // free TD
    if ( (p_qtd->delay_interrupt == OHCI_INT_ON_COMPLETE_YES) || (event != TUSB_EVENT_XFER_COMPLETE) )
    {
      ohci_ed_t * const p_ed  = gtd_get_ed(p_qtd);

      uint32_t const xferred_bytes = p_qtd->expected_bytes - gtd_xfer_byte_left((uint32_t) p_qtd->buffer_end, (uint32_t) p_qtd->current_buffer_pointer);

      // NOTE Assuming the current list is BULK and there is no other EDs in the list has queued TDs.
      // When there is a error resulting this ED is halted, and this EP still has other queued TD
      // --> the Bulk list only has this halted EP queueing TDs (remaining)
      // --> Bulk list will be considered as not empty by HC !!! while there is no attempt transaction on this list
      // --> HC will not process Control list (due to service ratio when Bulk list not empty)
      // To walk-around this, the halted ED will have TailP = HeadP (empty list condition), when clearing halt
      // the TailP must be set back to NULL for processing remaining TDs
      if ((event != TUSB_EVENT_XFER_COMPLETE))
      {
        p_ed->td_tail.address &= 0x0Ful;
        p_ed->td_tail.address |= align16(p_ed->td_head.address); // mark halted EP as empty queue
        if ( event == TUSB_EVENT_XFER_STALLED ) p_ed->is_stalled = 1;
      }

      pipe_handle_t pipe_hdl =
      {
          .dev_addr  = p_ed->device_address,
          .xfer_type = ed_get_xfer_type(p_ed),
      };

      if ( pipe_hdl.xfer_type != TUSB_XFER_CONTROL) pipe_hdl.index = ed_get_index(p_ed);

      usbh_xfer_isr(pipe_hdl, p_ed->td_tail.class_code, event, xferred_bytes);
    }

    td_head = (ohci_td_item_t*) td_head->next_td;
    max_loop--;
  }
}

void hcd_isr(uint8_t hostid)
{
  uint32_t const int_en     = OHCI_REG->interrupt_enable;
  uint32_t const int_status = OHCI_REG->interrupt_status & int_en;

  if (int_status == 0) return;

  //------------- RootHub status -------------//
  if ( int_status & OHCI_INT_RHPORT_STATUS_CHANGE_MASK )
  {
    uint32_t const rhport_status = OHCI_REG->rhport_status[0] & OHCI_RHPORT_ALL_CHANGE_MASK;

    // TODO dual port is not yet supported
    if ( rhport_status & OHCI_RHPORT_CONNECT_STATUS_CHANGE_MASK )
    {
      // TODO check if remote wake-up
      if ( OHCI_REG->rhport_status_bit[0].current_connect_status )
      {
        // TODO reset port immediately, without this controller will got 2-3 (debouncing connection status change)
        OHCI_REG->rhport_status[0] = OHCI_RHPORT_PORT_RESET_STATUS_MASK;
        usbh_hcd_rhport_plugged_isr(0);
      }else
      {
        usbh_hcd_rhport_unplugged_isr(0);
      }
    }

    if ( rhport_status & OHCI_RHPORT_PORT_SUSPEND_CHANGE_MASK)
    {

    }

    OHCI_REG->rhport_status[0] = rhport_status; // acknowledge all interrupt
  }

  //------------- Transfer Complete -------------//
  if ( int_status & OHCI_INT_WRITEBACK_DONEHEAD_MASK)
  {
    done_queue_isr(hostid);
  }

  OHCI_REG->interrupt_status = int_status; // Acknowledge handled interrupt
}
//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+


#endif

