/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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
 */

/* Device-side peer of the Linux kernel host test driver drivers/usb/misc/usbtest.c
 * (driven from userspace by tools/usb/testusb.c). Implements the Gadget-Zero
 * style source/sink protocol on a vendor interface (alt 0 = no endpoints,
 * alt 1 = full set, selected by the host usbtest driver):
 *   - bulk/interrupt/isochronous IN  = infinite source (pattern 0: all zeros)
 *   - bulk/interrupt/isochronous OUT = infinite sink (data discarded)
 *   - EP0 0x5b/0x5c = control write then read-back (ctrl_out tests)
 * See examples/device/usbtest/README.md and test/hil/usbtest.py for usage.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED     = 1000,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Source data, all zeros = usbtest pattern 0. Sizes are a multiple of the
// endpoint max packet size: transfers are always whole packets, never an
// unintended short packet or ZLP.
static uint8_t const tx_chunk[CFG_TUD_VENDOR_TX_EPSIZE];
static uint8_t const int_tx_chunk[USBTEST_INT_EP_MPS];
#if USBTEST_TIER >= 4
static uint8_t const iso_tx_chunk[USBTEST_ISO_EP_MPS];
#endif

// Interrupt/iso submit one packet per (micro)frame, sized to the NEGOTIATED speed's mps — a
// high-speed build enumerated at full speed must submit the FS length, not the HS-capacity buffer
// size (bulk is exempt: it streams multi-packet transfers). See usb_descriptors.h.
static inline uint16_t usbtest_int_len(void) {
  return (tud_speed_get() == TUSB_SPEED_HIGH) ? USBTEST_INT_EP_MPS_HS : USBTEST_INT_EP_MPS_FS;
}
#if USBTEST_TIER >= 4
static inline uint16_t usbtest_iso_len(void) {
  return (tud_speed_get() == TUSB_SPEED_HIGH) ? USBTEST_ISO_EP_MPS_HS : USBTEST_ISO_EP_MPS_FS;
}
#endif

//------------- prototypes -------------//
void led_blinking_task(void* param);
void usbtest_task(void* param);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
void freertos_init(void);
#endif

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  // If using FreeRTOS: create blinky, tinyusb device, and usbtest source/sink tasks
#if CFG_TUSB_OS == OPT_OS_FREERTOS
  freertos_init();
#else
  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();

  while (1) {
    tud_task(); // tinyusb device task
    usbtest_task(NULL);
    led_blinking_task(NULL);
  }
#endif
}

//--------------------------------------------------------------------+
// Source/sink pumps
//--------------------------------------------------------------------+

// Polling keeps all four endpoints armed and self-heals after endpoint halt
// (set/clear feature tests): stall marks the endpoint busy so the calls fail
// quietly until the host clears the halt, then the next tick re-arms.
static void usbtest_pump(void) {
  if (tud_vendor_mounted()) {
    tud_vendor_read_xfer();     // bulk sink: arm/re-arm, quiet fail if armed or halted
    if (tud_vendor_write_available()) { // 0 while bulk IN is busy or halted
      tud_vendor_write(tx_chunk, sizeof(tx_chunk));
    }

    tud_vendor_int_read_xfer(); // interrupt sink
    if (tud_vendor_int_write_available()) {
      tud_vendor_int_write(int_tx_chunk, usbtest_int_len());
    }

#if USBTEST_TIER >= 4
    tud_vendor_iso_read_xfer(); // isochronous sink
    if (tud_vendor_iso_write_available()) {
      tud_vendor_iso_write(iso_tx_chunk, usbtest_iso_len());
    }
#endif
  }
}

void usbtest_task(void* param) {
  (void) param;
  #if CFG_TUSB_OS == OPT_OS_FREERTOS
  while (1) {
    usbtest_pump();
    vTaskDelay(1); // yield; tx/rx completion callbacks keep the pipes saturated between polls
  }
  #else
  usbtest_pump(); // called from the main loop, one tick per call
  #endif
}

// Invoked when received data from host: discard and immediately re-arm
void tud_vendor_rx_cb(uint8_t idx, const uint8_t* buffer, uint32_t bufsize) {
  (void) idx;
  (void) buffer;
  (void) bufsize;
  tud_vendor_read_xfer();
}

// Invoked when last bulk tx transfer finished: keep the source saturated
void tud_vendor_tx_cb(uint8_t idx, uint32_t sent_bytes) {
  (void) idx;
  (void) sent_bytes;
  tud_vendor_write(tx_chunk, sizeof(tx_chunk));
}

// Interrupt pair: same discard/refill pumps as bulk
void tud_vendor_int_rx_cb(uint8_t idx, const uint8_t* buffer, uint32_t bufsize) {
  (void) idx;
  (void) buffer;
  (void) bufsize;
  tud_vendor_int_read_xfer();
}

void tud_vendor_int_tx_cb(uint8_t idx, uint32_t sent_bytes) {
  (void) idx;
  (void) sent_bytes;
  tud_vendor_int_write(int_tx_chunk, usbtest_int_len());
}

// Isochronous pair: same discard/refill pumps; a completion may be a missed
// frame, re-arm regardless
#if USBTEST_TIER >= 4
void tud_vendor_iso_rx_cb(uint8_t idx, const uint8_t* buffer, uint32_t bufsize) {
  (void) idx;
  (void) buffer;
  (void) bufsize;
  tud_vendor_iso_read_xfer();
}

void tud_vendor_iso_tx_cb(uint8_t idx, uint32_t sent_bytes) {
  (void) idx;
  (void) sent_bytes;
  tud_vendor_iso_write(iso_tx_chunk, usbtest_iso_len());
}
#endif

//--------------------------------------------------------------------+
// Vendor control requests (EP0)
//--------------------------------------------------------------------+

// Control write/read-back for the ctrl_out tests (14/21), same protocol as
// Gadget Zero: 0x5b stores the host's wLength bytes, 0x5c returns them.
static uint8_t ctrl_buf[1024];

// Invoked on vendor control transfers, and by usbd for forwarded standard
// endpoint requests (halt set/clear) whose return value it ignores — return
// false for anything that is not a supported vendor request.
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
  if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR) {
    return false;
  }

  switch (request->bRequest) {
    case 0x5b: // control WRITE: receive wLength bytes into ctrl_buf
      TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_OUT);
      TU_VERIFY(request->wValue == 0 && request->wIndex == 0);
      TU_VERIFY(request->wLength <= sizeof(ctrl_buf));
      if (stage == CONTROL_STAGE_SETUP) {
        return tud_control_xfer(rhport, request, ctrl_buf, request->wLength);
      }
      return true; // DATA/ACK: payload already landed in ctrl_buf

    case 0x5c: // control READ: send back the previously written bytes
      TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_IN);
      TU_VERIFY(request->wValue == 0 && request->wIndex == 0);
      TU_VERIFY(request->wLength <= sizeof(ctrl_buf));
      if (stage == CONTROL_STAGE_SETUP) {
        return tud_control_xfer(rhport, request, ctrl_buf, request->wLength);
      }
      return true;

    default:
      return false;
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void* param) {
  (void) param;
  static uint32_t start_ms = 0;
  static bool led_state = false;

  while (1) {
    #if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
    #else
    // Blink every interval ms
    if (tusb_time_millis_api() - start_ms < blink_interval_ms) {
      return; // not enough time
    }
    #endif

    start_ms += blink_interval_ms;
    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
}

//--------------------------------------------------------------------+
// FreeRTOS
//--------------------------------------------------------------------+
#if CFG_TUSB_OS == OPT_OS_FREERTOS

#define BLINKY_STACK_SIZE    configMINIMAL_STACK_SIZE
#define USBTEST_STACK_SIZE   (configMINIMAL_STACK_SIZE*2)

#ifdef ESP_PLATFORM
  #define USBD_STACK_SIZE    4096
  int main(void);
  void app_main(void) {
    main();
  }
#else
  // Increase stack size when debug log is enabled
  #define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#endif

// static task allocation
#if configSUPPORT_STATIC_ALLOCATION
StackType_t  blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

StackType_t  usbtest_stack[USBTEST_STACK_SIZE];
StaticTask_t usbtest_taskdef;
#endif

// USB Device Driver task: processes all usb events and invokes callbacks
void usb_device_task(void* param) {
  (void) param;

  // init device stack on configured roothub port. Must be called after the
  // scheduler starts: the USB IRQ handler uses RTOS queue APIs.
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();

  // RTOS forever loop
  while (1) {
    tud_task(); // put thread to waiting state until there is a new event
  }
}

void freertos_init(void) {
  #if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, blinky_stack, &blinky_taskdef);
  xTaskCreateStatic(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_device_stack, &usb_device_taskdef);
  xTaskCreateStatic(usbtest_task, "usbtest", USBTEST_STACK_SIZE, NULL, configMAX_PRIORITIES-2, usbtest_stack, &usbtest_taskdef);
  #else
  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, NULL);
  xTaskCreate(usbtest_task, "usbtest", USBTEST_STACK_SIZE, NULL, configMAX_PRIORITIES-2, NULL);
  #endif

  // only start scheduler for non-espressif mcu (espressif starts it in startup code)
  #ifndef ESP_PLATFORM
  vTaskStartScheduler();
  #endif
}
#endif
