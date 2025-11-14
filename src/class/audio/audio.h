/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber
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

/** \ingroup group_class
 *  \defgroup ClassDriver_Audio Audio
 *            Currently only MIDI subclass is supported
 *  @{ */

#ifndef TUSB_AUDIO_H__
#define TUSB_AUDIO_H__

#include "common/tusb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// GENERIC AUDIO CLASS CODES (COMMON TO UAC1 AND UAC2)
//--------------------------------------------------------------------+

/// A.2 - Audio Function Subclass Codes
typedef enum {
  AUDIO_FUNCTION_SUBCLASS_UNDEFINED = 0x00,
} audio_function_subclass_type_t;

/// A.3 - Audio Function Protocol Codes
typedef enum {
  AUDIO_FUNC_PROTOCOL_CODE_UNDEF = 0x00,
  AUDIO_FUNC_PROTOCOL_CODE_V1 = 0x00,///< Version 1.0 - same as undefined for backward compatibility
  AUDIO_FUNC_PROTOCOL_CODE_V2 = 0x20,///< Version 2.0
} audio_function_protocol_code_t;

/// A.5 - Audio Interface Subclass Codes
typedef enum {
  AUDIO_SUBCLASS_UNDEFINED = 0x00,
  AUDIO_SUBCLASS_CONTROL,       ///< Audio Control
  AUDIO_SUBCLASS_STREAMING,     ///< Audio Streaming
  AUDIO_SUBCLASS_MIDI_STREAMING,///< MIDI Streaming
} audio_subclass_type_t;

/// A.6 - Audio Interface Protocol Codes
typedef enum {
  AUDIO_INT_PROTOCOL_CODE_UNDEF = 0x00,
  AUDIO_INT_PROTOCOL_CODE_V1 = 0x00,///< Version 1.0 - same as undefined for backward compatibility
  AUDIO_INT_PROTOCOL_CODE_V2 = 0x20,///< Version 2.0
} audio_interface_protocol_code_t;

/// Terminal Types

/// 2.1 - Audio Class-Terminal Types
typedef enum {
  AUDIO_TERM_TYPE_USB_UNDEFINED = 0x0100,
  AUDIO_TERM_TYPE_USB_STREAMING = 0x0101,
  AUDIO_TERM_TYPE_USB_VENDOR_SPEC = 0x01FF,
} audio_terminal_type_t;

/// 2.2 - Audio Class-Input Terminal Types
typedef enum {
  AUDIO_TERM_TYPE_IN_UNDEFINED = 0x0200,
  AUDIO_TERM_TYPE_IN_GENERIC_MIC = 0x0201,
  AUDIO_TERM_TYPE_IN_DESKTOP_MIC = 0x0202,
  AUDIO_TERM_TYPE_IN_PERSONAL_MIC = 0x0203,
  AUDIO_TERM_TYPE_IN_OMNI_MIC = 0x0204,
  AUDIO_TERM_TYPE_IN_ARRAY_MIC = 0x0205,
  AUDIO_TERM_TYPE_IN_PROC_ARRAY_MIC = 0x0206,
} audio_terminal_input_type_t;

/// 2.3 - Audio Class-Output Terminal Types
typedef enum {
  AUDIO_TERM_TYPE_OUT_UNDEFINED = 0x0300,
  AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER = 0x0301,
  AUDIO_TERM_TYPE_OUT_HEADPHONES = 0x0302,
  AUDIO_TERM_TYPE_OUT_HEAD_MNT_DISP_AUIDO = 0x0303,
  AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER = 0x0304,
  AUDIO_TERM_TYPE_OUT_ROOM_SPEAKER = 0x0305,
  AUDIO_TERM_TYPE_OUT_COMMUNICATION_SPEAKER = 0x0306,
  AUDIO_TERM_TYPE_OUT_LOW_FRQ_EFFECTS_SPEAKER = 0x0307,
} audio_terminal_output_type_t;

/// Rest is yet to be implemented

//--------------------------------------------------------------------+
// USB AUDIO CLASS 1.0 (UAC1) DEFINITIONS
//--------------------------------------------------------------------+

/// A.5 - Audio Class-Specific AC Interface Descriptor Subtypes UAC1
typedef enum {
  AUDIO10_CS_AC_INTERFACE_AC_DESCRIPTOR_UNDEF = 0x00,
  AUDIO10_CS_AC_INTERFACE_HEADER = 0x01,
  AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL = 0x02,
  AUDIO10_CS_AC_INTERFACE_OUTPUT_TERMINAL = 0x03,
  AUDIO10_CS_AC_INTERFACE_MIXER_UNIT = 0x04,
  AUDIO10_CS_AC_INTERFACE_SELECTOR_UNIT = 0x05,
  AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT = 0x06,
  AUDIO10_CS_AC_INTERFACE_PROCESSING_UNIT = 0x07,
  AUDIO10_CS_AC_INTERFACE_EXTENSION_UNIT = 0x08,
} audio10_cs_ac_interface_subtype_t;

/// A.6 - Audio Class-Specific AS Interface Descriptor Subtypes UAC1
typedef enum {
  AUDIO10_CS_AS_INTERFACE_AS_DESCRIPTOR_UNDEF = 0x00,
  AUDIO10_CS_AS_INTERFACE_AS_GENERAL = 0x01,
  AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE = 0x02,
} audio10_cs_as_interface_subtype_t;

/// A.8 - Audio Class-Specific EP Descriptor Subtypes UAC1
typedef enum {
  AUDIO10_CS_EP_SUBTYPE_UNDEF = 0x00,
  AUDIO10_CS_EP_SUBTYPE_GENERAL = 0x01,
} audio10_cs_ep_subtype_t;

/// A.9 - Audio Class-Specific Request Codes UAC1
typedef enum {
  AUDIO10_CS_REQ_UNDEF = 0x00,
  AUDIO10_CS_REQ_SET_CUR = 0x01,
  AUDIO10_CS_REQ_GET_CUR = 0x81,
  AUDIO10_CS_REQ_SET_MIN = 0x02,
  AUDIO10_CS_REQ_GET_MIN = 0x82,
  AUDIO10_CS_REQ_SET_MAX = 0x03,
  AUDIO10_CS_REQ_GET_MAX = 0x83,
  AUDIO10_CS_REQ_SET_RES = 0x04,
  AUDIO10_CS_REQ_GET_RES = 0x84,
  AUDIO10_CS_REQ_SET_MEM = 0x05,
  AUDIO10_CS_REQ_GET_MEM = 0x85,
  AUDIO10_CS_REQ_GET_STAT = 0xFF,
} audio10_cs_req_t;

/// A.10.1 - Terminal Control Selectors UAC1
typedef enum {
  AUDIO10_TE_CTRL_UNDEF = 0x00,
  AUDIO10_TE_CTRL_COPY_PROTECT = 0x01,
} audio10_terminal_control_selector_t;

/// A.10.2 - Feature Unit Control Selectors UAC1
typedef enum {
  AUDIO10_FU_CTRL_UNDEF = 0x00,
  AUDIO10_FU_CTRL_MUTE = 0x01,
  AUDIO10_FU_CTRL_VOLUME = 0x02,
  AUDIO10_FU_CTRL_BASS = 0x03,
  AUDIO10_FU_CTRL_MID = 0x04,
  AUDIO10_FU_CTRL_TREBLE = 0x05,
  AUDIO10_FU_CTRL_GRAPHIC_EQUALIZER = 0x06,
  AUDIO10_FU_CTRL_AGC = 0x07,
  AUDIO10_FU_CTRL_DELAY = 0x08,
  AUDIO10_FU_CTRL_BASS_BOOST = 0x09,
  AUDIO10_FU_CTRL_LOUDNESS = 0x0A,
} audio10_feature_unit_control_selector_t;

/// A.10.3.1 - Up/Down-mix Processing Unit Control Selectors UAC1
typedef enum {
  AUDIO10_UD_CTRL_UNDEF = 0x00,
  AUDIO10_UD_CTRL_ENABLE = 0x01,
  AUDIO10_UD_CTRL_MODE_SELECT = 0x02,
} audio10_up_down_mix_control_selector_t;

/// A.10.3.2 - Dolby Prologic Processing Unit Control Selectors UAC1
typedef enum {
  AUDIO10_DP_CTRL_UNDEF = 0x00,
  AUDIO10_DP_CTRL_ENABLE = 0x01,
  AUDIO10_DP_CTRL_MODE_SELECT = 0x02,
} audio10_dolby_prologic_control_selector_t;

/// A.10.3.3 - 3D Stereo Extender Processing Unit Control Selectors UAC1
typedef enum {
  AUDIO10_3D_CTRL_UNDEF = 0x00,
  AUDIO10_3D_CTRL_ENABLE = 0x01,
  AUDIO10_3D_CTRL_SPACIOUSNESS = 0x02,
} audio10_3d_stereo_extender_control_selector_t;

/// A.10.3.4 - Reverberation Processing Unit Control Selectors UAC1
typedef enum {
  AUDIO10_RV_CTRL_UNDEF = 0x00,
  AUDIO10_RV_CTRL_ENABLE = 0x01,
  AUDIO10_RV_CTRL_REVERB_LEVEL = 0x02,
  AUDIO10_RV_CTRL_REVERB_TIME = 0x03,
  AUDIO10_RV_CTRL_REVERB_FEEDBACK = 0x04,
} audio10_reverberation_control_selector_t;

/// A.10.3.5 - Chorus Processing Unit Control Selectors UAC1
typedef enum {
  AUDIO10_CH_CTRL_UNDEF = 0x00,
  AUDIO10_CH_CTRL_ENABLE = 0x01,
  AUDIO10_CH_CTRL_CHORUS_LEVEL = 0x02,
  AUDIO10_CH_CTRL_CHORUS_RATE = 0x03,
  AUDIO10_CH_CTRL_CHORUS_DEPTH = 0x04,
} audio10_chorus_control_selector_t;

