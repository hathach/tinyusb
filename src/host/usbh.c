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

#if CFG_TUH_ENABLED

#include "hcd.h"
#include "tusb.h"
#include "usbh_pvt.h"
#include "hub.h"

//--------------------------------------------------------------------+
// Configuration
//--------------------------------------------------------------------+
#ifndef CFG_TUH_TASK_QUEUE_SZ
  #define CFG_TUH_TASK_QUEUE_SZ   16
#endif

#ifndef CFG_TUH_INTERFACE_MAX
  #define CFG_TUH_INTERFACE_MAX   8
#endif

enum {
  USBH_CONTROL_RETRY_MAX = 3,
};

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+
TU_ATTR_WEAK bool hcd_deinit(uint8_t rhport) {
  (void) rhport; return false;
}

TU_ATTR_WEAK bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport; (void) cfg_id; (void) cfg_param;
  return false;
}

TU_ATTR_WEAK void tuh_enum_descriptor_device_cb(uint8_t daddr, const tusb_desc_device_t *desc_device) {
  (void) daddr; (void) desc_device;
}

TU_ATTR_WEAK bool tuh_enum_descriptor_configuration_cb(uint8_t daddr, uint8_t cfg_index, const tusb_desc_configuration_t *desc_config) {
  (void) daddr; (void) cfg_index; (void) desc_config;
  return true;
}

TU_ATTR_WEAK void tuh_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr) {
  (void) rhport; (void) eventid; (void) in_isr;
}

TU_ATTR_WEAK bool hcd_dcache_clean(const void* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
  return false;
}

TU_ATTR_WEAK bool hcd_dcache_invalidate(const void* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
  return false;
}

TU_ATTR_WEAK bool hcd_dcache_clean_invalidate(const void* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
  return false;
}

TU_ATTR_WEAK usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count) {
  *driver_count = 0;
  return NULL;
}

TU_ATTR_WEAK void tuh_mount_cb(uint8_t daddr) {
  (void) daddr;
}

TU_ATTR_WEAK void tuh_umount_cb(uint8_t daddr) {
  (void) daddr;
}

//--------------------------------------------------------------------+
// Data Structure
//--------------------------------------------------------------------+
typedef struct {
  tuh_bus_info_t bus_info;

  // Device Descriptor
  uint16_t bcdUSB;
  uint8_t  bDeviceClass;
  uint8_t  bDeviceSubClass;
  uint8_t  bDeviceProtocol;
  uint8_t  bMaxPacketSize0;
  uint16_t idVendor;
  uint16_t idProduct;
  uint16_t bcdDevice;
  uint8_t  iManufacturer;
  uint8_t  iProduct;
  uint8_t  iSerialNumber;
  uint8_t  bNumConfigurations;

  // Device State
  struct TU_ATTR_PACKED {
    volatile uint8_t connected  : 1; // After 1st transfer
    volatile uint8_t addressed  : 1; // After SET_ADDR
    volatile uint8_t configured : 1; // After SET_CONFIG and all drivers are configured
    volatile uint8_t suspended  : 1; // Bus suspended
    // volatile uint8_t removing : 1; // Physically disconnected, waiting to be processed by usbh
  };

  // Endpoint & Interface
  uint8_t itf2drv[CFG_TUH_INTERFACE_MAX];  // map interface number to driver (0xff is invalid)
  uint8_t ep2drv[CFG_TUH_ENDPOINT_MAX][2]; // map endpoint to driver ( 0xff is invalid ), can use only 4-bit each

  tu_edpt_state_t ep_status[CFG_TUH_ENDPOINT_MAX][2];

#if CFG_TUH_API_EDPT_XFER
  // TODO array can be CFG_TUH_ENDPOINT_MAX-1
  struct {
    tuh_xfer_cb_t complete_cb;
    uintptr_t user_data;
  }ep_callback[CFG_TUH_ENDPOINT_MAX][2];
#endif

} usbh_device_t;

// sum of end device + hub
#define TOTAL_DEVICES   (CFG_TUH_DEVICE_MAX + CFG_TUH_HUB)

// all devices excluding zero-address
// hub address start from CFG_TUH_DEVICE_MAX+1
// TODO: hub can has its own simpler struct to save memory
static usbh_device_t _usbh_devices[TOTAL_DEVICES];

// Mutex for claiming endpoint
#if OSAL_MUTEX_REQUIRED
static osal_mutex_def_t _usbh_mutexdef;
static osal_mutex_t _usbh_mutex;
#else
#define _usbh_mutex   NULL
#endif

// Spinlock for interrupt handler
static OSAL_SPINLOCK_DEF(_usbh_spin, usbh_int_set);

// Event queue: usbh_int_set() is used as mutex in OS NONE config
OSAL_QUEUE_DEF(usbh_int_set, _usbh_qdef, CFG_TUH_TASK_QUEUE_SZ, hcd_event_t);
static osal_queue_t _usbh_q;

// Control transfers: since most controllers do not support multiple control transfers
// on multiple devices concurrently and control transfers are not used much except for
// enumeration, we will only execute control transfers one at a time.
typedef struct {
  uint8_t* buffer;
  tuh_xfer_cb_t complete_cb;
  uintptr_t user_data;

  volatile uint8_t stage;
  uint8_t daddr;
  volatile uint16_t actual_len;
  uint8_t failed_count;
} usbh_ctrl_xfer_info_t;

typedef struct {
  uint8_t controller_id;      // controller ID
  uint8_t enumerating_daddr;  // device address of the device being enumerated
  uint8_t attach_debouncing_bm;  // bitmask for roothub port attach debouncing
  tuh_bus_info_t dev0_bus;    // bus info for dev0 in enumeration
  usbh_ctrl_xfer_info_t ctrl_xfer_info; // control transfer
} usbh_data_t;

static usbh_data_t _usbh_data = {
  .controller_id = TUSB_INDEX_INVALID_8,
};

typedef struct {
  TUH_EPBUF_TYPE_DEF(tusb_control_request_t, request);
  TUH_EPBUF_DEF(ctrl, CFG_TUH_ENUMERATION_BUFSIZE);
} usbh_epbuf_t;
CFG_TUH_MEM_SECTION static usbh_epbuf_t _usbh_epbuf;

//--------------------------------------------------------------------+
// Class Driver
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= CFG_TUH_LOG_LEVEL
  #define DRIVER_NAME(_name)  _name
#else
  #define DRIVER_NAME(_name)  NULL
#endif

static usbh_class_driver_t const usbh_class_drivers[] = {
  #if CFG_TUH_CDC
  {
      .name       = DRIVER_NAME("CDC"),
      .init       = cdch_init,
      .deinit     = cdch_deinit,
      .open       = cdch_open,
      .set_config = cdch_set_config,
      .xfer_cb    = cdch_xfer_cb,
      .close      = cdch_close
  },
  #endif

  #if CFG_TUH_MSC
  {
      .name       = DRIVER_NAME("MSC"),
      .init       = msch_init,
      .deinit     = msch_deinit,
      .open       = msch_open,
      .set_config = msch_set_config,
      .xfer_cb    = msch_xfer_cb,
      .close      = msch_close
  },
  #endif

  #if CFG_TUH_HID
  {
      .name       = DRIVER_NAME("HID"),
      .init       = hidh_init,
      .deinit     = hidh_deinit,
      .open       = hidh_open,
      .set_config = hidh_set_config,
      .xfer_cb    = hidh_xfer_cb,
      .close      = hidh_close
  },
  #endif

  #if CFG_TUH_MIDI
  {
      .name       = DRIVER_NAME("MIDI"),
      .init       = midih_init,
      .deinit     = midih_deinit,
      .open       = midih_open,
      .set_config = midih_set_config,
      .xfer_cb    = midih_xfer_cb,
      .close      = midih_close
  },
  #endif

  #if CFG_TUH_HUB
  {
      .name       = DRIVER_NAME("HUB"),
      .init       = hub_init,
      .deinit     = hub_deinit,
      .open       = hub_open,
      .set_config = hub_set_config,
      .xfer_cb    = hub_xfer_cb,
      .close      = hub_close
  },
  #endif

  #if CFG_TUH_VENDOR
  {
    .name       = DRIVER_NAME("VENDOR"),
    .init       = cush_init,
    .deinit     = cush_deinit,
    .open       = cush_open,
    .set_config = cush_set_config,
    .xfer_cb    = cush_isr,
    .close      = cush_close
  }
  #endif
};

enum { BUILTIN_DRIVER_COUNT = TU_ARRAY_SIZE(usbh_class_drivers) };

// Additional class drivers implemented by application
static usbh_class_driver_t const * _app_driver = NULL;
static uint8_t _app_driver_count = 0;

#define TOTAL_DRIVER_COUNT    (_app_driver_count + BUILTIN_DRIVER_COUNT)

static inline usbh_class_driver_t const *get_driver(uint8_t drv_id) {
  usbh_class_driver_t const *driver = NULL;

  if ( drv_id < _app_driver_count ) {
    driver = &_app_driver[drv_id];
  } else if ( drv_id < TOTAL_DRIVER_COUNT && BUILTIN_DRIVER_COUNT > 0) {
    driver = &usbh_class_drivers[drv_id - _app_driver_count];
  }

  return driver;
}

