// PVS-Studio compile-only configuration (tools/pvs_studio.py): every host class, hub, all CDC
// serial bridges and the non-default side of each boolean option.
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifndef BOARD_TUH_RHPORT
  #define BOARD_TUH_RHPORT 0
#endif
#ifndef BOARD_TUH_MAX_SPEED
  #define BOARD_TUH_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

#define CFG_TUSB_OS          OPT_OS_NONE
#define CFG_TUSB_DEBUG       3
#define CFG_TUSB_MEM_ALIGN   __attribute__((aligned(4)))
#define CFG_TUH_ENABLED      1
#define CFG_TUH_MAX_SPEED    BOARD_TUH_MAX_SPEED
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB          2
#define CFG_TUH_DEVICE_MAX   (3 * CFG_TUH_HUB + 1)
#define CFG_TUH_API_EDPT_XFER 1

#define CFG_TUH_CDC          2
#define CFG_TUH_CDC_FTDI     1
#define CFG_TUH_CDC_CP210X   1
#define CFG_TUH_CDC_CH34X    1
#define CFG_TUH_CDC_PL2303   1
#define CFG_TUH_CDC_LINE_CONTROL_ON_ENUM 0x03
#define CFG_TUH_CDC_LINE_CODING_ON_ENUM { 115200, CDC_LINE_CODING_STOP_BITS_1, CDC_LINE_CODING_PARITY_NONE, 8 }
#define CFG_TUH_HID          4
#define CFG_TUH_HID_SET_PROTOCOL_ON_ENUM 0
#define CFG_TUH_MSC          1
#define CFG_TUH_MSC_MAXLUN   2
#define CFG_TUH_MIDI         2
#define CFG_TUH_MIDI_STREAM_API 1
#define CFG_TUH_MIDI2        2
#define CFG_TUH_AUDIO        2

#endif
