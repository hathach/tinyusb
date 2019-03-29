/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

#if TUSB_OPT_DEVICE_ENABLED

#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"

#ifndef CFG_TUD_TASK_QUEUE_SZ
#define CFG_TUD_TASK_QUEUE_SZ   16
#endif

//--------------------------------------------------------------------+
// Device Data
//--------------------------------------------------------------------+
typedef struct {
  volatile uint8_t config_num; // 0 is non-configured ~ disconnect
  bool remote_wakeup_en;

  uint8_t itf2drv[16];      // map interface number to driver (0xff is invalid)
  uint8_t ep2drv[8][2];     // map endpoint to driver ( 0xff is invalid )

//  uint8_t ep_busy_mask[2];  // bit mask for busy endpoint
  uint8_t ep_stall_mask[2]; // bit mask for stalled endpoint
}usbd_device_t;

static usbd_device_t _usbd_dev = { 0 };

// Auto descriptor is enabled, descriptor set point to auto generated one
#if CFG_TUD_DESC_AUTO
extern tud_desc_set_t const _usbd_auto_desc_set;
tud_desc_set_t const* usbd_desc_set = &_usbd_auto_desc_set;
#else
tud_desc_set_t const* usbd_desc_set = &tud_desc_set;
#endif

//--------------------------------------------------------------------+
// Class Driver
//--------------------------------------------------------------------+
typedef struct {
  uint8_t class_code;

  void (* init           ) (void);
  bool (* open           ) (uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t* p_length);
  bool (* control_request ) (uint8_t rhport, tusb_control_request_t const * request);
  bool (* control_request_complete ) (uint8_t rhport, tusb_control_request_t const * request);
  bool (* xfer_cb        ) (uint8_t rhport, uint8_t ep_addr, xfer_result_t, uint32_t);
  void (* sof            ) (uint8_t rhport);
  void (* reset          ) (uint8_t);
} usbd_class_driver_t;

static usbd_class_driver_t const usbd_class_drivers[] =
{
  #if CFG_TUD_CDC
    {
        .class_code      = TUSB_CLASS_CDC,
        .init            = cdcd_init,
        .open            = cdcd_open,
        .control_request = cdcd_control_request,
        .control_request_complete = cdcd_control_request_complete,
        .xfer_cb         = cdcd_xfer_cb,
        .sof             = NULL,
        .reset           = cdcd_reset
    },
  #endif

  #if CFG_TUD_MSC
    {
        .class_code      = TUSB_CLASS_MSC,
        .init            = mscd_init,
        .open            = mscd_open,
        .control_request = mscd_control_request,
        .control_request_complete = mscd_control_request_complete,
        .xfer_cb         = mscd_xfer_cb,
        .sof             = NULL,
        .reset           = mscd_reset
    },
  #endif

  #if CFG_TUD_HID
    {
        .class_code      = TUSB_CLASS_HID,
        .init            = hidd_init,
        .open            = hidd_open,
        .control_request = hidd_control_request,
        .control_request_complete = hidd_control_request_complete,
        .xfer_cb         = hidd_xfer_cb,
        .sof             = NULL,
        .reset           = hidd_reset
    },
  #endif

  #if CFG_TUD_MIDI
    {
        .class_code      = TUSB_CLASS_AUDIO,
        .init            = midid_init,
        .open            = midid_open,
        .control_request = midid_control_request,
        .control_request_complete = midid_control_request_complete,
        .xfer_cb         = midid_xfer_cb,
        .sof             = NULL,
        .reset           = midid_reset
    },
  #endif

  #if CFG_TUD_CUSTOM_CLASS
    {
        .class_code      = TUSB_CLASS_VENDOR_SPECIFIC,
        .init            = cusd_init,
        .open            = cusd_open,
        .control_request = cusd_control_request,
        .control_request_complete = cusd_control_request_complete,
        .xfer_cb         = cusd_xfer_cb,
        .sof             = NULL,
        .reset           = cusd_reset
    },
  #endif
};

enum { USBD_CLASS_DRIVER_COUNT = TU_ARRAY_SZIE(usbd_class_drivers) };

//--------------------------------------------------------------------+
// DCD Event
//--------------------------------------------------------------------+

// Event queue
// OPT_MODE_DEVICE is used by OS NONE for mutex (disable usb isr)
OSAL_QUEUE_DEF(OPT_MODE_DEVICE, _usbd_qdef, CFG_TUD_TASK_QUEUE_SZ, dcd_event_t);
static osal_queue_t _usbd_q;

