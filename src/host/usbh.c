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

#include "common/tusb_common.h"

#if TUSB_OPT_HOST_ENABLED

#ifndef CFG_TUH_TASK_QUEUE_SZ
#define CFG_TUH_TASK_QUEUE_SZ   16
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "tusb.h"
#include "hub.h"
#include "usbh_hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= 2
  #define DRIVER_NAME(_name)    .name = _name,
#else
  #define DRIVER_NAME(_name)
#endif

static usbh_class_driver_t const usbh_class_drivers[] =
{
  #if CFG_TUH_CDC
    {
      DRIVER_NAME("CDC")
      .class_code = TUSB_CLASS_CDC,
      .init       = cdch_init,
      .open       = cdch_open,
      .set_config = cdch_set_config,
      .xfer_cb    = cdch_xfer_cb,
      .close      = cdch_close
    },
  #endif

  #if CFG_TUH_MSC
    {
      DRIVER_NAME("MSC")
      .class_code = TUSB_CLASS_MSC,
      .init       = msch_init,
      .open       = msch_open,
      .set_config = msch_set_config,
      .xfer_cb    = msch_xfer_cb,
      .close      = msch_close
    },
  #endif

  #if CFG_TUH_HID
    {
      DRIVER_NAME("HID")
      .class_code = TUSB_CLASS_HID,
      .init       = hidh_init,
      .open       = hidh_open,
      .set_config = hidh_set_config,
      .xfer_cb    = hidh_xfer_cb,
      .close      = hidh_close
    },
  #endif

  #if CFG_TUH_HUB
    {
      DRIVER_NAME("HUB")
      .class_code = TUSB_CLASS_HUB,
      .init       = hub_init,
      .open       = hub_open,
      .set_config = hub_set_config,
      .xfer_cb    = hub_xfer_cb,
      .close      = hub_close
    },
  #endif

  #if CFG_TUH_VENDOR
    {
      DRIVER_NAME("VENDOR")
      .class_code = TUSB_CLASS_VENDOR_SPECIFIC,
      .init       = cush_init,
      .open       = cush_open_subtask,
      .xfer_cb    = cush_isr,
      .close      = cush_close
    }
  #endif
};

enum { USBH_CLASS_DRIVER_COUNT = TU_ARRAY_SIZE(usbh_class_drivers) };

enum { RESET_DELAY = 500 };  // 200 USB specs say only 50ms but many devices require much longer

enum { CONFIG_NUM = 1 }; // default to use configuration 1


//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

static bool _usbh_initialized = false;

// including zero-address
CFG_TUSB_MEM_SECTION usbh_device_t _usbh_devices[CFG_TUSB_HOST_DEVICE_MAX+1];

// Event queue
// role device/host is used by OS NONE for mutex (disable usb isr)
OSAL_QUEUE_DEF(OPT_MODE_HOST, _usbh_qdef, CFG_TUH_TASK_QUEUE_SZ, hcd_event_t);
static osal_queue_t _usbh_q;

CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t _usbh_ctrl_buf[CFG_TUH_ENUMERATION_BUFSZIE];

//------------- Helper Function Prototypes -------------//
static bool enum_new_device(hcd_event_t* event);
static void process_device_unplugged(uint8_t rhport, uint8_t hub_addr, uint8_t hub_port);