/// A.10.3.6 - Dynamic Range Compressor Processing Unit Control Selectors UAC1
typedef enum {
  AUDIO10_DR_CTRL_UNDEF = 0x00,
  AUDIO10_DR_CTRL_ENABLE = 0x01,
  AUDIO10_DR_CTRL_COMPRESSION_RATE = 0x02,
  AUDIO10_DR_CTRL_MAXAMPL = 0x03,
  AUDIO10_DR_CTRL_THRESHOLD = 0x04,
  AUDIO10_DR_CTRL_ATTACK_TIME = 0x05,
  AUDIO10_DR_CTRL_RELEASE_TIME = 0x06,
} audio10_dynamic_range_compression_control_selector_t;

/// A.10.4 - Extension Unit Control Selectors UAC1
typedef enum {
  AUDIO10_XU_CTRL_UNDEF = 0x00,
  AUDIO10_XU_CTRL_ENABLE = 0x01,
} audio10_extension_unit_control_selector_t;

/// A.10.5 - Endpoint Control Selectors UAC1
typedef enum {
  AUDIO10_EP_CTRL_UNDEF = 0x00,
  AUDIO10_EP_CTRL_SAMPLING_FREQ = 0x01,
  AUDIO10_EP_CTRL_PITCH = 0x02,
} audio10_ep_control_selector_t;

/// Audio Class-Specific AS Isochronous Data EP Attributes UAC1
typedef enum {
  AUDIO10_CS_AS_ISO_DATA_EP_ATT_MAX_PACKETS_ONLY = 0x80,
  AUDIO10_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK = 0x00,
  AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ = 0x01,
  AUDIO10_CS_AS_ISO_DATA_EP_ATT_PITCH = 0x02,
} audio10_cs_as_iso_data_ep_attribute_t;

/// Audio Class-Specific AS Isochronous Data EP Lock Delay Units UAC1
typedef enum {
  AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED = 0x00,
  AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC = 0x01,
  AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_PCM_SAMPLES = 0x02,
} audio10_cs_as_iso_data_ep_lock_delay_unit_t;

/// Audio Class-Feature Unit Controls UAC1
typedef enum {
  AUDIO10_FU_CONTROL_BM_MUTE = 1 << 0,
  AUDIO10_FU_CONTROL_BM_VOLUME = 1 << 1,
  AUDIO10_FU_CONTROL_BM_BASS = 1 << 2,
  AUDIO10_FU_CONTROL_BM_MID = 1 << 3,
  AUDIO10_FU_CONTROL_BM_TREBLE = 1 << 4,
  AUDIO10_FU_CONTROL_BM_GRAPHIC_EQUALIZER = 1 << 5,
  AUDIO10_FU_CONTROL_BM_AGC = 1 << 6,
  AUDIO10_FU_CONTROL_BM_DELAY = 1 << 7,
  AUDIO10_FU_CONTROL_BM_BASS_BOOST = 1 << 8,
  AUDIO10_FU_CONTROL_BM_LOUDNESS = 1 << 9,
} audio10_feature_unit_control_bitmap_t;

/// A.1 - Audio Class-Format Type Codes UAC1
typedef enum {
  AUDIO10_FORMAT_TYPE_UNDEFINED = 0x00,
  AUDIO10_FORMAT_TYPE_I = 0x01,
  AUDIO10_FORMAT_TYPE_II = 0x02,
  AUDIO10_FORMAT_TYPE_III = 0x03,
} audio10_format_type_t;

// A.1.1 - Audio Class-Audio Data Format Type I UAC1
typedef enum {
  AUDIO10_DATA_FORMAT_TYPE_I_PCM = 0x0001,
  AUDIO10_DATA_FORMAT_TYPE_I_PCM8 = 0x0002,
  AUDIO10_DATA_FORMAT_TYPE_I_IEEE_FLOAT = 0x0003,
  AUDIO10_DATA_FORMAT_TYPE_I_ALAW = 0x0004,
  AUDIO10_DATA_FORMAT_TYPE_I_MULAW = 0x0005,
} audio10_data_format_type_I_t;

// A.1.2 - Audio Class-Audio Data Format Type II UAC1
typedef enum {
  AUDIO10_DATA_FORMAT_TYPE_II_MPEG = 0x1001,
  AUDIO10_DATA_FORMAT_TYPE_II_AC3 = 0x1002,
} audio10_data_format_type_II_t;

// A.1.3 - Audio Class-Audio Data Format Type III UAC1
typedef enum {
  AUDIO10_DATA_FORMAT_TYPE_III_IEC1937_AC3_1 = 0x2001,
  AUDIO10_DATA_FORMAT_TYPE_III_IEC1937_MPEG1_L1_1 = 0x2002,
  AUDIO10_DATA_FORMAT_TYPE_III_IEC1937_MPEG1_L23_1 = 0x2003,
  AUDIO10_DATA_FORMAT_TYPE_III_IEC1937_MPEG2_EXT_1 = 0x2004,
  AUDIO10_DATA_FORMAT_TYPE_III_IEC1937_MPEG2_L1_LS_1 = 0x2005,
  AUDIO10_DATA_FORMAT_TYPE_III_IEC1937_MPEG2_L23_LS_1 = 0x2006,
} audio10_data_format_type_III_t;

/// Audio Class-Audio Channel Configuration UAC1 (Table A-7)
typedef enum {
  AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED = 0x0000,
  AUDIO10_CHANNEL_CONFIG_LEFT_FRONT = 0x0001,
  AUDIO10_CHANNEL_CONFIG_RIGHT_FRONT = 0x0002,
  AUDIO10_CHANNEL_CONFIG_CENTER_FRONT = 0x0004,
  AUDIO10_CHANNEL_CONFIG_LOW_FRQ_EFFECTS = 0x0008,
  AUDIO10_CHANNEL_CONFIG_LEFT_SURROUND = 0x0010,
  AUDIO10_CHANNEL_CONFIG_RIGHT_SURROUND = 0x0020,
  AUDIO10_CHANNEL_CONFIG_LEFT_OF_CENTER = 0x0040,
  AUDIO10_CHANNEL_CONFIG_RIGHT_OF_CENTER = 0x0080,
  AUDIO10_CHANNEL_CONFIG_SURROUND = 0x0100,
  AUDIO10_CHANNEL_CONFIG_SIDE_LEFT = 0x0200,
  AUDIO10_CHANNEL_CONFIG_SIDE_RIGHT = 0x0400,
  AUDIO10_CHANNEL_CONFIG_TOP = 0x0800,
} audio10_channel_config_t;


//--------------------------------------------------------------------+
// USB AUDIO CLASS 1.0 (UAC1) DESCRIPTORS
//--------------------------------------------------------------------+

/// AUDIO Class-Specific AC Interface Header Descriptor UAC1 (4.3.2)
#define audio10_desc_cs_ac_interface_n_t(numInterfaces)                                                                                          \
  struct TU_ATTR_PACKED {                                                                                                                        \
    uint8_t bLength;                      /* Size of this descriptor in bytes: 8+n. */                                                           \
    uint8_t bDescriptorType;              /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                                                  \
    uint8_t bDescriptorSubType;           /* Descriptor SubType. Value: AUDIO10_CS_AC_INTERFACE_HEADER. */                                       \
    uint16_t bcdADC;                      /* Audio Device Class Specification Release Number in Binary-Coded Decimal. Value: 0x0100 for UAC1. */ \
    uint16_t wTotalLength;                /* Total number of bytes returned for the class-specific AudioControl interface descriptor. */         \
    uint8_t bInCollection;                /* The number of AudioStreaming and MIDIStreaming interfaces in the Audio Interface Collection. */     \
    uint8_t baInterfaceNr[numInterfaces]; /* Interface number of the AudioStreaming or MIDIStreaming interface in the Collection. */             \
  }

/// AUDIO Input Terminal Descriptor UAC1 (4.3.2.1)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor in bytes: 12.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL.
  uint8_t bTerminalID;       ///< Constant uniquely identifying the Terminal within the audio function.
  uint16_t wTerminalType;    ///< Constant characterizing the type of Terminal.
  uint8_t bAssocTerminal;    ///< ID of the Output Terminal to which this Input Terminal is associated.
  uint8_t bNrChannels;       ///< Number of logical output channels in the Terminal's output audio channel cluster.
  uint16_t wChannelConfig;   ///< Describes the spatial location of the logical channels.
  uint8_t iChannelNames;     ///< Index of a string descriptor, describing the name of the first logical channel.
  uint8_t iTerminal;         ///< Index of a string descriptor, describing the Input Terminal.
} audio10_desc_input_terminal_t;

/// AUDIO Output Terminal Descriptor UAC1 (4.3.2.2)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor in bytes: 9.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO10_CS_AC_INTERFACE_OUTPUT_TERMINAL.
  uint8_t bTerminalID;       ///< Constant uniquely identifying the Terminal within the audio function.
  uint16_t wTerminalType;    ///< Constant characterizing the type of Terminal.
  uint8_t bAssocTerminal;    ///< Constant, identifying the Input Terminal to which this Output Terminal is associated.
  uint8_t bSourceID;         ///< ID of the Unit or Terminal to which this Terminal is connected.
  uint8_t iTerminal;         ///< Index of a string descriptor, describing the Output Terminal.
} audio10_desc_output_terminal_t;

