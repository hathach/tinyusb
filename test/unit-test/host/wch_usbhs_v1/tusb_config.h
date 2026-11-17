// SPDX-License-Identifier: MIT
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_
#define CFG_TUSB_MCU OPT_MCU_NONE
#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUSB_DEBUG 0
#define CFG_TUH_ENABLED 1
#ifndef CFG_TUH_MAX_SPEED
  #define CFG_TUH_MAX_SPEED OPT_MODE_HIGH_SPEED
#endif
#define CFG_TUH_DEVICE_MAX 4
#ifndef CFG_TUH_HUB
  #define CFG_TUH_HUB 1
#endif
#define CFG_TUH_WCH_USBIP_USBHS 1
#define TUP_USBIP_WCH_USBHS_V1
#endif
