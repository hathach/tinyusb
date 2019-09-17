/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
#include "dcd.h"

#ifndef CFG_TUD_TASK_QUEUE_SZ
#define CFG_TUD_TASK_QUEUE_SZ   16
#endif

//--------------------------------------------------------------------+
// Device Data
//--------------------------------------------------------------------+

enum
{
  STATE_POWERED,
  STATE_DEFAULT,
  STATE_ADDRESS,
  STATE_CONFIGURED
};

typedef struct {
  struct
  {
    volatile unsigned int state        : 2; // disconnected, default, address, or configured
    volatile unsigned int suspended    : 1;

    unsigned int remote_wakeup_en      : 1; // enable/disable by host
    unsigned int remote_wakeup_support : 1; // configuration descriptor's attribute
    unsigned int self_powered          : 1; // configuration descriptor's attribute
  } state_bits;

  volatile uint8_t selected_config; // Which configuration has been opened?

  uint8_t ep_busy_map[2];  // bit mask for busy endpoint
  uint8_t ep_stall_map[2]; // bit map for stalled endpoint

  uint8_t itf2drv[16];     // map interface number to driver (0xff is invalid)
  uint8_t ep2drv[8][2];    // map endpoint to driver ( 0xff is invalid )
}usbd_device_t;

static usbd_device_t _usbd_dev = { 0 };

// Invalid driver ID in itf2drv[] ep2drv[][] mapping
enum { DRVID_INVALID = 0xFFu };