/// AUDIO Mixer Unit Descriptor UAC1 (4.3.2.3)
#define audio10_desc_mixer_unit_n_t(numInputPins, numControlBytes)                                                                 \
  struct TU_ATTR_PACKED {                                                                                                          \
    uint8_t bLength;                     /* Size of this descriptor in bytes: 10+p+n. */                                           \
    uint8_t bDescriptorType;             /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                                     \
    uint8_t bDescriptorSubType;          /* Descriptor SubType. Value: AUDIO10_CS_AC_INTERFACE_MIXER_UNIT. */                      \
    uint8_t bUnitID;                     /* Constant uniquely identifying the Unit within the audio function. */                   \
    uint8_t bNrInPins;                   /* Number of Input Pins of this Unit: p. */                                               \
    uint8_t baSourceID[numInputPins];    /* ID of the Unit or Terminal to which Input Pins of this Mixer Unit are connected. */    \
    uint8_t bNrChannels;                 /* Number of logical output channels in the Mixer Unit's output audio channel cluster. */ \
    uint16_t wChannelConfig;             /* Describes the spatial location of the logical channels. */                             \
    uint8_t iChannelNames;               /* Index of a string descriptor, describing the name of the first logical channel. */     \
    uint8_t bmControls[numControlBytes]; /* Mixer Unit Controls bitmap. */                                                         \
    uint8_t iMixer;                      /* Index of a string descriptor, describing the Mixer Unit. */                            \
  }

/// AUDIO Selector Unit Descriptor UAC1 (4.3.2.4)
#define audio10_desc_selector_unit_n_t(numInputPins)                                                                            \
  struct TU_ATTR_PACKED {                                                                                                       \
    uint8_t bLength;                  /* Size of this descriptor in bytes: 6+p. */                                              \
    uint8_t bDescriptorType;          /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                                     \
    uint8_t bDescriptorSubType;       /* Descriptor SubType. Value: AUDIO10_CS_AC_INTERFACE_SELECTOR_UNIT. */                   \
    uint8_t bUnitID;                  /* Constant uniquely identifying the Unit within the audio function. */                   \
    uint8_t bNrInPins;                /* Number of Input Pins of this Unit: p. */                                               \
    uint8_t baSourceID[numInputPins]; /* ID of the Unit or Terminal to which Input Pins of this Selector Unit are connected. */ \
    uint8_t iSelector;                /* Index of a string descriptor, describing the Selector Unit. */                         \
  }

/// AUDIO Feature Unit Descriptor UAC1 (4.3.2.5)
#define audio10_desc_feature_unit_n_t(numChannels, controlSize)                                                                     \
  struct TU_ATTR_PACKED {                                                                                                           \
    uint8_t bLength;                                      /* Size of this descriptor in bytes: 7+(ch+1)*n. */                       \
    uint8_t bDescriptorType;                              /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                     \
    uint8_t bDescriptorSubType;                           /* Descriptor SubType. Value: AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT. */    \
    uint8_t bUnitID;                                      /* Constant uniquely identifying the Unit within the audio function. */   \
    uint8_t bSourceID;                                    /* ID of the Unit or Terminal to which this Feature Unit is connected. */ \
    uint8_t bControlSize;                                 /* Size in bytes of an element of the bmaControls() array. */             \
    uint8_t bmaControls[(numChannels + 1) * controlSize]; /* Control bitmaps for master + logical channels. */                      \
    uint8_t iFeature;                                     /* Index of a string descriptor, describing this Feature Unit. */         \
  }

/// AUDIO Processing Unit Descriptor UAC1 (4.3.2.6)
#define audio10_desc_processing_unit_n_t(numInputPins, numControlBytes)                                                                 \
  struct TU_ATTR_PACKED {                                                                                                               \
    uint8_t bLength;                     /* Size of this descriptor in bytes: 13+p+n. */                                                \
    uint8_t bDescriptorType;             /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                                          \
    uint8_t bDescriptorSubType;          /* Descriptor SubType. Value: AUDIO10_CS_AC_INTERFACE_PROCESSING_UNIT. */                      \
    uint8_t bUnitID;                     /* Constant uniquely identifying the Unit within the audio function. */                        \
    uint16_t wProcessType;               /* Constant identifying the type of processing this Unit is performing. */                     \
    uint8_t bNrInPins;                   /* Number of Input Pins of this Unit: p. */                                                    \
    uint8_t baSourceID[numInputPins];    /* ID of the Unit or Terminal to which Input Pins of this Processing Unit are connected. */    \
    uint8_t bNrChannels;                 /* Number of logical output channels in the Processing Unit's output audio channel cluster. */ \
    uint16_t wChannelConfig;             /* Describes the spatial location of the logical channels. */                                  \
    uint8_t iChannelNames;               /* Index of a string descriptor, describing the name of the first logical channel. */          \
    uint8_t bControlSize;                /* Size in bytes of the bmControls field. */                                                   \
    uint8_t bmControls[numControlBytes]; /* Processing Unit Controls bitmap. */                                                         \
    uint8_t iProcessing;                 /* Index of a string descriptor, describing the Processing Unit. */                            \
  }

/// AUDIO Extension Unit Descriptor UAC1 (4.3.2.7)
#define audio10_desc_extension_unit_n_t(numInputPins, numControlBytes)                                                                 \
  struct TU_ATTR_PACKED {                                                                                                              \
    uint8_t bLength;                     /* Size of this descriptor in bytes: 13+p+n. */                                               \
    uint8_t bDescriptorType;             /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                                         \
    uint8_t bDescriptorSubType;          /* Descriptor SubType. Value: AUDIO10_CS_AC_INTERFACE_EXTENSION_UNIT. */                      \
    uint8_t bUnitID;                     /* Constant uniquely identifying the Unit within the audio function. */                       \
    uint16_t wExtensionCode;             /* Vendor-specific code identifying the Extension Unit. */                                    \
    uint8_t bNrInPins;                   /* Number of Input Pins of this Unit: p. */                                                   \
    uint8_t baSourceID[numInputPins];    /* ID of the Unit or Terminal to which Input Pins of this Extension Unit are connected. */    \
    uint8_t bNrChannels;                 /* Number of logical output channels in the Extension Unit's output audio channel cluster. */ \
    uint16_t wChannelConfig;             /* Describes the spatial location of the logical channels. */                                 \
    uint8_t iChannelNames;               /* Index of a string descriptor, describing the name of the first logical channel. */         \
    uint8_t bControlSize;                /* Size in bytes of the bmControls field. */                                                  \
    uint8_t bmControls[numControlBytes]; /* Extension Unit Controls bitmap. */                                                         \
    uint8_t iExtension;                  /* Index of a string descriptor, describing the Extension Unit. */                            \
  }

/// AUDIO Class-Specific AS Interface Descriptor UAC1 (4.5.2)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor in bytes: 7.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO10_CS_AS_INTERFACE_AS_GENERAL.
  uint8_t bTerminalLink;     ///< The Terminal ID of the Terminal to which the endpoint of this interface is connected.
  uint8_t bDelay;            ///< Expressed in number of frames.
  uint16_t wFormatTag;       ///< The Audio Data Format that has to be used to communicate with this interface.
} audio10_desc_cs_as_interface_t;

/// AUDIO Type I Format Type Descriptor UAC1 (2.2.5)
#define audio10_desc_type_I_format_n_t(numSamFreq)                                                                            \
  struct TU_ATTR_PACKED {                                                                                                     \
    uint8_t bLength;                  /* Size of this descriptor in bytes: 8+(ns*3). */                                       \
    uint8_t bDescriptorType;          /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                                   \
    uint8_t bDescriptorSubType;       /* Descriptor SubType. Value: AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE. */                   \
    uint8_t bFormatType;              /* Constant identifying the Format Type the AudioStreaming interface is using. */       \
    uint8_t bNrChannels;              /* Indicates the number of physical channels in the audio data stream. */               \
    uint8_t bSubFrameSize;            /* The number of bytes occupied by one audio subframe. */                               \
    uint8_t bBitResolution;           /* The number of effectively used bits from the available bits in an audio subframe. */ \
    uint8_t bSamFreqType;             /* Indicates how the sampling frequency can be programmed. */                           \
    uint8_t tSamFreq[numSamFreq * 3]; /* Sampling frequency or lower/upper bounds in Hz for the sampling frequency range. */  \
  }

/// AUDIO Type II Format Type Descriptor UAC1 (2.3.5)
#define audio10_desc_type_II_format_n_t(numSamFreq)                                                                          \
  struct TU_ATTR_PACKED {                                                                                                    \
    uint8_t bLength;                  /* Size of this descriptor in bytes: 9+(ns*3). */                                      \
    uint8_t bDescriptorType;          /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                                  \
    uint8_t bDescriptorSubType;       /* Descriptor SubType. Value: AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE. */                  \
    uint8_t bFormatType;              /* Constant identifying the Format Type the AudioStreaming interface is using. */      \
    uint16_t wMaxBitRate;             /* Indicates the maximum number of bits per second this interface can handle. */       \
    uint16_t wSamplesPerFrame;        /* Indicates the number of PCM audio samples contained in one encoded audio frame. */  \
    uint8_t bSamFreqType;             /* Indicates how the sampling frequency can be programmed. */                          \
    uint8_t tSamFreq[numSamFreq * 3]; /* Sampling frequency or lower/upper bounds in Hz for the sampling frequency range. */ \
  }

