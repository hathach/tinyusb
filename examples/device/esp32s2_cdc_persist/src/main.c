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
 */

/*
 * This example showcases ESP32-S2's ability to keep the USB peripheral active
 * while rebooting into download mode to flash a new firmware. This makes the CDC
 * behave just like a normal USB-UART chip and not lose/change the serial port
 * at each step of the process.
 *
 * It is important to remember that this feature should be used in cases where only
 * CDC is active. This feature will break USB standarts for any other device driver
 * and can lead to different issues, most rendering the USB unusable until full
 * power-on reset is performed.
 *
 * To see this feature in action, first build and upload this example to the board,
 * then execute the following command in the example root:
 * idf.py -p [usb cdc port] monitor
 * Once you are connected to the CDC, you can press Ctrl+T and then Ctrl+A to reflash
 * the firmware with persistence. idf-monitor will automatically reconnect to
 * CDC once flashing has completed.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#include "bsp/board.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// ESP32-S2 USB Persistence
//--------------------------------------------------------------------+

#include "soc/usb_periph.h"
#include "soc/usb_wrap_struct.h"
#include "driver/periph_ctrl.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32s2/rom/usb/usb_persist.h"

typedef enum {
  PERSIST_DISABLED,
  PERSIST_ENABLED
} persist_type_t;

typedef enum {
  RESET_DISCONNECTED,
  RESET_IDLE,
  RESET_STEP_1,//!dtr &&  rts
  RESET_STEP_2,// dtr &&  rts
  RESET_STEP_3,// dtr && !rts
  RESET_DONE   //!dtr && !rts
} reset_step_t;

static persist_type_t persist_type = PERSIST_DISABLED;
static reset_step_t reset_step = RESET_DISCONNECTED;
static bool usb_did_persist = false;

static void IRAM_ATTR usb_persist_shutdown_handler(void)
{
  if (persist_type != PERSIST_DISABLED)
  {
    // Disable USB/IO_MUX peripheral reset
    REG_SET_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_IO_MUX_RESET_DISABLE);
    REG_SET_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_USB_RESET_DISABLE);
    // Set persistence flag for the bootloader
    USB_WRAP.date.val = USBDC_PERSIST_ENA;
    // Reboot into download mode
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    periph_module_disable(PERIPH_TIMG1_MODULE);
    SET_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_SW_PROCPU_RST);
  }
}

static void usb_persist_restart(void)
{
  persist_type = PERSIST_ENABLED;
  esp_restart();
}

static void usb_persist_init(void)
{
  usb_did_persist = (USB_WRAP.date.val == USBDC_PERSIST_ENA);
  ets_printf("DID PERSIST: %s\n", usb_did_persist?"TRUE":"FALSE");
  ESP_ERROR_CHECK(esp_register_shutdown_handler(usb_persist_shutdown_handler));
}

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

// static timer
StaticTimer_t blinky_tmdef;
TimerHandle_t blinky_tm;

// static task for usbd
// Increase stack size when debug log is enabled
#if CFG_TUSB_DEBUG
  #define USBD_STACK_SIZE     (3*configMINIMAL_STACK_SIZE)
#else
  #define USBD_STACK_SIZE     (3*configMINIMAL_STACK_SIZE/2)
#endif

StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

// static task for cdc
#define CDC_STACK_SZIE      configMINIMAL_STACK_SIZE
StackType_t  cdc_stack[CDC_STACK_SZIE];
StaticTask_t cdc_taskdef;


void led_blinky_cb(TimerHandle_t xTimer);
void usb_device_task(void* param);
void cdc_task(void* params);

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

void app_main(void)
{
  usb_persist_init();
  board_init();
  tusb_init();

  // soft timer for blinky
  blinky_tm = xTimerCreateStatic(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), true, NULL, led_blinky_cb, &blinky_tmdef);
  xTimerStart(blinky_tm, 0);

  // Create a task for tinyusb device stack
  (void) xTaskCreateStatic( usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_device_stack, &usb_device_taskdef);

  // Create CDC task
  (void) xTaskCreateStatic( cdc_task, "cdc", CDC_STACK_SZIE, NULL, configMAX_PRIORITIES-2, cdc_stack, &cdc_taskdef);
}

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
void usb_device_task(void* param)
{
  (void) param;

  // RTOS forever loop
  while (1)
  {
    // tinyusb device task
    tud_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_SUSPENDED), 0);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void* params)
{
  (void) params;

  // RTOS forever loop
  while ( 1 )
  {
    if ( tud_cdc_connected() )
    {
      // connected and there are data available
      if ( tud_cdc_available() )
      {
        uint8_t buf[64];

        // read and echo back
        uint32_t count = tud_cdc_read(buf, sizeof(buf));

        for(uint32_t i=0; i<count; i++)
        {
          tud_cdc_write_char(buf[i]);

          // it seems idf-monitor is not happy with just /r/n
          if ( buf[i] == '\r' ) tud_cdc_write_str("\r\n");
        }

        tud_cdc_write_flush();
      }
    }

    // For ESP32-S2 this delay is essential to allow idle how to run and reset wdt
    vTaskDelay(2); // 2 OS Ticks
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;

  ets_printf("DTR: %u, RTS: %u\n", dtr, rts);

  if(!dtr && rts){
    if(reset_step == RESET_IDLE){
      reset_step = RESET_STEP_1;
    } else {
      reset_step = RESET_IDLE;
    }
  } else if(dtr && rts){
    if(reset_step == RESET_STEP_1){
      reset_step = RESET_STEP_2;
    } else {
      ets_printf("CDC Connected\n");
      reset_step = RESET_IDLE;
    }
  } else if(dtr && !rts){
    if(reset_step == RESET_STEP_2){
      reset_step = RESET_STEP_3;
    } else {
      reset_step = RESET_IDLE;
    }
  } else if(!dtr && !rts){
    if(reset_step == RESET_STEP_3){
      //restart into download mode
      usb_persist_restart();
    } else {
      reset_step = RESET_DISCONNECTED;
      ets_printf("CDC Disconnected\n");
    }
  }

  //connected
  if ( dtr && rts && reset_step == RESET_IDLE )
  {
    // print initial message when connected
    tud_cdc_write_str("\r\nTinyUSB CDC device with Persistence example\r\n");
    tud_cdc_write_flush();
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinky_cb(TimerHandle_t xTimer)
{
  (void) xTimer;
  static bool led_state = false;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
