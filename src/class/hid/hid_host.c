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

#include "host/usbh.h"
#include "host/usbh_classdriver.h"

#include "hid_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;

  uint8_t itf_protocol;   // None, Keyboard, Mouse
  uint8_t protocol_mode;  // Boot (0) or Report protocol (1)

  uint8_t  report_desc_type;
  uint16_t report_desc_len;

  uint16_t epin_size;
  uint16_t epout_size;

  uint8_t epin_buf[CFG_TUH_HID_EPIN_BUFSIZE];
  uint8_t epout_buf[CFG_TUH_HID_EPOUT_BUFSIZE];
} hidh_interface_t;

typedef struct
{
  uint8_t inst_count;
  hidh_interface_t instances[CFG_TUH_HID];
} hidh_device_t;

static hidh_device_t _hidh_dev[CFG_TUH_DEVICE_MAX];

//------------- Internal prototypes -------------//

// Get HID device & interface
TU_ATTR_ALWAYS_INLINE static inline hidh_device_t* get_dev(uint8_t dev_addr);
TU_ATTR_ALWAYS_INLINE static inline hidh_interface_t* get_instance(uint8_t dev_addr, uint8_t instance);
static uint8_t get_instance_id_by_itfnum(uint8_t dev_addr, uint8_t itf);
static uint8_t get_instance_id_by_epaddr(uint8_t dev_addr, uint8_t ep_addr);

//--------------------------------------------------------------------+
// Interface API
//--------------------------------------------------------------------+

uint8_t tuh_hid_instance_count(uint8_t dev_addr)
{
  return get_dev(dev_addr)->inst_count;
}

bool tuh_hid_mounted(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return (hid_itf->ep_in != 0) || (hid_itf->ep_out != 0);
}

uint8_t tuh_hid_interface_protocol(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return hid_itf->itf_protocol;
}

//--------------------------------------------------------------------+
// Control Endpoint API
//--------------------------------------------------------------------+

uint8_t tuh_hid_get_protocol(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return hid_itf->protocol_mode;
}

static bool set_protocol_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  uint8_t const itf_num     = (uint8_t) request->wIndex;
  uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  if (XFER_RESULT_SUCCESS == result) hid_itf->protocol_mode = (uint8_t) request->wValue;

  if (tuh_hid_set_protocol_complete_cb)
  {
    tuh_hid_set_protocol_complete_cb(dev_addr, instance, hid_itf->protocol_mode);
  }

  return true;
}

bool tuh_hid_set_protocol(uint8_t dev_addr, uint8_t instance, uint8_t protocol)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  TU_VERIFY(hid_itf->itf_protocol != HID_ITF_PROTOCOL_NONE);

  TU_LOG2("HID Set Protocol = %d\r\n", protocol);

  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HID_REQ_CONTROL_SET_PROTOCOL,
    .wValue   = protocol,
    .wIndex   = hid_itf->itf_num,
    .wLength  = 0
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, NULL, set_protocol_complete) );
  return true;
}

static bool set_report_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_LOG2("HID Set Report complete\r\n");

  if (tuh_hid_set_report_complete_cb)
  {
    uint8_t const itf_num     = (uint8_t) request->wIndex;
    uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);

    uint8_t const report_type = tu_u16_high(request->wValue);
    uint8_t const report_id   = tu_u16_low(request->wValue);

    tuh_hid_set_report_complete_cb(dev_addr, instance, report_id, report_type, (result == XFER_RESULT_SUCCESS) ? request->wLength : 0);
  }

  return true;
}

bool tuh_hid_set_report(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, void* report, uint16_t len)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  TU_LOG2("HID Set Report: id = %u, type = %u, len = %u\r\n", report_id, report_type, len);

  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HID_REQ_CONTROL_SET_REPORT,
    .wValue   = tu_u16(report_type, report_id),
    .wIndex   = hid_itf->itf_num,
    .wLength  = len
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, report, set_report_complete) );
  return true;
}