//--------------------------------------------------------------------+
// Function Inline and Prototypes
//--------------------------------------------------------------------+
static bool enum_new_device(hcd_event_t* event);
static void process_removed_device(uint8_t rhport, uint8_t hub_addr, uint8_t hub_port);
static bool usbh_edpt_control_open(uint8_t dev_addr, uint8_t max_packet_size);
static bool usbh_control_xfer_cb (uint8_t daddr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

TU_ATTR_ALWAYS_INLINE static inline usbh_device_t* get_device(uint8_t dev_addr) {
  TU_VERIFY(dev_addr > 0 && dev_addr <= TOTAL_DEVICES, NULL);
  return &_usbh_devices[dev_addr-1];
}

TU_ATTR_ALWAYS_INLINE static inline bool is_hub_addr(uint8_t daddr) {
  return (CFG_TUH_HUB > 0) && (daddr > CFG_TUH_DEVICE_MAX);
}

TU_ATTR_ALWAYS_INLINE static inline bool queue_event(hcd_event_t const * event, bool in_isr) {
  TU_ASSERT(osal_queue_send(_usbh_q, event, in_isr));
  tuh_event_hook_cb(event->rhport, event->event_id, in_isr);
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline void _control_set_xfer_stage(uint8_t stage) {
  if (_usbh_data.ctrl_xfer_info.stage != stage) {
    (void) osal_mutex_lock(_usbh_mutex, OSAL_TIMEOUT_WAIT_FOREVER);
    _usbh_data.ctrl_xfer_info.stage = stage;
    (void) osal_mutex_unlock(_usbh_mutex);
  }
}

TU_ATTR_ALWAYS_INLINE static inline bool usbh_setup_send(uint8_t daddr, const uint8_t setup_packet[8]) {
  const uint8_t rhport = usbh_get_rhport(daddr);
  const bool ret = hcd_setup_send(rhport, daddr, setup_packet);
  if (!ret) {
    _control_set_xfer_stage(CONTROL_STAGE_IDLE);
  }
  return ret;
}

TU_ATTR_ALWAYS_INLINE static inline void usbh_device_close(uint8_t rhport, uint8_t daddr) {
  hcd_device_close(rhport, daddr);

  // abort any ongoing control transfer
  if (daddr == _usbh_data.ctrl_xfer_info.daddr) {
    _control_set_xfer_stage(CONTROL_STAGE_IDLE);
  }

  // invalidate if enumerating
  if (daddr == _usbh_data.enumerating_daddr) {
    _usbh_data.enumerating_daddr = TUSB_INDEX_INVALID_8;
  }
}

//--------------------------------------------------------------------+
// Device API
//--------------------------------------------------------------------+
bool tuh_mounted(uint8_t dev_addr) {
  usbh_device_t *dev = get_device(dev_addr);
  TU_VERIFY(dev);
  return dev->configured;
}

bool tuh_connected(uint8_t daddr) {
  if (daddr == 0) {
    return _usbh_data.enumerating_daddr == 0;
  } else {
    const usbh_device_t* dev = get_device(daddr);
    return dev && dev->connected;
  }
}

bool tuh_vid_pid_get(uint8_t dev_addr, uint16_t *vid, uint16_t *pid) {
  *vid = *pid = 0;

  usbh_device_t const *dev = get_device(dev_addr);
  TU_VERIFY(dev && dev->addressed && dev->idVendor != 0);

  *vid = dev->idVendor;
  *pid = dev->idProduct;

  return true;
}

bool tuh_descriptor_get_device_local(uint8_t daddr, tusb_desc_device_t* desc_device) {
  usbh_device_t *dev = get_device(daddr);
  TU_VERIFY(dev && desc_device);

  desc_device->bLength = sizeof(tusb_desc_device_t);
  desc_device->bDescriptorType = TUSB_DESC_DEVICE;
  desc_device->bcdUSB = dev->bcdUSB;
  desc_device->bDeviceClass = dev->bDeviceClass;
  desc_device->bDeviceSubClass = dev->bDeviceSubClass;
  desc_device->bDeviceProtocol = dev->bDeviceProtocol;
  desc_device->bMaxPacketSize0 = dev->bMaxPacketSize0;
  desc_device->idVendor = dev->idVendor;
  desc_device->idProduct = dev->idProduct;
  desc_device->bcdDevice = dev->bcdDevice;
  desc_device->iManufacturer = dev->iManufacturer;
  desc_device->iProduct = dev->iProduct;
  desc_device->iSerialNumber = dev->iSerialNumber;
  desc_device->bNumConfigurations = dev->bNumConfigurations;

  return true;
}

tusb_speed_t tuh_speed_get(uint8_t daddr) {
  tuh_bus_info_t bus_info;
  tuh_bus_info_get(daddr, &bus_info);
  return (tusb_speed_t)bus_info.speed;
}

bool tuh_rhport_is_active(uint8_t rhport) {
  return _usbh_data.controller_id == rhport;
}

bool tuh_rhport_reset_bus(uint8_t rhport, bool active) {
  TU_VERIFY(tuh_rhport_is_active(rhport));
  if (active) {
    hcd_port_reset(rhport);
  } else {
    hcd_port_reset_end(rhport);
  }
  return true;
}

//--------------------------------------------------------------------+
// PUBLIC API (Parameter Verification is required)
//--------------------------------------------------------------------+
bool tuh_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param) {
  return hcd_configure(rhport, cfg_id, cfg_param);
}

static void clear_device(usbh_device_t* dev) {
  tu_memclr(dev, sizeof(usbh_device_t));
  memset(dev->itf2drv, TUSB_INDEX_INVALID_8, sizeof(dev->itf2drv)); // invalid mapping
  memset(dev->ep2drv , TUSB_INDEX_INVALID_8, sizeof(dev->ep2drv )); // invalid mapping
}

bool tuh_inited(void) {
  return _usbh_data.controller_id != TUSB_INDEX_INVALID_8;
}

bool tuh_rhport_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  if (tuh_rhport_is_active(rhport)) {
    return true; // skip if already initialized
  }
#if CFG_TUSB_DEBUG >= CFG_TUH_LOG_LEVEL
  char const* speed_str = 0;
  switch (rh_init->speed) {
    case TUSB_SPEED_HIGH:
      speed_str = "High";
    break;
    case TUSB_SPEED_FULL:
      speed_str = "Full";
    break;
    case TUSB_SPEED_LOW:
      speed_str = "Low";
    break;
    case TUSB_SPEED_AUTO:
      speed_str = "Auto";
    break;
  default:
    break;
  }
  TU_LOG_USBH("USBH init on controller %u, speed = %s\r\n", rhport, speed_str);
#endif

  // Init host stack if not already
  if (!tuh_inited()) {
    TU_LOG_INT_USBH(sizeof(usbh_data_t));
    TU_LOG_INT_USBH(sizeof(usbh_device_t));
    TU_LOG_INT_USBH(sizeof(hcd_event_t));
    TU_LOG_INT_USBH(sizeof(tuh_xfer_t));
    TU_LOG_INT_USBH(sizeof(tu_fifo_t));
    TU_LOG_INT_USBH(sizeof(tu_edpt_stream_t));

    osal_spin_init(&_usbh_spin);

    // Event queue
    _usbh_q = osal_queue_create(&_usbh_qdef);
    TU_ASSERT(_usbh_q != NULL);

#if OSAL_MUTEX_REQUIRED
    // Init mutex
    _usbh_mutex = osal_mutex_create(&_usbh_mutexdef);
    TU_ASSERT(_usbh_mutex);
#endif

    // Get application driver if available
    _app_driver = usbh_app_driver_get_cb(&_app_driver_count);

    // Device
    tu_memclr(_usbh_devices, sizeof(_usbh_devices));
    tu_memclr(&_usbh_data, sizeof(_usbh_data));

    _usbh_data.controller_id = TUSB_INDEX_INVALID_8;
    _usbh_data.enumerating_daddr = TUSB_INDEX_INVALID_8;

    for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
      clear_device(&_usbh_devices[i]);
    }

    // Class drivers
    for (uint8_t drv_id = 0; drv_id < TOTAL_DRIVER_COUNT; drv_id++) {
      usbh_class_driver_t const* driver = get_driver(drv_id);
      if (driver) {
        TU_LOG_USBH("%s init\r\n", driver->name);
        driver->init();
      }
    }
  }

  // Init host controller
  _usbh_data.controller_id = rhport;
  TU_ASSERT(hcd_init(rhport, rh_init));
  hcd_int_enable(rhport);

  return true;
}

bool tuh_deinit(uint8_t rhport) {
  if (!tuh_rhport_is_active(rhport)) {
    return true;
  }

  // deinit host controller
  hcd_int_disable(rhport);
  hcd_deinit(rhport);
  _usbh_data.controller_id = TUSB_INDEX_INVALID_8;

  // "unplug" all devices on this rhport (hub_addr = 0, hub_port = 0)
  process_removed_device(rhport, 0, 0);

  // deinit host stack if no controller is active
  if (!tuh_inited()) {
    // Class drivers
    for (uint8_t drv_id = 0; drv_id < TOTAL_DRIVER_COUNT; drv_id++) {
      usbh_class_driver_t const* driver = get_driver(drv_id);
      if (driver && driver->deinit) {
        TU_LOG_USBH("%s deinit\r\n", driver->name);
        driver->deinit();
      }
    }

    osal_queue_delete(_usbh_q);
    _usbh_q = NULL;

    #if OSAL_MUTEX_REQUIRED
    // TODO make sure there is no task waiting on this mutex
    osal_mutex_delete(_usbh_mutex);
    _usbh_mutex = NULL;
    #endif
  }

  return true;
}

