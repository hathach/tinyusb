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

#if (TUSB_OPT_HOST_ENABLED && CFG_TUH_HID)

#include "common/tusb_common.h"
#include "hid_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

/*
 "KEYBOARD"               : in_len=8 , out_len=1, usage_page=0x01, usage=0x06   # Generic Desktop, Keyboard
 "MOUSE"                  : in_len=4 , out_len=0, usage_page=0x01, usage=0x02   # Generic Desktop, Mouse
 "CONSUMER"               : in_len=2 , out_len=0, usage_page=0x0C, usage=0x01   # Consumer, Consumer Control
 "SYS_CONTROL"            : in_len=1 , out_len=0, usage_page=0x01, usage=0x80   # Generic Desktop, Sys Control
 "GAMEPAD"                : in_len=6 , out_len=0, usage_page=0x01, usage=0x05   # Generic Desktop, Game Pad
 "DIGITIZER"              : in_len=5 , out_len=0, usage_page=0x0D, usage=0x02   # Digitizers, Pen
 "XAC_COMPATIBLE_GAMEPAD" : in_len=3 , out_len=0, usage_page=0x01, usage=0x05   # Generic Desktop, Game Pad
 "RAW"                    : in_len=64, out_len=0, usage_page=0xFFAF, usage=0xAF # Vendor 0xFFAF "Adafruit", 0xAF
 */
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;

  uint8_t  report_desc_type;
  uint16_t report_desc_len;
  uint16_t ep_size;

  uint8_t boot_protocol; // None, Keyboard, Mouse
  bool    boot_mode;     // Boot or Report protocol

  tuh_hid_report_info_t report_info;

  // Parsed Report ID for convenient API
  uint8_t rid_keyboard;
  uint8_t rid_mouse;
  uint8_t rid_gamepad;
  uint8_t rid_consumer;
} hidh_interface_t;

typedef struct
{
  uint8_t inst_count;
  hidh_interface_t instances[CFG_TUH_HID];
} hidh_device_t;

static hidh_device_t _hidh_dev[CFG_TUSB_HOST_DEVICE_MAX-1];

//------------- Internal prototypes -------------//
TU_ATTR_ALWAYS_INLINE static inline hidh_device_t* get_dev(uint8_t dev_addr);
TU_ATTR_ALWAYS_INLINE static inline hidh_interface_t* get_instance(uint8_t dev_addr, uint8_t instance);
static uint8_t get_instance_id_by_itfnum(uint8_t dev_addr, uint8_t itf);
static hidh_interface_t* get_instance_by_itfnum(uint8_t dev_addr, uint8_t itf);
static uint8_t get_instance_id_by_epaddr(uint8_t dev_addr, uint8_t ep_addr);

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

uint8_t tuh_n_hid_instance_count(uint8_t dev_addr)
{
  return get_dev(dev_addr)->inst_count;
}

bool tuh_n_hid_n_mounted(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return (hid_itf->ep_in != 0) || (hid_itf->ep_out != 0);
}

uint8_t tuh_n_hid_n_boot_protocol(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return hid_itf->boot_protocol;
}

bool tuh_n_hid_n_boot_mode(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return hid_itf->boot_mode;
}

tuh_hid_report_info_t const* tuh_n_hid_n_get_report_info(uint8_t dev_addr, uint8_t instance)
{
  return &get_instance(dev_addr, instance)->report_info;
}

bool tuh_n_hid_n_ready(uint8_t dev_addr, uint8_t instance)
{
  TU_VERIFY(tuh_n_hid_n_mounted(dev_addr, instance));

  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return !hcd_edpt_busy(dev_addr, hid_itf->ep_in);
}

bool tuh_n_hid_n_get_report(uint8_t dev_addr, uint8_t instance, void* report, uint16_t len)
{
  TU_VERIFY( tuh_device_ready(dev_addr) && report && len);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // TODO change to claim endpoint
  TU_VERIFY( !hcd_edpt_busy(dev_addr, hid_itf->ep_in) );

  len = tu_min16(len, hid_itf->ep_size);

  return usbh_edpt_xfer(dev_addr, hid_itf->ep_in, report, len);
}

//--------------------------------------------------------------------+
// KEYBOARD
//--------------------------------------------------------------------+

bool tuh_n_hid_n_keyboard_mounted(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // TODO check rid_keyboard
  return tuh_device_ready(dev_addr) && (hid_itf->ep_in != 0);
}

//--------------------------------------------------------------------+
// MOUSE
//--------------------------------------------------------------------+

// TODO remove
static hidh_interface_t mouseh_data[CFG_TUSB_HOST_DEVICE_MAX]; // does not have addr0, index = dev_address-1

//------------- Public API -------------//
bool tuh_n_hid_n_mouse_mounted(uint8_t dev_addr, uint8_t instance)
{
//  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return tuh_device_ready(dev_addr) && (mouseh_data[dev_addr-1].ep_in != 0);
}