//--------------------------------------------------------------------+
// Interrupt Endpoint API
//--------------------------------------------------------------------+

bool tuh_hid_receive_report(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // claim endpoint
  TU_VERIFY( usbh_edpt_claim(dev_addr, hid_itf->ep_in) );

  return usbh_edpt_xfer(dev_addr, hid_itf->ep_in, hid_itf->epin_buf, hid_itf->epin_size);
}

//bool tuh_n_hid_n_ready(uint8_t dev_addr, uint8_t instance)
//{
//  TU_VERIFY(tuh_n_hid_n_mounted(dev_addr, instance));
//
//  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
//  return !usbh_edpt_busy(dev_addr, hid_itf->ep_in);
//}

//void tuh_hid_send_report(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t const* report, uint16_t len);

//--------------------------------------------------------------------+
// USBH API
//--------------------------------------------------------------------+
void hidh_init(void)
{
  tu_memclr(_hidh_dev, sizeof(_hidh_dev));
}

bool hidh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;

  uint8_t const dir = tu_edpt_dir(ep_addr);
  uint8_t const instance = get_instance_id_by_epaddr(dev_addr, ep_addr);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  if ( dir == TUSB_DIR_IN )
  {
    TU_LOG2("  Get Report callback (%u, %u)\r\n", dev_addr, instance);
    TU_LOG3_MEM(hid_itf->epin_buf, xferred_bytes, 2);
    tuh_hid_report_received_cb(dev_addr, instance, hid_itf->epin_buf, xferred_bytes);
  }else
  {
    if (tuh_hid_report_sent_cb) tuh_hid_report_sent_cb(dev_addr, instance, hid_itf->epout_buf, xferred_bytes);
  }

  return true;
}

void hidh_close(uint8_t dev_addr)
{
  TU_VERIFY(dev_addr <= CFG_TUH_DEVICE_MAX, );

  hidh_device_t* hid_dev = get_dev(dev_addr);

  if (tuh_hid_umount_cb)
  {
    for (uint8_t inst = 0; inst < hid_dev->inst_count; inst++ ) tuh_hid_umount_cb(dev_addr, inst);
  }

  tu_memclr(hid_dev, sizeof(hidh_device_t));
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+

static bool config_set_protocol             (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool config_get_report_desc          (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool config_get_report_desc_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);

static void config_driver_mount_complete(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);

bool hidh_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
  (void) max_len;

  TU_VERIFY(TUSB_CLASS_HID == desc_itf->bInterfaceClass);

  TU_LOG2("HID opening Interface %u (addr = %u)\r\n", desc_itf->bInterfaceNumber, dev_addr);

  // len = interface + hid + n*endpoints
  uint16_t const drv_len = sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + desc_itf->bNumEndpoints*sizeof(tusb_desc_endpoint_t);
  TU_ASSERT(max_len >= drv_len);

  uint8_t const *p_desc = (uint8_t const *) desc_itf;

  //------------- HID descriptor -------------//
  p_desc = tu_desc_next(p_desc);
  tusb_hid_descriptor_hid_t const *desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  TU_ASSERT(HID_DESC_TYPE_HID == desc_hid->bDescriptorType);

  // not enough interface, try to increase CFG_TUH_HID
  // TODO multiple devices
  hidh_device_t* hid_dev = get_dev(dev_addr);
  TU_ASSERT(hid_dev->inst_count < CFG_TUH_HID, 0);

  //------------- Endpoint Descriptor -------------//
  p_desc = tu_desc_next(p_desc);
  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;
  TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType);

  // first endpoint may be OUT, skip to IN endpoint
  // TODO also open endpoint OUT
  if(tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_OUT)
  {
    p_desc = tu_desc_next(p_desc);
    desc_ep = (tusb_desc_endpoint_t const *) p_desc;
    TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType);
  }

  TU_ASSERT( usbh_edpt_open(rhport, dev_addr, desc_ep) );

  hidh_interface_t* hid_itf = get_instance(dev_addr, hid_dev->inst_count);
  hid_dev->inst_count++;

  hid_itf->itf_num   = desc_itf->bInterfaceNumber;
  hid_itf->ep_in     = desc_ep->bEndpointAddress;
  hid_itf->epin_size = tu_edpt_packet_size(desc_ep);

  // Assume bNumDescriptors = 1
  hid_itf->report_desc_type = desc_hid->bReportType;
  hid_itf->report_desc_len  = tu_unaligned_read16(&desc_hid->wReportLength);

  // Per HID Specs: default is Report protocol, though we will force Boot protocol when set_config
  hid_itf->protocol_mode = HID_PROTOCOL_BOOT;
  if ( HID_SUBCLASS_BOOT == desc_itf->bInterfaceSubClass ) hid_itf->itf_protocol = desc_itf->bInterfaceProtocol;

  return true;
}