bool tuh_task_event_ready(void) {
  if (!tuh_inited()) {
    return false; // Skip if stack is not initialized
  }
  return !osal_queue_empty(_usbh_q);
}

/* USB Host Driver task
 * This top level thread manages all host controller event and delegates events to class-specific drivers.
 * This should be called periodically within the mainloop or rtos thread.
 *
   @code
    int main(void) {
      application_init();
      tusb_init(0, TUSB_ROLE_HOST);

      while(1) { // the mainloop
        application_code();
        tuh_task(); // tinyusb host task
      }
    }
    @endcode
 */
void tuh_task_ext(uint32_t timeout_ms, bool in_isr) {
  (void) in_isr; // not implemented yet

  // Skip if stack is not initialized
  if (!tuh_inited()) {
    return;
  }

  // Loop until there is no more events in the queue
  while (1) {
    hcd_event_t event;
    if (!osal_queue_receive(_usbh_q, &event, timeout_ms)) { return; }

    switch (event.event_id) {
      case HCD_EVENT_DEVICE_ATTACH:
        // due to the shared control buffer, we must fully complete enumerating one device first.
        // TODO better to have an separated queue for newly attached devices
        if (_usbh_data.enumerating_daddr == TUSB_INDEX_INVALID_8) {
          // New device attached and we are ready
          TU_LOG_USBH("[%u:] USBH Device Attach\r\n", event.rhport);
          _usbh_data.enumerating_daddr = 0; // enumerate new device with address 0
          enum_new_device(&event);
        } else {
          // currently enumerating another device
          TU_LOG_USBH("[%u:] USBH Defer Attach until current enumeration complete\r\n", event.rhport);
          const bool is_empty = osal_queue_empty(_usbh_q);
          queue_event(&event, in_isr);
          if (is_empty) {
            return; // Exit if this is the only event in the queue, otherwise we loop forever
          }
        }
        break;

      case HCD_EVENT_DEVICE_REMOVE:
        TU_LOG_USBH("[%u:%u:%u] USBH DEVICE REMOVED\r\n", event.rhport, event.connection.hub_addr, event.connection.hub_port);
        if (_usbh_data.enumerating_daddr == 0 &&
            event.rhport == _usbh_data.dev0_bus.rhport &&
            event.connection.hub_addr == _usbh_data.dev0_bus.hub_addr &&
            event.connection.hub_port == _usbh_data.dev0_bus.hub_port) {
          // dev0 is unplugged while enumerating (not yet assigned an address)
          usbh_device_close(_usbh_data.dev0_bus.rhport, 0);
        } else {
          process_removed_device(event.rhport, event.connection.hub_addr, event.connection.hub_port);
        }
        break;

      case HCD_EVENT_XFER_COMPLETE: {
        uint8_t const ep_addr = event.xfer_complete.ep_addr;
        uint8_t const epnum = tu_edpt_number(ep_addr);
        uint8_t const ep_dir = (uint8_t) tu_edpt_dir(ep_addr);

        TU_LOG_USBH("[:%u] on EP %02X with %u bytes: %s\r\n",
                    event.dev_addr, ep_addr, (unsigned int) event.xfer_complete.len, tu_str_xfer_result[event.xfer_complete.result]);

        if (event.dev_addr == 0) {
          // device 0 only has control endpoint
          TU_ASSERT(epnum == 0,);
          usbh_control_xfer_cb(event.dev_addr, ep_addr, (xfer_result_t) event.xfer_complete.result, event.xfer_complete.len);
        } else {
          usbh_device_t* dev = get_device(event.dev_addr);
          TU_VERIFY(dev && dev->connected,);

          dev->ep_status[epnum][ep_dir].busy = 0;
          dev->ep_status[epnum][ep_dir].claimed = 0;

          if (0 == epnum) {
            usbh_control_xfer_cb(event.dev_addr, ep_addr, (xfer_result_t) event.xfer_complete.result, event.xfer_complete.len);
          } else {
            // Prefer application callback over built-in one if available. This occurs when tuh_edpt_xfer() is used
            // with enabled driver e.g HID endpoint
            #if CFG_TUH_API_EDPT_XFER
            tuh_xfer_cb_t const complete_cb = dev->ep_callback[epnum][ep_dir].complete_cb;
            if ( complete_cb ) {
              // re-construct xfer info
              tuh_xfer_t xfer = {
                  .daddr       = event.dev_addr,
                  .ep_addr     = ep_addr,
                  .result      = (xfer_result_t)event.xfer_complete.result,
                  .actual_len  = event.xfer_complete.len,
                  .buflen      = 0,    // not available
                  .buffer      = NULL, // not available
                  .complete_cb = complete_cb,
                  .user_data   = dev->ep_callback[epnum][ep_dir].user_data
              };
              complete_cb(&xfer);
            }else
            #endif
            {
              uint8_t drv_id = dev->ep2drv[epnum][ep_dir];
              usbh_class_driver_t const* driver = get_driver(drv_id);
              if (driver) {
                TU_LOG_USBH("  %s xfer callback\r\n", driver->name);
                driver->xfer_cb(event.dev_addr, ep_addr, (xfer_result_t) event.xfer_complete.result,
                                event.xfer_complete.len);
              } else {
                // no driver/callback responsible for this transfer
                TU_ASSERT(false,);
              }
            }
          }
        }
        break;
      }

      case USBH_EVENT_FUNC_CALL:
        if (event.func_call.func) event.func_call.func(event.func_call.param);
        break;

      default:
        break;
    }

#if CFG_TUSB_OS != OPT_OS_NONE && CFG_TUSB_OS != OPT_OS_PICO
    // return if there is no more events, for application to run other background
    if (osal_queue_empty(_usbh_q)) return;
#endif
  }
}

//--------------------------------------------------------------------+
// Control transfer
//--------------------------------------------------------------------+

static void _control_blocking_complete_cb(tuh_xfer_t* xfer) {
  // update result
  *((xfer_result_t*) xfer->user_data) = xfer->result;
}

// TODO timeout_ms is not supported yet
bool tuh_control_xfer (tuh_xfer_t* xfer) {
  TU_VERIFY(xfer->ep_addr == 0 && xfer->setup); // EP0 with setup packet
  const uint8_t daddr = xfer->daddr;
  TU_VERIFY(tuh_connected(daddr));

  usbh_ctrl_xfer_info_t* ctrl_info = &_usbh_data.ctrl_xfer_info;

  TU_VERIFY(ctrl_info->stage == CONTROL_STAGE_IDLE); // pre-check to help reducing mutex lock
  (void) osal_mutex_lock(_usbh_mutex, OSAL_TIMEOUT_WAIT_FOREVER);
  bool const is_idle = (ctrl_info->stage == CONTROL_STAGE_IDLE);
  if (is_idle) {
    ctrl_info->stage        = CONTROL_STAGE_SETUP;
    ctrl_info->daddr        = daddr;
    ctrl_info->actual_len   = 0;
    ctrl_info->failed_count = 0;

    ctrl_info->buffer       = xfer->buffer;
    ctrl_info->complete_cb  = xfer->complete_cb;
    ctrl_info->user_data    = xfer->user_data;
    _usbh_epbuf.request     = (*xfer->setup);
  }
  (void) osal_mutex_unlock(_usbh_mutex);

  TU_VERIFY(is_idle);
  TU_LOG_USBH("[%u:%u] %s: ", usbh_get_rhport(daddr), daddr,
              (xfer->setup->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD && xfer->setup->bRequest <= TUSB_REQ_SYNCH_FRAME) ?
                  tu_str_std_request[xfer->setup->bRequest] : "Class Request");
  TU_LOG_BUF_USBH(xfer->setup, 8);

  if (xfer->complete_cb) {
    TU_ASSERT(usbh_setup_send(daddr, (uint8_t const *) &_usbh_epbuf.request));
  }else {
    // blocking if complete callback is not provided
    // change callback to internal blocking, and result as user argument
    volatile xfer_result_t result = XFER_RESULT_INVALID;

    // use user_data to point to xfer_result_t
    ctrl_info->user_data   = (uintptr_t) &result;
    ctrl_info->complete_cb = _control_blocking_complete_cb;

    TU_ASSERT(usbh_setup_send(daddr, (uint8_t const *) &_usbh_epbuf.request));

    while (result == XFER_RESULT_INVALID) {
      // Note: this can be called within an callback ie. part of tuh_task()
      // therefore event with RTOS tuh_task() still need to be invoked
      if (tuh_task_event_ready()) {
        tuh_task();
      }
      // TODO probably some timeout to prevent hanged
    }

    // update transfer result, user_data is expected to point to xfer_result_t
    if (xfer->user_data != 0) {
      *((xfer_result_t*) xfer->user_data) = result;
    }
    xfer->result     = result;
    xfer->actual_len = ctrl_info->actual_len;
  }

  return true;
}

