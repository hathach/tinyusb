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

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]  VIDEO | AUDIO | MIDI | HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
    _PID_MAP(MIDI, 3) | _PID_MAP(AUDIO, 4) | _PID_MAP(VIDEO, 5) | _PID_MAP(VENDOR, 6) )

#define USB_VID   0xCafe
#define USB_BCD   0x0200

// String Descriptor Index
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_UVC_CONTROL_1,
  STRID_UVC_STREAMING_1,
  STRID_UVC_CONTROL_2,
  STRID_UVC_STREAMING_2,
};

// array of pointer to string descriptors
char const* string_desc_arr[] = {
    (const char[]) {0x09, 0x04}, // 0: is supported language is English (0x0409)
    "TinyUSB",                   // 1: Manufacturer
    "TinyUSB Device",            // 2: Product
    NULL,                        // 3: Serials will use unique ID if possible
    "UVC Control 1",             // 4: UVC Interface 1
    "UVC Streaming 1",           // 5: UVC Interface 1
    "UVC Control 2",             // 6: UVC Interface 2
    "UVC Streaming 2",           // 7: UVC Interface 2

};

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

    // Use Interface Association Descriptor (IAD) for Video
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const* tud_descriptor_device_cb(void) {
  return (uint8_t const*) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

/* Time stamp base clock. It is a deprecated parameter. */
#define UVC_CLOCK_FREQUENCY    27000000

/* video capture path */
#define UVC_ENTITY_CAP_INPUT_TERMINAL  0x01
#define UVC_ENTITY_CAP_OUTPUT_TERMINAL 0x02

enum {
  ITF_NUM_VIDEO_CONTROL_1,
  ITF_NUM_VIDEO_STREAMING_1,
  ITF_NUM_VIDEO_CONTROL_2,
  ITF_NUM_VIDEO_STREAMING_2,
  ITF_NUM_TOTAL
};

#define EPNUM_VIDEO_IN_1    0x81
#define EPNUM_VIDEO_IN_2    0x82

#if defined(CFG_EXAMPLE_VIDEO_READONLY) && !defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
  #define USE_MJPEG 1
#else
  #define USE_MJPEG 0
#endif

#define USE_ISO_STREAMING (!CFG_TUD_VIDEO_STREAMING_BULK)

typedef struct TU_ATTR_PACKED {
  tusb_desc_interface_t itf;
  tusb_desc_video_control_header_1itf_t header;
  tusb_desc_video_control_camera_terminal_t camera_terminal;
  tusb_desc_video_control_output_terminal_t output_terminal;
} uvc_control_desc_t;

/* Windows support YUY2 and NV12
 * https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/usb-video-class-driver-overview */

typedef struct TU_ATTR_PACKED {
  tusb_desc_interface_t itf;
  tusb_desc_video_streaming_input_header_1byte_t header;
  tusb_desc_video_format_uncompressed_t format;
  tusb_desc_video_frame_uncompressed_continuous_t frame;
  tusb_desc_video_streaming_color_matching_t color;

#if USE_ISO_STREAMING
  // For ISO streaming, USB spec requires to alternate interface
  tusb_desc_interface_t itf_alt;
#endif

  tusb_desc_endpoint_t ep;
} uvc_streaming_yuy2_desc_t;

typedef struct TU_ATTR_PACKED {
  tusb_desc_interface_t itf;
  tusb_desc_video_streaming_input_header_1byte_t header;
  tusb_desc_video_format_mjpeg_t format;
  tusb_desc_video_frame_mjpeg_continuous_t frame;
  tusb_desc_video_streaming_color_matching_t color;

#if USE_ISO_STREAMING
  // For ISO streaming, USB spec requires to alternate interface
  tusb_desc_interface_t itf_alt;
#endif

  tusb_desc_endpoint_t ep;
} uvc_streaming_mpeg_desc_t;

typedef struct TU_ATTR_PACKED {
  tusb_desc_configuration_t config;

  struct TU_ATTR_PACKED {
    tusb_desc_interface_assoc_t iad;
    uvc_control_desc_t video_control;
    uvc_streaming_yuy2_desc_t video_streaming;
  } uvc_yuy2;

  struct TU_ATTR_PACKED {
    tusb_desc_interface_assoc_t iad;
    uvc_control_desc_t video_control;
    uvc_streaming_mpeg_desc_t video_streaming;
  } uvc_mpeg;
} uvc_cfg_desc_t;

const uvc_cfg_desc_t desc_fs_configuration = {
    .config = {
        .bLength = sizeof(tusb_desc_configuration_t),
        .bDescriptorType = TUSB_DESC_CONFIGURATION,

        .wTotalLength = sizeof(uvc_cfg_desc_t),
        .bNumInterfaces = ITF_NUM_TOTAL,
        .bConfigurationValue = 1,
        .iConfiguration = 0,
        .bmAttributes =  TU_BIT(7),
        .bMaxPower = 100 / 2
    },
    //------------- Stream 0: YUY2 -------------//
    .uvc_yuy2 = {
        .iad = {
            .bLength = sizeof(tusb_desc_interface_assoc_t),
            .bDescriptorType = TUSB_DESC_INTERFACE_ASSOCIATION,

            .bFirstInterface = ITF_NUM_VIDEO_CONTROL_1,
            .bInterfaceCount = 2,
            .bFunctionClass = TUSB_CLASS_VIDEO,
            .bFunctionSubClass = VIDEO_SUBCLASS_INTERFACE_COLLECTION,
            .bFunctionProtocol = VIDEO_ITF_PROTOCOL_UNDEFINED,
            .iFunction = 0
        },
        .video_control = {
            .itf = {
                .bLength = sizeof(tusb_desc_interface_t),
                .bDescriptorType = TUSB_DESC_INTERFACE,

                .bInterfaceNumber = ITF_NUM_VIDEO_CONTROL_1,
                .bAlternateSetting = 0,
                .bNumEndpoints = 0,
                .bInterfaceClass = TUSB_CLASS_VIDEO,
                .bInterfaceSubClass = VIDEO_SUBCLASS_CONTROL,
                .bInterfaceProtocol = VIDEO_ITF_PROTOCOL_15,
                .iInterface = STRID_UVC_CONTROL_1
            },
            .header = {
                .bLength = sizeof(tusb_desc_video_control_header_1itf_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VC_HEADER,

                .bcdUVC = VIDEO_BCD_1_50,
                .wTotalLength = sizeof(uvc_control_desc_t) - sizeof(tusb_desc_interface_t), // CS VC descriptors only
                .dwClockFrequency = UVC_CLOCK_FREQUENCY,
                .bInCollection = 1,
                .baInterfaceNr = {ITF_NUM_VIDEO_STREAMING_1}
            },
            .camera_terminal = {
                .bLength = sizeof(tusb_desc_video_control_camera_terminal_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VC_INPUT_TERMINAL,

                .bTerminalID = UVC_ENTITY_CAP_INPUT_TERMINAL,
                .wTerminalType = VIDEO_ITT_CAMERA,
                .bAssocTerminal = 0,
                .iTerminal = 0,
                .wObjectiveFocalLengthMin = 0,
                .wObjectiveFocalLengthMax = 0,
                .wOcularFocalLength = 0,
                .bControlSize = 3,
                .bmControls = {0, 0, 0}
            },
            .output_terminal = {
                .bLength = sizeof(tusb_desc_video_control_output_terminal_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VC_OUTPUT_TERMINAL,

                .bTerminalID = UVC_ENTITY_CAP_OUTPUT_TERMINAL,
                .wTerminalType = VIDEO_TT_STREAMING,
                .bAssocTerminal = 0,
                .bSourceID = UVC_ENTITY_CAP_INPUT_TERMINAL,
                .iTerminal = 0
            }
        },

        .video_streaming = {
            .itf = {
                .bLength = sizeof(tusb_desc_interface_t),
                .bDescriptorType = TUSB_DESC_INTERFACE,

                .bInterfaceNumber = ITF_NUM_VIDEO_STREAMING_1,
                .bAlternateSetting = 0,
                .bNumEndpoints = CFG_TUD_VIDEO_STREAMING_BULK, // bulk 1, iso 0
                .bInterfaceClass = TUSB_CLASS_VIDEO,
                .bInterfaceSubClass = VIDEO_SUBCLASS_STREAMING,
                .bInterfaceProtocol = VIDEO_ITF_PROTOCOL_15,
                .iInterface = STRID_UVC_STREAMING_1
            },
            .header = {
                .bLength = sizeof(tusb_desc_video_streaming_input_header_1byte_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VS_INPUT_HEADER,

                .bNumFormats = 1,
                .wTotalLength = sizeof(uvc_streaming_yuy2_desc_t) - sizeof(tusb_desc_interface_t)
                                - sizeof(tusb_desc_endpoint_t) -
                                (USE_ISO_STREAMING ? sizeof(tusb_desc_interface_t) : 0), // CS VS descriptors only
                .bEndpointAddress = EPNUM_VIDEO_IN_1,
                .bmInfo = 0,
                .bTerminalLink = UVC_ENTITY_CAP_OUTPUT_TERMINAL,
                .bStillCaptureMethod = 0,
                .bTriggerSupport = 0,
                .bTriggerUsage = 0,
                .bControlSize = 1,
                .bmaControls = {0}
            },
            .format = {
                .bLength = sizeof(tusb_desc_video_format_uncompressed_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED,
                .bFormatIndex = 1, // 1-based index
                .bNumFrameDescriptors = 1,
                .guidFormat = {TUD_VIDEO_GUID_YUY2},
                .bBitsPerPixel = 16,
                .bDefaultFrameIndex = 1,
                .bAspectRatioX = 0,
                .bAspectRatioY = 0,
                .bmInterlaceFlags = 0,
                .bCopyProtect = 0
            },
            .frame = {
                .bLength = sizeof(tusb_desc_video_frame_uncompressed_continuous_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VS_FRAME_UNCOMPRESSED,
                .bFrameIndex = 1, // 1-based index
                .bmCapabilities = 0,
                .wWidth = FRAME_WIDTH,
                .wHeight = FRAME_HEIGHT,
                .dwMinBitRate = FRAME_WIDTH * FRAME_HEIGHT * 16 * 1,
                .dwMaxBitRate = FRAME_WIDTH * FRAME_HEIGHT * 16 * FRAME_RATE,
                .dwMaxVideoFrameBufferSize = FRAME_WIDTH * FRAME_HEIGHT * 16 / 8,
                .dwDefaultFrameInterval = 10000000 / FRAME_RATE,
                .bFrameIntervalType = 0, // continuous
                .dwFrameInterval = {
                    10000000 / FRAME_RATE, // min
                    10000000, // max
                    10000000 / FRAME_RATE // step
                }
            },
            .color = {
                .bLength = sizeof(tusb_desc_video_streaming_color_matching_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VS_COLORFORMAT,

                .bColorPrimaries = VIDEO_COLOR_PRIMARIES_BT709,
                .bTransferCharacteristics = VIDEO_COLOR_XFER_CH_BT709,
                .bMatrixCoefficients = VIDEO_COLOR_COEF_SMPTE170M
            },

#if USE_ISO_STREAMING
            .itf_alt = {
                .bLength = sizeof(tusb_desc_interface_t),
                .bDescriptorType = TUSB_DESC_INTERFACE,

                .bInterfaceNumber = ITF_NUM_VIDEO_STREAMING_1,
                .bAlternateSetting = 1,
                .bNumEndpoints = 1,
                .bInterfaceClass = TUSB_CLASS_VIDEO,
                .bInterfaceSubClass = VIDEO_SUBCLASS_STREAMING,
                .bInterfaceProtocol = VIDEO_ITF_PROTOCOL_15,
                .iInterface = STRID_UVC_STREAMING_1
            },
#endif
            .ep = {
                .bLength = sizeof(tusb_desc_endpoint_t),
                .bDescriptorType = TUSB_DESC_ENDPOINT,

                .bEndpointAddress = EPNUM_VIDEO_IN_1,
                .bmAttributes = {
                    .xfer = CFG_TUD_VIDEO_STREAMING_BULK ? TUSB_XFER_BULK : TUSB_XFER_ISOCHRONOUS,
                    .sync = CFG_TUD_VIDEO_STREAMING_BULK ? 0 : 1 // asynchronous
                },
                .wMaxPacketSize = CFG_TUD_VIDEO_STREAMING_BULK ? 64 : CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE,
                .bInterval = 1
            }
        }
    },
      //------------- Stream 1: MPEG -------------//
    .uvc_mpeg = {
        .iad = {
            .bLength = sizeof(tusb_desc_interface_assoc_t),
            .bDescriptorType = TUSB_DESC_INTERFACE_ASSOCIATION,

            .bFirstInterface = ITF_NUM_VIDEO_CONTROL_2,
            .bInterfaceCount = 2,
            .bFunctionClass = TUSB_CLASS_VIDEO,
            .bFunctionSubClass = VIDEO_SUBCLASS_INTERFACE_COLLECTION,
            .bFunctionProtocol = VIDEO_ITF_PROTOCOL_UNDEFINED,
            .iFunction = 0
        },

        .video_control = {
            .itf = {
                .bLength = sizeof(tusb_desc_interface_t),
                .bDescriptorType = TUSB_DESC_INTERFACE,

                .bInterfaceNumber = ITF_NUM_VIDEO_CONTROL_2,
                .bAlternateSetting = 0,
                .bNumEndpoints = 0,
                .bInterfaceClass = TUSB_CLASS_VIDEO,
                .bInterfaceSubClass = VIDEO_SUBCLASS_CONTROL,
                .bInterfaceProtocol = VIDEO_ITF_PROTOCOL_15,
                .iInterface = STRID_UVC_CONTROL_2
            },
            .header = {
                .bLength = sizeof(tusb_desc_video_control_header_1itf_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VC_HEADER,

                .bcdUVC = VIDEO_BCD_1_50,
                .wTotalLength = sizeof(uvc_control_desc_t) - sizeof(tusb_desc_interface_t), // CS VC descriptors only
                .dwClockFrequency = UVC_CLOCK_FREQUENCY,
                .bInCollection = 1,
                .baInterfaceNr = { ITF_NUM_VIDEO_STREAMING_2 }
            },
            .camera_terminal = {
                .bLength = sizeof(tusb_desc_video_control_camera_terminal_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VC_INPUT_TERMINAL,

                .bTerminalID = UVC_ENTITY_CAP_INPUT_TERMINAL,
                .wTerminalType = VIDEO_ITT_CAMERA,
                .bAssocTerminal = 0,
                .iTerminal = 0,
                .wObjectiveFocalLengthMin = 0,
                .wObjectiveFocalLengthMax = 0,
                .wOcularFocalLength = 0,
                .bControlSize = 3,
                .bmControls = { 0, 0, 0 }
            },
            .output_terminal = {
                .bLength = sizeof(tusb_desc_video_control_output_terminal_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VC_OUTPUT_TERMINAL,

                .bTerminalID = UVC_ENTITY_CAP_OUTPUT_TERMINAL,
                .wTerminalType = VIDEO_TT_STREAMING,
                .bAssocTerminal = 0,
                .bSourceID = UVC_ENTITY_CAP_INPUT_TERMINAL,
                .iTerminal = 0
            }
        },

        .video_streaming = {
            .itf = {
                .bLength = sizeof(tusb_desc_interface_t),
                .bDescriptorType = TUSB_DESC_INTERFACE,

                .bInterfaceNumber = ITF_NUM_VIDEO_STREAMING_2,
                .bAlternateSetting = 0,
                .bNumEndpoints = CFG_TUD_VIDEO_STREAMING_BULK, // bulk 1, iso 0
                .bInterfaceClass = TUSB_CLASS_VIDEO,
                .bInterfaceSubClass = VIDEO_SUBCLASS_STREAMING,
                .bInterfaceProtocol = VIDEO_ITF_PROTOCOL_15,
                .iInterface = STRID_UVC_STREAMING_2
            },
            .header = {
                .bLength = sizeof(tusb_desc_video_streaming_input_header_1byte_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VS_INPUT_HEADER,

                .bNumFormats = 1,
                .wTotalLength = sizeof(uvc_streaming_mpeg_desc_t) - sizeof(tusb_desc_interface_t)
                                - sizeof(tusb_desc_endpoint_t) - (USE_ISO_STREAMING ? sizeof(tusb_desc_interface_t) : 0) , // CS VS descriptors only
                .bEndpointAddress = EPNUM_VIDEO_IN_2,
                .bmInfo = 0,
                .bTerminalLink = UVC_ENTITY_CAP_OUTPUT_TERMINAL,
                .bStillCaptureMethod = 0,
                .bTriggerSupport = 0,
                .bTriggerUsage = 0,
                .bControlSize = 1,
                .bmaControls = { 0 }
            },
            .format = {
                .bLength = sizeof(tusb_desc_video_format_mjpeg_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VS_FORMAT_MJPEG,
                .bFormatIndex = 1, // 1-based index
                .bNumFrameDescriptors = 1,
                .bmFlags = 0,
                .bDefaultFrameIndex = 1,
                .bAspectRatioX = 0,
                .bAspectRatioY = 0,
                .bmInterlaceFlags = 0,
                .bCopyProtect = 0
            },
            .frame = {
                .bLength = sizeof(tusb_desc_video_frame_mjpeg_continuous_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VS_FRAME_MJPEG,
                .bFrameIndex = 1, // 1-based index
                .bmCapabilities = 0,
                .wWidth = FRAME_WIDTH,
                .wHeight = FRAME_HEIGHT,
                .dwMinBitRate = FRAME_WIDTH * FRAME_HEIGHT * 16 * 1,
                .dwMaxBitRate = FRAME_WIDTH * FRAME_HEIGHT * 16 * FRAME_RATE,
                .dwMaxVideoFrameBufferSize = FRAME_WIDTH * FRAME_HEIGHT * 16 / 8,
                .dwDefaultFrameInterval = 10000000 / FRAME_RATE,
                .bFrameIntervalType = 0, // continuous
                .dwFrameInterval = {
                    10000000 / FRAME_RATE, // min
                    10000000, // max
                    10000000 / FRAME_RATE // step
                }
            },
            .color = {
                .bLength = sizeof(tusb_desc_video_streaming_color_matching_t),
                .bDescriptorType = TUSB_DESC_CS_INTERFACE,
                .bDescriptorSubType = VIDEO_CS_ITF_VS_COLORFORMAT,

                .bColorPrimaries = VIDEO_COLOR_PRIMARIES_BT709,
                .bTransferCharacteristics = VIDEO_COLOR_XFER_CH_BT709,
                .bMatrixCoefficients = VIDEO_COLOR_COEF_SMPTE170M
            },

#if USE_ISO_STREAMING
            .itf_alt = {
                .bLength = sizeof(tusb_desc_interface_t),
                .bDescriptorType = TUSB_DESC_INTERFACE,

                .bInterfaceNumber = ITF_NUM_VIDEO_STREAMING_2,
                .bAlternateSetting = 1,
                .bNumEndpoints = 1,
                .bInterfaceClass = TUSB_CLASS_VIDEO,
                .bInterfaceSubClass = VIDEO_SUBCLASS_STREAMING,
                .bInterfaceProtocol = VIDEO_ITF_PROTOCOL_15,
                .iInterface = STRID_UVC_STREAMING_2
            },
#endif
            .ep = {
                .bLength = sizeof(tusb_desc_endpoint_t),
                .bDescriptorType = TUSB_DESC_ENDPOINT,

                .bEndpointAddress = EPNUM_VIDEO_IN_2,
                .bmAttributes = {
                    .xfer = CFG_TUD_VIDEO_STREAMING_BULK ? TUSB_XFER_BULK : TUSB_XFER_ISOCHRONOUS,
                    .sync = CFG_TUD_VIDEO_STREAMING_BULK ? 0 : 1 // asynchronous
                },
                .wMaxPacketSize = CFG_TUD_VIDEO_STREAMING_BULK ? 64 : CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE,
                .bInterval = 1
            }
        }
    }
};

#if TUD_OPT_HIGH_SPEED
uvc_cfg_desc_t desc_hs_configuration;

static uint8_t * get_hs_configuration_desc(void) {
  static bool init = false;

  if (!init) {
    desc_hs_configuration = desc_fs_configuration;
    // change endpoint bulk size to 512 if bulk streaming
    if (CFG_TUD_VIDEO_STREAMING_BULK) {
      desc_hs_configuration.uvc_yuy2.video_streaming.ep.wMaxPacketSize = 512;
      desc_hs_configuration.uvc_mpeg.video_streaming.ep.wMaxPacketSize = 512;
    }
  }
  init = true;

  return (uint8_t *) &desc_hs_configuration;
}

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const* tud_descriptor_device_qualifier_cb(void) {
  return (uint8_t const*) &desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index) {
  (void) index; // for multiple configurations
  // if link speed is high return fullspeed config, and vice versa
  if (tud_speed_get() == TUSB_SPEED_HIGH) {
    return (uint8_t const*) &desc_fs_configuration;
  } else {
    return get_hs_configuration_desc();
  }
}
#endif // highspeed

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  if (tud_speed_get() == TUSB_SPEED_HIGH) {
    return get_hs_configuration_desc();
  } else
#endif
  {
    return (uint8_t const*) &desc_fs_configuration;
  }
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  switch (index) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;

      const char* str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
      if (chr_count > max_count) chr_count = max_count;

      // Convert ASCII string into UTF-16
      for (size_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}