bool hidh_set_config(uint8_t dev_addr, uint8_t itf_num)
{
  uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // Idle rate = 0 mean only report when there is changes
  uint16_t const idle_rate = 0;

  // SET IDLE request, device can stall if not support this request
  TU_LOG2("HID Set Idle \r\n");
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

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, NULL, (hid_itf->itf_protocol != HID_ITF_PROTOCOL_NONE) ? config_set_protocol : config_get_report_desc) );

  return true;
}

// Force device to work in BOOT protocol
static bool config_set_protocol(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  // Stall is a valid response for SET_IDLE, therefore we could ignore its result
  (void) result;

  uint8_t const itf_num     = (uint8_t) request->wIndex;
  uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  TU_LOG2("HID Set Protocol to Boot Mode\r\n");
  hid_itf->protocol_mode = HID_PROTOCOL_BOOT;
  tusb_control_request_t const new_request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HID_REQ_CONTROL_SET_PROTOCOL,
    .wValue   = HID_PROTOCOL_BOOT,
    .wIndex   = hid_itf->itf_num,
    .wLength  = 0
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &new_request, NULL, config_get_report_desc) );
  return true;
}

static bool config_get_report_desc(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  // We can be here after SET_IDLE or SET_PROTOCOL (boot device)
  // Trigger assert if result is not successful with set protocol
  if ( request->bRequest != HID_REQ_CONTROL_SET_IDLE )
  {
    TU_ASSERT(result == XFER_RESULT_SUCCESS);
  }

  uint8_t const itf_num     = (uint8_t) request->wIndex;
  uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // Get Report Descriptor if possible
  // using usbh enumeration buffer since report descriptor can be very long
  if( hid_itf->report_desc_len > CFG_TUH_ENUMERATION_BUFSIZE )
  {
    TU_LOG2("HID Skip Report Descriptor since it is too large %u bytes\r\n", hid_itf->report_desc_len);

    // Driver is mounted without report descriptor
    config_driver_mount_complete(dev_addr, instance, NULL, 0);
  }else
  {
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
  }

  return true;
}

static bool config_get_report_desc_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  uint8_t const itf_num      = (uint8_t) request->wIndex;
  uint8_t const instance     = get_instance_id_by_itfnum(dev_addr, itf_num);

  uint8_t const* desc_report = usbh_get_enum_buf();
  uint16_t const desc_len    = request->wLength;

  config_driver_mount_complete(dev_addr, instance, desc_report, desc_len);

  return true;
}

static void config_driver_mount_complete(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // enumeration is complete
  tuh_hid_mount_cb(dev_addr, instance, desc_report, desc_len);

  // notify usbh that driver enumeration is complete
  usbh_driver_set_config_complete(dev_addr, hid_itf->itf_num);
}

//--------------------------------------------------------------------+
// Report Descriptor Parser
//--------------------------------------------------------------------+

