#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#define CFG_TUSB_MCU OPT_MCU_NONE
#define CFG_TUSB_OS OPT_OS_NONE

// Required by tusb_option.h's unconditional endpoint-limit check.
// Matches the other synthetic-controller harnesses; does not enable USB device mode.
#define TUP_DCD_ENDPOINT_MAX 8

#define CFG_TUH_ENABLED 1
#define CFG_TUD_ENABLED 0
#define CFG_TUH_DEVICE_MAX 1
#define CFG_TUH_HUB 0
#define CFG_TUH_ENUMERATION_BUFSIZE 256

#endif
