// PVS-Studio compile-only configuration (tools/pvs_studio.py): device and host on OPT_MCU_NONE,
// the configuration src/portable/template/ compiles under.
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#define CFG_TUSB_OS          OPT_OS_NONE
#define TUP_DCD_ENDPOINT_MAX 8 // OPT_MCU_NONE names no controller
#define CFG_TUSB_MEM_ALIGN   __attribute__((aligned(4)))
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUSB_RHPORT1_MODE (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)
#define CFG_TUD_ENABLED      1
#define CFG_TUH_ENABLED      1
#define CFG_TUD_ENDPOINT0_SIZE 64
#define CFG_TUD_CDC          1
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 64
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_DEVICE_MAX   1

#endif