//--------------------------------------------------------------------+
// USBH API
//--------------------------------------------------------------------+
void hidh_init(void)
{
  tu_memclr(_hidh_dev, sizeof(_hidh_dev));
}

bool hidh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  uint8_t const dir = tu_edpt_dir(ep_addr);
  uint8_t const instance = get_instance_id_by_epaddr(dev_addr, ep_addr);

  if ( dir == TUSB_DIR_IN )
  {
    if (tuh_hid_get_report_complete_cb) tuh_hid_get_report_complete_cb(dev_addr, instance, xferred_bytes);
  }else
  {
    if (tuh_hid_set_report_complete_cb) tuh_hid_set_report_complete_cb(dev_addr, instance, xferred_bytes);
  }

  return true;
}

void hidh_close(uint8_t dev_addr)
{
  hidh_device_t* hid_dev = get_dev(dev_addr);
  if (tuh_hid_unmounted_cb)
  {
    for ( uint8_t inst = 0; inst < hid_dev->inst_count; inst++) tuh_hid_unmounted_cb(dev_addr, inst);
  }

  tu_memclr(hid_dev, sizeof(hidh_device_t));
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+

static void parse_report_descriptor(hidh_interface_t* hid_itf, uint8_t const* desc_report, uint16_t desc_len);
static bool config_set_idle_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool config_get_report_desc_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);

bool hidh_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t *p_length)
{
  TU_VERIFY(TUSB_CLASS_HID == desc_itf->bInterfaceClass);

  uint8_t const *p_desc = (uint8_t const *) desc_itf;

  //------------- HID descriptor -------------//
  p_desc = tu_desc_next(p_desc);
  tusb_hid_descriptor_hid_t const *desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  TU_ASSERT(HID_DESC_TYPE_HID == desc_hid->bDescriptorType);

  // not enough interface, try to increase CFG_TUH_HID
  // TODO multiple devices
  hidh_device_t* hid_dev = get_dev(dev_addr);
  TU_ASSERT(hid_dev->inst_count < CFG_TUH_HID);

  //------------- Endpoint Descriptor -------------//
  p_desc = tu_desc_next(p_desc);
  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;
  TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType);

  // TODO also open endpoint OUT
  TU_ASSERT( usbh_edpt_open(rhport, dev_addr, desc_ep) );

  hidh_interface_t* hid_itf = get_instance(dev_addr, hid_dev->inst_count);
  hid_dev->inst_count++;

  hid_itf->itf_num = desc_itf->bInterfaceNumber;
  hid_itf->ep_in   = desc_ep->bEndpointAddress;
  hid_itf->ep_size = desc_ep->wMaxPacketSize.size;

  // Assume bNumDescriptors = 1
  hid_itf->report_desc_type = desc_hid->bReportType;
  hid_itf->report_desc_len = tu_unaligned_read16(&desc_hid->wReportLength);

  hid_itf->boot_mode = false; // default is report mode
  if ( HID_SUBCLASS_BOOT == desc_itf->bInterfaceSubClass )
  {
    hid_itf->boot_protocol = desc_itf->bInterfaceProtocol;

    if ( HID_PROTOCOL_KEYBOARD == desc_itf->bInterfaceProtocol)
    {
      TU_LOG2("  Boot Keyboard\r\n");
      // TODO boot protocol may still have more report in report mode
      hid_itf->report_info.count = 1;

      hid_itf->report_info.info[0].usage_page = HID_USAGE_PAGE_DESKTOP;
      hid_itf->report_info.info[0].usage = HID_USAGE_DESKTOP_KEYBOARD;
      hid_itf->report_info.info[0].in_len = 8;
      hid_itf->report_info.info[0].out_len = 1;
    }
    else if ( HID_PROTOCOL_MOUSE == desc_itf->bInterfaceProtocol)
    {
      TU_LOG2("  Boot Mouse\r\n");
      // TODO boot protocol may still have more report in report mode
      hid_itf->report_info.count = 1;

      hid_itf->report_info.info[0].usage_page = HID_USAGE_PAGE_DESKTOP;
      hid_itf->report_info.info[0].usage = HID_USAGE_DESKTOP_MOUSE;
      hid_itf->report_info.info[0].in_len = 5;
      hid_itf->report_info.info[0].out_len = 0;
    }
    else
    {
      // Unknown protocol
      TU_ASSERT(false);
    }
  }

  *p_length = sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + desc_itf->bNumEndpoints*sizeof(tusb_desc_endpoint_t);

  return true;
}

bool hidh_set_config(uint8_t dev_addr, uint8_t itf_num)
{
  // Idle rate = 0 mean only report when there is changes
  uint16_t const idle_rate = 0;

  // SET IDLE request, device can stall if not support this request
  TU_LOG2("Set Idle \r\n");
  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HID_REQ_CONTROL_SET_IDLE,
    .wValue   = idle_rate,
    .wIndex   = itf_num,
    .wLength  = 0
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, NULL, config_set_idle_complete) );

  return true;
}

