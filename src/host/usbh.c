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

#if TUSB_OPT_HOST_ENABLED

#include "tusb.h"
#include "host/usbh.h"
#include "host/usbh_classdriver.h"
#include "hub.h"

//--------------------------------------------------------------------+
// USBH Configuration
//--------------------------------------------------------------------+

// TODO remove,update
#ifndef CFG_TUH_EP_MAX
#define CFG_TUH_EP_MAX          9
#endif

#ifndef CFG_TUH_TASK_QUEUE_SZ
#define CFG_TUH_TASK_QUEUE_SZ   16
#endif

// Debug level of USBD
#define USBH_DBG_LVL   2

//--------------------------------------------------------------------+
// USBH-HCD common data structure
//--------------------------------------------------------------------+

typedef struct {
  //------------- port -------------//
  uint8_t rhport;
  uint8_t hub_addr;
  uint8_t hub_port;
  uint8_t speed;

  //------------- device descriptor -------------//
  uint16_t vid;
  uint16_t pid;

  uint8_t  ep0_size;
  uint8_t  i_manufacturer;
  uint8_t  i_product;
  uint8_t  i_serial;

  //------------- configuration descriptor -------------//
  // uint8_t interface_count; // bNumInterfaces alias

  //------------- device -------------//
  struct TU_ATTR_PACKED
  {
    uint8_t connected    : 1;
    uint8_t addressed    : 1;
    uint8_t configured   : 1;
    uint8_t suspended    : 1;
  };

  volatile uint8_t state;            // device state, value from enum tusbh_device_state_t

  uint8_t itf2drv[16];               // map interface number to driver (0xff is invalid)
  uint8_t ep2drv[CFG_TUH_EP_MAX][2]; // map endpoint to driver ( 0xff is invalid )

  struct TU_ATTR_PACKED
  {
    volatile bool busy    : 1;
    volatile bool stalled : 1;
    volatile bool claimed : 1;

    // TODO merge ep2drv here, 4-bit should be sufficient
  }ep_status[CFG_TUH_EP_MAX][2];

  // Mutex for claiming endpoint, only needed when using with preempted RTOS
#if CFG_TUSB_OS != OPT_OS_NONE
  osal_mutex_def_t mutexdef;
  osal_mutex_t mutex;
#endif

} usbh_device_t;

typedef struct
{
  uint8_t rhport;
  uint8_t hub_addr;
  uint8_t hub_port;
  uint8_t speed;

  volatile uint8_t connected;
} usbh_dev0_t;


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Invalid driver ID in itf2drv[] ep2drv[][] mapping
enum { DRVID_INVALID = 0xFFu };
enum { ADDR_INVALID  = 0xFFu };

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

// Device with address = 0 for enumeration
static usbh_dev0_t _dev0;

// all devices excluding zero-address
// hub address start from CFG_TUH_DEVICE_MAX+1
CFG_TUSB_MEM_SECTION usbh_device_t _usbh_devices[CFG_TUH_DEVICE_MAX + CFG_TUH_HUB];

// Event queue
// role device/host is used by OS NONE for mutex (disable usb isr)
OSAL_QUEUE_DEF(OPT_MODE_HOST, _usbh_qdef, CFG_TUH_TASK_QUEUE_SZ, hcd_event_t);
static osal_queue_t _usbh_q;

CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t _usbh_ctrl_buf[CFG_TUH_ENUMERATION_BUFSIZE];

//------------- Helper Function -------------//

TU_ATTR_ALWAYS_INLINE
static inline usbh_device_t* get_device(uint8_t dev_addr)
{
  TU_ASSERT(dev_addr, NULL);
  return &_usbh_devices[dev_addr-1];
}

static bool enum_new_device(hcd_event_t* event);
static void process_device_unplugged(uint8_t rhport, uint8_t hub_addr, uint8_t hub_port);
static bool usbh_edpt_control_open(uint8_t dev_addr, uint8_t max_packet_size);

