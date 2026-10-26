// PVS-Studio compile-only configuration (tools/pvs_studio.py): one device, no hub, CDC ACM only,
// OS custom - the alternatives host_all does not take.
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifndef BOARD_TUH_RHPORT
  #define BOARD_TUH_RHPORT 0
#endif
#ifndef BOARD_TUH_MAX_SPEED
  #define BOARD_TUH_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

#define CFG_TUSB_OS          OPT_OS_CUSTOM
#define CFG_TUSB_DEBUG       0
#define CFG_TUSB_MEM_ALIGN   __attribute__((aligned(4)))
#define CFG_TUH_ENABLED      1
#define CFG_TUH_MAX_SPEED    BOARD_TUH_MAX_SPEED
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB          0
#define CFG_TUH_DEVICE_MAX   1

#define CFG_TUH_CDC          1
#define CFG_TUH_CDC_FTDI     0
#define CFG_TUH_CDC_CP210X   0
#define CFG_TUH_CDC_CH34X    0
#define CFG_TUH_CDC_PL2303   0
#define CFG_TUH_HID          1
#define CFG_TUH_MSC          1
#define CFG_TUH_MIDI         1
#define CFG_TUH_MIDI_STREAM_API 0
#define CFG_TUH_AUDIO        1

#endif