// from usbh_control.c
extern bool usbh_control_xfer_cb (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

//--------------------------------------------------------------------+
// PUBLIC API (Parameter Verification is required)
//--------------------------------------------------------------------+
bool tuh_device_configured(uint8_t dev_addr)
{
  return _usbh_devices[dev_addr].configured;
}

tusb_speed_t tuh_device_get_speed (uint8_t const dev_addr)
{
  TU_ASSERT( dev_addr <= CFG_TUSB_HOST_DEVICE_MAX, TUSB_SPEED_INVALID);
  return (tusb_speed_t) _usbh_devices[dev_addr].speed;
}

#if CFG_TUSB_OS == OPT_OS_NONE
void osal_task_delay(uint32_t msec)
{
  (void) msec;

  const uint32_t start = hcd_frame_number(TUH_OPT_RHPORT);
  while ( ( hcd_frame_number(TUH_OPT_RHPORT) - start ) < msec ) {}
}
#endif

//--------------------------------------------------------------------+
// CLASS-USBD API (don't require to verify parameters)
//--------------------------------------------------------------------+

bool tuh_inited(void)
{
  return _usbh_initialized;
}

bool tuh_init(uint8_t rhport)
{
  // skip if already initialized
  if (_usbh_initialized) return _usbh_initialized;

  TU_LOG2("USBH init\r\n");

  tu_memclr(_usbh_devices, sizeof(usbh_device_t)*(CFG_TUSB_HOST_DEVICE_MAX+1));

  //------------- Enumeration & Reporter Task init -------------//
  _usbh_q = osal_queue_create( &_usbh_qdef );
  TU_ASSERT(_usbh_q != NULL);

  //------------- Semaphore, Mutex for Control Pipe -------------//
  for(uint8_t i=0; i<CFG_TUSB_HOST_DEVICE_MAX+1; i++) // including address zero
  {
    usbh_device_t * const dev = &_usbh_devices[i];

#if CFG_TUSB_OS != OPT_OS_NONE
    dev->mutex = osal_mutex_create(&dev->mutexdef);
    TU_ASSERT(dev->mutex);
#endif

    memset(dev->itf2drv, 0xff, sizeof(dev->itf2drv)); // invalid mapping
    memset(dev->ep2drv , 0xff, sizeof(dev->ep2drv )); // invalid mapping
  }

  // Class drivers init
  for (uint8_t drv_id = 0; drv_id < USBH_CLASS_DRIVER_COUNT; drv_id++)
  {
    TU_LOG2("%s init\r\n", usbh_class_drivers[drv_id].name);
    usbh_class_drivers[drv_id].init();
  }

  TU_ASSERT(hcd_init(rhport));
  hcd_int_enable(rhport);

  _usbh_initialized = true;
  return true;
}

/* USB Host Driver task
 * This top level thread manages all host controller event and delegates events to class-specific drivers.
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
        tuh_task(); // tinyusb host task
      }
    }
    @endcode
 */
void tuh_task(void)
{
  // Skip if stack is not initialized
  if ( !tusb_inited() ) return;

  // Loop until there is no more events in the queue
  while (1)
  {
    hcd_event_t event;
    if ( !osal_queue_receive(_usbh_q, &event) ) return;

    switch (event.event_id)
    {
      case HCD_EVENT_DEVICE_ATTACH:
        // TODO due to the shared _usbh_ctrl_buf, we must complete enumerating
        // one device before enumerating another one.
        TU_LOG2("USBH DEVICE ATTACH\r\n");
        enum_new_device(&event);
      break;

      case HCD_EVENT_DEVICE_REMOVE:
        TU_LOG2("USBH DEVICE REMOVED\r\n");
        process_device_unplugged(event.rhport, event.connection.hub_addr, event.connection.hub_port);

        #if CFG_TUH_HUB
        // TODO remove
        if ( event.connection.hub_addr != 0)
        {
          // done with hub, waiting for next data on status pipe
          (void) hub_status_pipe_queue( event.connection.hub_addr );
        }
        #endif
      break;

      case HCD_EVENT_XFER_COMPLETE:
      {
        usbh_device_t* dev = &_usbh_devices[event.dev_addr];
        uint8_t const ep_addr = event.xfer_complete.ep_addr;
        uint8_t const epnum   = tu_edpt_number(ep_addr);
        uint8_t const ep_dir  = tu_edpt_dir(ep_addr);

        TU_LOG2("on EP %02X with %u bytes\r\n", ep_addr, (unsigned int) event.xfer_complete.len);

        dev->ep_status[epnum][ep_dir].busy = false;
        dev->ep_status[epnum][ep_dir].claimed = 0;

        if ( 0 == epnum )
        {
          usbh_control_xfer_cb(event.dev_addr, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
        }else
        {
          uint8_t drv_id = dev->ep2drv[epnum][ep_dir];
          TU_ASSERT(drv_id < USBH_CLASS_DRIVER_COUNT, );

          TU_LOG2("%s xfer callback\r\n", usbh_class_drivers[drv_id].name);
          usbh_class_drivers[drv_id].xfer_cb(event.dev_addr, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
        }
      }
      break;

      case USBH_EVENT_FUNC_CALL:
        if ( event.func_call.func ) event.func_call.func(event.func_call.param);
      break;

      default: break;
    }
  }
}

//--------------------------------------------------------------------+
// USBH API For Class Driver
//--------------------------------------------------------------------+

uint8_t usbh_get_rhport(uint8_t dev_addr)
{
  return _usbh_devices[dev_addr].rhport;
}

uint8_t* usbh_get_enum_buf(void)
{
  return _usbh_ctrl_buf;
}

//------------- Endpoint API -------------//

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  usbh_device_t* dev = &_usbh_devices[dev_addr];

#if CFG_TUSB_OS != OPT_OS_NONE
  // pre-check to help reducing mutex lock
  TU_VERIFY((dev->ep_status[epnum][dir].busy == 0) && (dev->ep_status[epnum][dir].claimed == 0));
  osal_mutex_lock(dev->mutex, OSAL_TIMEOUT_WAIT_FOREVER);
#endif

  // can only claim the endpoint if it is not busy and not claimed yet.
  bool const ret = (dev->ep_status[epnum][dir].busy == 0) && (dev->ep_status[epnum][dir].claimed == 0);
  if (ret)
  {
    dev->ep_status[epnum][dir].claimed = 1;
  }

#if CFG_TUSB_OS != OPT_OS_NONE
  osal_mutex_unlock(dev->mutex);
#endif

  return ret;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  usbh_device_t* dev = &_usbh_devices[dev_addr];

#if CFG_TUSB_OS != OPT_OS_NONE
  osal_mutex_lock(dev->mutex, OSAL_TIMEOUT_WAIT_FOREVER);
#endif

  // can only release the endpoint if it is claimed and not busy
  bool const ret = (dev->ep_status[epnum][dir].busy == 0) && (dev->ep_status[epnum][dir].claimed == 1);
  if (ret)
  {
    dev->ep_status[epnum][dir].claimed = 0;
  }

#if CFG_TUSB_OS != OPT_OS_NONE
  osal_mutex_unlock(dev->mutex);
#endif

  return ret;
}

bool usbh_edpt_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  usbh_device_t* dev = &_usbh_devices[dev_addr];
  TU_LOG2("  Queue EP %02X with %u bytes ... OK\r\n", ep_addr, total_bytes);
  return hcd_edpt_xfer(dev->rhport, dev_addr, ep_addr, buffer, total_bytes);
}

bool usbh_edpt_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  TU_LOG2("Open EP Control with Size = %u\r\n", max_packet_size);

  tusb_desc_endpoint_t ep0_desc =
  {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0,
    .bmAttributes     = { .xfer = TUSB_XFER_CONTROL },
    .wMaxPacketSize   = { .size = max_packet_size },
    .bInterval        = 0
  };

  return hcd_edpt_open(_usbh_devices[dev_addr].rhport, dev_addr, &ep0_desc);
}