//--------------------------------------------------------------------+
// Class Driver
//--------------------------------------------------------------------+
typedef struct {
  uint8_t class_code;

  void (* init             ) (void);
  void (* reset            ) (uint8_t rhport);
  bool (* open             ) (uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t* p_length);
  bool (* control_request  ) (uint8_t rhport, tusb_control_request_t const * request);
  bool (* control_complete ) (uint8_t rhport, tusb_control_request_t const * request);
  bool (* xfer_cb          ) (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
  void (* sof              ) (uint8_t rhport);
} usbd_class_driver_t;

static usbd_class_driver_t const usbd_class_drivers[] =
{
  #if CFG_TUD_CDC
  {
      .class_code       = TUSB_CLASS_CDC,
      .init             = cdcd_init,
      .reset            = cdcd_reset,
      .open             = cdcd_open,
      .control_request  = cdcd_control_request,
      .control_complete = cdcd_control_complete,
      .xfer_cb          = cdcd_xfer_cb,
      .sof              = NULL
  },
  #endif

  #if CFG_TUD_MSC
  {
      .class_code       = TUSB_CLASS_MSC,
      .init             = mscd_init,
      .reset            = mscd_reset,
      .open             = mscd_open,
      .control_request  = mscd_control_request,
      .control_complete = mscd_control_complete,
      .xfer_cb          = mscd_xfer_cb,
      .sof              = NULL
  },
  #endif

  #if CFG_TUD_HID
  {
      .class_code       = TUSB_CLASS_HID,
      .init             = hidd_init,
      .reset            = hidd_reset,
      .open             = hidd_open,
      .control_request  = hidd_control_request,
      .control_complete = hidd_control_complete,
      .xfer_cb          = hidd_xfer_cb,
      .sof              = NULL
  },
  #endif

  #if CFG_TUD_MIDI
  {
      .class_code       = TUSB_CLASS_AUDIO,
      .init             = midid_init,
      .open             = midid_open,
      .reset            = midid_reset,
      .control_request  = midid_control_request,
      .control_complete = midid_control_complete,
      .xfer_cb          = midid_xfer_cb,
      .sof              = NULL
  },
  #endif

  #if CFG_TUD_VENDOR
  {
      .class_code       = TUSB_CLASS_VENDOR_SPECIFIC,
      .init             = vendord_init,
      .reset            = vendord_reset,
      .open             = vendord_open,
      .control_request  = tud_vendor_control_request_cb,
      .control_complete = tud_vendor_control_complete_cb,
      .xfer_cb          = vendord_xfer_cb,
      .sof              = NULL
  },
  #endif
};

enum { USBD_CLASS_DRIVER_COUNT = TU_ARRAY_SIZE(usbd_class_drivers) };

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
static bool process_set_config(uint8_t rhport, uint8_t cfg_num);
static bool process_get_descriptor(uint8_t rhport, tusb_control_request_t const * p_request);

void usbd_control_reset (uint8_t rhport);
bool usbd_control_xfer_cb (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void usbd_control_set_complete_callback( bool (*fp) (uint8_t, tusb_control_request_t const * ) );

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+
bool tud_mounted(void)
{
  return (_usbd_dev.state_bits.state == STATE_CONFIGURED);
}

bool tud_suspended(void)
{
  return _usbd_dev.state_bits.suspended;
}

bool tud_remote_wakeup(void)
{
  // only wake up host if this feature is supported and enabled and we are suspended
  TU_VERIFY (
      _usbd_dev.state_bits.suspended &&
      _usbd_dev.state_bits.remote_wakeup_support &&
      _usbd_dev.state_bits.remote_wakeup_en );
  dcd_remote_wakeup(TUD_OPT_RHPORT);
  return true;
}

//--------------------------------------------------------------------+
// USBD Task
//--------------------------------------------------------------------+
bool usbd_init (void)
{
  tu_varclr(&_usbd_dev);

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

  memset(_usbd_dev.itf2drv, DRVID_INVALID, sizeof(_usbd_dev.itf2drv)); // invalid mapping
  memset(_usbd_dev.ep2drv , DRVID_INVALID, sizeof(_usbd_dev.ep2drv )); // invalid mapping

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
      case DCD_EVENT_BUS_RESET:
        usbd_reset(event.rhport);
      break;

      case DCD_EVENT_UNPLUGGED:
        usbd_reset(event.rhport);

        // invoke callback
        if (tud_umount_cb) tud_umount_cb();
      break;

      case DCD_EVENT_SETUP_RECEIVED:
        // Mark as connected after receiving 1st setup packet.
        // But it is easier to set it every time instead of wasting time to check then set
        if (_usbd_dev.state_bits.state == STATE_POWERED)
          _usbd_dev.state_bits.state = STATE_DEFAULT;

        // Process control request
        if ( !process_control_request(event.rhport, &event.setup_received) )
        {
          // Failed -> stall both control endpoint IN and OUT
          dcd_edpt_stall(event.rhport, 0);
          dcd_edpt_stall(event.rhport, 0 | TUSB_DIR_IN_MASK);
        }
      break;

      case DCD_EVENT_XFER_COMPLETE:
        // Only handle xfer callback in ready state
        // if (_usbd_dev.connected && !_usbd_dev.suspended)
        {
          // Invoke the class callback associated with the endpoint address
          uint8_t const ep_addr = event.xfer_complete.ep_addr;
          uint8_t const epnum   = tu_edpt_number(ep_addr);
          uint8_t const ep_dir  = tu_edpt_dir(ep_addr);

          _usbd_dev.ep_busy_map[ep_dir] = (uint8_t) tu_bit_clear(_usbd_dev.ep_busy_map[ep_dir], epnum);

          if ( 0 == epnum )
          {
            // control transfer DATA stage callback
            usbd_control_xfer_cb(event.rhport, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
          }
          else
          {
            uint8_t const drv_id = _usbd_dev.ep2drv[epnum][ep_dir];
            TU_ASSERT(drv_id < USBD_CLASS_DRIVER_COUNT,);

            usbd_class_drivers[drv_id].xfer_cb(event.rhport, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
          }
        }
      break;

      case DCD_EVENT_SUSPEND:
        if (tud_suspend_cb)
        {
          tud_suspend_cb(_usbd_dev.state_bits.remote_wakeup_en);
        }
      break;

      case DCD_EVENT_RESUME:
        if (tud_resume_cb) tud_resume_cb();
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

      case USBD_EVENT_FUNC_CALL:
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

  TU_ASSERT(p_request->bmRequestType_bit.type < TUSB_REQ_TYPE_INVALID);

  // Vendor request
  if ( p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR )
  {
    TU_VERIFY(tud_vendor_control_request_cb);

    if (tud_vendor_control_complete_cb) usbd_control_set_complete_callback(tud_vendor_control_complete_cb);
    return tud_vendor_control_request_cb(rhport, p_request);
  }

  switch ( p_request->bmRequestType_bit.recipient )
  {
    //------------- Device Requests e.g in enumeration -------------//
    case TUSB_REQ_RCPT_DEVICE:
      if ( TUSB_REQ_TYPE_STANDARD != p_request->bmRequestType_bit.type )
      {
        // Non standard request is not supported
        TU_BREAKPOINT();
        return false;
      }

      switch ( p_request->bRequest )
      {
        case TUSB_REQ_SET_ADDRESS:
          // Depending on mcu, status phase could be sent either before or after changing device address
          // Therefore DCD must include zero-length status response
          dcd_set_address(rhport, (uint8_t) p_request->wValue);
          return true; // skip status
        break;

        case TUSB_REQ_GET_CONFIGURATION:
        {
          uint8_t cfgnum = _usbd_dev.selected_config;
          if(_usbd_dev.state_bits.state != STATE_CONFIGURED)
            cfgnum = 0;
          tud_control_xfer(rhport, p_request, &cfgnum, 1);
        }
        break;

        case TUSB_REQ_SET_CONFIGURATION:
        {
          uint8_t const cfg_num = (uint8_t) p_request->wValue;

          dcd_set_config(rhport, cfg_num);

          if( cfg_num == 0)
          {
            _usbd_dev.state_bits.state = STATE_ADDRESS;
            if (tud_umount_cb)
            {
              tud_umount_cb();
            }
          }
          else
          {
            if(_usbd_dev.selected_config != 0)
            {
              TU_VERIFY(_usbd_dev.selected_config == cfg_num);
            }
            else
            {
              TU_VERIFY( process_set_config(rhport, cfg_num) );
              _usbd_dev.selected_config = cfg_num;
            }
            if (tud_mount_cb)
            {
              tud_mount_cb();
            }
            _usbd_dev.state_bits.state = STATE_CONFIGURED;
          }

          tud_control_status(rhport, p_request);
        }
        break;

        case TUSB_REQ_GET_DESCRIPTOR:
          TU_VERIFY( process_get_descriptor(rhport, p_request) );
        break;

        case TUSB_REQ_SET_FEATURE:
          // Only support remote wakeup for device feature
          TU_VERIFY(TUSB_REQ_FEATURE_REMOTE_WAKEUP == p_request->wValue);

          // Host may enable remote wake up before suspending especially HID device
          _usbd_dev.state_bits.remote_wakeup_en = true;
          tud_control_status(rhport, p_request);
        break;

        case TUSB_REQ_CLEAR_FEATURE:
          // Only support remote wakeup for device feature
          TU_VERIFY(TUSB_REQ_FEATURE_REMOTE_WAKEUP == p_request->wValue);

          // Host may disable remote wake up after resuming
          _usbd_dev.state_bits.remote_wakeup_en = false;
          tud_control_status(rhport, p_request);
        break;

        case TUSB_REQ_GET_STATUS:
        {
          // Device status bit mask
          // - Bit 0: Self Powered
          // - Bit 1: Remote Wakeup enabled
          uint16_t status = (uint16_t)(
                (_usbd_dev.state_bits.self_powered ? 1 : 0) | (_usbd_dev.state_bits.remote_wakeup_en ? 2 : 0)
              );
          tud_control_xfer(rhport, p_request, &status, 2);
        }
        break;

        // Unknown/Unsupported request
        default: TU_BREAKPOINT(); return false;
      }
    break;

    //------------- Class/Interface Specific Request -------------//
    case TUSB_REQ_RCPT_INTERFACE:
    {
      uint8_t const itf = tu_u16_low(p_request->wIndex);
      TU_VERIFY(itf < TU_ARRAY_SIZE(_usbd_dev.itf2drv));

      uint8_t const drvid = _usbd_dev.itf2drv[itf];
      TU_VERIFY(drvid < USBD_CLASS_DRIVER_COUNT);

      if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD)
      {
        switch ( p_request->bRequest )
        {
          case TUSB_REQ_GET_INTERFACE:
          {
            // TODO not support alternate interface yet
            uint8_t alternate = 0;
            tud_control_xfer(rhport, p_request, &alternate, 1);
          }
          break;

          case TUSB_REQ_SET_INTERFACE:
          {
            // USB 2.0: 9.4.10
            // TODO: not support alternate interface yet
            uint8_t const alternate = (uint8_t) p_request->wValue;
            // Must respond with error in request state (and optionally in default state)
            TU_VERIFY(_usbd_dev.state_bits.state == STATE_CONFIGURED);
            TU_VERIFY(alternate == 0);
            tud_control_status(rhport, p_request);
          }
          break;

          default:
            // forward to class driver: "STD request to Interface"
            // GET HID REPORT DESCRIPTOR falls into this case
            // stall control endpoint if driver return false
            usbd_control_set_complete_callback(usbd_class_drivers[drvid].control_complete);
            TU_VERIFY(usbd_class_drivers[drvid].control_request != NULL &&
                      usbd_class_drivers[drvid].control_request(rhport, p_request));
          break;
        }
      }
      else
      {
        // forward to class driver: "non-STD request to Interface"
        // stall control endpoint if driver return false
        usbd_control_set_complete_callback(usbd_class_drivers[drvid].control_complete);
        TU_ASSERT(usbd_class_drivers[drvid].control_request != NULL &&
                  usbd_class_drivers[drvid].control_request(rhport, p_request));
      }
    }
    break;

    //------------- Endpoint Request -------------//
    case TUSB_REQ_RCPT_ENDPOINT:
    {
      uint8_t const ep_addr = tu_u16_low(p_request->wIndex);
      uint8_t const ep_num  = tu_edpt_number(ep_addr);
      uint8_t const ep_dir  = tu_edpt_dir(ep_addr);

      TU_ASSERT(ep_num < TU_ARRAY_SIZE(_usbd_dev.ep2drv) );

      uint8_t const drv_id = _usbd_dev.ep2drv[ep_num][ep_dir];
      TU_ASSERT(drv_id < USBD_CLASS_DRIVER_COUNT);

      // Some classes such as TMC needs to clear/re-init its buffer when receiving CLEAR_FEATURE request
      // We will forward all request targeted endpoint to its class driver
      // - For non-standard request: driver can ACK or Stall the request by return true/false
      // - For standard request: usbd decide the ACK stage regardless of driver return value
      bool ret = false;

      if ( TUSB_REQ_TYPE_STANDARD != p_request->bmRequestType_bit.type )
      {
        // complete callback is also capable of stalling/acking the request
        usbd_control_set_complete_callback(usbd_class_drivers[drv_id].control_complete);
      }

      // Invoke class driver first if available
      if ( usbd_class_drivers[drv_id].control_request )
      {
        ret = usbd_class_drivers[drv_id].control_request(rhport, p_request);
      }

      // Then handle if it is standard request
      if ( TUSB_REQ_TYPE_STANDARD == p_request->bmRequestType_bit.type )
      {
        // force return true for standard request
        ret = true;

        switch ( p_request->bRequest )
        {
          case TUSB_REQ_GET_STATUS:
          {
            uint16_t status = usbd_edpt_stalled(rhport, ep_addr) ? 0x0001 : 0x0000;
            tud_control_xfer(rhport, p_request, &status, 2);
          }
          break;

          case TUSB_REQ_CLEAR_FEATURE:
            if ( TUSB_REQ_FEATURE_EDPT_HALT == p_request->wValue )
            {
              usbd_edpt_clear_stall(rhport, ep_addr);
            }
            tud_control_status(rhport, p_request);
          break;

          case TUSB_REQ_SET_FEATURE:
            if ( TUSB_REQ_FEATURE_EDPT_HALT == p_request->wValue )
            {
              usbd_edpt_stall(rhport, ep_addr);
            }
            tud_control_status(rhport, p_request);
          break;

          // Unknown/Unsupported request
          default: TU_BREAKPOINT(); return false;
        }
      }

      return ret;
    }
    break;

    // Unknown recipient
    default: TU_BREAKPOINT(); return false;
  }

  return true;
}

// Process Set Configure Request
// This function parse configuration descriptor & open drivers accordingly
// It assumes (config_index == (config_num - 1))
static bool process_set_config(uint8_t rhport, uint8_t cfg_num)
{
  TU_ASSERT(cfg_num != 0);
  tusb_desc_configuration_t const * desc_cfg =
      (tusb_desc_configuration_t const *) tud_descriptor_configuration_cb((uint8_t)(cfg_num-1)); // index is cfg_num-1
  TU_VERIFY(desc_cfg != NULL);
  TU_ASSERT(desc_cfg->bDescriptorType == TUSB_DESC_CONFIGURATION && desc_cfg->bConfigurationValue == cfg_num);

  // Parse configuration descriptor
  _usbd_dev.state_bits.remote_wakeup_support =
      (desc_cfg->bmAttributes & TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP) ? 1u : 0u;
  _usbd_dev.state_bits.self_powered =
      (desc_cfg->bmAttributes & TUSB_DESC_CONFIG_ATT_SELF_POWERED) ? 1u : 0u;

  // Parse interface descriptor
  uint8_t const * p_desc   = ((uint8_t const*) desc_cfg) + sizeof(tusb_desc_configuration_t);
  uint8_t const * desc_end = ((uint8_t const*) desc_cfg) + desc_cfg->wTotalLength;

  while( p_desc < desc_end )
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
      TU_ASSERT( DRVID_INVALID == _usbd_dev.itf2drv[desc_itf->bInterfaceNumber] );
      _usbd_dev.itf2drv[desc_itf->bInterfaceNumber] = drv_id;

      uint16_t itf_len=0;
      TU_ASSERT( usbd_class_drivers[drv_id].open(rhport, desc_itf, &itf_len) );
      TU_ASSERT( itf_len >= sizeof(tusb_desc_interface_t) );

      mark_interface_endpoint(_usbd_dev.ep2drv, p_desc, itf_len, drv_id);

      p_desc += itf_len; // next interface
    }
  }

  return true;
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

    len   = (uint16_t)(len + tu_desc_len(p_desc));
    p_desc = tu_desc_next(p_desc);
  }
}