//--------------------------------------------------------------------+
// Prototypes
//--------------------------------------------------------------------+
static void mark_interface_endpoint(uint8_t ep2drv[8][2], uint8_t const* p_desc, uint16_t desc_len, uint8_t driver_id);
static bool process_control_request(uint8_t rhport, tusb_control_request_t const * p_request);
static bool process_set_config(uint8_t rhport);
static void const* get_descriptor(tusb_control_request_t const * p_request, uint16_t* desc_len);

void usbd_control_reset (uint8_t rhport);
bool usbd_control_xfer_cb (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void usbd_control_set_complete_callback( bool (*fp) (uint8_t, tusb_control_request_t const * ) );

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+
bool tud_mounted(void)
{
  return _usbd_dev.config_num > 0;
}

bool tud_remote_wakeup(void)
{
  // only wake up host if this feature is enabled
  if (_usbd_dev.remote_wakeup_en ) dcd_remote_wakeup(TUD_OPT_RHPORT);

  return _usbd_dev.remote_wakeup_en;
}

//--------------------------------------------------------------------+
// USBD Task
//--------------------------------------------------------------------+
bool usbd_init (void)
{
  // Init device queue & task
  _usbd_q = osal_queue_create(&_usbd_qdef);
  TU_ASSERT(_usbd_q != NULL);

  // Init class drivers
  for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++) usbd_class_drivers[i].init();

  // Init device controller driver
  dcd_init(TUD_OPT_RHPORT);
  dcd_int_enable(TUD_OPT_RHPORT);

  return true;
}

static void usbd_reset(uint8_t rhport)
{
  tu_varclr(&_usbd_dev);
  memset(_usbd_dev.itf2drv, 0xff, sizeof(_usbd_dev.itf2drv)); // invalid mapping
  memset(_usbd_dev.ep2drv , 0xff, sizeof(_usbd_dev.ep2drv )); // invalid mapping

  usbd_control_reset(rhport);

  for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++)
  {
    if ( usbd_class_drivers[i].reset ) usbd_class_drivers[i].reset( rhport );
  }
}

/* USB Device Driver task
 * This top level thread manages all device controller event and delegates events to class-specific drivers.
 * This should be called periodically within the mainloop or rtos thread.
 *
   @code
    int main(void)
    {
      application_init();
      tusb_init();

      while(1) // the mainloop
      {
        application_code();

        tud_task(); // tinyusb device task
      }
    }
    @endcode
 */