// from usbh_control.c
extern bool usbh_control_xfer_cb (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

//--------------------------------------------------------------------+
// PUBLIC API (Parameter Verification is required)
//--------------------------------------------------------------------+
bool tuh_mounted(uint8_t dev_addr)
{
  return get_device(dev_addr)->configured;
}

bool tuh_vid_pid_get(uint8_t dev_addr, uint16_t* vid, uint16_t* pid)
{
  *vid = *pid = 0;

  TU_VERIFY(tuh_mounted(dev_addr));

  usbh_device_t const* dev = get_device(dev_addr);

  *vid = dev->vid;
  *pid = dev->pid;

  return true;
}

tusb_speed_t tuh_speed_get (uint8_t dev_addr)
{
  return (tusb_speed_t) (dev_addr ? get_device(dev_addr)->speed : _dev0.speed);
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

  tu_memclr(_usbh_devices, sizeof(_usbh_devices));
  tu_memclr(&_dev0, sizeof(_dev0));

  //------------- Enumeration & Reporter Task init -------------//
  _usbh_q = osal_queue_create( &_usbh_qdef );
  TU_ASSERT(_usbh_q != NULL);

  //------------- Semaphore, Mutex for Control Pipe -------------//
  for(uint8_t i=0; i<TU_ARRAY_SIZE(_usbh_devices); i++)
  {
    usbh_device_t * dev = &_usbh_devices[i];

#if CFG_TUSB_OS != OPT_OS_NONE
    dev->mutex = osal_mutex_create(&dev->mutexdef);
    TU_ASSERT(dev->mutex);
#endif

    memset(dev->itf2drv, DRVID_INVALID, sizeof(dev->itf2drv)); // invalid mapping
    memset(dev->ep2drv , DRVID_INVALID, sizeof(dev->ep2drv )); // invalid mapping
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
        uint8_t const ep_addr = event.xfer_complete.ep_addr;
        uint8_t const epnum   = tu_edpt_number(ep_addr);
        uint8_t const ep_dir  = tu_edpt_dir(ep_addr);

        TU_LOG2("on EP %02X with %u bytes\r\n", ep_addr, (unsigned int) event.xfer_complete.len);

        if (event.dev_addr == 0)
        {
          // device 0 only has control endpoint
          TU_ASSERT(epnum == 0, );
          usbh_control_xfer_cb(event.dev_addr, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
        }
        else
        {
          usbh_device_t* dev = get_device(event.dev_addr);
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
  return (dev_addr == 0) ? _dev0.rhport : get_device(dev_addr)->rhport;
}

uint8_t* usbh_get_enum_buf(void)
{
  return _usbh_ctrl_buf;
}

//--------------------------------------------------------------------+
// HCD Event Handler
//--------------------------------------------------------------------+

void hcd_devtree_get_info(uint8_t dev_addr, hcd_devtree_info_t* devtree_info)
{
  if (dev_addr)
  {
    usbh_device_t const* dev = get_device(dev_addr);

    devtree_info->rhport   = dev->rhport;
    devtree_info->hub_addr = dev->hub_addr;
    devtree_info->hub_port = dev->hub_port;
    devtree_info->speed    = dev->speed;
  }else
  {
    devtree_info->rhport   = _dev0.rhport;
    devtree_info->hub_addr = _dev0.hub_addr;
    devtree_info->hub_port = _dev0.hub_port;
    devtree_info->speed    = _dev0.speed;
  }
}

void hcd_event_handler(hcd_event_t const* event, bool in_isr)
{
  switch (event->event_id)
  {
    default:
      osal_queue_send(_usbh_q, event, in_isr);
    break;
  }
}

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
  // TODO mark as disconnected in ISR, also handle dev0
  for ( uint8_t dev_id = 0; dev_id < TU_ARRAY_SIZE(_usbh_devices); dev_id++ )
  {
    usbh_device_t* dev = &_usbh_devices[dev_id];
    uint8_t const dev_addr = dev_id+1;

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

      memset(dev->itf2drv, DRVID_INVALID, sizeof(dev->itf2drv)); // invalid mapping
      memset(dev->ep2drv , DRVID_INVALID, sizeof(dev->ep2drv )); // invalid mapping

      hcd_device_close(rhport, dev_addr);

      dev->state = TUSB_DEVICE_STATE_UNPLUG;
    }
  }
}

//--------------------------------------------------------------------+
// INTERNAL HELPER
//--------------------------------------------------------------------+
static uint8_t get_new_address(bool is_hub)
{
  uint8_t const start = (is_hub ? CFG_TUH_DEVICE_MAX : 0) + 1;
  uint8_t const count = (is_hub ? CFG_TUH_HUB : CFG_TUH_DEVICE_MAX);

  for (uint8_t i=0; i < count; i++)
  {
    uint8_t const addr = start + i;
    if (get_device(addr)->state == TUSB_DEVICE_STATE_UNPLUG) return addr;
  }
  return ADDR_INVALID;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num)
{
  usbh_device_t* dev = get_device(dev_addr);

  for(itf_num++; itf_num < sizeof(dev->itf2drv); itf_num++)
  {
    // continue with next valid interface
    // TODO skip IAD binding interface such as CDCs
    uint8_t const drv_id = dev->itf2drv[itf_num];
    if (drv_id != DRVID_INVALID)
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

#if CFG_TUH_HUB
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

  enum_request_set_addr();

  // done with hub, waiting for next data on status pipe
  (void) hub_status_pipe_queue( _dev0.hub_addr );

  return true;
}

static bool enum_hub_get_status1_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) dev_addr; (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  hub_port_status_response_t port_status;
  memcpy(&port_status, _usbh_ctrl_buf, sizeof(hub_port_status_response_t));

  // Acknowledge Port Reset Change if Reset Successful
  if (port_status.change.reset)
  {
    TU_ASSERT( hub_port_clear_feature(_dev0.hub_addr, _dev0.hub_port, HUB_FEATURE_PORT_RESET_CHANGE, enum_hub_clear_reset1_complete) );
  }

  return true;
}

static bool enum_hub_get_status0_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) dev_addr; (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  hub_port_status_response_t port_status;
  memcpy(&port_status, _usbh_ctrl_buf, sizeof(hub_port_status_response_t));

  if ( !port_status.status.connection )
  {
    // device unplugged while delaying, nothing else to do, queue hub status
    return hub_status_pipe_queue(dev_addr);
  }

  _dev0.speed = (port_status.status.high_speed) ? TUSB_SPEED_HIGH :
                (port_status.status.low_speed ) ? TUSB_SPEED_LOW  : TUSB_SPEED_FULL;

  // Acknowledge Port Reset Change
  if (port_status.change.reset)
  {
    hub_port_clear_feature(_dev0.hub_addr, _dev0.hub_port, HUB_FEATURE_PORT_RESET_CHANGE, enum_hub_clear_reset0_complete);
  }

  return true;
}
#endif

static bool enum_new_device(hcd_event_t* event)
{
  _dev0.rhport   = event->rhport; // TODO refractor integrate to device_pool
  _dev0.hub_addr = event->connection.hub_addr;
  _dev0.hub_port = event->connection.hub_port;

  //------------- connected/disconnected directly with roothub -------------//
  if (_dev0.hub_addr == 0)
  {
    // wait until device is stable TODO non blocking
    osal_task_delay(RESET_DELAY);

    // device unplugged while delaying
    if ( !hcd_port_connect_status(_dev0.rhport) ) return true;

    _dev0.speed = hcd_port_speed_get(_dev0.rhport );

    enum_request_addr0_device_desc();
  }
#if CFG_TUH_HUB
  //------------- connected/disconnected via hub -------------//
  else
  {
    // wait until device is stable
    osal_task_delay(RESET_DELAY);
    TU_ASSERT( hub_port_get_status(_dev0.hub_addr, _dev0.hub_port, _usbh_ctrl_buf, enum_hub_get_status0_complete) );
  }
#endif // CFG_TUH_HUB

  return true;
}

static bool enum_request_addr0_device_desc(void)
{
  // TODO probably doesn't need to open/close each enumeration
  uint8_t const addr0 = 0;
  TU_ASSERT( usbh_edpt_control_open(addr0, 8) );

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
  TU_ASSERT( tuh_control_xfer(addr0, &request, _usbh_ctrl_buf, enum_get_addr0_device_desc_complete) );

  return true;
}

// After Get Device Descriptor of Address 0
static bool enum_get_addr0_device_desc_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;
  TU_ASSERT(0 == dev_addr);

  if (XFER_RESULT_SUCCESS != result)
  {
#if CFG_TUH_HUB
    // TODO remove, waiting for next data on status pipe
    if (_dev0.hub_addr != 0) hub_status_pipe_queue(_dev0.hub_addr);
#endif

    return false;
  }

  tusb_desc_device_t const * desc_device = (tusb_desc_device_t const*) _usbh_ctrl_buf;
  TU_ASSERT( tu_desc_type(desc_device) == TUSB_DESC_DEVICE );

  // Reset device again before Set Address
  TU_LOG2("Port reset \r\n");

  if (_dev0.hub_addr == 0)
  {
    // connected directly to roothub
    hcd_port_reset( _dev0.rhport ); // reset port after 8 byte descriptor
    osal_task_delay(RESET_DELAY);

    enum_request_set_addr();
  }
#if CFG_TUH_HUB
  else
  {
    // after RESET_DELAY the hub_port_reset() already complete
    TU_ASSERT( hub_port_reset(_dev0.hub_addr, _dev0.hub_port, NULL) );
    osal_task_delay(RESET_DELAY);

    tuh_task(); // FIXME temporarily to clean up port_reset control transfer

    TU_ASSERT( hub_port_get_status(_dev0.hub_addr, _dev0.hub_port, _usbh_ctrl_buf, enum_hub_get_status1_complete) );
  }
#endif

  return true;
}