static void _control_xfer_complete(uint8_t daddr, xfer_result_t result) {
  TU_LOG_USBH("\r\n");
  usbh_ctrl_xfer_info_t* ctrl_info = &_usbh_data.ctrl_xfer_info;

  // duplicate xfer since user can execute control transfer within callback
  tusb_control_request_t const request = _usbh_epbuf.request;
  tuh_xfer_t xfer_temp = {
    .daddr       = daddr,
    .ep_addr     = 0,
    .result      = result,
    .setup       = &request,
    .actual_len  = (uint32_t) ctrl_info->actual_len,
    .buffer      = ctrl_info->buffer,
    .complete_cb = ctrl_info->complete_cb,
    .user_data   = ctrl_info->user_data
  };

  _control_set_xfer_stage(CONTROL_STAGE_IDLE);

  if (xfer_temp.complete_cb) {
    xfer_temp.complete_cb(&xfer_temp);
  }
}

static bool usbh_control_xfer_cb (uint8_t daddr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) ep_addr;

  const uint8_t rhport = usbh_get_rhport(daddr);
  tusb_control_request_t const * request = &_usbh_epbuf.request;
  usbh_ctrl_xfer_info_t* ctrl_info = &_usbh_data.ctrl_xfer_info;

  switch (result) {
    case XFER_RESULT_STALLED:
      TU_LOG_USBH("[%u:%u] Control STALLED, xferred_bytes = %" PRIu32 "\r\n", rhport, daddr, xferred_bytes);
      TU_LOG_BUF_USBH(request, 8);
      _control_xfer_complete(daddr, result);
    break;

    case XFER_RESULT_FAILED:
      if (tuh_connected(daddr) && ctrl_info->failed_count < USBH_CONTROL_RETRY_MAX) {
        TU_LOG_USBH("[%u:%u] Control FAILED %u/%u, retrying\r\n", rhport, daddr, ctrl_info->failed_count+1, USBH_CONTROL_RETRY_MAX);
        (void) osal_mutex_lock(_usbh_mutex, OSAL_TIMEOUT_WAIT_FOREVER);
        ctrl_info->stage = CONTROL_STAGE_SETUP;
        ctrl_info->failed_count++;
        ctrl_info->actual_len = 0; // reset actual_len
        (void) osal_mutex_unlock(_usbh_mutex);

        TU_ASSERT(usbh_setup_send(daddr, (uint8_t const *) request));
      } else {
        TU_LOG_USBH("[%u:%u] Control FAILED, xferred_bytes = %" PRIu32 "\r\n", rhport, daddr, xferred_bytes);
        TU_LOG_BUF_USBH(request, 8);
        _control_xfer_complete(daddr, result);
      }
    break;

    case XFER_RESULT_SUCCESS:
      switch(ctrl_info->stage) {
        case CONTROL_STAGE_SETUP:
          if (request->wLength) {
            // DATA stage: initial data toggle is always 1
            _control_set_xfer_stage(CONTROL_STAGE_DATA);
            const uint8_t ep_data = tu_edpt_addr(0, request->bmRequestType_bit.direction);
            TU_ASSERT(hcd_edpt_xfer(rhport, daddr, ep_data, ctrl_info->buffer, request->wLength));
            return true;
          }
          TU_ATTR_FALLTHROUGH;

        case CONTROL_STAGE_DATA: {
            if (request->wLength) {
              TU_LOG_USBH("[%u:%u] Control data:\r\n", rhport, daddr);
              TU_LOG_MEM_USBH(ctrl_info->buffer, xferred_bytes, 2);
            }
            ctrl_info->actual_len = (uint16_t) xferred_bytes;

            // ACK stage: toggle is always 1
            _control_set_xfer_stage(CONTROL_STAGE_ACK);
            const uint8_t ep_status = tu_edpt_addr(0, 1 - request->bmRequestType_bit.direction);
            TU_ASSERT(hcd_edpt_xfer(rhport, daddr, ep_status, NULL, 0));
            break;
          }

        case CONTROL_STAGE_ACK: {
          // Abort all pending transfers if SET_CONFIGURATION request
          // NOTE: should we force closing all non-control endpoints in the future?
          if (request->bRequest == TUSB_REQ_SET_CONFIGURATION && request->bmRequestType == 0x00) {
            for(uint8_t epnum=1; epnum<CFG_TUH_ENDPOINT_MAX; epnum++) {
              for(uint8_t dir=0; dir<2; dir++) {
                tuh_edpt_abort_xfer(daddr, tu_edpt_addr(epnum, dir));
              }
            }
          }

          _control_xfer_complete(daddr, result);
          break;
        }

        default: return false; // unsupported stage
      }
      break;

    default: return false; // unsupported result
  }

  return true;
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool tuh_edpt_xfer(tuh_xfer_t* xfer) {
  uint8_t const daddr = xfer->daddr;
  uint8_t const ep_addr = xfer->ep_addr;

  TU_VERIFY(daddr && ep_addr);
  TU_VERIFY(usbh_edpt_claim(daddr, ep_addr));

  if (!usbh_edpt_xfer_with_callback(daddr, ep_addr, xfer->buffer, (uint16_t) xfer->buflen,
                                    xfer->complete_cb, xfer->user_data)) {
    usbh_edpt_release(daddr, ep_addr);
    return false;
  }

  return true;
}

bool tuh_edpt_abort_xfer(uint8_t daddr, uint8_t ep_addr) {
  TU_LOG_USBH("[%u] Aborted transfer on EP %02X\r\n", daddr, ep_addr);
  const uint8_t epnum = tu_edpt_number(ep_addr);
  const uint8_t dir   = tu_edpt_dir(ep_addr);

  if (epnum == 0) {
    // Also include dev0 for aborting enumerating
    const uint8_t rhport = usbh_get_rhport(daddr);

    // control transfer: only 1 control at a time, check if we are aborting the current one
    const usbh_ctrl_xfer_info_t* ctrl_info = &_usbh_data.ctrl_xfer_info;
    TU_VERIFY(daddr == ctrl_info->daddr && ctrl_info->stage != CONTROL_STAGE_IDLE);
    hcd_edpt_abort_xfer(rhport, daddr, ep_addr);
    _control_set_xfer_stage(CONTROL_STAGE_IDLE); // reset control transfer state to idle
  } else {
    usbh_device_t* dev = get_device(daddr);
    TU_VERIFY(dev);

    TU_VERIFY(dev->ep_status[epnum][dir].busy); // non-control skip if not busy
    // abort then mark as ready and release endpoint
    hcd_edpt_abort_xfer(dev->bus_info.rhport, daddr, ep_addr);
    dev->ep_status[epnum][dir].busy = false;
    tu_edpt_release(&dev->ep_status[epnum][dir], _usbh_mutex);
  }

  return true;
}

//--------------------------------------------------------------------+
// USBH API For Class Driver
//--------------------------------------------------------------------+

uint8_t usbh_get_rhport(uint8_t daddr) {
  tuh_bus_info_t bus_info;
  tuh_bus_info_get(daddr, &bus_info);
  return bus_info.rhport;
}

uint8_t *usbh_get_enum_buf(void) {
  return _usbh_epbuf.ctrl;
}

void usbh_int_set(bool enabled) {
  // TODO all host controller if multiple are used since they shared the same event queue
  if (enabled) {
    hcd_int_enable(_usbh_data.controller_id);
  } else {
    hcd_int_disable(_usbh_data.controller_id);
  }
}

void usbh_spin_lock(bool in_isr) {
  osal_spin_lock(&_usbh_spin, in_isr);
}

void usbh_spin_unlock(bool in_isr) {
  osal_spin_unlock(&_usbh_spin, in_isr);
}

void usbh_defer_func(osal_task_func_t func, void *param, bool in_isr) {
  hcd_event_t event = { 0 };
  event.event_id = USBH_EVENT_FUNC_CALL;
  event.func_call.func = func;
  event.func_call.param = param;
  queue_event(&event, in_isr);
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Claim an endpoint for transfer
bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  // Note: addr0 only use tuh_control_xfer
  usbh_device_t* dev = get_device(dev_addr);
  TU_ASSERT(dev && dev->connected);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  TU_VERIFY(tu_edpt_claim(&dev->ep_status[epnum][dir], _usbh_mutex));
  TU_LOG_USBH("[%u] Claimed EP 0x%02x\r\n", dev_addr, ep_addr);

  return true;
}

// Release an claimed endpoint due to failed transfer attempt
bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  // Note: addr0 only use tuh_control_xfer
  usbh_device_t* dev = get_device(dev_addr);
  TU_VERIFY(dev && dev->connected);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  TU_VERIFY(tu_edpt_release(&dev->ep_status[epnum][dir], _usbh_mutex));
  TU_LOG_USBH("[%u] Released EP 0x%02x\r\n", dev_addr, ep_addr);

  return true;
}

// Submit an transfer
bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) complete_cb;
  (void) user_data;

  usbh_device_t* dev = get_device(dev_addr);
  TU_VERIFY(dev);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);
  tu_edpt_state_t* ep_state = &dev->ep_status[epnum][dir];

  TU_LOG_USBH("  Queue EP %02X with %u bytes ... \r\n", ep_addr, total_bytes);

  // Attempt to transfer on a busy endpoint, sound like an race condition !
  TU_ASSERT(ep_state->busy == 0);

  // Set busy first since the actual transfer can be complete before hcd_edpt_xfer()
  // could return and USBH task can preempt and clear the busy
  ep_state->busy = 1;

