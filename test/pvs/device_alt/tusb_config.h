// PVS-Studio compile-only configuration (tools/pvs_studio.py): the alternatives device_all does not
// take - DFU runtime, NCM, unbuffered vendor, no CDC notification - one instance each, OS custom.
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifndef BOARD_TUD_RHPORT
  #define BOARD_TUD_RHPORT 0
#endif
#ifndef BOARD_TUD_MAX_SPEED
  #define BOARD_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

#define CFG_TUSB_OS          OPT_OS_CUSTOM
#define CFG_TUSB_DEBUG       0
#define CFG_TUSB_MEM_ALIGN   __attribute__((aligned(4)))
#define CFG_TUD_ENABLED      1
#define CFG_TUD_MAX_SPEED    BOARD_TUD_MAX_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_CDC          1
#define CFG_TUD_CDC_NOTIFY   0
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 64
#define CFG_TUD_CDC_TX_OVERWRITABLE_IF_NOT_CONNECTED 1
#define CFG_TUD_MSC          1
#define CFG_TUD_MSC_EP_BUFSIZE 64
#define CFG_TUD_HID          1
#define CFG_TUD_MIDI         1
#define CFG_TUD_MIDI_RX_BUFSIZE 64
#define CFG_TUD_MIDI_TX_BUFSIZE 64
#define CFG_TUD_MIDI2        1
#define CFG_TUD_VENDOR       1
#define CFG_TUD_VENDOR_TXRX_BUFFERED 0
#define CFG_TUD_VENDOR_RX_BUFSIZE 0
#define CFG_TUD_VENDOR_TX_BUFSIZE 0
#define CFG_TUD_VENDOR_ALT_SETTINGS 1
#define CFG_TUD_VENDOR_EP_ISO_IN 1
#define CFG_TUD_VENDOR_EP_ISO_OUT 1
#define CFG_TUD_AUDIO        1
#define CFG_TUD_AUDIO_CTRL_BUF_SZ 64
#define CFG_TUD_AUDIO_ENABLE_EP_IN 1
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX 64
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ 128
#define CFG_TUD_VIDEO        1
#define CFG_TUD_VIDEO_STREAMING 1
#define CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE 64
#define CFG_TUD_DFU_RUNTIME  1
#define CFG_TUD_NCM          1
#define CFG_TUD_USBTMC       1
#define CFG_TUD_USBTMC_ENABLE_488 0
#define CFG_TUD_PRINTER      1
#define CFG_TUD_PRINTER_RX_BUFSIZE 64
#define CFG_TUD_PRINTER_TX_BUFSIZE 64
#define CFG_TUD_BTH          1
#define CFG_TUD_BTH_ISO_ALT_COUNT 0

// application breakpoint hook instead of the CPU halt
#define CFG_TUSB_DEBUG_BREAKPOINT app_debug_breakpoint

#endif