static bool enum_request_set_addr(void)
{
  uint8_t const addr0 = 0;
  tusb_desc_device_t const * desc_device = (tusb_desc_device_t const*) _usbh_ctrl_buf;

  // Get new address
  uint8_t const new_addr = get_new_address(desc_device->bDeviceClass == TUSB_CLASS_HUB);
  TU_ASSERT(new_addr != ADDR_INVALID);

  TU_LOG2("Set Address = %d\r\n", new_addr);

  usbh_device_t* new_dev = get_device(new_addr);

  new_dev->rhport    = _dev0.rhport;
  new_dev->hub_addr  = _dev0.hub_addr;
  new_dev->hub_port  = _dev0.hub_port;
  new_dev->speed     = _dev0.speed;
  new_dev->connected = 1;
  new_dev->ep0_size  = desc_device->bMaxPacketSize0;

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

  TU_ASSERT( tuh_control_xfer(addr0, &new_request, NULL, enum_set_address_complete) );

  return true;
}

// After SET_ADDRESS is complete
static bool enum_set_address_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_ASSERT(0 == dev_addr);
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  uint8_t const new_addr = (uint8_t const) request->wValue;

  usbh_device_t* new_dev = get_device(new_addr);
  new_dev->addressed = 1;

  // TODO close device 0, may not be needed
  hcd_device_close(_dev0.rhport, 0);

  // open control pipe for new address
  TU_ASSERT( usbh_edpt_control_open(new_addr, new_dev->ep0_size) );

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
  usbh_device_t* dev = get_device(dev_addr);

  dev->vid            = desc_device->idVendor;
  dev->pid            = desc_device->idProduct;
  dev->i_manufacturer = desc_device->iManufacturer;
  dev->i_product      = desc_device->iProduct;
  dev->i_serial       = desc_device->iSerialNumber;

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

  TU_ASSERT(total_len <= CFG_TUH_ENUMERATION_BUFSIZE);

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
  TU_ASSERT( parse_configuration_descriptor(dev_addr, (tusb_desc_configuration_t*) _usbh_ctrl_buf) );

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
  usbh_device_t* dev = get_device(dev_addr);
  dev->configured = 1;
  dev->state = TUSB_DEVICE_STATE_CONFIGURED;

  // Start the Set Configuration process for interfaces (itf = DRVID_INVALID)
  // Since driver can perform control transfer within its set_config, this is done asynchronously.
  // The process continue with next interface when class driver complete its sequence with usbh_driver_set_config_complete()
  // TODO use separated API instead of using DRVID_INVALID
  usbh_driver_set_config_complete(dev_addr, DRVID_INVALID);

  return true;
}