#if CFG_TUH_API_EDPT_XFER
  dev->ep_callback[epnum][dir].complete_cb = complete_cb;
  dev->ep_callback[epnum][dir].user_data   = user_data;
#endif

  if (hcd_edpt_xfer(dev->bus_info.rhport, dev_addr, ep_addr, buffer, total_bytes)) {
    TU_LOG_USBH("OK\r\n");
    return true;
  } else {
    // HCD error, mark endpoint as ready to allow next transfer
    ep_state->busy = 0;
    ep_state->claimed = 0;
    TU_LOG1("Failed\r\n");
//    TU_BREAKPOINT();
    return false;
  }
}

static bool usbh_edpt_control_open(uint8_t dev_addr, uint8_t max_packet_size) {
  TU_LOG_USBH("[%u:%u] Open EP0 with Size = %u\r\n", usbh_get_rhport(dev_addr), dev_addr, max_packet_size);
  tusb_desc_endpoint_t ep0_desc = {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0,
    .bmAttributes     = { .xfer = TUSB_XFER_CONTROL },
    .wMaxPacketSize   = max_packet_size,
    .bInterval        = 0
  };

  return hcd_edpt_open(usbh_get_rhport(dev_addr), dev_addr, &ep0_desc);
}

bool tuh_edpt_open(uint8_t dev_addr, tusb_desc_endpoint_t const* desc_ep) {
  TU_ASSERT(tu_edpt_validate(desc_ep, tuh_speed_get(dev_addr), true));
  return hcd_edpt_open(usbh_get_rhport(dev_addr), dev_addr, desc_ep);
}

bool tuh_edpt_close(uint8_t daddr, uint8_t ep_addr) {
  TU_VERIFY(0 != tu_edpt_number(ep_addr)); // cannot close EP0
  tuh_edpt_abort_xfer(daddr, ep_addr); // abort any pending transfer
  return hcd_edpt_close(usbh_get_rhport(daddr), daddr, ep_addr);
}

bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr) {
  usbh_device_t* dev = get_device(dev_addr);
  TU_VERIFY(dev);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  return dev->ep_status[epnum][dir].busy;
}

//--------------------------------------------------------------------+
// HCD Event Handler
//--------------------------------------------------------------------+

bool tuh_bus_info_get(uint8_t daddr, tuh_bus_info_t* bus_info) {
  usbh_device_t const* dev = get_device(daddr);
  if (dev) {
    *bus_info = dev->bus_info;
  } else {
    *bus_info = _usbh_data.dev0_bus;
  }
  return true;
}

TU_ATTR_FAST_FUNC void hcd_event_handler(hcd_event_t const* event, bool in_isr) {
  switch (event->event_id) {
    case HCD_EVENT_DEVICE_ATTACH:
    case HCD_EVENT_DEVICE_REMOVE:
      // Attach debouncing on roothub: skip attach/remove while debouncing delay
      if (event->connection.hub_addr == 0) {
        if (tu_bit_test(_usbh_data.attach_debouncing_bm, event->rhport)) {
          return;
        }

        if (event->event_id == HCD_EVENT_DEVICE_ATTACH) {
          // No debouncing, set flag if attach event
          _usbh_data.attach_debouncing_bm |= TU_BIT(event->rhport);
        }
      }
      break;

    default: break;
  }

  queue_event(event, in_isr);
}

//--------------------------------------------------------------------+
// Descriptors Async
//--------------------------------------------------------------------+