bool usbh_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * desc_ep)
{
  TU_LOG2("  Open EP %02X with Size = %u\r\n", desc_ep->bEndpointAddress, desc_ep->wMaxPacketSize.size);

  bool ret = hcd_edpt_open(rhport, dev_addr, desc_ep);

  if (ret)
  {
    usbh_device_t* dev = &_usbh_devices[dev_addr];

    // new endpoints belongs to latest interface (last valid value)
    // TODO FIXME not true with ISO
    uint8_t drvid = 0xff;
    for(uint8_t i=0; i < sizeof(dev->itf2drv); i++)
    {
      if ( dev->itf2drv[i] == 0xff ) break;
      drvid = dev->itf2drv[i];
    }
    TU_ASSERT(drvid < USBH_CLASS_DRIVER_COUNT);

    uint8_t const ep_addr = desc_ep->bEndpointAddress;
    dev->ep2drv[tu_edpt_number(ep_addr)][tu_edpt_dir(ep_addr)] = drvid;
  }

  return ret;
}

//--------------------------------------------------------------------+
// HCD Event Handler
//--------------------------------------------------------------------+

void hcd_event_handler(hcd_event_t const* event, bool in_isr)
{
  switch (event->event_id)
  {
    default:
      osal_queue_send(_usbh_q, event, in_isr);
    break;
  }
}

// interrupt caused by a TD (with IOC=1) in pipe of class class_code
void hcd_event_xfer_complete(uint8_t dev_addr, uint8_t ep_addr, uint32_t xferred_bytes, xfer_result_t result, bool in_isr)
{
  hcd_event_t event =
  {
    .rhport   = 0, // TODO correct rhport
    .event_id = HCD_EVENT_XFER_COMPLETE,
    .dev_addr = dev_addr,
    .xfer_complete =
    {
      .ep_addr = ep_addr,
      .result  = result,
      .len     = xferred_bytes
    }
  };

  hcd_event_handler(&event, in_isr);
}