void tud_task (void)
{
  // Skip if stack is not initialized
  if ( !tusb_inited() ) return;

  // Loop until there is no more events in the queue
  while (1)
  {
    dcd_event_t event;

    if ( !osal_queue_receive(_usbd_q, &event) ) return;

    switch ( event.event_id )
    {
      case DCD_EVENT_SETUP_RECEIVED:
        // Process control request
        if ( !process_control_request(event.rhport, &event.setup_received) )
        {
          // Failed -> stall both control endpoint IN and OUT
          dcd_edpt_stall(event.rhport, 0);
          dcd_edpt_stall(event.rhport, 0 | TUSB_DIR_IN_MASK);
        }
      break;

      case DCD_EVENT_XFER_COMPLETE:
      {
        // Invoke the class callback associated with the endpoint address
        uint8_t const ep_addr = event.xfer_complete.ep_addr;

        if ( 0 == tu_edpt_number(ep_addr) )
        {
          // control transfer DATA stage callback
          usbd_control_xfer_cb(event.rhport, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
        }
        else
        {
          uint8_t const drv_id = _usbd_dev.ep2drv[tu_edpt_number(ep_addr)][tu_edpt_dir(ep_addr)];
          TU_ASSERT(drv_id < USBD_CLASS_DRIVER_COUNT,);

          usbd_class_drivers[drv_id].xfer_cb(event.rhport, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
        }
      }
      break;

      case DCD_EVENT_BUS_RESET:
        usbd_reset(event.rhport);
        // TODO remove since if task is too slow, we could clear the event of the new attached
        osal_queue_reset(_usbd_q);
      break;

      case DCD_EVENT_UNPLUGGED:
        usbd_reset(event.rhport);
        // TODO remove since if task is too slow, we could clear the event of the new attached
        osal_queue_reset(_usbd_q);

        // invoke callback
        if (tud_umount_cb) tud_umount_cb();
      break;

      case DCD_EVENT_SOF:
        for ( uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++ )
        {
          if ( usbd_class_drivers[i].sof )
          {
            usbd_class_drivers[i].sof(event.rhport);
          }
        }
      break;

      case USBD_EVT_FUNC_CALL:
        if ( event.func_call.func ) event.func_call.func(event.func_call.param);
      break;

      default:
        TU_BREAKPOINT();
      break;
    }
  }
}

//--------------------------------------------------------------------+
// Control Request Parser & Handling
//--------------------------------------------------------------------+

// This handles the actual request and its response.
// return false will cause its caller to stall control endpoint
static bool process_control_request(uint8_t rhport, tusb_control_request_t const * p_request)
{
  usbd_control_set_complete_callback(NULL);

  if ( TUSB_REQ_RCPT_DEVICE == p_request->bmRequestType_bit.recipient &&
       TUSB_REQ_TYPE_STANDARD == p_request->bmRequestType_bit.type )
  {
    //------------- Standard Device Requests e.g in enumeration -------------//
    void* data_buf = NULL;
    uint16_t data_len = 0;

    switch ( p_request->bRequest )
    {
      case TUSB_REQ_SET_ADDRESS:
        // DCD must include zero-length status response since depending on mcu,
        // status could be sent either before or after changing device address
        dcd_set_address(rhport, (uint8_t) p_request->wValue);
        return true; // skip the rest
      break;

      case TUSB_REQ_GET_CONFIGURATION:
        data_buf = (uint8_t*) &_usbd_dev.config_num;
        data_len = 1;
      break;

      case TUSB_REQ_SET_CONFIGURATION:
      {
        uint8_t const config = (uint8_t) p_request->wValue;

        dcd_set_config(rhport, config);
        _usbd_dev.config_num = config;

        TU_ASSERT( TUSB_ERROR_NONE == process_set_config(rhport) );
      }
      break;

      case TUSB_REQ_GET_DESCRIPTOR:
        data_buf = (void*) get_descriptor(p_request, &data_len);
        if ( data_buf == NULL || data_len == 0 ) return false;
      break;

      case TUSB_REQ_SET_FEATURE:
        if ( TUSB_REQ_FEATURE_REMOTE_WAKEUP == p_request->wValue )
        {
          // Host enable remote wake up before suspending especially HID device
          _usbd_dev.remote_wakeup_en = true;
        }
      break;

      case TUSB_REQ_CLEAR_FEATURE:
        if ( TUSB_REQ_FEATURE_REMOTE_WAKEUP == p_request->wValue )
        {
          // Host disable remote wake up after resuming
          _usbd_dev.remote_wakeup_en = false;
        }
      break;

      default:
        TU_BREAKPOINT();
      return false;
    }

    usbd_control_xfer(rhport, p_request, data_buf, data_len);
  }
  else if ( TUSB_REQ_RCPT_INTERFACE == p_request->bmRequestType_bit.recipient )
  {
    //------------- Class/Interface Specific Request -------------//
    uint8_t const itf = tu_u16_low(p_request->wIndex);
    uint8_t const drvid = _usbd_dev.itf2drv[ itf ];

    TU_VERIFY(drvid < USBD_CLASS_DRIVER_COUNT);

    usbd_control_set_complete_callback(usbd_class_drivers[drvid].control_request_complete );

    // control endpoint will be stalled if driver return false
    return usbd_class_drivers[drvid].control_request(rhport, p_request);
  }
  else if ( p_request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT &&
            p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD )
  {
    //------------- Endpoint Request -------------//
    switch ( p_request->bRequest )
    {
      case TUSB_REQ_GET_STATUS:
      {
        uint16_t status = usbd_edpt_stalled(rhport, tu_u16_low(p_request->wIndex)) ? 0x0001 : 0x0000;
        usbd_control_xfer(rhport, p_request, &status, 2);
      }
      break;

      case TUSB_REQ_CLEAR_FEATURE:
        if ( TUSB_REQ_FEATURE_EDPT_HALT == p_request->wValue )
        {
          dcd_edpt_clear_stall(rhport, tu_u16_low(p_request->wIndex));
          usbd_control_status(rhport, p_request);
        }
      break;

      case TUSB_REQ_SET_FEATURE:
        if ( TUSB_REQ_FEATURE_EDPT_HALT == p_request->wValue )
        {
          usbd_edpt_stall(rhport, tu_u16_low(p_request->wIndex));
          usbd_control_status(rhport, p_request);
        }
      break;

      default:
        TU_BREAKPOINT();
      return false;
    }
  }
  else
  {
    //------------- Unsupported Request -------------//
    TU_BREAKPOINT();
    return false;
  }

  return true;
}

// Process Set Configure Request
// This function parse configuration descriptor & open drivers accordingly
static bool process_set_config(uint8_t rhport)
{
  uint8_t const * desc_cfg = (uint8_t const *) usbd_desc_set->config;
  TU_ASSERT(desc_cfg != NULL);

  uint8_t const * p_desc = desc_cfg + sizeof(tusb_desc_configuration_t);
  uint16_t const cfg_len = ((tusb_desc_configuration_t*)desc_cfg)->wTotalLength;

  while( p_desc < desc_cfg + cfg_len )
  {
    // Each interface always starts with Interface or Association descriptor
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc) )
    {
      p_desc = tu_desc_next(p_desc); // ignore Interface Association
    }else
    {
      TU_ASSERT( TUSB_DESC_INTERFACE == tu_desc_type(p_desc) );

      tusb_desc_interface_t* desc_itf = (tusb_desc_interface_t*) p_desc;

      // Check if class is supported
      uint8_t drv_id;
      for (drv_id = 0; drv_id < USBD_CLASS_DRIVER_COUNT; drv_id++)
      {
        if ( usbd_class_drivers[drv_id].class_code == desc_itf->bInterfaceClass ) break;
      }
      TU_ASSERT( drv_id < USBD_CLASS_DRIVER_COUNT );

      // Interface number must not be used already TODO alternate interface
      TU_ASSERT( 0xff == _usbd_dev.itf2drv[desc_itf->bInterfaceNumber] );
      _usbd_dev.itf2drv[desc_itf->bInterfaceNumber] = drv_id;

      uint16_t itf_len=0;
      TU_ASSERT( usbd_class_drivers[drv_id].open( rhport, desc_itf, &itf_len ) );
      TU_ASSERT( itf_len >= sizeof(tusb_desc_interface_t) );

      mark_interface_endpoint(_usbd_dev.ep2drv, p_desc, itf_len, drv_id);

      p_desc += itf_len; // next interface
    }
  }

  // invoke callback
  if (tud_mount_cb) tud_mount_cb();

  return TUSB_ERROR_NONE;
}