/// AUDIO Type III Format Type Descriptor UAC1 (2.4.5)
#define audio10_desc_type_III_format_n_t(numSamFreq)                                                                          \
  struct TU_ATTR_PACKED {                                                                                                     \
    uint8_t bLength;                  /* Size of this descriptor in bytes: 8+(ns*3). */                                       \
    uint8_t bDescriptorType;          /* Descriptor Type. Value: TUSB_DESC_CS_INTERFACE. */                                   \
    uint8_t bDescriptorSubType;       /* Descriptor SubType. Value: AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE. */                   \
    uint8_t bFormatType;              /* Constant identifying the Format Type the AudioStreaming interface is using. */       \
    uint8_t bNrChannels;              /* Indicates the number of physical channels in the audio data stream. */               \
    uint8_t bSubFrameSize;            /* The number of bytes occupied by one audio subframe. */                               \
    uint8_t bBitResolution;           /* The number of effectively used bits from the available bits in an audio subframe. */ \
    uint8_t bSamFreqType;             /* Indicates how the sampling frequency can be programmed. */                           \
    uint8_t tSamFreq[numSamFreq * 3]; /* Sampling frequency or lower/upper bounds in Hz for the sampling frequency range. */  \
  }

/// Standard AS Isochronous Audio Data Endpoint Descriptor UAC1 (4.6.1.1)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;         ///< Size of this descriptor in bytes: 9.
  uint8_t bDescriptorType; ///< Descriptor Type. Value: TUSB_DESC_ENDPOINT.
  uint8_t bEndpointAddress;///< The address of the endpoint on the USB device described by this descriptor.
  struct TU_ATTR_PACKED {
    uint8_t xfer  : 2;        // Control, ISO, Bulk, Interrupt
    uint8_t sync  : 2;        // None, Asynchronous, Adaptive, Synchronous
    uint8_t usage : 2;        // Data, Feedback, Implicit feedback
    uint8_t       : 2;
  } bmAttributes;
  uint16_t wMaxPacketSize; ///< Maximum packet size this endpoint is capable of sending or receiving when this configuration is selected.
  uint8_t bInterval;       ///< Interval for polling endpoint for data transfers.
  uint8_t bRefresh;        ///< The rate at which the endpoint is refreshed.
  uint8_t bSynchAddress;   ///< The address of the endpoint used to send synchronization information for the data endpoint.
} audio10_desc_as_iso_data_ep_t;

/// AUDIO Class-Specific AS Isochronous Audio Data Endpoint Descriptor UAC1 (4.6.1.2)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor in bytes: 7.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_ENDPOINT.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO10_CS_EP_SUBTYPE_GENERAL.
  uint8_t bmAttributes;      ///< Bit 0: Sampling Frequency, Bit 1: Pitch, Bit 7: MaxPacketsOnly.
  uint8_t bLockDelayUnits;   ///< Indicates the units used for the wLockDelay field.
  uint16_t wLockDelay;       ///< Indicates the time it takes this endpoint to reliably lock its internal clock recovery circuitry.
} audio10_desc_cs_as_iso_data_ep_t;

/// AUDIO Interrupt Data Message Format UAC1 (3.7.1.2)
typedef struct TU_ATTR_PACKED {
  uint8_t bStatusType;///< Indicates the type of status information being reported.
  uint8_t bOriginator;///< Indicates the entity that originated this status information.
} audio10_interrupt_data_t;

//--------------------------------------------------------------------+
// USB AUDIO CLASS 2.0 (UAC2) DEFINITIONS
//--------------------------------------------------------------------+

/// A.7 - Audio Function Category Codes
typedef enum {
  AUDIO20_FUNC_UNDEF = 0x00,
  AUDIO20_FUNC_DESKTOP_SPEAKER = 0x01,
  AUDIO20_FUNC_HOME_THEATER = 0x02,
  AUDIO20_FUNC_MICROPHONE = 0x03,
  AUDIO20_FUNC_HEADSET = 0x04,
  AUDIO20_FUNC_TELEPHONE = 0x05,
  AUDIO20_FUNC_CONVERTER = 0x06,
  AUDIO20_FUNC_SOUND_RECODER = 0x07,
  AUDIO20_FUNC_IO_BOX = 0x08,
  AUDIO20_FUNC_MUSICAL_INSTRUMENT = 0x09,
  AUDIO20_FUNC_PRO_AUDIO = 0x0A,
  AUDIO20_FUNC_AUDIO_VIDEO = 0x0B,
  AUDIO20_FUNC_CONTROL_PANEL = 0x0C,
  AUDIO20_FUNC_OTHER = 0xFF,
} audio20_function_code_t;

/// A.9 - Audio Class-Specific AC Interface Descriptor Subtypes UAC2
typedef enum {
  AUDIO20_CS_AC_INTERFACE_AC_DESCRIPTOR_UNDEF = 0x00,
  AUDIO20_CS_AC_INTERFACE_HEADER = 0x01,
  AUDIO20_CS_AC_INTERFACE_INPUT_TERMINAL = 0x02,
  AUDIO20_CS_AC_INTERFACE_OUTPUT_TERMINAL = 0x03,
  AUDIO20_CS_AC_INTERFACE_MIXER_UNIT = 0x04,
  AUDIO20_CS_AC_INTERFACE_SELECTOR_UNIT = 0x05,
  AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT = 0x06,
  AUDIO20_CS_AC_INTERFACE_EFFECT_UNIT = 0x07,
  AUDIO20_CS_AC_INTERFACE_PROCESSING_UNIT = 0x08,
  AUDIO20_CS_AC_INTERFACE_EXTENSION_UNIT = 0x09,
  AUDIO20_CS_AC_INTERFACE_CLOCK_SOURCE = 0x0A,
  AUDIO20_CS_AC_INTERFACE_CLOCK_SELECTOR = 0x0B,
  AUDIO20_CS_AC_INTERFACE_CLOCK_MULTIPLIER = 0x0C,
  AUDIO20_CS_AC_INTERFACE_SAMPLE_RATE_CONVERTER = 0x0D,
} audio20_cs_ac_interface_subtype_t;

/// A.10 - Audio Class-Specific AS Interface Descriptor Subtypes UAC2
typedef enum {
  AUDIO20_CS_AS_INTERFACE_AS_DESCRIPTOR_UNDEF = 0x00,
  AUDIO20_CS_AS_INTERFACE_AS_GENERAL = 0x01,
  AUDIO20_CS_AS_INTERFACE_FORMAT_TYPE = 0x02,
  AUDIO20_CS_AS_INTERFACE_ENCODER = 0x03,
  AUDIO20_CS_AS_INTERFACE_DECODER = 0x04,
} audio20_cs_as_interface_subtype_t;

/// A.11 - Effect Unit Effect Types
typedef enum {
  AUDIO20_EFFECT_TYPE_UNDEF = 0x00,
  AUDIO20_EFFECT_TYPE_PARAM_EQ_SECTION = 0x01,
  AUDIO20_EFFECT_TYPE_REVERBERATION = 0x02,
  AUDIO20_EFFECT_TYPE_MOD_DELAY = 0x03,
  AUDIO20_EFFECT_TYPE_DYN_RANGE_COMP = 0x04,
} audio20_effect_unit_effect_type_t;

/// A.12 - Processing Unit Process Types
typedef enum {
  AUDIO20_PROCESS_TYPE_UNDEF = 0x00,
  AUDIO20_PROCESS_TYPE_UP_DOWN_MIX = 0x01,
  AUDIO20_PROCESS_TYPE_DOLBY_PROLOGIC = 0x02,
  AUDIO20_PROCESS_TYPE_STEREO_EXTENDER = 0x03,
} audio20_processing_unit_process_type_t;

/// A.13 - Audio Class-Specific EP Descriptor Subtypes UAC2
typedef enum {
  AUDIO20_CS_EP_SUBTYPE_UNDEF = 0x00,
  AUDIO20_CS_EP_SUBTYPE_GENERAL = 0x01,
} audio20_cs_ep_subtype_t;

/// A.14 - Audio Class-Specific Request Codes UAC2
typedef enum {
  AUDIO20_CS_REQ_UNDEF = 0x00,
  AUDIO20_CS_REQ_CUR = 0x01,
  AUDIO20_CS_REQ_RANGE = 0x02,
  AUDIO20_CS_REQ_MEM = 0x03,
} audio20_cs_req_t;

/// A.17 - Control Selector Codes UAC2

/// A.17.1 - Clock Source Control Selectors
typedef enum {
  AUDIO20_CS_CTRL_UNDEF = 0x00,
  AUDIO20_CS_CTRL_SAM_FREQ = 0x01,
  AUDIO20_CS_CTRL_CLK_VALID = 0x02,
} audio20_clock_src_control_selector_t;

/// A.17.2 - Clock Selector Control Selectors
typedef enum {
  AUDIO20_CX_CTRL_UNDEF = 0x00,
  AUDIO20_CX_CTRL_CONTROL = 0x01,
} audio20_clock_sel_control_selector_t;

/// A.17.3 - Clock Multiplier Control Selectors
typedef enum {
  AUDIO20_CM_CTRL_UNDEF = 0x00,
  AUDIO20_CM_CTRL_NUMERATOR_CONTROL = 0x01,
  AUDIO20_CM_CTRL_DENOMINATOR_CONTROL = 0x02,
} audio20_clock_mul_control_selector_t;

/// A.17.4 - Terminal Control Selectors UAC2
typedef enum {
  AUDIO20_TE_CTRL_UNDEF = 0x00,
  AUDIO20_TE_CTRL_COPY_PROTECT = 0x01,
  AUDIO20_TE_CTRL_CONNECTOR = 0x02,
  AUDIO20_TE_CTRL_OVERLOAD = 0x03,
  AUDIO20_TE_CTRL_CLUSTER = 0x04,
  AUDIO20_TE_CTRL_UNDERFLOW = 0x05,
  AUDIO20_TE_CTRL_OVERFLOW = 0x06,
  AUDIO20_TE_CTRL_LATENCY = 0x07,
} audio20_terminal_control_selector_t;