void hcd_event_device_attach(uint8_t rhport, bool in_isr)
{
  hcd_event_t event =
  {
    .rhport   = rhport,
    .event_id = HCD_EVENT_DEVICE_ATTACH
  };

  event.connection.hub_addr = 0;
  event.connection.hub_port = 0;

  hcd_event_handler(&event, in_isr);
}

void hcd_event_device_remove(uint8_t hostid, bool in_isr)
{
  hcd_event_t event =
  {
    .rhport   = hostid,
    .event_id = HCD_EVENT_DEVICE_REMOVE
  };

  event.connection.hub_addr = 0;
  event.connection.hub_port = 0;

  hcd_event_handler(&event, in_isr);
}


// a device unplugged on hostid, hub_addr, hub_port
// return true if found and unmounted device, false if cannot find
void process_device_unplugged(uint8_t rhport, uint8_t hub_addr, uint8_t hub_port)
{
  //------------- find the all devices (star-network) under port that is unplugged -------------//
  for (uint8_t dev_addr = 0; dev_addr <= CFG_TUSB_HOST_DEVICE_MAX; dev_addr ++)
  {
    usbh_device_t* dev = &_usbh_devices[dev_addr];

    // TODO Hub multiple level
    if (dev->rhport == rhport   &&
        (hub_addr == 0 || dev->hub_addr == hub_addr) && // hub_addr == 0 & hub_port == 0 means roothub
        (hub_port == 0 || dev->hub_port == hub_port) &&
        dev->state    != TUSB_DEVICE_STATE_UNPLUG)
    {
      // Invoke callback before close driver
      if (tuh_umount_cb) tuh_umount_cb(dev_addr);

      // Close class driver
      for (uint8_t drv_id = 0; drv_id < USBH_CLASS_DRIVER_COUNT; drv_id++)
      {
        TU_LOG2("%s close\r\n", usbh_class_drivers[drv_id].name);
        usbh_class_drivers[drv_id].close(dev_addr);
      }

      memset(dev->itf2drv, 0xff, sizeof(dev->itf2drv)); // invalid mapping
      memset(dev->ep2drv , 0xff, sizeof(dev->ep2drv )); // invalid mapping

      hcd_device_close(rhport, dev_addr);

      dev->state = TUSB_DEVICE_STATE_UNPLUG;
    }
  }
}

//--------------------------------------------------------------------+
// INTERNAL HELPER
//--------------------------------------------------------------------+
static uint8_t get_new_address(void)
{
  for (uint8_t addr=1; addr <= CFG_TUSB_HOST_DEVICE_MAX; addr++)
  {
    if (_usbh_devices[addr].state == TUSB_DEVICE_STATE_UNPLUG) return addr;
  }
  return CFG_TUSB_HOST_DEVICE_MAX+1;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num)
{
  usbh_device_t* dev = &_usbh_devices[dev_addr];

  for(itf_num++; itf_num < sizeof(dev->itf2drv); itf_num++)
  {
    // continue with next valid interface
    uint8_t const drv_id = dev->itf2drv[itf_num];
    if (drv_id != 0xff)
    {
      usbh_class_driver_t const * driver = &usbh_class_drivers[drv_id];
      TU_LOG2("%s set config: itf = %u\r\n", driver->name, itf_num);
      driver->set_config(dev_addr, itf_num);
      break;
    }
  }

  // all interface are configured
  if (itf_num == sizeof(dev->itf2drv))
  {
    // Invoke callback if available
    if (tuh_mount_cb) tuh_mount_cb(dev_addr);
  }
}

//--------------------------------------------------------------------+
// Enumeration Process
// is a lengthy process with a seires of control transfer to configure
// newly attached device. Each step is handled by a function in this
// section
// TODO due to the shared _usbh_ctrl_buf, we must complete enumerating
// one device before enumerating another one.
//--------------------------------------------------------------------+

static bool enum_request_addr0_device_desc(void);
static bool enum_request_set_addr(void);

