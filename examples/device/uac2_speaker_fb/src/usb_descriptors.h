/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 HiFiPhile
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

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

//--------------------------------------------------------------------+
// UAC2 DESCRIPTOR TEMPLATES
//--------------------------------------------------------------------+

// Defined in TUD_AUDIO20_SPEAKER_STEREO_FB_DESCRIPTOR
#define UAC2_ENTITY_CLOCK               0x04
#define UAC2_ENTITY_INPUT_TERMINAL      0x01
#define UAC2_ENTITY_FEATURE_UNIT        0x02
#define UAC2_ENTITY_OUTPUT_TERMINAL     0x03

#define TUD_AUDIO20_SPEAKER_STEREO_FB_DESC_LEN (TUD_AUDIO20_DESC_IAD_LEN\
  + TUD_AUDIO20_DESC_STD_AC_LEN\
  + TUD_AUDIO20_DESC_CS_AC_LEN\
  + TUD_AUDIO20_DESC_CLK_SRC_LEN\
  + TUD_AUDIO20_DESC_INPUT_TERM_LEN\
  + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN\
  + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2)\
  + TUD_AUDIO20_DESC_STD_AS_LEN\
  + TUD_AUDIO20_DESC_STD_AS_LEN\
  + TUD_AUDIO20_DESC_CS_AS_INT_LEN\
  + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN\
  + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN\
  + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN\
  + TUD_AUDIO20_DESC_STD_AS_ISO_FB_EP_LEN)

#define TUD_AUDIO20_SPEAKER_STEREO_FB_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epout, _epoutsize, _epfb, _epfbsize) \
  /* Standard Interface Association Descriptor (IAD) */\
  TUD_AUDIO20_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),\
  /* Standard AC Interface Descriptor(4.7.1) */\
  TUD_AUDIO20_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
  /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
  TUD_AUDIO20_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO20_FUNC_DESKTOP_SPEAKER, /*_totallen*/ TUD_AUDIO20_DESC_CLK_SRC_LEN+TUD_AUDIO20_DESC_INPUT_TERM_LEN+TUD_AUDIO20_DESC_OUTPUT_TERM_LEN+TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2), /*_ctrl*/ AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
  /* Clock Source Descriptor(4.7.2.1) */\
  TUD_AUDIO20_DESC_CLK_SRC(/*_clkid*/ 0x04, /*_attr*/ AUDIO20_CLOCK_SOURCE_ATT_INT_PRO_CLK, /*_ctrl*/ (AUDIO20_CTRL_RW << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ 0x01,  /*_stridx*/ 0x00),\
  /* Input Terminal Descriptor(4.7.2.4) */\
  TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ 0x01, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ 0x04, /*_nchannelslogical*/ 0x02, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
  /* Output Terminal Descriptor(4.7.2.5) */\
  TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ 0x03, /*_termtype*/ AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, /*_assocTerm*/ 0x01, /*_srcid*/ 0x02, /*_clkid*/ 0x04, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
  /* Feature Unit Descriptor(4.7.2.8) */\
  TUD_AUDIO20_DESC_FEATURE_UNIT(/*_unitid*/ 0x02, /*_srcid*/ 0x01, /*_stridx*/ 0x00, /*_ctrlch0master*/ AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/ AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch2*/ AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS),\
  /* Standard AS Interface Descriptor(4.9.1) */\
  /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
  TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum) + 1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.9.1) */\
  /* Interface 1, Alternate 1 - alternate interface for data streaming */\
  TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum) + 1), /*_altset*/ 0x01, /*_nEPs*/ 0x02, /*_stridx*/ 0x00),\
  /* Class-Specific AS Interface Descriptor(4.9.2) */\
  TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ 0x01, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ 0x02, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
  /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
  TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
  /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
  TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ _epoutsize, /*_interval*/ 0x01),\
  /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
  TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),\
  /* Standard AS Isochronous Feedback Endpoint Descriptor(4.10.2.1) */\
  TUD_AUDIO20_DESC_STD_AS_ISO_FB_EP(/*_ep*/ _epfb, /*_epsize*/ _epfbsize, /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1)

//--------------------------------------------------------------------+
// UAC1 DESCRIPTOR TEMPLATES
//--------------------------------------------------------------------+

// Defined in TUD_AUDIO10_SPEAKER_STEREO_FB_DESCRIPTOR
#define UAC1_ENTITY_INPUT_TERMINAL      0x01
#define UAC1_ENTITY_FEATURE_UNIT        0x02
#define UAC1_ENTITY_OUTPUT_TERMINAL     0x03