/// A.17.5 - Mixer Control Selectors
typedef enum {
  AUDIO20_MU_CTRL_UNDEF = 0x00,
  AUDIO20_MU_CTRL_MIXER = 0x01,
  AUDIO20_MU_CTRL_CLUSTER = 0x02,
  AUDIO20_MU_CTRL_UNDERFLOW = 0x03,
  AUDIO20_MU_CTRL_OVERFLOW = 0x04,
  AUDIO20_MU_CTRL_LATENCY = 0x05,
} audio20_mixer_control_selector_t;

/// A.17.6 - Selector Control Selectors
typedef enum {
  AUDIO20_SU_CTRL_UNDEF = 0x00,
  AUDIO20_SU_CTRL_SELECTOR = 0x01,
  AUDIO20_SU_CTRL_LATENCY = 0x02,
} audio20_sel_control_selector_t;

/// A.17.7 - Feature Unit Control Selectors UAC2
typedef enum {
  AUDIO20_FU_CTRL_UNDEF = 0x00,
  AUDIO20_FU_CTRL_MUTE = 0x01,
  AUDIO20_FU_CTRL_VOLUME = 0x02,
  AUDIO20_FU_CTRL_BASS = 0x03,
  AUDIO20_FU_CTRL_MID = 0x04,
  AUDIO20_FU_CTRL_TREBLE = 0x05,
  AUDIO20_FU_CTRL_GRAPHIC_EQUALIZER = 0x06,
  AUDIO20_FU_CTRL_AGC = 0x07,
  AUDIO20_FU_CTRL_DELAY = 0x08,
  AUDIO20_FU_CTRL_BASS_BOOST = 0x09,
  AUDIO20_FU_CTRL_LOUDNESS = 0x0A,
  AUDIO20_FU_CTRL_INPUT_GAIN = 0x0B,
  AUDIO20_FU_CTRL_GAIN_PAD = 0x0C,
  AUDIO20_FU_CTRL_INVERTER = 0x0D,
  AUDIO20_FU_CTRL_UNDERFLOW = 0x0E,
  AUDIO20_FU_CTRL_OVERVLOW = 0x0F,
  AUDIO20_FU_CTRL_LATENCY = 0x10,
} audio20_feature_unit_control_selector_t;

/// A.17.8 Effect Unit Control Selectors

/// A.17.8.1 Parametric Equalizer Section Effect Unit Control Selectors
typedef enum {
  AUDIO20_PE_CTRL_UNDEF = 0x00,
  AUDIO20_PE_CTRL_ENABLE = 0x01,
  AUDIO20_PE_CTRL_CENTERFREQ = 0x02,
  AUDIO20_PE_CTRL_QFACTOR = 0x03,
  AUDIO20_PE_CTRL_GAIN = 0x04,
  AUDIO20_PE_CTRL_UNDERFLOW = 0x05,
  AUDIO20_PE_CTRL_OVERFLOW = 0x06,
  AUDIO20_PE_CTRL_LATENCY = 0x07,
} audio20_parametric_equalizer_control_selector_t;

/// A.17.8.2 Reverberation Effect Unit Control Selectors
typedef enum {
  AUDIO20_RV_CTRL_UNDEF = 0x00,
  AUDIO20_RV_CTRL_ENABLE = 0x01,
  AUDIO20_RV_CTRL_TYPE = 0x02,
  AUDIO20_RV_CTRL_LEVEL = 0x03,
  AUDIO20_RV_CTRL_TIME = 0x04,
  AUDIO20_RV_CTRL_FEEDBACK = 0x05,
  AUDIO20_RV_CTRL_PREDELAY = 0x06,
  AUDIO20_RV_CTRL_DENSITY = 0x07,
  AUDIO20_RV_CTRL_HIFREQ_ROLLOFF = 0x08,
  AUDIO20_RV_CTRL_UNDERFLOW = 0x09,
  AUDIO20_RV_CTRL_OVERFLOW = 0x0A,
  AUDIO20_RV_CTRL_LATENCY = 0x0B,
} audio20_reverberation_effect_control_selector_t;

/// A.17.8.3 Modulation Delay Effect Unit Control Selectors
typedef enum {
  AUDIO20_MD_CTRL_UNDEF = 0x00,
  AUDIO20_MD_CTRL_ENABLE = 0x01,
  AUDIO20_MD_CTRL_BALANCE = 0x02,
  AUDIO20_MD_CTRL_RATE = 0x03,
  AUDIO20_MD_CTRL_DEPTH = 0x04,
  AUDIO20_MD_CTRL_TIME = 0x05,
  AUDIO20_MD_CTRL_FEEDBACK = 0x06,
  AUDIO20_MD_CTRL_UNDERFLOW = 0x07,
  AUDIO20_MD_CTRL_OVERFLOW = 0x08,
  AUDIO20_MD_CTRL_LATENCY = 0x09,
} audio20_modulation_delay_control_selector_t;

/// A.17.8.4 Dynamic Range Compressor Effect Unit Control Selectors
typedef enum {
  AUDIO20_DR_CTRL_UNDEF = 0x00,
  AUDIO20_DR_CTRL_ENABLE = 0x01,
  AUDIO20_DR_CTRL_COMPRESSION_RATE = 0x02,
  AUDIO20_DR_CTRL_MAXAMPL = 0x03,
  AUDIO20_DR_CTRL_THRESHOLD = 0x04,
  AUDIO20_DR_CTRL_ATTACK_TIME = 0x05,
  AUDIO20_DR_CTRL_RELEASE_TIME = 0x06,
  AUDIO20_DR_CTRL_UNDERFLOW = 0x07,
  AUDIO20_DR_CTRL_OVERFLOW = 0x08,
  AUDIO20_DR_CTRL_LATENCY = 0x09,
} audio20_dynamic_range_compression_control_selector_t;

/// A.17.9 Processing Unit Control Selectors

/// A.17.9.1 Up/Down-mix Processing Unit Control Selectors
typedef enum {
  AUDIO20_UD_CTRL_UNDEF = 0x00,
  AUDIO20_UD_CTRL_ENABLE = 0x01,
  AUDIO20_UD_CTRL_MODE_SELECT = 0x02,
  AUDIO20_UD_CTRL_CLUSTER = 0x03,
  AUDIO20_UD_CTRL_UNDERFLOW = 0x04,
  AUDIO20_UD_CTRL_OVERFLOW = 0x05,
  AUDIO20_UD_CTRL_LATENCY = 0x06,
} audio20_up_down_mix_control_selector_t;

/// A.17.9.2 Dolby Prologic ™ Processing Unit Control Selectors
typedef enum {
  AUDIO20_DP_CTRL_UNDEF = 0x00,
  AUDIO20_DP_CTRL_ENABLE = 0x01,
  AUDIO20_DP_CTRL_MODE_SELECT = 0x02,
  AUDIO20_DP_CTRL_CLUSTER = 0x03,
  AUDIO20_DP_CTRL_UNDERFLOW = 0x04,
  AUDIO20_DP_CTRL_OVERFLOW = 0x05,
  AUDIO20_DP_CTRL_LATENCY = 0x06,
} audio20_dolby_prologic_control_selector_t;

/// A.17.9.3 Stereo Extender Processing Unit Control Selectors
typedef enum {
  AUDIO20_ST_EXT_CTRL_UNDEF = 0x00,
  AUDIO20_ST_EXT_CTRL_ENABLE = 0x01,
  AUDIO20_ST_EXT_CTRL_WIDTH = 0x02,
  AUDIO20_ST_EXT_CTRL_UNDERFLOW = 0x03,
  AUDIO20_ST_EXT_CTRL_OVERFLOW = 0x04,
  AUDIO20_ST_EXT_CTRL_LATENCY = 0x05,
} audio20_stereo_extender_control_selector_t;

/// A.17.10 Extension Unit Control Selectors
typedef enum {
  AUDIO20_XU_CTRL_UNDEF = 0x00,
  AUDIO20_XU_CTRL_ENABLE = 0x01,
  AUDIO20_XU_CTRL_CLUSTER = 0x02,
  AUDIO20_XU_CTRL_UNDERFLOW = 0x03,
  AUDIO20_XU_CTRL_OVERFLOW = 0x04,
  AUDIO20_XU_CTRL_LATENCY = 0x05,
} audio20_extension_unit_control_selector_t;

/// A.17.11 AudioStreaming Interface Control Selectors
typedef enum {
  AUDIO20_AS_CTRL_UNDEF = 0x00,
  AUDIO20_AS_CTRL_ACT_ALT_SETTING = 0x01,
  AUDIO20_AS_CTRL_VAL_ALT_SETTINGS = 0x02,
  AUDIO20_AS_CTRL_AUDIO_DATA_FORMAT = 0x03,
} audio20_audiostreaming_interface_control_selector_t;

/// A.17.12 Encoder Control Selectors
typedef enum {
  AUDIO20_EN_CTRL_UNDEF = 0x00,
  AUDIO20_EN_CTRL_BIT_RATE = 0x01,
  AUDIO20_EN_CTRL_QUALITY = 0x02,
  AUDIO20_EN_CTRL_VBR = 0x03,
  AUDIO20_EN_CTRL_TYPE = 0x04,
  AUDIO20_EN_CTRL_UNDERFLOW = 0x05,
  AUDIO20_EN_CTRL_OVERFLOW = 0x06,
  AUDIO20_EN_CTRL_ENCODER_ERROR = 0x07,
  AUDIO20_EN_CTRL_PARAM1 = 0x08,
  AUDIO20_EN_CTRL_PARAM2 = 0x09,
  AUDIO20_EN_CTRL_PARAM3 = 0x0A,
  AUDIO20_EN_CTRL_PARAM4 = 0x0B,
  AUDIO20_EN_CTRL_PARAM5 = 0x0C,
  AUDIO20_EN_CTRL_PARAM6 = 0x0D,
  AUDIO20_EN_CTRL_PARAM7 = 0x0E,
  AUDIO20_EN_CTRL_PARAM8 = 0x0F,
} audio20_encoder_control_selector_t;

