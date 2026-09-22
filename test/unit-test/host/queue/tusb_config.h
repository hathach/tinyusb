// SPDX-License-Identifier: MIT
#define CFG_TUSB_MCU OPT_MCU_NONE
#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUSB_DEBUG 0
#define CFG_TUH_ENABLED 1
#define CFG_TUH_MAX_SPEED OPT_MODE_HIGH_SPEED
#define CFG_TUH_DEVICE_MAX 1
// The runner selects the stub HCD's capacity for deeper-queue experiments.
#ifndef TUP_HCD_XFER_QUEUE_DEPTH
  #define TUP_HCD_XFER_QUEUE_DEPTH 2
#endif
#define CFG_TUH_API_EDPT_XFER 1

#define CFG_TUH_AUDIO 1