uint8_t tuh_hid_parse_report_descriptor(tuh_hid_report_info_t* report_info_arr, uint8_t arr_count, uint8_t const* desc_report, uint16_t desc_len)
{
  // Report Item 6.2.2.2 USB HID 1.11
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

  tu_memclr(report_info_arr, arr_count*sizeof(tuh_hid_report_info_t));

  uint8_t report_num = 0;
  tuh_hid_report_info_t* info = report_info_arr;

  // current parsed report count & size from descriptor
//  uint8_t ri_report_count = 0;
//  uint8_t ri_report_size = 0;

  uint8_t ri_collection_depth = 0;

  while(desc_len && report_num < arr_count)
  {
    header.byte = *desc_report++;
    desc_len--;

    uint8_t const tag  = header.tag;
    uint8_t const type = header.type;
    uint8_t const size = header.size;

    uint8_t const data8 = desc_report[0];

    TU_LOG(3, "tag = %d, type = %d, size = %d, data = ", tag, type, size);
    for(uint32_t i=0; i<size; i++) TU_LOG(3, "%02X ", desc_report[i]);
    TU_LOG(3, "\r\n");

    switch(type)
    {
      case RI_TYPE_MAIN:
        switch (tag)
        {
          case RI_MAIN_INPUT: break;
          case RI_MAIN_OUTPUT: break;
          case RI_MAIN_FEATURE: break;

          case RI_MAIN_COLLECTION:
            ri_collection_depth++;
          break;

          case RI_MAIN_COLLECTION_END:
            ri_collection_depth--;
            if (ri_collection_depth == 0)
            {
              info++;
              report_num++;
            }
          break;

          default: break;
        }
      break;

      case RI_TYPE_GLOBAL:
        switch(tag)
        {
          case RI_GLOBAL_USAGE_PAGE:
            // only take in account the "usage page" before REPORT ID
            if ( ri_collection_depth == 0 ) memcpy(&info->usage_page, desc_report, size);
          break;

          case RI_GLOBAL_LOGICAL_MIN   : break;
          case RI_GLOBAL_LOGICAL_MAX   : break;
          case RI_GLOBAL_PHYSICAL_MIN  : break;
          case RI_GLOBAL_PHYSICAL_MAX  : break;

          case RI_GLOBAL_REPORT_ID:
            info->report_id = data8;
          break;

          case RI_GLOBAL_REPORT_SIZE:
//            ri_report_size = data8;
          break;

          case RI_GLOBAL_REPORT_COUNT:
//            ri_report_count = data8;
          break;

          case RI_GLOBAL_UNIT_EXPONENT : break;
          case RI_GLOBAL_UNIT          : break;
          case RI_GLOBAL_PUSH          : break;
          case RI_GLOBAL_POP           : break;

          default: break;
        }
      break;

      case RI_TYPE_LOCAL:
        switch(tag)
        {
          case RI_LOCAL_USAGE:
            // only take in account the "usage" before starting REPORT ID
            if ( ri_collection_depth == 0 ) info->usage = data8;
          break;

          case RI_LOCAL_USAGE_MIN        : break;
          case RI_LOCAL_USAGE_MAX        : break;
          case RI_LOCAL_DESIGNATOR_INDEX : break;
          case RI_LOCAL_DESIGNATOR_MIN   : break;
          case RI_LOCAL_DESIGNATOR_MAX   : break;
          case RI_LOCAL_STRING_INDEX     : break;
          case RI_LOCAL_STRING_MIN       : break;
          case RI_LOCAL_STRING_MAX       : break;
          case RI_LOCAL_DELIMITER        : break;
          default: break;
        }
      break;

      // error
      default: break;
    }

    desc_report += size;
    desc_len    -= size;
  }

  for ( uint8_t i = 0; i < report_num; i++ )
  {
    info = report_info_arr+i;
    TU_LOG2("%u: id = %u, usage_page = %u, usage = %u\r\n", i, info->report_id, info->usage_page, info->usage);
  }

  return report_num;
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