/// A.17.13 Decoder Control Selectors

/// A.17.13.1 MPEG Decoder Control Selectors
typedef enum {
  AUDIO20_MPD_CTRL_UNDEF = 0x00,
  AUDIO20_MPD_CTRL_DUAL_CHANNEL = 0x01,
  AUDIO20_MPD_CTRL_SECOND_STEREO = 0x02,
  AUDIO20_MPD_CTRL_MULTILINGUAL = 0x03,
  AUDIO20_MPD_CTRL_DYN_RANGE = 0x04,
  AUDIO20_MPD_CTRL_SCALING = 0x05,
  AUDIO20_MPD_CTRL_HILO_SCALING = 0x06,
  AUDIO20_MPD_CTRL_UNDERFLOW = 0x07,
  AUDIO20_MPD_CTRL_OVERFLOW = 0x08,
  AUDIO20_MPD_CTRL_DECODER_ERROR = 0x09,
} audio20_MPEG_decoder_control_selector_t;

/// A.17.13.2 AC-3 Decoder Control Selectors
typedef enum {
  AUDIO20_AD_CTRL_UNDEF = 0x00,
  AUDIO20_AD_CTRL_MODE = 0x01,
  AUDIO20_AD_CTRL_DYN_RANGE = 0x02,
  AUDIO20_AD_CTRL_SCALING = 0x03,
  AUDIO20_AD_CTRL_HILO_SCALING = 0x04,
  AUDIO20_AD_CTRL_UNDERFLOW = 0x05,
  AUDIO20_AD_CTRL_OVERFLOW = 0x06,
  AUDIO20_AD_CTRL_DECODER_ERROR = 0x07,
} audio20_AC3_decoder_control_selector_t;

/// A.17.13.3 WMA Decoder Control Selectors
typedef enum {
  AUDIO20_WD_CTRL_UNDEF = 0x00,
  AUDIO20_WD_CTRL_UNDERFLOW = 0x01,
  AUDIO20_WD_CTRL_OVERFLOW = 0x02,
  AUDIO20_WD_CTRL_DECODER_ERROR = 0x03,
} audio20_WMA_decoder_control_selector_t;

/// A.17.13.4 DTS Decoder Control Selectors
typedef enum {
  AUDIO20_DD_CTRL_UNDEF = 0x00,
  AUDIO20_DD_CTRL_UNDERFLOW = 0x01,
  AUDIO20_DD_CTRL_OVERFLOW = 0x02,
  AUDIO20_DD_CTRL_DECODER_ERROR = 0x03,
} audio20_DTS_decoder_control_selector_t;

/// A.17.14 Endpoint Control Selectors
typedef enum {
  AUDIO20_EP_CTRL_UNDEF = 0x00,
  AUDIO20_EP_CTRL_PITCH = 0x01,
  AUDIO20_EP_CTRL_DATA_OVERRUN = 0x02,
  AUDIO20_EP_CTRL_DATA_UNDERRUN = 0x03,
} audio20_EP_control_selector_t;

/// Additional Audio Device Class Codes - Source: Audio Data Formats

/// A.1 - Audio Class-Format Type Codes UAC2
typedef enum {
  AUDIO20_FORMAT_TYPE_UNDEFINED = 0x00,
  AUDIO20_FORMAT_TYPE_I = 0x01,
  AUDIO20_FORMAT_TYPE_II = 0x02,
  AUDIO20_FORMAT_TYPE_III = 0x03,
  AUDIO20_FORMAT_TYPE_IV = 0x04,
  AUDIO20_EXT_FORMAT_TYPE_I = 0x81,
  AUDIO20_EXT_FORMAT_TYPE_II = 0x82,
  AUDIO20_EXT_FORMAT_TYPE_III = 0x83,
} audio20_format_type_t;

// A.2.1 - Audio Class-Audio Data Format Type I UAC2
typedef enum {
  AUDIO20_DATA_FORMAT_TYPE_I_PCM = 1 << 0,
  AUDIO20_DATA_FORMAT_TYPE_I_PCM8 = 1 << 1,
  AUDIO20_DATA_FORMAT_TYPE_I_IEEE_FLOAT = 1 << 2,
  AUDIO20_DATA_FORMAT_TYPE_I_ALAW = 1 << 3,
  AUDIO20_DATA_FORMAT_TYPE_I_MULAW = 1 << 4,
  AUDIO20_DATA_FORMAT_TYPE_I_RAW_DATA = 0x80000000u,
} audio20_data_format_type_I_t;

/// Audio Class-Audio Channel Configuration UAC2 (Table A-11)
typedef enum {
  AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED = 0x00000000,
  AUDIO20_CHANNEL_CONFIG_FRONT_LEFT = 0x00000001,
  AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT = 0x00000002,
  AUDIO20_CHANNEL_CONFIG_FRONT_CENTER = 0x00000004,
  AUDIO20_CHANNEL_CONFIG_LOW_FRQ_EFFECTS = 0x00000008,
  AUDIO20_CHANNEL_CONFIG_BACK_LEFT = 0x00000010,
  AUDIO20_CHANNEL_CONFIG_BACK_RIGHT = 0x00000020,
  AUDIO20_CHANNEL_CONFIG_FRONT_LEFT_OF_CENTER = 0x00000040,
  AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT_OF_CENTER = 0x00000080,
  AUDIO20_CHANNEL_CONFIG_BACK_CENTER = 0x00000100,
  AUDIO20_CHANNEL_CONFIG_SIDE_LEFT = 0x00000200,
  AUDIO20_CHANNEL_CONFIG_SIDE_RIGHT = 0x00000400,
  AUDIO20_CHANNEL_CONFIG_TOP_CENTER = 0x00000800,
  AUDIO20_CHANNEL_CONFIG_TOP_FRONT_LEFT = 0x00001000,
  AUDIO20_CHANNEL_CONFIG_TOP_FRONT_CENTER = 0x00002000,
  AUDIO20_CHANNEL_CONFIG_TOP_FRONT_RIGHT = 0x00004000,
  AUDIO20_CHANNEL_CONFIG_TOP_BACK_LEFT = 0x00008000,
  AUDIO20_CHANNEL_CONFIG_TOP_BACK_CENTER = 0x00010000,
  AUDIO20_CHANNEL_CONFIG_TOP_BACK_RIGHT = 0x00020000,
  AUDIO20_CHANNEL_CONFIG_TOP_FRONT_LEFT_OF_CENTER = 0x00040000,
  AUDIO20_CHANNEL_CONFIG_TOP_FRONT_RIGHT_OF_CENTER = 0x00080000,
  AUDIO20_CHANNEL_CONFIG_LEFT_LOW_FRQ_EFFECTS = 0x00100000,
  AUDIO20_CHANNEL_CONFIG_RIGHT_LOW_FRQ_EFFECTS = 0x00200000,
  AUDIO20_CHANNEL_CONFIG_TOP_SIDE_LEFT = 0x00400000,
  AUDIO20_CHANNEL_CONFIG_TOP_SIDE_RIGHT = 0x00800000,
  AUDIO20_CHANNEL_CONFIG_BOTTOM_CENTER = 0x01000000,
  AUDIO20_CHANNEL_CONFIG_BACK_LEFT_OF_CENTER = 0x02000000,
  AUDIO20_CHANNEL_CONFIG_BACK_RIGHT_OF_CENTER = 0x04000000,
  AUDIO20_CHANNEL_CONFIG_RAW_DATA = 0x80000000u,
} audio20_channel_config_t;

/// All remaining definitions are taken from the descriptor descriptions in the UAC2 main specification

/// Audio Class-Control Values UAC2
typedef enum {
  AUDIO20_CTRL_NONE = 0x00,///< No Host access
  AUDIO20_CTRL_R = 0x01,   ///< Host read access only
  AUDIO20_CTRL_RW = 0x03,  ///< Host read write access
} audio20_control_t;

/// Audio Class-Specific AC Interface Descriptor Controls UAC2
typedef enum {
  AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS = 0,
} audio20_cs_ac_interface_control_pos_t;

/// Audio Class-Specific AS Interface Descriptor Controls UAC2
typedef enum {
  AUDIO20_CS_AS_INTERFACE_CTRL_ACTIVE_ALT_SET_POS = 0,
  AUDIO20_CS_AS_INTERFACE_CTRL_VALID_ALT_SET_POS = 2,
} audio20_cs_as_interface_control_pos_t;

/// Audio Class-Specific AS Isochronous Data EP Attributes UAC2
typedef enum {
  AUDIO20_CS_AS_ISO_DATA_EP_ATT_MAX_PACKETS_ONLY = 0x80,
  AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK = 0x00,
} audio20_cs_as_iso_data_ep_attribute_t;

/// Audio Class-Specific AS Isochronous Data EP Controls UAC2
typedef enum {
  AUDIO20_CS_AS_ISO_DATA_EP_CTRL_PITCH_POS = 0,
  AUDIO20_CS_AS_ISO_DATA_EP_CTRL_DATA_OVERRUN_POS = 2,
  AUDIO20_CS_AS_ISO_DATA_EP_CTRL_DATA_UNDERRUN_POS = 4,
} audio20_cs_as_iso_data_ep_control_pos_t;

/// Audio Class-Specific AS Isochronous Data EP Lock Delay Units UAC2
typedef enum {
  AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED = 0x00,
  AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC = 0x01,
  AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_PCM_SAMPLES = 0x02,
} audio20_cs_as_iso_data_ep_lock_delay_unit_t;