bool config_set_idle_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  // Stall is a valid response for SET_IDLE, therefore we could ignore its result
  (void) result;

  uint8_t const itf_num = (uint8_t) request->wIndex;

  hidh_interface_t* hid_itf = get_instance_by_itfnum(dev_addr, itf_num);

  // Get Report Descriptor
  // using usbh enumeration buffer since report descriptor can be very long
  TU_ASSERT( hid_itf->report_desc_len <= CFG_TUH_ENUMERATION_BUFSZIE );

  TU_LOG2("HID Get Report Descriptor\r\n");
  tusb_control_request_t const new_request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_IN
    },
    .bRequest = TUSB_REQ_GET_DESCRIPTOR,
    .wValue   = tu_u16(hid_itf->report_desc_type, 0),
    .wIndex   = itf_num,
    .wLength  = hid_itf->report_desc_len
  };

  TU_ASSERT(tuh_control_xfer(dev_addr, &new_request, usbh_get_enum_buf(), config_get_report_desc_complete));
  return true;
}

bool config_get_report_desc_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  uint8_t const itf_num     = (uint8_t) request->wIndex;
  uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  uint8_t const* desc_report = usbh_get_enum_buf();
  uint16_t const desc_len    = request->wLength;

  if (tuh_hid_descriptor_report_cb)
  {
    tuh_hid_descriptor_report_cb(dev_addr, instance, desc_report, desc_len);
  }

  parse_report_descriptor(hid_itf, desc_report, desc_len);

  // enumeration is complete
  if (tuh_hid_mounted_cb) tuh_hid_mounted_cb(dev_addr, instance);

  // notify usbh that driver enumeration is complete
  usbh_driver_set_config_complete(dev_addr, itf_num);

  return true;
}

// Parse Report Descriptor to tuh_hid_report_info_t
static void parse_report_descriptor(hidh_interface_t* hid_itf, uint8_t const* desc_report, uint16_t desc_len)
{
  enum
  {
    USAGE_PAGE = 0x05,
    USAGE = 0x09,
    USAGE_MIN = 0x19,
    USAGE_MAX = 0x29,
    LOGICAL_MIN = 0x15,
    LOGICAL_MAX = 0x25,
    REPORT_SIZE = 0x75,
    REPORT_COUNT = 0x95
  };

  // Short Item 6.2.2.2 USB HID 1.11
  union TU_ATTR_PACKED
  {
    uint8_t byte;
    struct TU_ATTR_PACKED
    {
        uint8_t size : 2;
        uint8_t type : 2;
        uint8_t tag  : 4;
    };
  } header;

  while(desc_len)
  {
    header.byte = *desc_report++;

    uint8_t const tag = header.tag;
    uint8_t const type = header.type;
    uint8_t const size = header.size;

    desc_len--;

    TU_LOG2("tag = %d, type = %d, size = %d, data = ", tag, type, size);
    for(uint32_t i=0; i<size; i++) TU_LOG2("%02X ", desc_report[i]);
    TU_LOG2("\r\n");

    switch(type)
    {
      case RI_TYPE_MAIN:
      break;

      case RI_TYPE_GLOBAL:
      break;

      case RI_TYPE_LOCAL:
      break;

      // error
      default: break;
    }

    desc_report += size;
    desc_len -= size;
  }
}

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+

// Get Device by address
TU_ATTR_ALWAYS_INLINE static inline hidh_device_t* get_dev(uint8_t dev_addr)
{
  return &_hidh_dev[dev_addr-1];
}

// Get Interface by instance number
TU_ATTR_ALWAYS_INLINE static inline hidh_interface_t* get_instance(uint8_t dev_addr, uint8_t instance)
{
  return &_hidh_dev[dev_addr-1].instances[instance];
}

// Get instance by interface number
static hidh_interface_t* get_instance_by_itfnum(uint8_t dev_addr, uint8_t itf)
{
  for ( uint8_t inst = 0; inst < CFG_TUH_HID; inst++ )
  {
    hidh_interface_t *hid = get_instance(dev_addr, inst);

    if ( (hid->itf_num == itf) && (hid->ep_in || hid->ep_out) ) return hid;
  }

  return NULL;
}

// Get instance ID by interface number
static uint8_t get_instance_id_by_itfnum(uint8_t dev_addr, uint8_t itf)
{
  for ( uint8_t inst = 0; inst < CFG_TUH_HID; inst++ )
  {
    hidh_interface_t *hid = get_instance(dev_addr, inst);

    if ( (hid->itf_num == itf) && (hid->ep_in || hid->ep_out) ) return inst;
  }

  return 0xff;
}

// Get instance ID by endpoint address
static uint8_t get_instance_id_by_epaddr(uint8_t dev_addr, uint8_t ep_addr)
{
  for ( uint8_t inst = 0; inst < CFG_TUH_HID; inst++ )
  {
    hidh_interface_t *hid = get_instance(dev_addr, inst);

    if ( (ep_addr == hid->ep_in) || ( ep_addr == hid->ep_out) ) return inst;
  }

  return 0xff;
}

#endif