#define TUD_AUDIO10_SPEAKER_STEREO_FB_DESC_LEN(_nfreqs) (\
  + TUD_AUDIO10_DESC_STD_AC_LEN\
  + TUD_AUDIO10_DESC_CS_AC_LEN(1)\
  + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
  + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
  + TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(2)\
  + TUD_AUDIO10_DESC_STD_AS_LEN\
  + TUD_AUDIO10_DESC_STD_AS_LEN\
  + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
  + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
  + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
  + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN\
  + TUD_AUDIO10_DESC_STD_AS_ISO_SYNC_EP_LEN)

#define TUD_AUDIO10_SPEAKER_STEREO_FB_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epout, _epoutsize, _epfb, ...) \
  /* Standard AC Interface Descriptor(4.3.1) */\
  TUD_AUDIO10_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
  /* Class-Specific AC Interface Header Descriptor(4.3.2) */\
  TUD_AUDIO10_DESC_CS_AC(/*_bcdADC*/ 0x0100, /*_totallen*/ (TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN+TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(2)), /*_itf*/ ((_itfnum)+1)),\
  /* Input Terminal Descriptor(4.3.2.1) */\
  TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ 0x01, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_nchannels*/ 0x02, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_LEFT_FRONT | AUDIO10_CHANNEL_CONFIG_RIGHT_FRONT, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
  /* Output Terminal Descriptor(4.3.2.2) */\
  TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ 0x03, /*_termtype*/ AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, /*_assocTerm*/ 0x00, /*_srcid*/ 0x02, /*_stridx*/ 0x00),\
  /* Feature Unit Descriptor(4.3.2.5) */\
  TUD_AUDIO10_DESC_FEATURE_UNIT(/*_unitid*/ 0x02, /*_srcid*/ 0x01, /*_stridx*/ 0x00, /*_ctrlmaster*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch1*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch2*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME)),\
  /* Standard AS Interface Descriptor(4.5.1) */\
  /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
  TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.5.1) */\
  /* Interface 1, Alternate 1 - alternate interface for data streaming */\
  TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x02, /*_stridx*/ 0x00),\
  /* Class-Specific AS Interface Descriptor(4.5.2) */\
  TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ 0x01, /*_delay*/ 0x00, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
  /* Type I Format Type Descriptor(2.2.5) */\
  TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ 0x02, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
  /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
  TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS), /*_maxEPsize*/ _epoutsize, /*_interval*/ 0x01, /*_sync_ep*/ _epfb),\
  /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
  TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000),\
  /* Standard AS Isochronous Synch Endpoint Descriptor (4.6.2.1) */\
  TUD_AUDIO10_DESC_STD_AS_ISO_SYNC_EP(/*_ep*/ _epfb, /*_bRefresh*/ 0)

//---------------------------------------------------------------------------+
//          UAC1 Isochronous Synch Endpoint bRefresh Workaround
//
// bRefresh value is set to 0, while UAC1 spec requires it to be between
// 1 (2 ms) and 9 (512 ms)
//
// This value has been tested to work with Windows, macOS and Linux.
//
// Rationale:
// Some USB device controllers (e.g. Synopsys DWC2) require a known transfer
// interval to manually schedule isochronous IN transfers. For data isochronous
// endpoints, the bInterval field in the endpoint descriptor is used. However,
// for synch endpoint it's unclear which field the host uses to determine the
// transfer interval. Windows and macOS use bRefresh, while Linux uses bInterval.
//
// Since bInterval is fixed to 1, if bRefresh is set to 2 then Windows and macOS
// will schedule the feedback transfer every 4 ms, but Linux will schedule it
// every 1 ms. DWC2 controller cannot handle this discrepancy without knwowing
// the actual interval, therefore we set bRefresh to 0 to let the transfer
// execute every 1 ms, which is the same as bInterval.
//
// Rant:
// WTF USB-IF? Why have two fields that mean the same thing? Why not just use
// bInterval for both data and synch endpoints? Why is bRefresh even necessary?
//
// Note:
// For the moment DWC2 driver doesn't have proper support for bInterval > 1
// for isochronous IN endpoints. The implementation would be complex and CPU
// intensive (cfr.
// https://github.com/torvalds/linux/blob/master/drivers/usb/dwc2/gadget.c)
// It MAY work in some cases if you are lucky, but it's not guaranteed.
//---------------------------------------------------------------------------+

#endif