/// Audio Class-Clock Source Attributes UAC2
typedef enum {
  AUDIO20_CLOCK_SOURCE_ATT_EXT_CLK = 0x00,
  AUDIO20_CLOCK_SOURCE_ATT_INT_FIX_CLK = 0x01,
  AUDIO20_CLOCK_SOURCE_ATT_INT_VAR_CLK = 0x02,
  AUDIO20_CLOCK_SOURCE_ATT_INT_PRO_CLK = 0x03,
  AUDIO20_CLOCK_SOURCE_ATT_CLK_SYC_SOF = 0x04,
} audio20_clock_source_attribute_t;

/// Audio Class-Clock Source Controls UAC2
typedef enum {
  AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS = 0,
  AUDIO20_CLOCK_SOURCE_CTRL_CLK_VAL_POS = 2,
} audio20_clock_source_control_pos_t;

/// Audio Class-Clock Selector Controls UAC2
typedef enum {
  AUDIO20_CLOCK_SELECTOR_CTRL_POS = 0,
} audio20_clock_selector_control_pos_t;

/// Audio Class-Clock Multiplier Controls UAC2
typedef enum {
  AUDIO20_CLOCK_MULTIPLIER_CTRL_NUMERATOR_POS = 0,
  AUDIO20_CLOCK_MULTIPLIER_CTRL_DENOMINATOR_POS = 2,
} audio20_clock_multiplier_control_pos_t;

/// Audio Class-Input Terminal Controls UAC2
typedef enum {
  AUDIO20_IN_TERM_CTRL_CPY_PROT_POS = 0,
  AUDIO20_IN_TERM_CTRL_CONNECTOR_POS = 2,
  AUDIO20_IN_TERM_CTRL_OVERLOAD_POS = 4,
  AUDIO20_IN_TERM_CTRL_CLUSTER_POS = 6,
  AUDIO20_IN_TERM_CTRL_UNDERFLOW_POS = 8,
  AUDIO20_IN_TERM_CTRL_OVERFLOW_POS = 10,
} audio20_terminal_input_control_pos_t;

/// Audio Class-Output Terminal Controls UAC2
typedef enum {
  AUDIO20_OUT_TERM_CTRL_CPY_PROT_POS = 0,
  AUDIO20_OUT_TERM_CTRL_CONNECTOR_POS = 2,
  AUDIO20_OUT_TERM_CTRL_OVERLOAD_POS = 4,
  AUDIO20_OUT_TERM_CTRL_UNDERFLOW_POS = 6,
  AUDIO20_OUT_TERM_CTRL_OVERFLOW_POS = 8,
} audio20_terminal_output_control_pos_t;

/// Audio Class-Feature Unit Controls UAC2
typedef enum {
  AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS = 0,
  AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS = 2,
  AUDIO20_FEATURE_UNIT_CTRL_BASS_POS = 4,
  AUDIO20_FEATURE_UNIT_CTRL_MID_POS = 6,
  AUDIO20_FEATURE_UNIT_CTRL_TREBLE_POS = 8,
  AUDIO20_FEATURE_UNIT_CTRL_GRAPHIC_EQU_POS = 10,
  AUDIO20_FEATURE_UNIT_CTRL_AGC_POS = 12,
  AUDIO20_FEATURE_UNIT_CTRL_DELAY_POS = 14,
  AUDIO20_FEATURE_UNIT_CTRL_BASS_BOOST_POS = 16,
  AUDIO20_FEATURE_UNIT_CTRL_LOUDNESS_POS = 18,
  AUDIO20_FEATURE_UNIT_CTRL_INPUT_GAIN_POS = 20,
  AUDIO20_FEATURE_UNIT_CTRL_INPUT_GAIN_PAD_POS = 22,
  AUDIO20_FEATURE_UNIT_CTRL_PHASE_INV_POS = 24,
  AUDIO20_FEATURE_UNIT_CTRL_UNDERFLOW_POS = 26,
  AUDIO20_FEATURE_UNIT_CTRL_OVERFLOW_POS = 28,
} audio20_feature_unit_control_pos_t;

//--------------------------------------------------------------------+
// USB AUDIO CLASS 2.0 (UAC2) DESCRIPTORS
//--------------------------------------------------------------------+

/// AUDIO Channel Cluster Descriptor UAC2 (4.1)
typedef struct TU_ATTR_PACKED {
  uint8_t bNrChannels;                     ///< Number of channels currently connected.
  uint32_t bmChannelConfig;///< Bitmap according to 'audio20_channel_config_t' with a 1 set if channel is connected and 0 else. In case channels are non-predefined ignore them here (see UAC2 specification 4.1 Audio Channel Cluster Descriptor.
  uint8_t iChannelNames;                   ///< Index of a string descriptor, describing the name of the first inserted channel with a non-predefined spatial location.
} audio20_desc_channel_cluster_t;

/// AUDIO Class-Specific AC Interface Header Descriptor UAC2 (4.7.2)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor in bytes: 9.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_AC_INTERFACE_HEADER.
  uint16_t bcdADC;           ///< Audio Device Class Specification Release Number in Binary-Coded Decimal. Value: U16_TO_U8S_LE(0x0200).
  uint8_t bCategory;         ///< Constant, indicating the primary use of this audio function, as intended by the manufacturer. See: audio20_function_code_t.
  uint16_t wTotalLength;     ///< Total number of bytes returned for the class-specific AudioControl interface descriptor. Includes the combined length of this descriptor header and all Clock Source, Unit and Terminal descriptors.
  uint8_t bmControls;        ///< See: audio20_cs_ac_interface_control_pos_t.
} audio20_desc_cs_ac_interface_t;
TU_VERIFY_STATIC(sizeof(audio20_desc_cs_ac_interface_t) == 9, "size is not correct");

/// AUDIO Clock Source Descriptor UAC2 (4.7.2.1)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor in bytes: 8.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_AC_INTERFACE_CLOCK_SOURCE.
  uint8_t bClockID;          ///< Constant uniquely identifying the Clock Source Entity within the audio function. This value is used in all requests to address this Entity.
  uint8_t bmAttributes;      ///< See: audio20_clock_source_attribute_t.
  uint8_t bmControls;        ///< See: audio20_clock_source_control_pos_t.
  uint8_t bAssocTerminal;    ///< Terminal ID of the Terminal that is associated with this Clock Source.
  uint8_t iClockSource;      ///< Index of a string descriptor, describing the Clock Source Entity.
} audio20_desc_clock_source_t;

/// AUDIO Clock Selector Descriptor UAC2 (4.7.2.2) for ONE pin
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor, in bytes: 7+p.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_AC_INTERFACE_CLOCK_SELECTOR.
  uint8_t bClockID;          ///< Constant uniquely identifying the Clock Selector Entity within the audio function. This value is used in all requests to address this Entity.
  uint8_t bNrInPins;         ///< Number of Input Pins of this Unit: p = 1 thus bNrInPins = 1.
  uint8_t baCSourceID;       ///< ID of the Clock Entity to which the first Clock Input Pin of this Clock Selector Entity is connected..
  uint8_t bmControls;        ///< See: audio20_clock_selector_control_pos_t.
  uint8_t iClockSource;      ///< Index of a string descriptor, describing the Clock Selector Entity.
} audio20_desc_clock_selector_t;

/// AUDIO Clock Selector Descriptor (4.7.2.2) for multiple pins
#define audio20_desc_clock_selector_n_t(source_num) \
  struct TU_ATTR_PACKED {                           \
    uint8_t bLength;                                \
    uint8_t bDescriptorType;                        \
    uint8_t bDescriptorSubType;                     \
    uint8_t bClockID;                               \
    uint8_t bNrInPins;                              \
    struct TU_ATTR_PACKED {                         \
      uint8_t baSourceID;                           \
    } sourceID[source_num];                         \
    uint8_t bmControls;                             \
    uint8_t iClockSource;                           \
  }

/// AUDIO Clock Multiplier Descriptor UAC2 (4.7.2.3)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor, in bytes: 7.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_AC_INTERFACE_CLOCK_MULTIPLIER.
  uint8_t bClockID;          ///< Constant uniquely identifying the Clock Multiplier Entity within the audio function. This value is used in all requests to address this Entity.
  uint8_t bCSourceID;        ///< ID of the Clock Entity to which the last Clock Input Pin of this Clock Selector Entity is connected.
  uint8_t bmControls;        ///< See: audio20_clock_multiplier_control_pos_t.
  uint8_t iClockSource;      ///< Index of a string descriptor, describing the Clock Multiplier Entity.
} audio20_desc_clock_multiplier_t;

/// AUDIO Input Terminal Descriptor(4.7.2.4)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor, in bytes: 17.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL.
  uint8_t bTerminalID;       ///< Constant uniquely identifying the Terminal within the audio function. This value is used in all requests to address this terminal.
  uint16_t wTerminalType;    ///< Constant characterizing the type of Terminal. See: audio_terminal_type_t for USB streaming and audio_terminal_input_type_t for other input types.
  uint8_t bAssocTerminal;    ///< ID of the Output Terminal to which this Input Terminal is associated.
  uint8_t bCSourceID;        ///< ID of the Clock Entity to which this Input Terminal is connected.
  uint8_t bNrChannels;       ///< Number of logical output channels in the Terminal’s output audio channel cluster.
  uint32_t bmChannelConfig;  ///< Describes the spatial location of the logical channels. See:audio20_channel_config_t.
  uint8_t iChannelNames;     ///< Index of a string descriptor, describing the name of the first logical channel.
  uint16_t bmControls;       ///< See: audio_terminal_input_control_pos_t.
  uint8_t iTerminal;         ///< Index of a string descriptor, describing the Input Terminal.
} audio20_desc_input_terminal_t;