// generic helper to get a descriptor
// if blocking, user_data is pointed to xfer_result
TU_ATTR_ALWAYS_INLINE static inline
bool _get_descriptor(uint8_t daddr, uint8_t type, uint8_t index, uint16_t language_id, void* buffer, uint16_t len,
                    tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_IN
    },
    .bRequest = TUSB_REQ_GET_DESCRIPTOR,
    .wValue   = tu_htole16( TU_U16(type, index) ),
    .wIndex   = tu_htole16(language_id),
    .wLength  = tu_htole16(len)
  };
  tuh_xfer_t xfer = {
    .daddr       = daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = buffer,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

bool tuh_descriptor_get(uint8_t daddr, uint8_t type, uint8_t index, void* buffer, uint16_t len,
                        tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return _get_descriptor(daddr, type, index, 0x0000, buffer, len, complete_cb, user_data);
}

bool tuh_descriptor_get_device(uint8_t daddr, void* buffer, uint16_t len,
                               tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  len = tu_min16(len, sizeof(tusb_desc_device_t));
  return tuh_descriptor_get(daddr, TUSB_DESC_DEVICE, 0, buffer, len, complete_cb, user_data);
}

bool tuh_descriptor_get_configuration(uint8_t daddr, uint8_t index, void* buffer, uint16_t len,
                                      tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return tuh_descriptor_get(daddr, TUSB_DESC_CONFIGURATION, index, buffer, len, complete_cb, user_data);
}

//------------- String Descriptor -------------//
bool tuh_descriptor_get_string(uint8_t daddr, uint8_t index, uint16_t language_id, void* buffer, uint16_t len,
                               tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return _get_descriptor(daddr, TUSB_DESC_STRING, index, language_id, buffer, len, complete_cb, user_data);
}

// Get manufacturer string descriptor
bool tuh_descriptor_get_manufacturer_string(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len,
                                            tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  usbh_device_t const* dev = get_device(daddr);
  TU_VERIFY(dev && dev->iManufacturer);
  return tuh_descriptor_get_string(daddr, dev->iManufacturer, language_id, buffer, len, complete_cb, user_data);
}

// Get product string descriptor
bool tuh_descriptor_get_product_string(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len,
                                       tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  usbh_device_t const* dev = get_device(daddr);
  TU_VERIFY(dev && dev->iProduct);
  return tuh_descriptor_get_string(daddr, dev->iProduct, language_id, buffer, len, complete_cb, user_data);
}

// Get serial string descriptor
bool tuh_descriptor_get_serial_string(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len,
                                      tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  usbh_device_t const* dev = get_device(daddr);
  TU_VERIFY(dev && dev->iSerialNumber);
  return tuh_descriptor_get_string(daddr, dev->iSerialNumber, language_id, buffer, len, complete_cb, user_data);
}

// Get HID report descriptor
// if blocking, user_data is pointed to xfer_result
bool tuh_descriptor_get_hid_report(uint8_t daddr, uint8_t itf_num, uint8_t desc_type, uint8_t index, void* buffer, uint16_t len,
                                   tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_USBH("HID Get Report Descriptor\r\n");
  tusb_control_request_t const request = {
      .bmRequestType_bit = {
          .recipient = TUSB_REQ_RCPT_INTERFACE,
          .type      = TUSB_REQ_TYPE_STANDARD,
          .direction = TUSB_DIR_IN
      },
      .bRequest = TUSB_REQ_GET_DESCRIPTOR,
      .wValue   = tu_htole16(TU_U16(desc_type, index)),
      .wIndex   = tu_htole16((uint16_t) itf_num),
      .wLength  = len
  };
  tuh_xfer_t xfer = {
      .daddr       = daddr,
      .ep_addr     = 0,
      .setup       = &request,
      .buffer      = buffer,
      .complete_cb = complete_cb,
      .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

bool tuh_address_set(uint8_t daddr, uint8_t new_addr,
                     tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_USBH("Set Address = %d\r\n", new_addr);
  const tusb_control_request_t request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_STANDARD,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = TUSB_REQ_SET_ADDRESS,
    .wValue   = tu_htole16(new_addr),
    .wIndex   = 0,
    .wLength  = 0
  };
  tuh_xfer_t xfer = {
    .daddr       = daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = NULL,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  TU_ASSERT(tuh_control_xfer(&xfer));
  return true;
}

bool tuh_configuration_set(uint8_t daddr, uint8_t config_num,
                           tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_USBH("Set Configuration = %d\r\n", config_num);
  tusb_control_request_t const request = {
      .bmRequestType_bit = {
          .recipient = TUSB_REQ_RCPT_DEVICE,
          .type      = TUSB_REQ_TYPE_STANDARD,
          .direction = TUSB_DIR_OUT
      },
      .bRequest = TUSB_REQ_SET_CONFIGURATION,
      .wValue   = tu_htole16(config_num),
      .wIndex   = 0,
      .wLength  = 0
  };
  tuh_xfer_t xfer = {
      .daddr       = daddr,
      .ep_addr     = 0,
      .setup       = &request,
      .buffer      = NULL,
      .complete_cb = complete_cb,
      .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

bool tuh_interface_set(uint8_t daddr, uint8_t itf_num, uint8_t itf_alt,
                       tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_USBH("Set Interface %u Alternate %u\r\n", itf_num, itf_alt);
  tusb_control_request_t const request = {
      .bmRequestType_bit = {
          .recipient = TUSB_REQ_RCPT_INTERFACE,
          .type      = TUSB_REQ_TYPE_STANDARD,
          .direction = TUSB_DIR_OUT
      },
      .bRequest = TUSB_REQ_SET_INTERFACE,
      .wValue   = tu_htole16(itf_alt),
      .wIndex   = tu_htole16(itf_num),
      .wLength  = 0
  };
  tuh_xfer_t xfer = {
      .daddr       = daddr,
      .ep_addr     = 0,
      .setup       = &request,
      .buffer      = NULL,
      .complete_cb = complete_cb,
      .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

//--------------------------------------------------------------------+
// Detaching
//--------------------------------------------------------------------+
// a device unplugged from rhport:hub_addr:hub_port
static void process_removed_device(uint8_t rhport, uint8_t hub_addr, uint8_t hub_port) {
  // Find the all devices (star-network) under port that is unplugged
  #if CFG_TUH_HUB
  uint8_t removing_hubs[CFG_TUH_HUB] = { 0 };
  #endif

  do {
    for (uint8_t dev_id = 0; dev_id < TOTAL_DEVICES; dev_id++) {
      usbh_device_t* dev = &_usbh_devices[dev_id];
      uint8_t const daddr = dev_id + 1;

      // hub_addr = 0 means roothub, hub_port = 0 means all devices of downstream hub
      if (dev->bus_info.rhport == rhport && dev->connected &&
          (hub_addr == 0 || dev->bus_info.hub_addr == hub_addr) &&
          (hub_port == 0 || dev->bus_info.hub_port == hub_port)) {
        TU_LOG_USBH("[%u:%u:%u] unplugged address = %u\r\n", rhport, hub_addr, hub_port, daddr);

        #if CFG_TUH_HUB
        if (is_hub_addr(daddr)) {
          TU_LOG_USBH("  is a HUB device %u\r\n", daddr);
          removing_hubs[dev_id - CFG_TUH_DEVICE_MAX] = 1;
        } else
        #endif
        {
          // Invoke callback before closing driver (maybe call it later ?)
          tuh_umount_cb(daddr);
        }

        // Close class driver
        for (uint8_t drv_id = 0; drv_id < TOTAL_DRIVER_COUNT; drv_id++) {
          usbh_class_driver_t const* driver = get_driver(drv_id);
          if (driver) {
            driver->close(daddr);
          }
        }

        usbh_device_close(rhport, daddr);
        clear_device(dev);
      }
    }

#if CFG_TUH_HUB
    // if a hub is removed, we need to remove all of its downstream devices
    if (tu_mem_is_zero(removing_hubs, CFG_TUH_HUB)) {
      break;
    }

    // find a marked hub to process
    for (uint8_t h_id = 0; h_id < CFG_TUH_HUB; h_id++) {
      if (removing_hubs[h_id]) {
        removing_hubs[h_id] = 0;

        // update hub_addr and hub_port for next loop
        hub_addr = h_id + 1 + CFG_TUH_DEVICE_MAX;
        hub_port = 0;
        break;
      }
    }
#else
    break;
#endif

  } while(1);
}

//--------------------------------------------------------------------+
// Enumeration Process
// is a lengthy process with a series of control transfer to configure newly attached device.
// NOTE: due to the shared control buffer, we must complete enumerating
// one device before enumerating another one.
//--------------------------------------------------------------------+
enum {                               // USB 2.0 specs 7.1.7 for timing
  ENUM_DEBOUNCING_DELAY_MS = 150,    // T(ATTDB)  minimum 100 ms for stable connection
  ENUM_RESET_ROOT_DELAY_MS = 50,     // T(DRSTr)  minimum 50 ms for reset from root port
  ENUM_RESET_HUB_DELAY_MS = 20,      // T(DRST)   10-20 ms for hub reset
  ENUM_RESET_RECOVERY_DELAY_MS = 10, // T(RSTRCY) minimum 10 ms for reset recovery
  ENUM_SET_ADDRESS_RECOVERY_DELAY_MS = 2, // USB 2.0 Spec 9.2.6.3 min is 2 ms
};

enum {
  ENUM_IDLE,
  ENUM_HUB_RERSET,
  ENUM_HUB_GET_STATUS_AFTER_RESET,
  ENUM_HUB_CLEAR_RESET,
  ENUM_HUB_CLEAR_RESET_COMPLETE,

  ENUM_ADDR0_DEVICE_DESC,
  ENUM_SET_ADDR,
  ENUM_GET_DEVICE_DESC,
  ENUM_GET_STRING_LANGUAGE_ID_LEN,
  ENUM_GET_STRING_LANGUAGE_ID,
  ENUM_GET_STRING_MANUFACTURER_LEN,
  ENUM_GET_STRING_MANUFACTURER,
  ENUM_GET_STRING_PRODUCT_LEN,
  ENUM_GET_STRING_PRODUCT,
  ENUM_GET_STRING_SERIAL_LEN,
  ENUM_GET_STRING_SERIAL,
  ENUM_GET_9BYTE_CONFIG_DESC,
  ENUM_GET_FULL_CONFIG_DESC,
  ENUM_SET_CONFIG,
  ENUM_CONFIG_DRIVER
};

static uint8_t enum_get_new_address(bool is_hub);
static bool enum_parse_configuration_desc (uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg);
static void enum_full_complete(void);
static void process_enumeration(tuh_xfer_t* xfer);

// start a new enumeration process
static bool enum_new_device(hcd_event_t* event) {
  tuh_bus_info_t* dev0_bus = &_usbh_data.dev0_bus;
  dev0_bus->rhport = event->rhport;
  dev0_bus->hub_addr = event->connection.hub_addr;
  dev0_bus->hub_port = event->connection.hub_port;

  // wait until device connection is stable TODO non blocking
  tusb_time_delay_ms_api(ENUM_DEBOUNCING_DELAY_MS);

  // clear roothub debouncing delay
  if (dev0_bus->hub_addr == 0) {
    _usbh_data.attach_debouncing_bm &= (uint8_t) ~TU_BIT(dev0_bus->rhport);
  }

  if (dev0_bus->hub_addr == 0) {
    // connected directly to roothub
    // USB bus not active and frame number is not available yet.
    // need to depend on tusb_time_millis_api() TODO non blocking

    if (!hcd_port_connect_status(dev0_bus->rhport)) {
      TU_LOG_USBH("Device unplugged while debouncing\r\n");
      enum_full_complete();
      return true;
    }

    // reset device
    hcd_port_reset(dev0_bus->rhport);
    tusb_time_delay_ms_api(ENUM_RESET_ROOT_DELAY_MS);
    hcd_port_reset_end(dev0_bus->rhport);

    if (!hcd_port_connect_status(dev0_bus->rhport)) {
      // device unplugged while delaying
      enum_full_complete();
      return true;
    }

    dev0_bus->speed = hcd_port_speed_get(dev0_bus->rhport);
    TU_LOG_USBH("%s Speed\r\n", tu_str_speed[dev0_bus->speed]);

    // fake transfer to kick-off the enumeration process
    tuh_xfer_t xfer;
    xfer.daddr = 0;
    xfer.result = XFER_RESULT_SUCCESS;
    xfer.user_data = ENUM_ADDR0_DEVICE_DESC;
    process_enumeration(&xfer);
  }
  #if CFG_TUH_HUB
  else {
    // connected via hub
    TU_VERIFY(dev0_bus->hub_port != 0);
    TU_ASSERT(hub_port_get_status(dev0_bus->hub_addr, dev0_bus->hub_port, NULL,
                                  process_enumeration, ENUM_HUB_RERSET));
  }
  #endif // hub

  return true;
}

// process device enumeration
static void process_enumeration(tuh_xfer_t* xfer) {
  // Retry a few times while enumerating since device can be unstable when starting up
  static uint8_t failed_count = 0;
  if (XFER_RESULT_FAILED == xfer->result) {
    enum {
      ATTEMPT_COUNT_MAX = 3,
      ATTEMPT_DELAY_MS = 100
    };

    // retry if not reaching max attempt
    failed_count++;
    bool retry = (_usbh_data.enumerating_daddr != TUSB_INDEX_INVALID_8) && (failed_count < ATTEMPT_COUNT_MAX);
    if (retry) {
      tusb_time_delay_ms_api(ATTEMPT_DELAY_MS); // delay a bit
      TU_LOG_USBH("Enumeration attempt %u/%u\r\n", failed_count+1, ATTEMPT_COUNT_MAX);
      retry = tuh_control_xfer(xfer);
    }

    if (!retry) {
      enum_full_complete(); // complete as failed
    }
    return;
  }
  failed_count = 0;

  uint8_t const daddr = xfer->daddr;
  uintptr_t const state = xfer->user_data;
  usbh_device_t* dev = get_device(daddr);
  tuh_bus_info_t* dev0_bus = &_usbh_data.dev0_bus;
  if (daddr > 0) {
    TU_ASSERT(dev,);
  }
  uint16_t langid = 0x0409; // default is English

  switch (state) {
    #if CFG_TUH_HUB
    case ENUM_HUB_RERSET: {
      hub_port_status_response_t port_status;
      hub_port_get_status_local(dev0_bus->hub_addr, dev0_bus->hub_port, &port_status);

      if (!port_status.status.connection) {
        TU_LOG_USBH("Device unplugged from hub while debouncing\r\n");
        enum_full_complete();
        return;
      }

      TU_ASSERT(hub_port_reset(dev0_bus->hub_addr, dev0_bus->hub_port, process_enumeration, ENUM_HUB_GET_STATUS_AFTER_RESET),);
      break;
    }

    case ENUM_HUB_GET_STATUS_AFTER_RESET: {
      tusb_time_delay_ms_api(ENUM_RESET_HUB_DELAY_MS); // wait for reset to take effect

      // get status to check for reset change
      TU_ASSERT(hub_port_get_status(dev0_bus->hub_addr, dev0_bus->hub_port, NULL, process_enumeration, ENUM_HUB_CLEAR_RESET),);
      break;
    }

    case ENUM_HUB_CLEAR_RESET: {
      hub_port_status_response_t port_status;
      hub_port_get_status_local(dev0_bus->hub_addr, dev0_bus->hub_port, &port_status);

      if (port_status.change.reset) {
        // Acknowledge Port Reset Change
        TU_ASSERT(hub_port_clear_reset_change(dev0_bus->hub_addr, dev0_bus->hub_port, process_enumeration, ENUM_HUB_CLEAR_RESET_COMPLETE),);
      } else {
        // maybe retry if reset change not set but we need timeout to prevent infinite loop
        // TU_ASSERT(hub_port_get_status(dev0_bus->hub_addr, dev0_bus->hub_port, NULL, process_enumeration, ENUM_HUB_CLEAR_RESET_COMPLETE),);
      }

      break;
    }

    case ENUM_HUB_CLEAR_RESET_COMPLETE: {
      hub_port_status_response_t port_status;
      hub_port_get_status_local(dev0_bus->hub_addr, dev0_bus->hub_port, &port_status);

      if (!port_status.status.connection) {
        TU_LOG_USBH("Device unplugged from hub (not addressed yet)\r\n");
        enum_full_complete();
        return;
      }

      dev0_bus->speed = (port_status.status.high_speed) ? TUSB_SPEED_HIGH :
                        (port_status.status.low_speed) ? TUSB_SPEED_LOW : TUSB_SPEED_FULL;

      TU_ATTR_FALLTHROUGH;
    }
    #endif

    case ENUM_ADDR0_DEVICE_DESC: {
      tusb_time_delay_ms_api(ENUM_RESET_RECOVERY_DELAY_MS); // reset recovery

      // TODO probably doesn't need to open/close each enumeration
      uint8_t const addr0 = 0;
      TU_ASSERT(usbh_edpt_control_open(addr0, 8),);

      // Get first 8 bytes of device descriptor for control endpoint size
      TU_LOG_USBH("Get 8 byte of Device Descriptor\r\n");
      TU_ASSERT(tuh_descriptor_get_device(addr0, _usbh_epbuf.ctrl, 8,
                                          process_enumeration, ENUM_SET_ADDR),);
      break;
    }

    case ENUM_SET_ADDR: {
      // Due to physical debouncing, some devices can cause multiple attaches (actually reset) without detach event
      // Force remove currently mounted with the same bus info (rhport, hub addr, hub port) if exists
      process_removed_device(dev0_bus->rhport, dev0_bus->hub_addr, dev0_bus->hub_port);

      const tusb_desc_device_t *desc_device = (const tusb_desc_device_t *) _usbh_epbuf.ctrl;
      const uint8_t new_addr = enum_get_new_address(desc_device->bDeviceClass == TUSB_CLASS_HUB);
      TU_ASSERT(new_addr != 0,);

      usbh_device_t* new_dev = get_device(new_addr);
      new_dev->bus_info = *dev0_bus;
      new_dev->connected = 1;
      new_dev->bMaxPacketSize0 = desc_device->bMaxPacketSize0;

      TU_ASSERT(tuh_address_set(0, new_addr, process_enumeration, ENUM_GET_DEVICE_DESC),);
      break;
    }

    case ENUM_GET_DEVICE_DESC: {
      tusb_time_delay_ms_api(ENUM_SET_ADDRESS_RECOVERY_DELAY_MS); // set address recovery

      const uint8_t new_addr = (uint8_t) tu_le16toh(xfer->setup->wValue);
      usbh_device_t* new_dev = get_device(new_addr);
      TU_ASSERT(new_dev,);
      new_dev->addressed = 1;
      _usbh_data.enumerating_daddr = new_addr;

      usbh_device_close(dev0_bus->rhport, 0); // close dev0

      TU_ASSERT(usbh_edpt_control_open(new_addr, new_dev->bMaxPacketSize0),); // open new control endpoint

      TU_LOG_USBH("Get Device Descriptor\r\n");
      TU_ASSERT(tuh_descriptor_get_device(new_addr, _usbh_epbuf.ctrl, sizeof(tusb_desc_device_t),
                                          process_enumeration, ENUM_GET_STRING_LANGUAGE_ID_LEN),);
      break;
    }

    // For string descriptor (langid, manufacturer, product, serila): always get the first 2 bytes
    // to determine the length first. otherwise, some device may have buffer overflow.
    case ENUM_GET_STRING_LANGUAGE_ID_LEN: {
      // save the received device descriptor
      tusb_desc_device_t const *desc_device = (tusb_desc_device_t const *) _usbh_epbuf.ctrl;
      dev->bcdUSB = desc_device->bcdUSB;
      dev->bDeviceClass = desc_device->bDeviceClass;
      dev->bDeviceSubClass = desc_device->bDeviceSubClass;
      dev->bDeviceProtocol = desc_device->bDeviceProtocol;
      dev->bMaxPacketSize0 = desc_device->bMaxPacketSize0;
      dev->idVendor = desc_device->idVendor;
      dev->idProduct = desc_device->idProduct;
      dev->bcdDevice = desc_device->bcdDevice;
      dev->iManufacturer = desc_device->iManufacturer;
      dev->iProduct = desc_device->iProduct;
      dev->iSerialNumber = desc_device->iSerialNumber;
      dev->bNumConfigurations = desc_device->bNumConfigurations;

      tuh_enum_descriptor_device_cb(daddr, desc_device); // callback
      tuh_descriptor_get_string_langid(daddr, _usbh_epbuf.ctrl, 2,
                                       process_enumeration, ENUM_GET_STRING_LANGUAGE_ID);
      break;
    }

    case ENUM_GET_STRING_LANGUAGE_ID: {
      const uint8_t str_len = xfer->buffer[0];
      tuh_descriptor_get_string_langid(daddr, _usbh_epbuf.ctrl, str_len,
                                       process_enumeration, ENUM_GET_STRING_MANUFACTURER_LEN);
      break;
    }

    case ENUM_GET_STRING_MANUFACTURER_LEN: {
      const tusb_desc_string_t* desc_langid = (const tusb_desc_string_t *) _usbh_epbuf.ctrl;
      if (desc_langid->bLength >= 4) {
        langid = tu_le16toh(desc_langid->utf16le[0]); // previous request is langid
      }
      if (dev->iManufacturer != 0) {
        tuh_descriptor_get_string(daddr, dev->iManufacturer, langid, _usbh_epbuf.ctrl, 2,
                                  process_enumeration, ENUM_GET_STRING_MANUFACTURER);
        break;
      }else {
        TU_ATTR_FALLTHROUGH;
      }
    }

    case ENUM_GET_STRING_MANUFACTURER: {
      if (dev->iManufacturer != 0)  {
        langid = tu_le16toh(xfer->setup->wIndex); // langid from length's request
        const uint8_t str_len = xfer->buffer[0];
        tuh_descriptor_get_string(daddr, dev->iManufacturer, langid, _usbh_epbuf.ctrl, str_len,
                                  process_enumeration, ENUM_GET_STRING_PRODUCT_LEN);
        break;
      } else {
        TU_ATTR_FALLTHROUGH;
      }
    }

    case ENUM_GET_STRING_PRODUCT_LEN:
      if (dev->iProduct != 0) {
        if (state == ENUM_GET_STRING_PRODUCT_LEN) {
          langid = tu_le16toh(xfer->setup->wIndex); // get langid from previous setup packet if not fall through
        }
        tuh_descriptor_get_string(daddr, dev->iProduct, langid, _usbh_epbuf.ctrl, 2,
                                  process_enumeration, ENUM_GET_STRING_PRODUCT);
        break;
      } else {
        TU_ATTR_FALLTHROUGH;
      }

    case ENUM_GET_STRING_PRODUCT: {
      if (dev->iProduct != 0) {
        langid = tu_le16toh(xfer->setup->wIndex); // langid from length's request
        const uint8_t str_len = xfer->buffer[0];
        tuh_descriptor_get_string(daddr, dev->iProduct, langid, _usbh_epbuf.ctrl, str_len,
                            process_enumeration, ENUM_GET_STRING_SERIAL_LEN);
        break;
      } else {
        TU_ATTR_FALLTHROUGH;
      }
    }

    case ENUM_GET_STRING_SERIAL_LEN:
      if (dev->iSerialNumber != 0) {
        if (state == ENUM_GET_STRING_SERIAL_LEN) {
          langid = tu_le16toh(xfer->setup->wIndex); // get langid from previous setup packet if not fall through
        }
        tuh_descriptor_get_string(daddr, dev->iSerialNumber, langid, _usbh_epbuf.ctrl, 2,
                                  process_enumeration, ENUM_GET_STRING_SERIAL);
        break;
      } else {
        TU_ATTR_FALLTHROUGH;
      }

    case ENUM_GET_STRING_SERIAL: {
      if (dev->iSerialNumber != 0) {
        langid = tu_le16toh(xfer->setup->wIndex); // langid from length's request
        const uint8_t str_len = xfer->buffer[0];
        tuh_descriptor_get_string(daddr, dev->iSerialNumber, langid, _usbh_epbuf.ctrl, str_len,
                                  process_enumeration, ENUM_GET_9BYTE_CONFIG_DESC);
        break;
      } else {
        TU_ATTR_FALLTHROUGH;
      }
    }

    case ENUM_GET_9BYTE_CONFIG_DESC: {
      // Get 9-byte for total length
      uint8_t const config_idx = 0;
      TU_LOG_USBH("Get Configuration[%u] Descriptor (9 bytes)\r\n", config_idx);
      TU_ASSERT(tuh_descriptor_get_configuration(daddr, config_idx, _usbh_epbuf.ctrl, 9,
                                                 process_enumeration, ENUM_GET_FULL_CONFIG_DESC),);
      break;
    }

    case ENUM_GET_FULL_CONFIG_DESC: {
      uint8_t const* desc_config = _usbh_epbuf.ctrl;

      // Use offsetof to avoid pointer to the odd/misaligned address
      uint16_t const total_len = tu_le16toh(tu_unaligned_read16(desc_config + offsetof(tusb_desc_configuration_t, wTotalLength)));

      // TODO not enough buffer to hold configuration descriptor
      TU_ASSERT(total_len <= CFG_TUH_ENUMERATION_BUFSIZE,);

      // Get full configuration descriptor
      uint8_t const config_idx = (uint8_t) tu_le16toh(xfer->setup->wIndex);
      TU_LOG_USBH("Get Configuration[%u] Descriptor\r\n", config_idx);
      TU_ASSERT(tuh_descriptor_get_configuration(daddr, config_idx, _usbh_epbuf.ctrl, total_len,
                                                 process_enumeration, ENUM_SET_CONFIG),);
      break;
    }

    case ENUM_SET_CONFIG: {
      uint8_t config_idx = (uint8_t) tu_le16toh(xfer->setup->wIndex);
      if (tuh_enum_descriptor_configuration_cb(daddr, config_idx, (const tusb_desc_configuration_t*) _usbh_epbuf.ctrl)) {
        TU_ASSERT(tuh_configuration_set(daddr, config_idx+1, process_enumeration, ENUM_CONFIG_DRIVER),);
      } else {
        config_idx++;
        TU_ASSERT(config_idx < dev->bNumConfigurations,);
        TU_LOG_USBH("Get Configuration[%u] Descriptor (9 bytes)\r\n", config_idx);
        TU_ASSERT(tuh_descriptor_get_configuration(daddr, config_idx, _usbh_epbuf.ctrl, 9,
                                                   process_enumeration, ENUM_GET_FULL_CONFIG_DESC),);
      }
      break;
    }

    case ENUM_CONFIG_DRIVER: {
      TU_LOG_USBH("Device configured\r\n");
      dev->configured = 1;

      // Parse configuration & set up drivers
      // driver_open() must not make any usb transfer
      TU_ASSERT(enum_parse_configuration_desc(daddr, (tusb_desc_configuration_t*) _usbh_epbuf.ctrl),);

      // Start the Set Configuration process for interfaces (itf = TUSB_INDEX_INVALID_8)
      // Since driver can perform control transfer within its set_config, this is done asynchronously.
      // The process continue with next interface when class driver complete its sequence with usbh_driver_set_config_complete()
      // TODO use separated API instead of using TUSB_INDEX_INVALID_8
      usbh_driver_set_config_complete(daddr, TUSB_INDEX_INVALID_8);
      break;
    }

    default:
      enum_full_complete(); // stop enumeration if unknown state
      break;
  }
}

static uint8_t enum_get_new_address(bool is_hub) {
  uint8_t start;
  uint8_t end;

  if ( is_hub ) {
    start = CFG_TUH_DEVICE_MAX;
    end   = start + CFG_TUH_HUB;
  }else {
    start = 0;
    end   = start + CFG_TUH_DEVICE_MAX;
  }

  for (uint8_t idx = start; idx < end; idx++) {
    if (!_usbh_devices[idx].connected) {
      return (idx + 1);
    }
  }

  return 0; // invalid address
}

static bool enum_parse_configuration_desc(uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg) {
  usbh_device_t* dev = get_device(dev_addr);
  uint16_t const total_len = tu_le16toh(desc_cfg->wTotalLength);
  uint8_t const* desc_end = ((uint8_t const*) desc_cfg) + total_len;
  uint8_t const* p_desc   = tu_desc_next(desc_cfg);

  TU_LOG_USBH("Parsing Configuration descriptor (wTotalLength = %u)\r\n", total_len);

  // parse each interfaces
  while( p_desc < desc_end ) {
    if ( 0 == tu_desc_len(p_desc) ) {
      // A zero length descriptor indicates that the device is off spec (e.g. wrong wTotalLength).
      // Parsed interfaces should still be usable
      TU_LOG_USBH("Encountered a zero-length descriptor after %" PRIu32 " bytes\r\n", (uint32_t)p_desc - (uint32_t)desc_cfg);
      break;
    }

    uint8_t assoc_itf_count = 1;

    // Class will always starts with Interface Association (if any) and then Interface descriptor
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc) ) {
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
    // manually force associated count = 2
    if (1                              == assoc_itf_count              &&
        TUSB_CLASS_AUDIO               == desc_itf->bInterfaceClass    &&
        AUDIO_SUBCLASS_CONTROL         == desc_itf->bInterfaceSubClass &&
        AUDIO_FUNC_PROTOCOL_CODE_UNDEF == desc_itf->bInterfaceProtocol) {
      assoc_itf_count = 2;
    }
#endif

#if CFG_TUH_CDC
    // Some legacy CDC device does not use IAD but rather use device class as hint to combine 2 interfaces
    // manually force associated count = 2
    if (1                                        == assoc_itf_count              &&
        TUSB_CLASS_CDC                           == desc_itf->bInterfaceClass    &&
        CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == desc_itf->bInterfaceSubClass) {
      assoc_itf_count = 2;
    }
#endif

    uint16_t const drv_len = tu_desc_get_interface_total_len(desc_itf, assoc_itf_count, (uint16_t) (desc_end-p_desc));
    TU_ASSERT(drv_len >= sizeof(tusb_desc_interface_t));

    // Find driver for this interface
    for (uint8_t drv_id = 0; drv_id < TOTAL_DRIVER_COUNT; drv_id++) {
      usbh_class_driver_t const * driver = get_driver(drv_id);
      if (driver && driver->open(dev->bus_info.rhport, dev_addr, desc_itf, drv_len) ) {
        // open successfully
        TU_LOG_USBH("  %s opened\r\n", driver->name);

        // bind (associated) interfaces to found driver
        for(uint8_t i=0; i<assoc_itf_count; i++) {
          uint8_t const itf_num = desc_itf->bInterfaceNumber+i;

          // Interface number must not be used already
          TU_ASSERT( TUSB_INDEX_INVALID_8 == dev->itf2drv[itf_num] );
          dev->itf2drv[itf_num] = drv_id;
        }

        // bind all endpoints to found driver
        tu_edpt_bind_driver(dev->ep2drv, desc_itf, drv_len, drv_id);

        break; // exit driver find loop
      }

      if (drv_id == TOTAL_DRIVER_COUNT - 1) {
        TU_LOG_USBH("[%u:%u] Interface %u: class = %u subclass = %u protocol = %u is not supported\r\n",
               dev->bus_info.rhport, dev_addr, desc_itf->bInterfaceNumber, desc_itf->bInterfaceClass, desc_itf->bInterfaceSubClass, desc_itf->bInterfaceProtocol);
      }
    }

    // next Interface or IAD descriptor
    p_desc += drv_len;
  }

  return true;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  usbh_device_t* dev = get_device(dev_addr);

  for(itf_num++; itf_num < CFG_TUH_INTERFACE_MAX; itf_num++) {
    // continue with next valid interface
    // IAD binding interface such as CDCs should return itf_num + 1 when complete
    // with usbh_driver_set_config_complete()
    uint8_t const drv_id = dev->itf2drv[itf_num];
    usbh_class_driver_t const * driver = get_driver(drv_id);
    if (driver) {
      TU_LOG_USBH("%s set config: itf = %u\r\n", driver->name, itf_num);
      driver->set_config(dev_addr, itf_num);
      break;
    }
  }

  // all interface are configured
  if (itf_num == CFG_TUH_INTERFACE_MAX) {
    enum_full_complete();

    if (is_hub_addr(dev_addr)) {
      TU_LOG_USBH("HUB address = %u is mounted\r\n", dev_addr);
    }else {
      // Invoke callback if available
      tuh_mount_cb(dev_addr);
    }
  }
}

static void enum_full_complete(void) {
  // mark enumeration as complete
  _usbh_data.enumerating_daddr = TUSB_INDEX_INVALID_8;

#if CFG_TUH_HUB
  if (_usbh_data.dev0_bus.hub_addr != 0) {
    hub_edpt_status_xfer(_usbh_data.dev0_bus.hub_addr); // get next hub status
  }
#endif

}

#endif