static bool enum_get_addr0_device_desc_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool enum_set_address_complete           (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool enum_get_device_desc_complete       (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool enum_get_9byte_config_desc_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool enum_get_config_desc_complete       (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool enum_set_config_complete            (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool parse_configuration_descriptor      (uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg);

static bool enum_hub_clear_reset0_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) dev_addr; (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);
  enum_request_addr0_device_desc();
  return true;
}

static bool enum_hub_clear_reset1_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) dev_addr; (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);
  usbh_device_t* dev0 = &_usbh_devices[0];

  enum_request_set_addr();

  // done with hub, waiting for next data on status pipe
  (void) hub_status_pipe_queue( dev0->hub_addr );

  return true;
}

static bool enum_hub_get_status1_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) dev_addr; (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);
  usbh_device_t* dev0 = &_usbh_devices[0];

  hub_port_status_response_t port_status;
  memcpy(&port_status, _usbh_ctrl_buf, sizeof(hub_port_status_response_t));

  // Acknowledge Port Reset Change if Reset Successful
  if (port_status.change.reset)
  {
    TU_ASSERT( hub_port_clear_feature(dev0->hub_addr, dev0->hub_port, HUB_FEATURE_PORT_RESET_CHANGE, enum_hub_clear_reset1_complete) );
  }

  return true;
}

static bool enum_hub_get_status0_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) dev_addr; (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);
  usbh_device_t* dev0 = &_usbh_devices[0];

  hub_port_status_response_t port_status;
  memcpy(&port_status, _usbh_ctrl_buf, sizeof(hub_port_status_response_t));

  if ( !port_status.status.connection )
  {
    // device unplugged while delaying, nothing else to do, queue hub status
    return hub_status_pipe_queue(dev_addr);
  }

  dev0->speed = (port_status.status.high_speed) ? TUSB_SPEED_HIGH :
                (port_status.status.low_speed ) ? TUSB_SPEED_LOW  : TUSB_SPEED_FULL;

  // Acknowledge Port Reset Change
  if (port_status.change.reset)
  {
    hub_port_clear_feature(dev0->hub_addr, dev0->hub_port, HUB_FEATURE_PORT_RESET_CHANGE, enum_hub_clear_reset0_complete);
  }

  return true;
}


static bool enum_request_set_addr(void)
{
  // Set Address
  uint8_t const new_addr = get_new_address();
  TU_ASSERT(new_addr <= CFG_TUSB_HOST_DEVICE_MAX); // TODO notify application we reach max devices

  TU_LOG2("Set Address = %d\r\n", new_addr);

  usbh_device_t* dev0    = &_usbh_devices[0];
  usbh_device_t* new_dev = &_usbh_devices[new_addr];

  new_dev->rhport          = dev0->rhport;
  new_dev->hub_addr        = dev0->hub_addr;
  new_dev->hub_port        = dev0->hub_port;
  new_dev->speed           = dev0->speed;
  new_dev->connected       = 1;
  new_dev->ep0_packet_size = ((tusb_desc_device_t*) _usbh_ctrl_buf)->bMaxPacketSize0;

  tusb_control_request_t const new_request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = TUSB_REQ_SET_ADDRESS,
    .wValue   = new_addr,
    .wIndex   = 0,
    .wLength  = 0
  };

  TU_ASSERT( tuh_control_xfer(0, &new_request, NULL, enum_set_address_complete) );

  return true;
}

static bool enum_new_device(hcd_event_t* event)
{
  usbh_device_t* dev0 = &_usbh_devices[0];
  dev0->rhport   = event->rhport; // TODO refractor integrate to device_pool
  dev0->hub_addr = event->connection.hub_addr;
  dev0->hub_port = event->connection.hub_port;
  dev0->state    = TUSB_DEVICE_STATE_UNPLUG;

  //------------- connected/disconnected directly with roothub -------------//
  if (dev0->hub_addr == 0)
  {
    // wait until device is stable TODO non blocking
    osal_task_delay(RESET_DELAY);

    // device unplugged while delaying
    if ( !hcd_port_connect_status(dev0->rhport) ) return true;

    dev0->speed = hcd_port_speed_get( dev0->rhport );

    enum_request_addr0_device_desc();
  }
#if CFG_TUH_HUB
  //------------- connected/disconnected via hub -------------//
  else
  {
    // wait until device is stable
    osal_task_delay(RESET_DELAY);
    TU_ASSERT( hub_port_get_status(dev0->hub_addr, dev0->hub_port, _usbh_ctrl_buf, enum_hub_get_status0_complete) );
  }
#endif // CFG_TUH_HUB

  return true;
}