static bool parse_configuration_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg)
{
  usbh_device_t* dev = get_device(dev_addr);

  uint8_t const* desc_end = ((uint8_t const*) desc_cfg) + tu_le16toh(desc_cfg->wTotalLength);
  uint8_t const* p_desc   = tu_desc_next(desc_cfg);

  // parse each interfaces
  while( p_desc < desc_end )
  {
    uint8_t assoc_itf_count = 1;

    // Class will always starts with Interface Association (if any) and then Interface descriptor
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc) )
    {
      tusb_desc_interface_assoc_t const * desc_iad = (tusb_desc_interface_assoc_t const *) p_desc;
      assoc_itf_count = desc_iad->bInterfaceCount;

      p_desc = tu_desc_next(p_desc); // next to Interface

      // IAD's first interface number and class should match with opened interface
      //TU_ASSERT(desc_iad->bFirstInterface == desc_itf->bInterfaceNumber &&
      //          desc_iad->bFunctionClass  == desc_itf->bInterfaceClass);
    }

    TU_ASSERT( TUSB_DESC_INTERFACE == tu_desc_type(p_desc) );
    tusb_desc_interface_t const* desc_itf = (tusb_desc_interface_t const*) p_desc;

#if CFG_TUH_MIDI
    // MIDI has 2 interfaces (Audio Control v1 + MIDIStreaming) but does not have IAD
    // manually increase the associated count
    if (1                              == assoc_itf_count              &&
        TUSB_CLASS_AUDIO               == desc_itf->bInterfaceClass    &&
        AUDIO_SUBCLASS_CONTROL         == desc_itf->bInterfaceSubClass &&
        AUDIO_FUNC_PROTOCOL_CODE_UNDEF == desc_itf->bInterfaceProtocol)
    {
      assoc_itf_count = 2;
    }
