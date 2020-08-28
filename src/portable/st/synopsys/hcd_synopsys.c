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

#include <inttypes.h>

#include "tusb_option.h"

// Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
// We disable SOF for now until needed later on
#define USE_SOF     0

#if TUSB_OPT_HOST_ENABLED && \
    ( (CFG_TUSB_MCU == OPT_MCU_STM32F1 && defined(STM32F1_SYNOPSYS)) || \
      CFG_TUSB_MCU == OPT_MCU_STM32F2 || \
      CFG_TUSB_MCU == OPT_MCU_STM32F4 || \
      CFG_TUSB_MCU == OPT_MCU_STM32F7 || \
      CFG_TUSB_MCU == OPT_MCU_STM32H7 || \
      (CFG_TUSB_MCU == OPT_MCU_STM32L4 && defined(STM32L4_SYNOPSYS)) \
    )

// EP_MAX       : Max number of bi-directional endpoints including EP0
// EP_FIFO_SIZE : Size of dedicated USB SRAM
#if CFG_TUSB_MCU == OPT_MCU_STM32F1
  #include "stm32f1xx.h"
  #define EP_MAX_FS       4
  #define EP_FIFO_SIZE_FS 1280

#elif CFG_TUSB_MCU == OPT_MCU_STM32F2
  #include "stm32f2xx.h"
  #define EP_MAX_FS       USB_OTG_FS_MAX_IN_ENDPOINTS
  #define EP_FIFO_SIZE_FS USB_OTG_FS_TOTAL_FIFO_SIZE

#elif CFG_TUSB_MCU == OPT_MCU_STM32F4
  #include "stm32f4xx.h"
  #define EP_MAX_FS       USB_OTG_FS_MAX_IN_ENDPOINTS
  #define EP_FIFO_SIZE_FS USB_OTG_FS_TOTAL_FIFO_SIZE
  #define EP_MAX_HS       USB_OTG_HS_MAX_IN_ENDPOINTS
  #define EP_FIFO_SIZE_HS USB_OTG_HS_TOTAL_FIFO_SIZE

#elif CFG_TUSB_MCU == OPT_MCU_STM32H7
  #include "stm32h7xx.h"
  #define EP_MAX_FS       9
  #define EP_FIFO_SIZE_FS 4096
  #define EP_MAX_HS       9
  #define EP_FIFO_SIZE_HS 4096

#elif CFG_TUSB_MCU == OPT_MCU_STM32F7
  #include "stm32f7xx.h"
  #define EP_MAX_FS       6
  #define EP_FIFO_SIZE_FS 1280
  #define EP_MAX_HS       9
  #define EP_FIFO_SIZE_HS 4096

#elif CFG_TUSB_MCU == OPT_MCU_STM32L4
  #include "stm32l4xx.h"
  #define EP_MAX_FS       6
  #define EP_FIFO_SIZE_FS 1280

#else
  #error "Unsupported MCUs"

#endif

#include "host/hcd.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

// On STM32 we associate Port0 to OTG_FS, and Port1 to OTG_HS
#if TUH_OPT_RHPORT == 0
  #define EP_MAX            EP_MAX_FS
  #define EP_FIFO_SIZE      EP_FIFO_SIZE_FS
  #define RHPORT_REGS_BASE  USB_OTG_FS_PERIPH_BASE
  #define RHPORT_IRQn       OTG_FS_IRQn
#else
  #define EP_MAX            EP_MAX_HS
  #define EP_FIFO_SIZE      EP_FIFO_SIZE_HS
  #define RHPORT_REGS_BASE  USB_OTG_HS_PERIPH_BASE
  #define RHPORT_IRQn       OTG_HS_IRQn
#endif

#define GLOBAL_BASE(_port)     ((USB_OTG_GlobalTypeDef*) RHPORT_REGS_BASE)
#define DEVICE_BASE(_port)     (USB_OTG_DeviceTypeDef *) (RHPORT_REGS_BASE + USB_OTG_DEVICE_BASE)
#define OUT_EP_BASE(_port)     (USB_OTG_OUTEndpointTypeDef *) (RHPORT_REGS_BASE + USB_OTG_OUT_ENDPOINT_BASE)
#define IN_EP_BASE(_port)      (USB_OTG_INEndpointTypeDef *) (RHPORT_REGS_BASE + USB_OTG_IN_ENDPOINT_BASE)
#define FIFO_BASE(_port, _x)   ((volatile uint32_t *) (RHPORT_REGS_BASE + USB_OTG_FIFO_BASE + (_x) * USB_OTG_FIFO_SIZE))

void hcd_int_enable(uint8_t rhport)
{
    (void) (rhport);
    NVIC_EnableIRQ(RHPORT_IRQn);
}

void hcd_int_disable(uint8_t rhport)
{
    (void) (rhport);
    NVIC_DisableIRQ(RHPORT_IRQn);
}

uint32_t hcd_ehci_register_addr(uint8_t rhport)
{
    (void) (rhport);
    return RHPORT_REGS_BASE + USB_OTG_HOST_BASE;
}

#endif