// Helper marking endpoint of interface belongs to class driver
static void mark_interface_endpoint(uint8_t ep2drv[8][2], uint8_t const* p_desc, uint16_t desc_len, uint8_t driver_id)
{
  uint16_t len = 0;

  while( len < desc_len )
  {
    if ( TUSB_DESC_ENDPOINT == tu_desc_type(p_desc) )
    {
      uint8_t const ep_addr = ((tusb_desc_endpoint_t const*) p_desc)->bEndpointAddress;

      ep2drv[tu_edpt_number(ep_addr)][tu_edpt_dir(ep_addr)] = driver_id;
    }

    len   += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);
  }
}

// return descriptor's buffer and update desc_len
static void const* get_descriptor(tusb_control_request_t const * p_request, uint16_t* desc_len)
{
  tusb_desc_type_t const desc_type = (tusb_desc_type_t) tu_u16_high(p_request->wValue);
  uint8_t const desc_index = tu_u16_low( p_request->wValue );

  uint8_t const * desc_data = NULL;
  uint16_t len = 0;

  *desc_len = 0;

  switch(desc_type)
  {
    case TUSB_DESC_DEVICE:
      desc_data = (uint8_t const *) usbd_desc_set->device;
      len       = sizeof(tusb_desc_device_t);
    break;

    case TUSB_DESC_CONFIGURATION:
      desc_data = (uint8_t const *) usbd_desc_set->config;
      len       = ((tusb_desc_configuration_t const*) desc_data)->wTotalLength;
    break;

    case TUSB_DESC_STRING:
      // String Descriptor always uses the desc set from user
      if ( desc_index < tud_desc_set.string_count )
      {
        desc_data = tud_desc_set.string_arr[desc_index];
        TU_VERIFY( desc_data != NULL, NULL );

        len  = desc_data[0];  // first byte of descriptor is its size
      }else
      {
        // out of range
        /* The 0xEE index string is a Microsoft USB extension.
         * It can be used to tell Windows what driver it should use for the device !!!
         */
        return NULL;
      }
    break;

    case TUSB_DESC_DEVICE_QUALIFIER:
      // TODO If not highspeed capable stall this request otherwise
      // return the descriptor that could work in highspeed
      return NULL;
    break;

    default: return NULL;
  }

  *desc_len = len;
  return desc_data;
}