static bool enum_request_addr0_device_desc(void)
{
  // TODO probably doesn't need to open/close each enumeration
  TU_ASSERT( usbh_edpt_control_open(0, 8) );

  //------------- Get first 8 bytes of device descriptor to get Control Endpoint Size -------------//
  TU_LOG2("Get 8 byte of Device Descriptor\r\n");
  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_IN
    },
    .bRequest = TUSB_REQ_GET_DESCRIPTOR,
    .wValue   = TUSB_DESC_DEVICE << 8,
    .wIndex   = 0,
    .wLength  = 8
  };
  TU_ASSERT( tuh_control_xfer(0, &request, _usbh_ctrl_buf, enum_get_addr0_device_desc_complete) );

  return true;
}

// After Get Device Descriptor of Address 0
static bool enum_get_addr0_device_desc_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;
  TU_ASSERT(0 == dev_addr);

  usbh_device_t* dev0 = &_usbh_devices[0];

  if (XFER_RESULT_SUCCESS != result)
  {
#if CFG_TUH_HUB
    // TODO remove, waiting for next data on status pipe
    if (dev0->hub_addr != 0) hub_status_pipe_queue(dev0->hub_addr);
#endif

    return false;
  }

  // Reset device again before Set Address
  TU_LOG2("Port reset \r\n");

  if (dev0->hub_addr == 0)
  {
    // connected directly to roothub
    hcd_port_reset( dev0->rhport ); // reset port after 8 byte descriptor
    osal_task_delay(RESET_DELAY);

    enum_request_set_addr();
  }
#if CFG_TUH_HUB
  else
  {
    // after RESET_DELAY the hub_port_reset() already complete
    TU_ASSERT( hub_port_reset(dev0->hub_addr, dev0->hub_port, NULL) );
    osal_task_delay(RESET_DELAY);

    tuh_task(); // FIXME temporarily to clean up port_reset control transfer

    TU_ASSERT( hub_port_get_status(dev0->hub_addr, dev0->hub_port, _usbh_ctrl_buf, enum_hub_get_status1_complete) );
  }
#endif

  return true;
}

// After SET_ADDRESS is complete
static bool enum_set_address_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_ASSERT(0 == dev_addr);
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  uint8_t const new_addr = (uint8_t const) request->wValue;

  usbh_device_t* new_dev = &_usbh_devices[new_addr];
  new_dev->addressed = 1;

  // TODO close device 0, may not be needed
  usbh_device_t* dev0 = &_usbh_devices[0];
  hcd_device_close(dev0->rhport, 0);
  dev0->state = TUSB_DEVICE_STATE_UNPLUG;

  // open control pipe for new address
  TU_ASSERT ( usbh_edpt_control_open(new_addr, new_dev->ep0_packet_size) );

  // Get full device descriptor
  TU_LOG2("Get Device Descriptor\r\n");
  tusb_control_request_t const new_request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_IN
    },
    .bRequest = TUSB_REQ_GET_DESCRIPTOR,
    .wValue   = TUSB_DESC_DEVICE << 8,
    .wIndex   = 0,
    .wLength  = sizeof(tusb_desc_device_t)
  };

  TU_ASSERT(tuh_control_xfer(new_addr, &new_request, _usbh_ctrl_buf, enum_get_device_desc_complete));

  return true;
}

static bool enum_get_device_desc_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  tusb_desc_device_t const * desc_device = (tusb_desc_device_t const*) _usbh_ctrl_buf;
  usbh_device_t* dev = &_usbh_devices[dev_addr];

  dev->vendor_id  = desc_device->idVendor;
  dev->product_id = desc_device->idProduct;

//  if (tuh_attach_cb) tuh_attach_cb((tusb_desc_device_t*) _usbh_ctrl_buf);

  TU_LOG2("Get 9 bytes of Configuration Descriptor\r\n");
  tusb_control_request_t const new_request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_IN
    },
    .bRequest = TUSB_REQ_GET_DESCRIPTOR,
    .wValue   = (TUSB_DESC_CONFIGURATION << 8) | (CONFIG_NUM - 1),
    .wIndex   = 0,
    .wLength  = 9
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &new_request, _usbh_ctrl_buf, enum_get_9byte_config_desc_complete) );

  return true;
}