// return descriptor's buffer and update desc_len
static bool process_get_descriptor(uint8_t rhport, tusb_control_request_t const * p_request)
{
  tusb_desc_type_t const desc_type = (tusb_desc_type_t) tu_u16_high(p_request->wValue);
  uint8_t const desc_index = tu_u16_low( p_request->wValue );

  switch(desc_type)
  {
    case TUSB_DESC_DEVICE:
      return tud_control_xfer(rhport, p_request, (void*) tud_descriptor_device_cb(), sizeof(tusb_desc_device_t));
    break;

    case TUSB_DESC_BOS:
    {
      // requested by host if USB > 2.0 ( i.e 2.1 or 3.x )
      if (!tud_descriptor_bos_cb) return false;

      tusb_desc_bos_t const* desc_bos = (tusb_desc_bos_t const*) tud_descriptor_bos_cb();
      uint16_t total_len;
      memcpy(&total_len, &desc_bos->wTotalLength, 2); // possibly mis-aligned memory

      return tud_control_xfer(rhport, p_request, (void*) desc_bos, total_len);
    }
    break;

    case TUSB_DESC_CONFIGURATION:
    {
      tusb_desc_configuration_t const* desc_config = (tusb_desc_configuration_t const*) tud_descriptor_configuration_cb(desc_index);
      uint16_t total_len;
      memcpy(&total_len, &desc_config->wTotalLength, 2); // possibly mis-aligned memory

      return tud_control_xfer(rhport, p_request, (void*) desc_config, total_len);
    }
    break;

    case TUSB_DESC_STRING:
      // String Descriptor always uses the desc set from user
      if ( desc_index == 0xEE )
      {
        // The 0xEE index string is a Microsoft OS Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
        return false;
      }else
      {
        uint8_t const* desc_str = (uint8_t const*) tud_descriptor_string_cb(desc_index);
        TU_ASSERT(desc_str);

        // first byte of descriptor is its size
        return tud_control_xfer(rhport, p_request, (void*) desc_str, desc_str[0]);
      }
    break;

    case TUSB_DESC_DEVICE_QUALIFIER:
      // TODO If not highspeed capable stall this request otherwise
      // return the descriptor that could work in highspeed
      return false;
    break;

    default: return false;
  }

  return true;
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
      _usbd_dev.state_bits.state = STATE_POWERED;
      _usbd_dev.selected_config = 0;
      _usbd_dev.state_bits.suspended = 0;
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_SOF:
      // nothing to do now
    break;

    case DCD_EVENT_SUSPEND:
      // NOTE: When plugging/unplugging device, the D+/D- state are unstable and can accidentally meet the
      // SUSPEND condition ( Idle for 3ms ). Some MCUs such as SAMD doesn't distinguish suspend vs disconnect as well.
      // We will skip handling SUSPEND/RESUME event if not currently connected
      if ( _usbd_dev.state_bits.state != STATE_POWERED )
      {
        _usbd_dev.state_bits.suspended = 1;
        osal_queue_send(_usbd_q, event, in_isr);
      }
    break;

    case DCD_EVENT_RESUME:
      if ( _usbd_dev.state_bits.state != STATE_POWERED )
      {
        _usbd_dev.state_bits.suspended = 0;
        osal_queue_send(_usbd_q, event, in_isr);
      }
    break;

    case DCD_EVENT_SETUP_RECEIVED:
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_XFER_COMPLETE:
      // skip zero-length control status complete event, should DCD notify us.
      // TODO could cause issue with actual zero length data used by class such as DFU
      if ( (0 == tu_edpt_number(event->xfer_complete.ep_addr)) && (event->xfer_complete.len == 0) ) break;

      osal_queue_send(_usbd_q, event, in_isr);
      TU_ASSERT(event->xfer_complete.result == XFER_RESULT_SUCCESS,);
    break;

    // Not an DCD event, just a convenient way to defer ISR function should we need to
    case USBD_EVENT_FUNC_CALL:
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

// Parse consecutive endpoint descriptors (IN & OUT)
// May be called with ep_count=1 to open a single endpoint
bool usbd_open_edpt_pair(uint8_t rhport, uint8_t const* p_desc, uint8_t ep_count, uint8_t xfer_type, uint8_t* ep_out, uint8_t* ep_in)
{
  for(int i=0; i<ep_count; i++)
  {
    tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;

    TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType && xfer_type == desc_ep->bmAttributes.xfer);
    TU_ASSERT(dcd_edpt_open(rhport, desc_ep));

    if ( tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN )
    {
      (*ep_in) = desc_ep->bEndpointAddress;
    }else
    {
      (*ep_out) = desc_ep->bEndpointAddress;
    }

    p_desc = tu_desc_next(p_desc);
  }

  return true;
}

