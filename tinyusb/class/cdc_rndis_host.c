/**************************************************************************/
/*!
    @file     cdc_rndis_host.c
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

#include "tusb_option.h"

#if (MODE_HOST_SUPPORTED && TUSB_CFG_HOST_CDC && TUSB_CFG_HOST_CDC_RNDIS)

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "cdc_host.h"
#include "cdc_rndis_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define RNDIS_MSG_PAYLOAD_MAX   (1024*4)

TUSB_CFG_ATTR_USBRAM static uint8_t msg_notification[TUSB_CFG_HOST_DEVICE_MAX][8];
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4) static uint8_t msg_payload[RNDIS_MSG_PAYLOAD_MAX];

STATIC_VAR rndish_data_t rndish_data[TUSB_CFG_HOST_DEVICE_MAX];

// TODO Microsoft requires message length for any get command must be at least 4096 bytes

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static tusb_error_t rndis_body_subtask(void);
static tusb_error_t send_message_get_response_subtask( uint8_t dev_addr, cdch_data_t *p_cdc,
                                                       uint8_t * p_mess, uint32_t mess_length,
                                                       uint8_t *p_response );

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
tusb_error_t tusbh_cdc_rndis_get_mac_addr(uint8_t dev_addr, uint8_t mac_address[6])
{
  ASSERT( tusbh_cdc_rndis_is_mounted(dev_addr),  TUSB_ERROR_CDCH_DEVICE_NOT_MOUNTED);
  ASSERT_PTR( mac_address,  TUSB_ERROR_INVALID_PARA);

  memcpy(mac_address, rndish_data[dev_addr-1].mac_address, 6);

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

// To enable the TASK_ASSERT style (quick return on false condition) in a real RTOS, a task must act as a wrapper
// and is used mainly to call subtasks. Within a subtask return statement can be called freely, the task with
// forever loop cannot have any return at all.
OSAL_TASK_FUNCTION(cdch_rndis_task) (void* p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  rndis_body_subtask();

  OSAL_TASK_LOOP_END
}

static tusb_error_t rndis_body_subtask(void)
{
  static uint8_t relative_addr;

  OSAL_SUBTASK_BEGIN

  for (relative_addr = 0; relative_addr < TUSB_CFG_HOST_DEVICE_MAX; relative_addr++)
  {

  }

  osal_task_delay(100);

  OSAL_SUBTASK_END
}

//--------------------------------------------------------------------+
// RNDIS-CDC Driver API
//--------------------------------------------------------------------+
void rndish_init(void)
{
  memclr_(rndish_data, sizeof(rndish_data_t)*TUSB_CFG_HOST_DEVICE_MAX);

  //------------- Task creation -------------//

  //------------- semaphore creation for notificaiton pipe -------------//
  for(uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX; i++)
  {
    rndish_data[i].sem_notification_hdl = osal_semaphore_create( OSAL_SEM_REF(rndish_data[i].semaphore_notification) );
  }
}

void rndish_close(uint8_t dev_addr)
{
  osal_semaphore_reset( rndish_data[dev_addr-1].sem_notification_hdl );
//  memclr_(&rndish_data[dev_addr-1], sizeof(rndish_data_t)); TODO need to move semaphore & its handle out before memclr
}


static rndis_msg_initialize_t const msg_init =
{
    .type          = RNDIS_MSG_INITIALIZE,
    .length        = sizeof(rndis_msg_initialize_t),
    .request_id    = 1, // TODO should use some magic number
    .major_version = 1,
    .minor_version = 0,
    .max_xfer_size = 0x4000 // TODO mimic windows
};

static rndis_msg_query_t const msg_query_permanent_addr =
{
    .type          = RNDIS_MSG_QUERY,
    .length        = sizeof(rndis_msg_query_t)+6,
    .request_id    = 1,
    .oid           = RNDIS_OID_802_3_PERMANENT_ADDRESS,
    .buffer_length = 6,
    .buffer_offset = 20,
};

static rndis_msg_set_t const msg_set_packet_filter =
{
    .type          = RNDIS_MSG_SET,
    .length        = sizeof(rndis_msg_set_t)+4,
    .request_id    = 1,
    .oid           = RNDIS_OID_GEN_CURRENT_PACKET_FILTER,
    .buffer_length = 4,
    .buffer_offset = 20,
};

tusb_error_t rndish_open_subtask(uint8_t dev_addr, cdch_data_t *p_cdc)
{
  tusb_error_t error;

  OSAL_SUBTASK_BEGIN

  //------------- Message Initialize -------------//
  memcpy(msg_payload, &msg_init, sizeof(rndis_msg_initialize_t));
  OSAL_SUBTASK_INVOKED_AND_WAIT(
      send_message_get_response_subtask( dev_addr, p_cdc,
                                         msg_payload, sizeof(rndis_msg_initialize_t),
                                         msg_payload),
      error
  );
  if ( TUSB_ERROR_NONE != error )   SUBTASK_EXIT(error);

  // TODO currently not support multiple data packets per xfer
  rndis_msg_initialize_cmplt_t * const p_init_cmpt = (rndis_msg_initialize_cmplt_t *) msg_payload;
  SUBTASK_ASSERT(p_init_cmpt->type == RNDIS_MSG_INITIALIZE_CMPLT && p_init_cmpt->status == RNDIS_STATUS_SUCCESS &&
                 p_init_cmpt->max_packet_per_xfer == 1 && p_init_cmpt->max_xfer_size <= RNDIS_MSG_PAYLOAD_MAX);
  rndish_data[dev_addr-1].max_xfer_size = p_init_cmpt->max_xfer_size;

  //------------- Message Query 802.3 Permanent Address -------------//
  memcpy(msg_payload, &msg_query_permanent_addr, sizeof(rndis_msg_query_t));
  memclr_(msg_payload + sizeof(rndis_msg_query_t), 6); // 6 bytes for MAC address

  OSAL_SUBTASK_INVOKED_AND_WAIT(
      send_message_get_response_subtask( dev_addr, p_cdc,
                                         msg_payload, sizeof(rndis_msg_query_t) + 6,
                                         msg_payload),
      error
  );
  if ( TUSB_ERROR_NONE != error )   SUBTASK_EXIT(error);

  rndis_msg_query_cmplt_t * const p_query_cmpt = (rndis_msg_query_cmplt_t *) msg_payload;
  SUBTASK_ASSERT(p_query_cmpt->type == RNDIS_MSG_QUERY_CMPLT && p_query_cmpt->status == RNDIS_STATUS_SUCCESS);
  memcpy(rndish_data[dev_addr-1].mac_address, msg_payload + 8 + p_query_cmpt->buffer_offset, 6);

  //------------- Set OID_GEN_CURRENT_PACKET_FILTER to (DIRECTED | MULTICAST | BROADCAST) -------------//
  memcpy(msg_payload, &msg_set_packet_filter, sizeof(rndis_msg_set_t));
  memclr_(msg_payload + sizeof(rndis_msg_set_t), 4); // 4 bytes for filter flags
  ((rndis_msg_set_t*) msg_payload)->oid_buffer[0] = (RNDIS_PACKET_TYPE_DIRECTED | RNDIS_PACKET_TYPE_MULTICAST | RNDIS_PACKET_TYPE_BROADCAST);

  OSAL_SUBTASK_INVOKED_AND_WAIT(
      send_message_get_response_subtask( dev_addr, p_cdc,
                                         msg_payload, sizeof(rndis_msg_set_t) + 4,
                                         msg_payload),
      error
  );
  if ( TUSB_ERROR_NONE != error )   SUBTASK_EXIT(error);

  rndis_msg_set_cmplt_t * const p_set_cmpt = (rndis_msg_set_cmplt_t *) msg_payload;
  SUBTASK_ASSERT(p_set_cmpt->type == RNDIS_MSG_SET_CMPLT && p_set_cmpt->status == RNDIS_STATUS_SUCCESS);

  tusbh_cdc_rndis_mounted_cb(dev_addr);

  OSAL_SUBTASK_END
}

void rndish_xfer_isr(cdch_data_t *p_cdc, pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
  if ( pipehandle_is_equal(pipe_hdl, p_cdc->pipe_notification) )
  {
    osal_semaphore_post( rndish_data[pipe_hdl.dev_addr-1].sem_notification_hdl );
  }
}

//--------------------------------------------------------------------+
// INTERNAL & HELPER
//--------------------------------------------------------------------+
static tusb_error_t send_message_get_response_subtask( uint8_t dev_addr, cdch_data_t *p_cdc,
                                                       uint8_t * p_mess, uint32_t mess_length,
                                                       uint8_t *p_response)
{
  tusb_error_t error;

  OSAL_SUBTASK_BEGIN

  //------------- Send RNDIS Control Message -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
      usbh_control_xfer_subtask( dev_addr, bm_request_type(TUSB_DIR_HOST_TO_DEV, TUSB_REQUEST_TYPE_CLASS, TUSB_REQUEST_RECIPIENT_INTERFACE),
                                 CDC_REQUEST_SEND_ENCAPSULATED_COMMAND, 0, p_cdc->interface_number,
                                 mess_length, p_mess),
      error
  );
  if ( TUSB_ERROR_NONE != error )   SUBTASK_EXIT(error);

  //------------- waiting for Response Available notification -------------//
  (void) hcd_pipe_xfer(p_cdc->pipe_notification, msg_notification[dev_addr-1], 8, true);
  osal_semaphore_wait(rndish_data[dev_addr-1].sem_notification_hdl, OSAL_TIMEOUT_NORMAL, &error);
  if ( TUSB_ERROR_NONE != error )   SUBTASK_EXIT(error);
  SUBTASK_ASSERT(msg_notification[dev_addr-1][0] == 1);

  //------------- Get RNDIS Message Initialize Complete -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask( dev_addr, bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_CLASS, TUSB_REQUEST_RECIPIENT_INTERFACE),
                               CDC_REQUEST_GET_ENCAPSULATED_RESPONSE, 0, p_cdc->interface_number,
                               RNDIS_MSG_PAYLOAD_MAX, p_response),
    error
  );
  if ( TUSB_ERROR_NONE != error )   SUBTASK_EXIT(error);

  OSAL_SUBTASK_END
}

//static tusb_error_t send_process_msg_initialize_subtask(uint8_t dev_addr, cdch_data_t *p_cdc)
//{
//  tusb_error_t error;
//
//  OSAL_SUBTASK_BEGIN
//
//  *((rndis_msg_initialize_t*) msg_payload) = (rndis_msg_initialize_t)
//                                            {
//                                                .type          = RNDIS_MSG_INITIALIZE,
//                                                .length        = sizeof(rndis_msg_initialize_t),
//                                                .request_id    = 1, // TODO should use some magic number
//                                                .major_version = 1,
//                                                .minor_version = 0,
//                                                .max_xfer_size = 0x4000 // TODO mimic windows
//                                            };
//
//
//
//  OSAL_SUBTASK_END
//}
#endif