/// AUDIO Output Terminal Descriptor UAC2 (4.7.2.5)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor, in bytes: 12.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_AC_INTERFACE_OUTPUT_TERMINAL.
  uint8_t bTerminalID;       ///< Constant uniquely identifying the Terminal within the audio function. This value is used in all requests to address this Terminal.
  uint16_t wTerminalType;    ///< Constant characterizing the type of Terminal. See: audio20_terminal_type_t for USB streaming and audio20_terminal_output_type_t for other output types.
  uint8_t bAssocTerminal;    ///< Constant, identifying the Input Terminal to which this Output Terminal is associated.
  uint8_t bSourceID;         ///< ID of the Unit or Terminal to which this Terminal is connected.
  uint8_t bCSourceID;        ///< ID of the Clock Entity to which this Output Terminal is connected.
  uint16_t bmControls;       ///< See: audio20_terminal_output_control_pos_t.
  uint8_t iTerminal;         ///< Index of a string descriptor, describing the Output Terminal.
} audio20_desc_output_terminal_t;

/// AUDIO Feature Unit Descriptor UAC2 (4.7.2.8) for ONE channel
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor, in bytes: 14.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT.
  uint8_t bUnitID;           ///< Constant uniquely identifying the Unit within the audio function. This value is used in all requests to address this Unit.
  uint8_t bSourceID;         ///< ID of the Unit or Terminal to which this Feature Unit is connected.
  struct TU_ATTR_PACKED {
    uint32_t bmaControls;///< See: audio20_feature_unit_control_pos_t. Controls0 is master channel 0 (always present) and Controls1 is logical channel 1.
  } controls[2];
  uint8_t iTerminal;///< Index of a string descriptor, describing this Feature Unit.
} audio20_desc_feature_unit_t;

/// AUDIO Feature Unit Descriptor(4.7.2.8) for multiple channels
#define audio20_desc_feature_unit_n_t(ch_num) \
  struct TU_ATTR_PACKED {                     \
    uint8_t bLength; /* 6+(ch_num+1)*4 */     \
    uint8_t bDescriptorType;                  \
    uint8_t bDescriptorSubType;               \
    uint8_t bUnitID;                          \
    uint8_t bSourceID;                        \
    struct TU_ATTR_PACKED {                   \
      uint32_t bmaControls;                   \
    } controls[ch_num + 1];                   \
    uint8_t iTerminal;                        \
  }

/// AUDIO Class-Specific AS Interface Descriptor(4.9.2)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor, in bytes: 16.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_AS_INTERFACE_AS_GENERAL.
  uint8_t bTerminalLink;     ///< The Terminal ID of the Terminal to which this interface is connected.
  uint8_t bmControls;        ///< See: audio20_cs_as_interface_control_pos_t.
  uint8_t bFormatType;       ///< Constant identifying the Format Type the AudioStreaming interface is using. See: audio20_format_type_t.
  uint32_t bmFormats;        ///< The Audio Data Format(s) that can be used to communicate with this interface.See: audio20_data_format_type_I_t.
  uint8_t bNrChannels;       ///< Number of physical channels in the AS Interface audio channel cluster.
  uint32_t bmChannelConfig;  ///< Describes the spatial location of the physical channels. See: audio20_channel_config_t.
  uint8_t iChannelNames;     ///< Index of a string descriptor, describing the name of the first physical channel.
} audio20_desc_cs_as_interface_t;

/// AUDIO Type I Format Type Descriptor(2.3.1.6 - Audio Formats)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor, in bytes: 6.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_AS_INTERFACE_FORMAT_TYPE.
  uint8_t bFormatType;       ///< Constant identifying the Format Type the AudioStreaming interface is using. Value: AUDIO20_FORMAT_TYPE_I.
  uint8_t bSubslotSize;      ///< The number of bytes occupied by one audio subslot. Can be 1, 2, 3 or 4.
  uint8_t bBitResolution;    ///< The number of effectively used bits from the available bits in an audio subslot.
} audio20_desc_type_I_format_t;

/// AUDIO Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2)
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;           ///< Size of this descriptor, in bytes: 8.
  uint8_t bDescriptorType;   ///< Descriptor Type. Value: TUSB_DESC_CS_ENDPOINT.
  uint8_t bDescriptorSubType;///< Descriptor SubType. Value: AUDIO20_CS_EP_SUBTYPE_GENERAL.
  uint8_t bmAttributes;      ///< See: audio20_cs_as_iso_data_ep_attribute_t.
  uint8_t bmControls;        ///< See: audio20_cs_as_iso_data_ep_control_pos_t.
  uint8_t bLockDelayUnits;   ///< Indicates the units used for the wLockDelay field. See: audio20_cs_as_iso_data_ep_lock_delay_unit_t.
  uint16_t wLockDelay;       ///< Indicates the time it takes this endpoint to reliably lock its internal clock recovery circuitry. Units used depend on the value of the bLockDelayUnits field.
} audio20_desc_cs_as_iso_data_ep_t;

// 5.2.2 Control Request Layout
typedef struct TU_ATTR_PACKED {
  union {
    struct TU_ATTR_PACKED {
      uint8_t recipient : 5;///< Recipient type tusb_request_recipient_t.
      uint8_t type : 2;     ///< Request type tusb_request_type_t.
      uint8_t direction : 1;///< Direction type. tusb_dir_t
    } bmRequestType_bit;

    uint8_t bmRequestType;
  };

  uint8_t bRequest;///< Request type audio_cs_req_t
  uint8_t bChannelNumber;
  uint8_t bControlSelector;
  union {
    uint8_t bInterface;
    uint8_t bEndpoint;
  };
  uint8_t bEntityID;
  uint16_t wLength;
} audio20_control_request_t;

//// 5.2.3 Control Request Parameter Block Layout

// 5.2.3.1 1-byte Control CUR Parameter Block
typedef struct TU_ATTR_PACKED {
  int8_t bCur;///< The setting for the CUR attribute of the addressed Control
} audio20_control_cur_1_t;

// 5.2.3.2 2-byte Control CUR Parameter Block
typedef struct TU_ATTR_PACKED {
  int16_t bCur;///< The setting for the CUR attribute of the addressed Control
} audio20_control_cur_2_t;

// 5.2.3.3 4-byte Control CUR Parameter Block
typedef struct TU_ATTR_PACKED {
  int32_t bCur;///< The setting for the CUR attribute of the addressed Control
} audio20_control_cur_4_t;

// Use the following ONLY for RECEIVED data - compiler does not know how many subranges are defined! Use the #define macros below for predefined lengths.

// 5.2.3.1 1-byte Control RANGE Parameter Block
#define audio20_control_range_1_n_t(numSubRanges)                                                      \
  struct TU_ATTR_PACKED {                                                                              \
    uint16_t wNumSubRanges;                                                                            \
    struct TU_ATTR_PACKED {                                                                            \
      int8_t bMin;  /*The setting for the MIN attribute of the nth subrange of the addressed Control*/ \
      int8_t bMax;  /*The setting for the MAX attribute of the nth subrange of the addressed Control*/ \
      uint8_t bRes; /*The setting for the RES attribute of the nth subrange of the addressed Control*/ \
    } subrange[numSubRanges];                                                                          \
  }

/// 5.2.3.2 2-byte Control RANGE Parameter Block
#define audio20_control_range_2_n_t(numSubRanges)                                                       \
  struct TU_ATTR_PACKED {                                                                               \
    uint16_t wNumSubRanges;                                                                             \
    struct TU_ATTR_PACKED {                                                                             \
      int16_t bMin;  /*The setting for the MIN attribute of the nth subrange of the addressed Control*/ \
      int16_t bMax;  /*The setting for the MAX attribute of the nth subrange of the addressed Control*/ \
      uint16_t bRes; /*The setting for the RES attribute of the nth subrange of the addressed Control*/ \
    } subrange[numSubRanges];                                                                           \
  }

// 5.2.3.3 4-byte Control RANGE Parameter Block
#define audio20_control_range_4_n_t(numSubRanges)                                                       \
  struct TU_ATTR_PACKED {                                                                               \
    uint16_t wNumSubRanges;                                                                             \
    struct TU_ATTR_PACKED {                                                                             \
      int32_t bMin;  /*The setting for the MIN attribute of the nth subrange of the addressed Control*/ \
      int32_t bMax;  /*The setting for the MAX attribute of the nth subrange of the addressed Control*/ \
      uint32_t bRes; /*The setting for the RES attribute of the nth subrange of the addressed Control*/ \
    } subrange[numSubRanges];                                                                           \
  }

// 6.1 Interrupt Data Message Format
typedef struct TU_ATTR_PACKED {
  uint8_t bInfo;
  uint8_t bAttribute;
  union {
    uint16_t wValue;
    struct {
      uint8_t wValue_cn_or_mcn;
      uint8_t wValue_cs;
    };
  };
  union {
    uint16_t wIndex;
    struct {
      uint8_t wIndex_ep_or_int;
      uint8_t wIndex_entity_id;
    };
  };
} audio20_interrupt_data_t;

//--------------------------------------------------------------------+
// APPLICATION HELPER DEFINITIONS
//--------------------------------------------------------------------+

// Combined Interrupt Data Message Format for both UAC1 and UAC2
typedef union {
  audio10_interrupt_data_t v1;
  audio20_interrupt_data_t v2;
} audio_interrupt_data_t;

// MIDI1.0 use the same CS AC Interface Descriptor as UAC1
typedef audio10_desc_cs_ac_interface_n_t(1) midi10_desc_cs_ac_interface_t;

// UAC1.0 AC Interface Descriptor with 1 interface, used to read fields other than baInterfaceNr
typedef audio10_desc_cs_ac_interface_n_t(1) audio10_desc_cs_ac_interface_1_t;

/** @} */

#ifdef __cplusplus
}
#endif

#endif

/** @} */