static bool enum_get_9byte_config_desc_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  // TODO not enough buffer to hold configuration descriptor
  tusb_desc_configuration_t const * desc_config = (tusb_desc_configuration_t const*) _usbh_ctrl_buf;
  uint16_t total_len;

  // Use offsetof to avoid pointer to the odd/misaligned address
  memcpy(&total_len, (uint8_t*) desc_config + offsetof(tusb_desc_configuration_t, wTotalLength), 2);

  TU_ASSERT(total_len <= CFG_TUH_ENUMERATION_BUFSZIE);

  // Get full configuration descriptor
  TU_LOG2("Get Configuration Descriptor\r\n");
  tusb_control_request_t const new_request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_IN
    },
    .bRequest = TUSB_REQ_GET_DESCRIPTOR,
    .wValue   = (TUSB_DESC_CONFIGURATION << 8) | (CONFIG_NUM - 1),
    .wIndex   = 0,
    .wLength  = total_len

  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &new_request, _usbh_ctrl_buf, enum_get_config_desc_complete) );

  return true;
}

static bool enum_get_config_desc_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  // Parse configuration & set up drivers
  // Driver open aren't allowed to make any usb transfer yet
  parse_configuration_descriptor(dev_addr, (tusb_desc_configuration_t*) _usbh_ctrl_buf);

  TU_LOG2("Set Configuration = %d\r\n", CONFIG_NUM);
  tusb_control_request_t const new_request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = TUSB_REQ_SET_CONFIGURATION,
    .wValue   = CONFIG_NUM,
    .wIndex   = 0,
    .wLength  = 0
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &new_request, NULL, enum_set_config_complete) );

  return true;
}

static bool enum_set_config_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  TU_LOG2("Device configured\r\n");
  usbh_device_t* dev = &_usbh_devices[dev_addr];
  dev->configured = 1;
  dev->state = TUSB_DEVICE_STATE_CONFIGURED;

  // Start the Set Configuration process for interfaces (itf = 0xff)
  // Since driver can perform control transfer within its set_config, this is done asynchronously.
  // The process continue with next interface when class driver complete its sequence with usbh_driver_set_config_complete()
  // TODO use separated API instead of usig 0xff
  usbh_driver_set_config_complete(dev_addr, 0xff);

  return true;
}

static bool parse_configuration_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg)
{
  usbh_device_t* dev = &_usbh_devices[dev_addr];
  uint8_t const* p_desc = (uint8_t const*) desc_cfg;
  p_desc = tu_desc_next(p_desc);

  // parse each interfaces
  while( p_desc < _usbh_ctrl_buf + desc_cfg->wTotalLength )
  {
    // skip until we see interface descriptor
    if ( TUSB_DESC_INTERFACE != tu_desc_type(p_desc) )
    {
      p_desc = tu_desc_next(p_desc); // skip the descriptor, increase by the descriptor's length
    }else
    {
      tusb_desc_interface_t const* desc_itf = (tusb_desc_interface_t const*) p_desc;

      // Check if class is supported
      uint8_t drv_id;
      for (drv_id = 0; drv_id < USBH_CLASS_DRIVER_COUNT; drv_id++)
      {
        if ( usbh_class_drivers[drv_id].class_code == desc_itf->bInterfaceClass ) break;
      }

      if( drv_id >= USBH_CLASS_DRIVER_COUNT )
      {
        // skip unsupported class
        p_desc = tu_desc_next(p_desc);
      }
      else
      {
        usbh_class_driver_t const * driver = &usbh_class_drivers[drv_id];

        // Interface number must not be used already TODO alternate interface
        TU_ASSERT( dev->itf2drv[desc_itf->bInterfaceNumber] == 0xff );
        dev->itf2drv[desc_itf->bInterfaceNumber] = drv_id;

        if (desc_itf->bInterfaceClass == TUSB_CLASS_HUB && dev->hub_addr != 0)
        {
          // TODO Attach hub to Hub is not currently supported
          // skip this interface
          p_desc = tu_desc_next(p_desc);
        }
        else
        {
          TU_LOG2("%s open\r\n", driver->name);

          uint16_t itf_len = 0;
          TU_ASSERT( driver->open(dev->rhport, dev_addr, desc_itf, &itf_len) );
          TU_ASSERT( itf_len >= sizeof(tusb_desc_interface_t) );
          p_desc += itf_len;
        }
      }
    }
  }

  return true;
}

#endif