// Helper to defer an isr function
void usbd_defer_func(osal_task_func_t func, void* param, bool in_isr)
{
  dcd_event_t event =
  {
      .rhport   = 0,
      .event_id = USBD_EVENT_FUNC_CALL,
  };

  event.func_call.func  = func;
  event.func_call.param = param;

  dcd_event_handler(&event, in_isr);
}

//--------------------------------------------------------------------+
// USBD Endpoint API
//--------------------------------------------------------------------+

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  TU_VERIFY( dcd_edpt_xfer(rhport, ep_addr, buffer, total_bytes) );

  _usbd_dev.ep_busy_map[dir] = (uint8_t) tu_bit_set(_usbd_dev.ep_busy_map[dir], epnum);

  return true;
}

bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  return tu_bit_test(_usbd_dev.ep_busy_map[dir], epnum);
}


void usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  dcd_edpt_stall(rhport, ep_addr);
  _usbd_dev.ep_stall_map[dir] = (uint8_t) tu_bit_set(_usbd_dev.ep_stall_map[dir], epnum);
}

void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  dcd_edpt_clear_stall(rhport, ep_addr);
  _usbd_dev.ep_stall_map[dir] = (uint8_t) tu_bit_clear(_usbd_dev.ep_stall_map[dir], epnum);
}

bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  return tu_bit_test(_usbd_dev.ep_stall_map[dir], epnum);
}

#endif