#endif

    uint16_t const drv_len = tu_desc_get_interface_total_len(desc_itf, assoc_itf_count, desc_end-p_desc);
    TU_ASSERT(drv_len >= sizeof(tusb_desc_interface_t));

    if (desc_itf->bInterfaceClass == TUSB_CLASS_HUB && dev->hub_addr != 0)
    {
      // TODO Attach hub to Hub is not currently supported
      // skip this interface
      TU_LOG(USBH_DBG_LVL, "Only 1 level of HUB is supported\r\n");
    }
    else
    {
      // Find driver for this interface
      uint8_t drv_id;
      for (drv_id = 0; drv_id < USBH_CLASS_DRIVER_COUNT; drv_id++)
      {
        usbh_class_driver_t const * driver = &usbh_class_drivers[drv_id];

        if ( driver->open(dev->rhport, dev_addr, desc_itf, drv_len) )
        {
          // open successfully
          TU_LOG2("  %s opened\r\n", driver->name);

          // bind (associated) interfaces to found driver
          for(uint8_t i=0; i<assoc_itf_count; i++)
          {
            uint8_t const itf_num = desc_itf->bInterfaceNumber+i;

            // Interface number must not be used already
            TU_ASSERT( DRVID_INVALID == dev->itf2drv[itf_num] );
            dev->itf2drv[itf_num] = drv_id;
          }

          // bind all endpoints to found driver
          tu_edpt_bind_driver(dev->ep2drv, desc_itf, drv_len, drv_id);

          break; // exit driver find loop
        }
      }

      if( drv_id >= USBH_CLASS_DRIVER_COUNT )
      {
        TU_LOG(USBH_DBG_LVL, "Interface %u: class = %u subclass = %u protocol = %u is not supported\r\n",
               desc_itf->bInterfaceNumber, desc_itf->bInterfaceClass, desc_itf->bInterfaceSubClass, desc_itf->bInterfaceProtocol);
      }
    }

    // next Interface or IAD descriptor
    p_desc += drv_len;
  }

  return true;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// TODO has some duplication code with device, refactor later
bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  usbh_device_t* dev = get_device(dev_addr);

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

// TODO has some duplication code with device, refactor later
bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  usbh_device_t* dev = get_device(dev_addr);

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

// TODO has some duplication code with device, refactor later
bool usbh_edpt_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  usbh_device_t* dev = get_device(dev_addr);

  TU_LOG2("  Queue EP %02X with %u bytes ... ", ep_addr, total_bytes);

  // Attempt to transfer on a busy endpoint, sound like an race condition !
  TU_ASSERT(dev->ep_status[epnum][dir].busy == 0);

  // Set busy first since the actual transfer can be complete before hcd_edpt_xfer()
  // could return and USBH task can preempt and clear the busy
  dev->ep_status[epnum][dir].busy = true;

  if ( hcd_edpt_xfer(dev->rhport, dev_addr, ep_addr, buffer, total_bytes) )
  {
    TU_LOG2("OK\r\n");
    return true;
  }else
  {
    // HCD error, mark endpoint as ready to allow next transfer
    dev->ep_status[epnum][dir].busy = false;
    dev->ep_status[epnum][dir].claimed = 0;
    TU_LOG2("failed\r\n");
    TU_BREAKPOINT();
    return false;
  }
}

static bool usbh_edpt_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  TU_LOG2("Open EP0 with Size = %u (addr = %u)\r\n", max_packet_size, dev_addr);

  tusb_desc_endpoint_t ep0_desc =
  {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0,
    .bmAttributes     = { .xfer = TUSB_XFER_CONTROL },
    .wMaxPacketSize   = { .size = max_packet_size },
    .bInterval        = 0
  };

  return hcd_edpt_open(usbh_get_rhport(dev_addr), dev_addr, &ep0_desc);
}

bool usbh_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * desc_ep)
{
  usbh_device_t* dev = get_device(dev_addr);
  TU_ASSERT(tu_edpt_validate(desc_ep, (tusb_speed_t) dev->speed));

  return hcd_edpt_open(rhport, dev_addr, desc_ep);
}

bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  usbh_device_t* dev = get_device(dev_addr);

  return dev->ep_status[epnum][dir].busy;
}



#endif