//--------------------------------------------------------------------+
// DCD Event Handler
//--------------------------------------------------------------------+
void dcd_event_handler(dcd_event_t const * event, bool in_isr)
{
  switch (event->event_id)
  {
    case DCD_EVENT_BUS_RESET:
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_UNPLUGGED:
      _usbd_dev.config_num = 0; // mark disconnected
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_SOF:
      // nothing to do now
    break;

    case DCD_EVENT_SUSPEND:
    case DCD_EVENT_RESUME:
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_SETUP_RECEIVED:
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_XFER_COMPLETE:
      // skip zero-length control status complete event, should dcd notifies us.
      if ( 0 == tu_edpt_number(event->xfer_complete.ep_addr) && event->xfer_complete.len == 0) break;

      osal_queue_send(_usbd_q, event, in_isr);
      TU_ASSERT(event->xfer_complete.result == XFER_RESULT_SUCCESS,);
    break;

    // Not an DCD event, just a convenient way to defer ISR function should we need
    case USBD_EVT_FUNC_CALL:
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    default: break;
  }
}

// helper to send bus signal event
void dcd_event_bus_signal (uint8_t rhport, dcd_eventid_t eid, bool in_isr)
{
  dcd_event_t event = { .rhport = rhport, .event_id = eid, };
  dcd_event_handler(&event, in_isr);
}

// helper to send setup received
void dcd_event_setup_received(uint8_t rhport, uint8_t const * setup, bool in_isr)
{
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_SETUP_RECEIVED };
  memcpy(&event.setup_received, setup, 8);

  dcd_event_handler(&event, in_isr);
}

// helper to send transfer complete event
void dcd_event_xfer_complete (uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, uint8_t result, bool in_isr)
{
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_XFER_COMPLETE };

  event.xfer_complete.ep_addr = ep_addr;
  event.xfer_complete.len     = xferred_bytes;
  event.xfer_complete.result  = result;

  dcd_event_handler(&event, in_isr);
}

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+

// Helper to parse an pair of endpoint descriptors (IN & OUT)
bool usbd_open_edpt_pair(uint8_t rhport, tusb_desc_endpoint_t const* ep_desc, uint8_t xfer_type, uint8_t* ep_out, uint8_t* ep_in)
{
  for(int i=0; i<2; i++)
  {
    TU_ASSERT(TUSB_DESC_ENDPOINT == ep_desc->bDescriptorType &&
              xfer_type          == ep_desc->bmAttributes.xfer );

    TU_ASSERT(dcd_edpt_open(rhport, ep_desc));

    if ( tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN )
    {
      (*ep_in) = ep_desc->bEndpointAddress;
    }else
    {
      (*ep_out) = ep_desc->bEndpointAddress;
    }

    ep_desc = (tusb_desc_endpoint_t const *) tu_desc_next(ep_desc);
  }

  return true;
}

// Helper to defer an isr function
void usbd_defer_func(osal_task_func_t func, void* param, bool in_isr)
{
  dcd_event_t event =
  {
      .rhport   = 0,
      .event_id = USBD_EVT_FUNC_CALL,
  };

  event.func_call.func  = func;
  event.func_call.param = param;

  dcd_event_handler(&event, in_isr);
}

//--------------------------------------------------------------------+
// USBD Endpoint API
//--------------------------------------------------------------------+
void usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  dcd_edpt_stall(rhport, ep_addr);
  _usbd_dev.ep_stall_mask[dir] = tu_bit_set(_usbd_dev.ep_stall_mask[dir], epnum);
}

void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  dcd_edpt_clear_stall(rhport, ep_addr);
  _usbd_dev.ep_stall_mask[dir] = tu_bit_clear(_usbd_dev.ep_stall_mask[dir], epnum);
}

bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  return tu_bit_test(_usbd_dev.ep_stall_mask[dir], epnum);
}

#endif
