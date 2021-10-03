#define _DECODE_C_

//----------------------------------------------------------------------------- ----------------------------------------
#include "class/audio/audio.h"
#include "class/midi/midi.h"

enum {
	TUSB_CLASS_AUDIO_UNDEFINED      = 0x00,
	TUSB_CLASS_AUDIO_AUDIO_CONTROL  = 0x01,
	TUSB_CLASS_AUDIO_AUDIOSTREAMING = 0x02,
	TUSB_CLASS_AUDIO_MIDISTREAMING  = 0x03,
} audio_subclass_t;

enum {
	TUSB_CLASS_HID_NONE           = 0x00,
	TUSB_CLASS_HID_BOOT_INTERFACE = 0x01,
} hid_subclass_t;

// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
// B.6.2 Class-specific MS Bulk IN  Endpoint Descriptor
typedef
	struct TU_ATTR_PACKED {
		uint8_t  bLength;            // Size of this descriptor, in bytes.
		uint8_t  bDescriptorType;    // CS_ENDPOINT descriptor
		uint8_t  bDescriptorSubtype; // MS_GENERAL subtype.
		uint8_t  bNrEmbMIDIJack;     // Number of embedded MIDI IN Jacks.
		uint8_t  baAssocJackID;      // ID of the Embedded MIDI IN Jack.
	}
audio_cs_ms_bulk_ep_t;

#define TUSB_STRING_MAX  (((255 - 2) / 2) +1)  // maxPktSz=255, usedBytes=2, unicode=2, NUL=1

#define IND  "? "    // indent string
#define EOL  "\r\n"  // end of line

//----------------------------------------------------------------------------- ----------------------------------------
static int(*declogf)(const char* fmt, ...) = NULL;  // NULL=Logging disabled

//+============================================================================ ========================================
// Set logging function
//
void  dec_logfSet(int(*fn)(const char* const fmt, ...))
{
	declogf = fn;
}

//+============================================================================ ========================================
// Hexdump function
//
void  dec_hexdump (const char* const msg,  const uint16_t len,  const void* const buf)
{
	declogf("%c [%04X]--%s::%s", *msg, len, (*msg == '>') ? "Tx" : "Rx", msg+1);

	const uint8_t*  bp = buf;

	for (uint16_t i = 0;  i < len;  i++) {
		if      (!(i & 0xF))  declogf(EOL "%c  %04X:  ", *msg, i) ;
		else if (!(i & 0x7))  declogf("  ") ;
		else if (!(i & 0x3))  declogf(" ") ;
		declogf("%02x ", *bp++);
	}
	declogf("%s", EOL);
}

//+============================================================================ ========================================
// Print a STRING DESCRIPTOR as if it were Unicode encoded ASCII
// ...this will probably work more than you might expect
// ...if not, you'll just get a string of ?'s and have to look at the hex dump
//
char*  dec_unicode2asc (const void* const vp,  int len,  char* s,  int max)
{
	const uint8_t*  from   = vp;          //! +1 if the data is big-endian
	const uint8_t*  fromNd = from + len;

	char*           to     = s;
	char* const     toNd   = to + max;

	for ( ;  (from < fromNd) && (to < toNd);  from +=2 , to++)
		*to = ((*from >= 32) && (*from <= 126)) ? *from : '?' ;
	*to = '\0';

	return s;
}

//+============================================================================ ========================================
// Retrieve string descriptor
//
char*  dec_getStringDesc (const uint8_t idx)
{
	if (!idx)  return "" ;

	return "?todo?";
}
//+============================================================================ ========================================
// Get midi jack type string
//
char* dec_midi_jack_type_t (const uint8_t type)
{
	switch (type) {
		case MIDI_JACK_EMBEDDED : return "EMBEDDED" ;  // 0x01
		case MIDI_JACK_EXTERNAL : return "EXTERNAL" ;  // 0x02
		default                 : return "UNKNOWN"  ;
	}
}

//+============================================================================ ========================================
// Audio Class-Specific AC Interface Descriptor Controls UAC2
//
char*  dec_audio_cs_ac_interface_control_pos_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS :  return "LATENCY_POS" ;  // 0
		default                                     :  return "-UNKNOWN-"   ;
	}
}

//+============================================================================ ========================================
// Audio Class-Clock Source Attributes UAC2
//
char*  dec_audio_clock_source_attribute_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_CLOCK_SOURCE_ATT_EXT_CLK     :  return "EXT_CLK"     ;  // 0x00
		case AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK :  return "INT_FIX_CLK" ;  // 0x01
		case AUDIO_CLOCK_SOURCE_ATT_INT_VAR_CLK :  return "INT_VAR_CLK" ;  // 0x02
		case AUDIO_CLOCK_SOURCE_ATT_INT_PRO_CLK :  return "INT_PRO_CLK" ;  // 0x03
		case AUDIO_CLOCK_SOURCE_ATT_CLK_SYC_SOF :  return "CLK_SYC_SOF" ;  // 0x04
		default                                 :  return "-UNKNOWN-"   ;
	}
}

//+============================================================================ ========================================
// Audio Class-Clock Source Controls UAC2
//
char*  dec_audio_clock_source_control_pos_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS :  return "FRQ_POS"   ;  // 0
		case AUDIO_CLOCK_SOURCE_CTRL_CLK_VAL_POS :  return "VAL_POS"   ;  // 2
		default                                  :  return "-UNKNOWN-" ;
	}
}

//+============================================================================ ========================================
// Audio Class-Clock Selector Controls UAC2
//
char*  dec_audio_clock_selector_control_pos_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_CLOCK_SELECTOR_CTRL_POS :  return "POS"       ;  // 0
		default                            :  return "-UNKNOWN-" ;
	}
}

//+============================================================================ ========================================
// Audio Class-Clock Multiplier Controls UAC2
//
char*  dec_audio_clock_multiplier_control_pos_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_CLOCK_MULTIPLIER_CTRL_NUMERATOR_POS   :  return "NUMERATOR_POS"   ;  // 0
		case AUDIO_CLOCK_MULTIPLIER_CTRL_DENOMINATOR_POS :  return "DENOMINATOR_POS" ;  // 2
		default                                          :  return "-UNKNOWN-"       ;
	}
}

//+============================================================================ ========================================
// 2.1 - Audio Class-Terminal Types UAC2
//
char*  dec_audio_terminal_type_t (const uint16_t w)
{
	switch (w) {
		case AUDIO_TERM_TYPE_USB_UNDEFINED   :  return "UNDEFINED"   ;  // 0x0100
		case AUDIO_TERM_TYPE_USB_STREAMING   :  return "STREAMING"   ;  // 0x0101
		case AUDIO_TERM_TYPE_USB_VENDOR_SPEC :  return "VENDOR_SPEC" ;  // 0x01FF
		default                              :  return "-UNKNOWN-"   ;
	}
}

//+============================================================================ ========================================
// 2.2 - Audio Class-Input Terminal Types UAC2
//
char*  dec_audio_terminal_input_type_t (const uint16_t w)
{
	switch (w) {
		case AUDIO_TERM_TYPE_IN_UNDEFINED      :  return "UNDEFINED"      ;  // 0x0200
		case AUDIO_TERM_TYPE_IN_GENERIC_MIC    :  return "GENERIC_MIC"    ;  // 0x0201
		case AUDIO_TERM_TYPE_IN_DESKTOP_MIC    :  return "DESKTOP_MIC"    ;  // 0x0202
		case AUDIO_TERM_TYPE_IN_PERSONAL_MIC   :  return "PERSONAL_MIC"   ;  // 0x0203
		case AUDIO_TERM_TYPE_IN_OMNI_MIC       :  return "OMNI_MIC"       ;  // 0x0204
		case AUDIO_TERM_TYPE_IN_ARRAY_MIC      :  return "ARRAY_MIC"      ;  // 0x0205
		case AUDIO_TERM_TYPE_IN_PROC_ARRAY_MIC :  return "PROC_ARRAY_MIC" ;  // 0x0206
		default                                :  return "-UNKNOWN-"      ;
	}
}

//+============================================================================ ========================================
// Audio Class-Audio Channel Configuration UAC2
//
char*  dec_audio_channel_config_t (const uint32_t d)
{
	switch (d) {
		case AUDIO_CHANNEL_CONFIG_NON_PREDEFINED            :  return "NON_PREDEFINED"            ;  // 0x00000000
		case AUDIO_CHANNEL_CONFIG_FRONT_LEFT                :  return "FRONT_LEFT"                ;  // 0x00000001
		case AUDIO_CHANNEL_CONFIG_FRONT_RIGHT               :  return "FRONT_RIGHT"               ;  // 0x00000002
		case AUDIO_CHANNEL_CONFIG_FRONT_CENTER              :  return "FRONT_CENTER"              ;  // 0x00000004
		case AUDIO_CHANNEL_CONFIG_LOW_FRQ_EFFECTS           :  return "LOW_FRQ_EFFECTS"           ;  // 0x00000008
		case AUDIO_CHANNEL_CONFIG_BACK_LEFT                 :  return "BACK_LEFT"                 ;  // 0x00000010
		case AUDIO_CHANNEL_CONFIG_BACK_RIGHT                :  return "BACK_RIGHT"                ;  // 0x00000020
		case AUDIO_CHANNEL_CONFIG_FRONT_LEFT_OF_CENTER      :  return "FRONT_LEFT_OF_CENTER"      ;  // 0x00000040
		case AUDIO_CHANNEL_CONFIG_FRONT_RIGHT_OF_CENTER     :  return "FRONT_RIGHT_OF_CENTER"     ;  // 0x00000080
		case AUDIO_CHANNEL_CONFIG_BACK_CENTER               :  return "BACK_CENTER"               ;  // 0x00000100
		case AUDIO_CHANNEL_CONFIG_SIDE_LEFT                 :  return "SIDE_LEFT"                 ;  // 0x00000200
		case AUDIO_CHANNEL_CONFIG_SIDE_RIGHT                :  return "SIDE_RIGHT"                ;  // 0x00000400
		case AUDIO_CHANNEL_CONFIG_TOP_CENTER                :  return "TOP_CENTER"                ;  // 0x00000800
		case AUDIO_CHANNEL_CONFIG_TOP_FRONT_LEFT            :  return "TOP_FRONT_LEFT"            ;  // 0x00001000
		case AUDIO_CHANNEL_CONFIG_TOP_FRONT_CENTER          :  return "TOP_FRONT_CENTER"          ;  // 0x00002000
		case AUDIO_CHANNEL_CONFIG_TOP_FRONT_RIGHT           :  return "TOP_FRONT_RIGHT"           ;  // 0x00004000
		case AUDIO_CHANNEL_CONFIG_TOP_BACK_LEFT             :  return "TOP_BACK_LEFT"             ;  // 0x00008000
		case AUDIO_CHANNEL_CONFIG_TOP_BACK_CENTER           :  return "TOP_BACK_CENTER"           ;  // 0x00010000
		case AUDIO_CHANNEL_CONFIG_TOP_BACK_RIGHT            :  return "TOP_BACK_RIGHT"            ;  // 0x00020000
		case AUDIO_CHANNEL_CONFIG_TOP_FRONT_LEFT_OF_CENTER  :  return "TOP_FRONT_LEFT_OF_CENTER"  ;  // 0x00040000
		case AUDIO_CHANNEL_CONFIG_TOP_FRONT_RIGHT_OF_CENTER :  return "TOP_FRONT_RIGHT_OF_CENTER" ;  // 0x00080000
		case AUDIO_CHANNEL_CONFIG_LEFT_LOW_FRQ_EFFECTS      :  return "LEFT_LOW_FRQ_EFFECTS"      ;  // 0x00100000
		case AUDIO_CHANNEL_CONFIG_RIGHT_LOW_FRQ_EFFECTS     :  return "RIGHT_LOW_FRQ_EFFECTS"     ;  // 0x00200000
		case AUDIO_CHANNEL_CONFIG_TOP_SIDE_LEFT             :  return "TOP_SIDE_LEFT"             ;  // 0x00400000
		case AUDIO_CHANNEL_CONFIG_TOP_SIDE_RIGHT            :  return "TOP_SIDE_RIGHT"            ;  // 0x00800000
		case AUDIO_CHANNEL_CONFIG_BOTTOM_CENTER             :  return "BOTTOM_CENTER"             ;  // 0x01000000
		case AUDIO_CHANNEL_CONFIG_BACK_LEFT_OF_CENTER       :  return "BACK_LEFT_OF_CENTER"       ;  // 0x02000000
		case AUDIO_CHANNEL_CONFIG_BACK_RIGHT_OF_CENTER      :  return "BACK_RIGHT_OF_CENTER"      ;  // 0x04000000
		case AUDIO_CHANNEL_CONFIG_RAW_DATA                  :  return "RAW_DATA"                  ;  // 0x80000000
		default                                             :  return "-UNKNOWN-"                 ;
	}
}

//+============================================================================ ========================================
// Audio Class-Input Terminal Controls UAC2
//
char*  dec_audio_terminal_input_control_pos_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_IN_TERM_CTRL_CPY_PROT_POS  :  return "CPY_PROT_POS"  ;  //  0
		case AUDIO_IN_TERM_CTRL_CONNECTOR_POS :  return "CONNECTOR_POS" ;  //  2
		case AUDIO_IN_TERM_CTRL_OVERLOAD_POS  :  return "OVERLOAD_POS"  ;  //  4
		case AUDIO_IN_TERM_CTRL_CLUSTER_POS   :  return "CLUSTER_POS"   ;  //  6
		case AUDIO_IN_TERM_CTRL_UNDERFLOW_POS :  return "UNDERFLOW_POS" ;  //  8
		case AUDIO_IN_TERM_CTRL_OVERFLOW_POS  :  return "OVERFLOW_POS"  ;  // 10
		default                               :  return "-UNKNOWN-"     ;
	}
}

//+============================================================================ ========================================
// 2.3 - Audio Class-Output Terminal Types UAC2
//
char*  dec_audio_terminal_output_type_t (const uint16_t w)
{
	switch (w) {
		case AUDIO_TERM_TYPE_OUT_UNDEFINED               :  return "UNDEFINED"               ;  // 0x0300
		case AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER         :  return "GENERIC_SPEAKER"         ;  // 0x0301
		case AUDIO_TERM_TYPE_OUT_HEADPHONES              :  return "HEADPHONES"              ;  // 0x0302
		case AUDIO_TERM_TYPE_OUT_HEAD_MNT_DISP_AUIDO     :  return "HEAD_MNT_DISP_AUIDO"     ;  // 0x0303
		case AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER         :  return "DESKTOP_SPEAKER"         ;  // 0x0304
		case AUDIO_TERM_TYPE_OUT_ROOM_SPEAKER            :  return "ROOM_SPEAKER"            ;  // 0x0305
		case AUDIO_TERM_TYPE_OUT_COMMUNICATION_SPEAKER   :  return "COMMUNICATION_SPEAKER"   ;  // 0x0306
		case AUDIO_TERM_TYPE_OUT_LOW_FRQ_EFFECTS_SPEAKER :  return "LOW_FRQ_EFFECTS_SPEAKER" ;  // 0x0307
		default                                          :  return "-UNKNOWN-"               ;
	}
}

//+============================================================================ ========================================
// Audio Class-Feature Unit Controls UAC2
//
char*  dec_audio_feature_unit_control_pos_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_FEATURE_UNIT_CTRL_MUTE_POS           :  return "MUTE_POS"           ;  //  0
		case AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS         :  return "VOLUME_POS"         ;  //  2
		case AUDIO_FEATURE_UNIT_CTRL_BASS_POS           :  return "BASS_POS"           ;  //  4
		case AUDIO_FEATURE_UNIT_CTRL_MID_POS            :  return "MID_POS"            ;  //  6
		case AUDIO_FEATURE_UNIT_CTRL_TREBLE_POS         :  return "TREBLE_POS"         ;  //  8
		case AUDIO_FEATURE_UNIT_CTRL_GRAPHIC_EQU_POS    :  return "GRAPHIC_EQU_POS"    ;  // 10
		case AUDIO_FEATURE_UNIT_CTRL_AGC_POS            :  return "AGC_POS"            ;  // 12
		case AUDIO_FEATURE_UNIT_CTRL_DELAY_POS          :  return "DELAY_POS"          ;  // 14
		case AUDIO_FEATURE_UNIT_CTRL_BASS_BOOST_POS     :  return "BASS_BOOST_POS"     ;  // 16
		case AUDIO_FEATURE_UNIT_CTRL_LOUDNESS_POS       :  return "LOUDNESS_POS"       ;  // 18
		case AUDIO_FEATURE_UNIT_CTRL_INPUT_GAIN_POS     :  return "INPUT_GAIN_POS"     ;  // 20
		case AUDIO_FEATURE_UNIT_CTRL_INPUT_GAIN_PAD_POS :  return "INPUT_GAIN_PAD_POS" ;  // 22
		case AUDIO_FEATURE_UNIT_CTRL_PHASE_INV_POS      :  return "PHASE_INV_POS"      ;  // 24
		case AUDIO_FEATURE_UNIT_CTRL_UNDERFLOW_POS      :  return "UNDERFLOW_POS"      ;  // 26
		case AUDIO_FEATURE_UNIT_CTRL_OVERFLOW_POS       :  return "OVERFLOW_POS"       ;  // 28
		default                                         :  return "-UNKNOWN-"          ;
	}
}

//+============================================================================ ========================================
// Audio Class-Specific AS Interface Descriptor Controls UAC2
//
char*  dec_audio_cs_as_interface_control_pos_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_CS_AS_INTERFACE_CTRL_ACTIVE_ALT_SET_POS :  return "ACTIVE_ALT_SET_POS" ;  // 0
		case AUDIO_CS_AS_INTERFACE_CTRL_VALID_ALT_SET_POS  :  return "VALID_ALT_SET_POS"  ;  // 2
		default                                            :  return "-UNKNOWN-"          ;
	}
}

//+============================================================================ ========================================
// A.1 - Audio Class-Format Type Codes UAC2
//
char*  dec_audio_format_type_t (const uint8_t b)
{
	switch (b) {
		case AUDIO_FORMAT_TYPE_UNDEFINED :  return "FORMAT_TYPE_UNDEFINED" ;  // 0x00
		case AUDIO_FORMAT_TYPE_I         :  return "FORMAT_TYPE_I"         ;  // 0x01
		case AUDIO_FORMAT_TYPE_II        :  return "FORMAT_TYPE_II"        ;  // 0x02
		case AUDIO_FORMAT_TYPE_III       :  return "FORMAT_TYPE_III"       ;  // 0x03
		case AUDIO_FORMAT_TYPE_IV        :  return "FORMAT_TYPE_IV"        ;  // 0x04
		case AUDIO_EXT_FORMAT_TYPE_I     :  return "EXT_FORMAT_TYPE_I"     ;  // 0x81
		case AUDIO_EXT_FORMAT_TYPE_II    :  return "EXT_FORMAT_TYPE_II"    ;  // 0x82
		case AUDIO_EXT_FORMAT_TYPE_III   :  return "EXT_FORMAT_TYPE_III"   ;  // 0x83
		default                          :  return "-UNKNOWN-"             ;
	}
}

//+============================================================================ ========================================
// A.2.1 - Audio Class-Audio Data Format Type I UAC2
//
char*  dec_audio_data_format_type_I_t (const uint32_t d)
{
	switch (d) {
		case AUDIO_DATA_FORMAT_TYPE_I_PCM        :  return "PCM"        ;  // 0x00000001
		case AUDIO_DATA_FORMAT_TYPE_I_PCM8       :  return "PCM8"       ;  // 0x00000002
		case AUDIO_DATA_FORMAT_TYPE_I_IEEE_FLOAT :  return "IEEE_FLOAT" ;  // 0x00000004
		case AUDIO_DATA_FORMAT_TYPE_I_ALAW       :  return "ALAW"       ;  // 0x00000008
		case AUDIO_DATA_FORMAT_TYPE_I_MULAW      :  return "MULAW"      ;  // 0x00000010
		case AUDIO_DATA_FORMAT_TYPE_I_RAW_DATA   :  return "RAW_DATA"   ;  // 0x80000000
		default                                  :  return "-UNKNOWN-"  ;
	}
}

//+============================================================================ ========================================
// Get device/interface class string
//
char*  dec_tusb_class_code_t (const uint8_t class)
{
	switch (class) {
		case TUSB_CLASS_UNSPECIFIED          :  return "UNSPECIFIED"          ;  // device
		case TUSB_CLASS_AUDIO                :  return "AUDIO"                ;  // interface
		case TUSB_CLASS_CDC                  :  return "CDC"                  ;  // both
		case TUSB_CLASS_HID                  :  return "HID"                  ;  // interface
		case TUSB_CLASS_RESERVED_4           :  return "RESERVED(4)"          ;
		case TUSB_CLASS_PHYSICAL             :  return "PHYSICAL"             ;  // interface
		case TUSB_CLASS_IMAGE                :  return "IMAGE"                ;  // interface
		case TUSB_CLASS_PRINTER              :  return "PRINTER"              ;  // interface
		case TUSB_CLASS_MSC                  :  return "MSC"                  ;  // interface
		case TUSB_CLASS_HUB                  :  return "HUB"                  ;  // device
		case TUSB_CLASS_CDC_DATA             :  return "CDC DATA"             ;  // interface
		case TUSB_CLASS_SMART_CARD           :  return "SMART CARD"           ;  // interface
		case TUSB_CLASS_RESERVED_12          :  return "RESERVED(12)"         ;
		case TUSB_CLASS_CONTENT_SECURITY     :  return "CONTENT SECURITY"     ;  // interface
		case TUSB_CLASS_VIDEO                :  return "VIDEO"                ;  // interface
		case TUSB_CLASS_PERSONAL_HEALTHCARE  :  return "PERSONAL HEALTHCARE"  ;  // interface
		case TUSB_CLASS_AUDIO_VIDEO          :  return "AUDIO/VIDEO"          ;  // interface
		case TUSB_CLASS_DIAGNOSTIC           :  return "DIAGNOSTIC"           ;  // both
		case TUSB_CLASS_WIRELESS_CONTROLLER	 :  return "WIRELESS CONTROLLER"  ;  // interface
		case TUSB_CLASS_MISC                 :  return "MISC"                 ;  // both
		case TUSB_CLASS_APPLICATION_SPECIFIC :  return "APPLICATION SPECIFIC" ;  // interface
		case TUSB_CLASS_VENDOR_SPECIFIC      :  return "VENDOR SPECIFIC"      ;  // both
		default	                             :  return "-UNKNOWN-"            ;
	}
}

//+============================================================================ ========================================
// Get interface sub-class string
//
char*  dec_subClass (const uint8_t class, const uint8_t subclass)
{
	switch (class) {
		case TUSB_CLASS_AUDIO :
			switch(subclass) {
				case TUSB_CLASS_AUDIO_UNDEFINED      :  return "UNDEFINED"        ;
				case TUSB_CLASS_AUDIO_AUDIO_CONTROL  :  return "AUDIO CONTROL"    ;
				case TUSB_CLASS_AUDIO_AUDIOSTREAMING :  return "AUDIOSTREAMING"   ;
				case TUSB_CLASS_AUDIO_MIDISTREAMING  :  return "MIDISTREAMING"    ;
				default                              :  return "UNKNOWN SUBCLASS" ;
			}
			break;
		case TUSB_CLASS_HID	:
			switch(subclass) {
				case TUSB_CLASS_HID_NONE             :  return "NO SUBCLASS"      ;
				case TUSB_CLASS_HID_BOOT_INTERFACE   :  return "BOOT INTERFACE"   ;
				default                              :  return "RESERVED"         ;
			}
		default	:  return "SUBCLASSES [TODO]" ;
	}
}

//+============================================================================ ========================================
// Get descriptor type string
//
char*  dec_tusb_desc_type_t (const uint8_t type)
{
	switch (type) {
		case TUSB_DESC_DEVICE                            :  return "DEVICE"                            ;  // 0x01
		case TUSB_DESC_CONFIGURATION                     :  return "CONFIGURATION"                     ;  // 0x02
		case TUSB_DESC_STRING                            :  return "STRING"                            ;  // 0x03
		case TUSB_DESC_INTERFACE                         :  return "INTERFACE"                         ;  // 0x04
		case TUSB_DESC_ENDPOINT                          :  return "ENDPOINT"                          ;  // 0x05
		case TUSB_DESC_DEVICE_QUALIFIER                  :  return "DEVICE QUALIFIER"                  ;  // 0x06
		case TUSB_DESC_OTHER_SPEED_CONFIG                :  return "OTHER SPEED CONFIG"                ;  // 0x07
		case TUSB_DESC_INTERFACE_POWER                   :  return "INTERFACE POWER"                   ;  // 0x08
		case TUSB_DESC_OTG                               :  return "OTG"                               ;  // 0x09
		case TUSB_DESC_DEBUG                             :  return "DEBUG"                             ;  // 0x0A
		case TUSB_DESC_INTERFACE_ASSOCIATION             :  return "INTERFACE ASSOCIATION"             ;  // 0x0B
		case TUSB_DESC_BOS                               :  return "BOS"                               ;  // 0x0F
		case TUSB_DESC_DEVICE_CAPABILITY                 :  return "DEVICE CAPABILITY"                 ;  // 0x10
//      case TUSB_DESC_FUNCTIONAL                        :  return "FUNCTIONAL"                        ;  // 0x21
		case TUSB_DESC_CS_DEVICE                         :  return "CLASS SPECIFIC DEVICE"             ;  // 0x21
		case TUSB_DESC_CS_CONFIGURATION                  :  return "CLASS SPECIFIC CONFIGURATION"      ;  // 0x22
		case TUSB_DESC_CS_STRING                         :  return "CLASS SPECIFIC STRING"             ;  // 0x23
		case TUSB_DESC_CS_INTERFACE                      :  return "CLASS SPECIFIC INTERFACE"          ;  // 0x24
		case TUSB_DESC_CS_ENDPOINT                       :  return "CLASS SPECIFIC ENDPOINT"           ;  // 0x25
		case TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION     :  return "SUPERSPEED ENDPOINT COMPANION"     ;  // 0x30
		case TUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION :  return "SUPERSPEED ISO ENDPOINT COMPANION" ;  // 0x31
		default                                          :  return "UNKNOWN DESCRIPTOR TYPE"           ;
	}
}

//+============================================================================ ========================================
//
//
bool  dec_TUSB_DESC_STRING (const void* const vp)
{
	const tusb_desc_string_t* const  p = vp;

	char  s[TUSB_STRING_MAX];

	declogf("TUSB_DESC_STRING" EOL);
	declogf(IND"  bLength                    = %d"             EOL, p->bLength        );
	declogf(IND"  bDescriptorType            = %0x02X"         EOL, p->bDescriptorType);
	declogf(IND"  unicode_string[]           = \"%s\" (ascii)" EOL,
	            dec_unicode2asc(p->unicode_string, p->bLength - 2, s, TUSB_STRING_MAX));
	return true;
}

//+============================================================================ ========================================
//
//
bool  dec_IF_TUSB_CLASS_AUDIO_AUDIO_CONTROL (const void* const vp)
{
	bool  rv = false;  // Assume decode fail

	switch (((const uint8_t* const)vp)[2]) {  // Descriptor subtype
		case AUDIO_CS_AC_INTERFACE_HEADER           : {  // 0x01,
			uint8_t         i;
			const uint8_t*  bp = NULL;

			const audio_desc_cs_ac_interface_t* const  p = vp;
			const char* const                          s = "AUDIO_DESC_CS_AC_INTERFACE";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bcdADC             = %04X (bcd)"  EOL, p->bcdADC               );
			// Total number of bytes returned for the class-specific AudioControl interface descriptor.
			// Includes the combined length of this descriptor header and all Clock Source, Unit and Terminal descriptors.
			declogf(IND"  wTotalLength       = %d"          EOL, p->wTotalLength         );
			declogf(IND"  bInCollection      = %d"          EOL, p->bInCollection        );
			declogf(IND"  baInterfaceNr      = ");
			for (i = 0,  bp = &(p->baInterfaceNr);  i < p->bLength - 8;  i++, bp++)
				printf("%s%d%s", !i?"{":"", *bp, i==p->bLength-8-1?"}"EOL:",");
			return true;
		}
		case AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL   : {  // 0x02,
			const audio_desc_input_terminal_t* const  p = vp;
			const char* const                         s = "AUDIO_DESC_INPUT_TERMINAL";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  wTerminalType      = %d - $s"     EOL, p->wTerminalType);//,
//! iff USB Streaming
//!			                           dec_audio_terminal_type_t(p->wTerminalType)       );
//! else if !(USB streaming)
//!			                     dec_audio_terminal_input_type_t(p->wTerminalType)       );
			declogf(IND"  bAssocTerminal     = %d"          EOL, p->bAssocTerminal       );  // Source terminal
			declogf(IND"  bCSourceID         = %d"          EOL, p->bCSourceID           );  // Clock source
			declogf(IND"  bNrChannels        = %d"          EOL, p->bNrChannels          );  // OUTPUT channels in this cluster
			declogf(IND"  bmChannelConfig    = %d - %s"     EOL, p->bmChannelConfig,
			                         dec_audio_channel_config_t(p->bmChannelConfig)      );
			declogf(IND"  bmControls         = %d - %s"     EOL, p->bmControls,
			             dec_audio_terminal_input_control_pos_t(p->bmControls)           );
			declogf(IND"  iTerminal          = %d \"%s\""   EOL, p->iTerminal,
			                                   dec_getStringDesc(p->iTerminal)           );
			return true;
		}
		case AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL  : {  // 0x03,
			const audio_desc_output_terminal_t* const  p = vp;
			const char* const                          s = "AUDIO_DESC_OUTPUT_TERMINAL";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bTerminalID        = %d"          EOL, p->bTerminalID          );
			declogf(IND"  wTerminalType      = %d - $s%"    EOL, p->wTerminalType);//,
//! iff USB Streaming
//!			                           dec_audio_terminal_type_t(p->wTerminalType)       );
//! else if !(USB streaming)
//!			                    dec_audio_terminal_output_type_t(p->wTerminalType)       );
			declogf(IND"  bAssocTerminal     = %d"          EOL, p->bAssocTerminal       );  // Output Destination
			declogf(IND"  bSourceID          = %d"          EOL, p->bSourceID            );
			declogf(IND"  bCSourceID         = %d"          EOL, p->bCSourceID           );  // Clock Source
			declogf(IND"  bmControls         = %d - %s"     EOL, p->bmControls,
			                    dec_audio_terminal_output_type_t(p->bmControls)          );
			declogf(IND"  iTerminal          = %d \"%s\""   EOL, p->iTerminal,
						                       dec_getStringDesc(p->iTerminal)           );
			return true;
		}
		case AUDIO_CS_AC_INTERFACE_FEATURE_UNIT     : {  // 0x06,
			const audio_desc_feature_unit_t* const  p = vp;
			const char* const                       s = "AUDIO_DESC_FEATURE_UNIT";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bUnitID            = %d"          EOL, p->bUnitID              );
			declogf(IND"  bSourceID          = %d"          EOL, p->bSourceID            );
			declogf(IND"  bmaControls[1] = %d - $s"         EOL, p->controls[0].bmaControls,    // Chn 0 (Master) - always present
			                dec_audio_feature_unit_control_pos_t(p->controls[0].bmaControls));
			declogf(IND"  bmaControls[2] = %d - $s"         EOL, p->controls[1].bmaControls,    // Chn 1 (Logical)
			                dec_audio_feature_unit_control_pos_t(p->controls[1].bmaControls));
			declogf(IND"  iTerminal          = %d \"%s\""   EOL, p->iTerminal,
			                                   dec_getStringDesc(p->iTerminal)           );
			return true;
		}
		case AUDIO_CS_AC_INTERFACE_CLOCK_SOURCE     : {  // 0x0A,
			const audio_desc_clock_source_t* const  p = vp;
			const char* const                       s = "AUDIO_DESC_CLOCK_SOURCE";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bClockID           = %"           EOL, p->bClockID             );  // Clock source
			declogf(IND"  bmAttributes       = %d - %s"     EOL, p->bmAttributes,
			                  dec_audio_clock_source_attribute_t(p->bmAttributes)        );
			declogf(IND"  bmControls         = %d - %s"     EOL, p->bmControls,
			                      dec_audio_clock_source_control_pos_t(p->bmControls)    );
			declogf(IND"  bAssocTerminal     = %d"          EOL, p->bAssocTerminal       );  // Terminal ID
			declogf(IND"  iClockSource       = %d \"%s\""   EOL, p->iClockSource,
			                                   dec_getStringDesc(p->iClockSource)        );
			return true;
		}
		case AUDIO_CS_AC_INTERFACE_CLOCK_SELECTOR   : {  // 0x0B,
			const audio_desc_clock_selector_t* const  p = vp;
			const char* const                         s = "AUDIO_DESC_CLOCK_SELECTOR";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bClockID           = %d"          EOL, p->bClockID             );

			declogf(IND"  bNrInPins          = %d"          EOL, p->bNrInPins            );
			declogf(IND"  baCSourceID        = %d"          EOL, p->baCSourceID          );
//!TODO   what if multiple pins ????? ... See MIDI_CS_INTERFACE_OUT_JACK

			declogf(IND"  bmControls         = %d - %s"     EOL, p->bmControls,
			              dec_audio_clock_selector_control_pos_t(p->bmControls)          );
			declogf(IND"  iClockSource       = %d \"%s\""   EOL, p->iClockSource,
			                                   dec_getStringDesc(p->iClockSource)        );
			return true;
		}
		case AUDIO_CS_AC_INTERFACE_CLOCK_MULTIPLIER : {  // 0x0C,
			const audio_desc_clock_multiplier_t* const  p = vp;
			const char* const                           s = "AUDIO_DESC_CLOCK_MULTIPLIER";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bClockID           = %d"          EOL, p->bClockID             );
			declogf(IND"  bCSourceID         = %"           EOL, p->bCSourceID           );  // Clock-In #max
			declogf(IND"  bmControls         = %d - %s"     EOL, p->bmControls,
			            dec_audio_clock_multiplier_control_pos_t(p->bmControls)          );
			declogf(IND"  iClockSource       = %d \"%s\""   EOL, p->iClockSource,
			                                   dec_getStringDesc(p->iClockSource)        );
			return true;
		}
		default:
			declogf("--UNKNOWN--" EOL);
			return false;
	}//switch(descriptor subtype)
}

//+============================================================================ ========================================
//
//
bool  dec_IF_TUSB_CLASS_AUDIO_AUDIOSTREAMING (const void* const vp)
{
	bool  rv = false;  // Assume decode fail

	switch (((const uint8_t* const)vp)[2]) {
		case AUDIO_CS_AS_INTERFACE_AS_GENERAL       : {  // 0x01,
			const audio_desc_cs_as_interface_t* const  p = vp;
			const char* const                          s = "AUDIO_DESC_CS_AS_INTERFACE";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bTerminalLink      = %d"          EOL, p->bTerminalLink        );
			declogf(IND"  bmControls         = %d - %s"     EOL, p->bmControls,
			             dec_audio_cs_as_interface_control_pos_t(p->bmControls)          );
			declogf(IND"  bFormatType        = %d - %s"     EOL, p->bFormatType,
			                             dec_audio_format_type_t(p->bFormatType)         );
			declogf(IND"  bmFormats          = %d - %s"     EOL, p->bmFormats,
			                      dec_audio_data_format_type_I_t(p->bmFormats)           );
			declogf(IND"  bNrChannels        = %d"          EOL, p->bNrChannels          );
			declogf(IND"  bmChannelConfig    = %d - %s"     EOL, p->bmChannelConfig,
			                          dec_audio_channel_config_t(p->bmChannelConfig)     );
			declogf(IND"  iChannelNames      = %d \"%s\""   EOL, p->iChannelNames,
			                                   dec_getStringDesc(p->iChannelNames)       );
			return true;
		}
		case AUDIO_CS_AS_INTERFACE_FORMAT_TYPE      : {  // 0x02
			const audio_desc_type_I_format_t* const    p = vp;
			const char* const                          s = "AUDIO_DESC_TYPE_I_FORMAT";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bFormatType        = %d"          EOL, p->bFormatType          );
			declogf(IND"  bSubslotSize       = %d"          EOL, p->bSubslotSize         );
			declogf(IND"  bBitResolution     = %d"          EOL, p->bBitResolution       );
			return true;
		}
		default:
			declogf("--UNKNOWN--" EOL);
			return false;
	}//switch(descriptor subtype)
}

//+============================================================================ ========================================
// MS CS Interface
//
bool  dec_IF_TUSB_CLASS_AUDIO_MIDISTREAMING (const void* const vp)
{
//	if ((((const uint8_t* const)vp)[1]) != TUSB_DESC_CS_INTERFACE) {  // ? bDescriptorType == USB_DESC_CS_INTERFACE
//		declogf("ERR - not a MS CS Interface desc type" EOL);
//		return false;
//	}
//
	switch (((const uint8_t* const)vp)[2]) {  // bDescriptorSubType
		case MIDI_CS_INTERFACE_HEADER :   {  // 0x01
			const midi_desc_header_t* const    p = vp;
			const char* const                  s = "MIDI_HEADER";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bcdMSC             = %04X (bcd)"  EOL, p->bcdMSC               );  // in the spec: bcdADC
			declogf(IND"  wTotalLength       = %d"          EOL, p->wTotalLength         );
			return true;
		}
		case MIDI_CS_INTERFACE_IN_JACK :  {  // 0x02
			const midi_desc_in_jack_t* const   p = vp;
			const char* const                  s = "MIDI_IN_JACK";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bJackType          = 0x%02X (%s)" EOL, p->bJackType, dec_midi_jack_type_t(p->bJackType));
			declogf(IND"  bJackID            = %d"          EOL, p->bJackID              );
			declogf(IND"  iJack              = %d \"%s\""   EOL, p->iJack,
			                                   dec_getStringDesc(p->iJack)               );
			return true;
		}
		case MIDI_CS_INTERFACE_OUT_JACK : {  // 0x03  //  6.1.2.3 MIDI OUT Jack Descriptor
			uint8_t         i;
			const uint8_t*  bp1 = NULL;
			const uint8_t*  bp2 = NULL;

			const midi_desc_out_jack_t* const  p  = vp;
			const char* const                  s  = "MIDI_OUT_JACK";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bJackType          = 0x%02X (%s)" EOL, p->bJackType, dec_midi_jack_type_t(p->bJackType));
			declogf(IND"  bJackID            = %d"          EOL, p->bJackID              );
			declogf(IND"  bNrInputPins       = %d"          EOL, p->bNrInputPins         );

			declogf(IND"  baSourceID         = ");
			for (i = 0,  bp1 = &(p->baSourceID);  i < p->bNrInputPins;  i++, bp1 += 2)
				printf("%s%d%s", !i?"{":"", *bp1, i==p->bNrInputPins-1?"}"EOL:",");

			declogf(IND"  baSourcePin        = ");
			for (i = 0,  bp2 = &(p->baSourcePin);  i < p->bNrInputPins;  i++, bp2 += 2)
				printf("%s%d%s", !i?"{":"", *bp2, i==p->bNrInputPins-1?"}"EOL:",");

//			declogf(IND"  iJack              = %d \"$s\""   EOL, p->iJack                );
			declogf(IND"  iJack              = %d \"%s\""   EOL, *bp1,
			                                   dec_getStringDesc(*bp1)                   );
			return true;
		}
		case MIDI_CS_INTERFACE_ELEMENT :  {  // 0x04  //  6.1.2.4 Element Descriptor
			const midi_desc_element_t* const   p = vp;
			const char* const                  s = "MIDI_ELEMENT";
			declogf(IND"  bLength            = %d"          EOL, p->bLength              );
			declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
			declogf(IND"  bDescriptorSubType = 0x%02X (%s)" EOL, p->bDescriptorSubType, s);
			declogf(IND"  bElementID         = %d"          EOL, p->bElementID           );

			declogf(IND"  bNrInputPins       = %d"          EOL, p->bNrInputPins         );
			declogf(IND"  baSourceID         = %d"          EOL, p->baSourceID           );
			declogf(IND"  baSourcePin        = %d"          EOL, p->baSourcePin          );
//!TODO   what if multiple pins ????? ... See MIDI_CS_INTERFACE_OUT_JACK

			declogf(IND"  bNROutputPins      = %d"          EOL, p->bNrOutputPins        );
			declogf(IND"  bInTerminalLink    = %d"          EOL, p->bInTerminalLink      );
			declogf(IND"  bOutTerminalLink   = %d"          EOL, p->bOutTerminalLink     );
			declogf(IND"  bElCapsSize        = %d"          EOL, p->bElCapsSize          );
			declogf(IND"  bmElementCaps      = 0x%02X"      EOL, p->bmElementCaps        );
			declogf(IND"  iElement           = %d \"%s\""   EOL, p->iElement,
			                                   dec_getStringDesc(p->iElement)            );
			return true;
		}
		default:
			declogf("--UNKNOWN--" EOL);
			return false;
	}
}

//+============================================================================ ========================================
// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
// B.6.2 Class-specific MS Bulk IN  Endpoint Descriptor
//
bool  dec_EP_TUSB_CLASS_AUDIO_MIDISTREAMING (const void* const vp)
{
	uint8_t         i;
	const uint8_t*  bp = NULL;

	const audio_cs_ms_bulk_ep_t*const  p = vp;
	const char* const                  s = "MIDISTREAMING 'BULK' ENDPOINT";
	declogf(IND"  bLength            = %d"          EOL, p->bLength              );
	declogf(IND"  bDescriptorType    = 0x%02X"      EOL, p->bDescriptorType      );
	declogf(IND"  bDescriptorSubtype = 0x%02X (%s)" EOL, p->bDescriptorSubtype, s);
	declogf(IND"  bNrEmbMIDIJack     = %d"          EOL, p->bNrEmbMIDIJack       );

	declogf(IND"  baAssocJackID      = ");
	for (i = 0,  bp = &(p->baAssocJackID);  i < p->bNrEmbMIDIJack;  i++, bp++)
		printf("%s%d%s", !i?"{":"", *bp, i==p->bNrEmbMIDIJack-1?"}"EOL:",");

	return true;
}

//+============================================================================ ========================================
//
//
bool  dec_TUSB_DESC_DEVICE (const void* const vp)
{
	const tusb_desc_device_t*const  p = vp;
	declogf(IND"TUSB_DESC_DEVICE" EOL);
	declogf(IND"  bLength            = %d"            EOL, p->bLength            );
	declogf(IND"  bDescriptorType    = 0x%02X (%s)"   EOL, p->bDescriptorType,
	                                  dec_tusb_desc_type_t(p->bDescriptorType)   );
	declogf(IND"  bcdUSB             = %04x (bcd)"    EOL, p->bcdUSB             );  // USB version
	declogf(IND"  bDeviceClass       = %d (%s)"       EOL, p->bDeviceClass,
	                                 dec_tusb_class_code_t(p->bDeviceClass)      );
	declogf(IND"  bDeviceSubClass    = 0x%02X (%s)"   EOL, p->bDeviceSubClass,
	                         dec_subClass(p->bDeviceClass, p->bDeviceSubClass)   );
	declogf(IND"  bDeviceProtocol    = %d"            EOL, p->bDeviceProtocol    );
	declogf(IND"  bMaxPacketSize0    = %d"            EOL, p->bMaxPacketSize0    );  // For ep0 only
	declogf(IND"  idVendor           = %d"            EOL, p->idVendor           );
	declogf(IND"  idProduct          = %d"            EOL, p->idProduct          );
	declogf(IND"  bcdDevice          = %04X (bcd)"    EOL, p->bcdDevice          );  // Release number
	declogf(IND"  iManufacturer      = %d \"%s\""     EOL, p->iManufacturer,
			                             dec_getStringDesc(p->iManufacturer)     );
	declogf(IND"  iProduct           = %d \"%s\""     EOL, p->iProduct,
			                             dec_getStringDesc(p->iProduct)          );
	declogf(IND"  iSerialNumber      = %d \"%s\""     EOL, p->iSerialNumber,
			                             dec_getStringDesc(p->iSerialNumber)     );
	declogf(IND"  bNumConfigurations = %d"            EOL, p->bNumConfigurations );
	return true;
}

//+============================================================================ ========================================
//
//
bool  dec_TUSB_DESC_CONFIGURATION (const void* const vp)
{
	const tusb_desc_configuration_t* const  p = vp;
	declogf(IND"TUSB_DESC_CONFIGURATION" EOL);
	declogf(IND"  bLength            = %d"            EOL, p->bLength            );
	declogf(IND"  bDescriptorType    = 0x%02X (%s)"   EOL, p->bDescriptorType,
	                                  dec_tusb_desc_type_t(p->bDescriptorType)   );
	declogf(IND"  wTotalLength       = %d"            EOL, p->wTotalLength       );  // Includes all sub-descriptors
	declogf(IND"  bNumInterfaces     = %d"            EOL, p->bNumInterfaces     );
	declogf(IND"  bConfigurationValue= %d"            EOL, p->bConfigurationValue);  // Value to select this config
	declogf(IND"  iConfiguration     = %d \"%s\""     EOL, p->iConfiguration,
			                             dec_getStringDesc(p->iConfiguration)    );
	declogf(IND"  bmAttributes       = %d"            EOL, p->bmAttributes       );  // Power options
	declogf(IND"    Rsvd:MustBe1     +--[7  ] = %d"   EOL,(p->bmAttributes & 0x80) >> 7);
	declogf(IND"    Self Powered     +--[6  ] = %d"   EOL,(p->bmAttributes & 0x40) >> 6);  // 0=BusPwrOnly, 1=CanSelfPwr
	declogf(IND"    Remote Wakeup    +--[5  ] = %d"   EOL,(p->bmAttributes & 0x20) >> 5);
	declogf(IND"    RESERVED         `--[4:0] = %d"   EOL,(p->bmAttributes & 0x1F)     );  // should be 00000
	declogf(IND"  bMaxPower          = %d (%dmA)"     EOL, p->bMaxPower,
	                                                       p->bMaxPower * 2      );  // Max current when bus-powered
	return true;
}

//+============================================================================ ========================================
//
//
bool  dec_TUSB_DESC_INTERFACE (const void* const vp)
{
	const tusb_desc_interface_t* const  p = vp;
	declogf("TUSB_DESC_INTERFACE" EOL);
	declogf(IND"  bLength            = %d"            EOL, p->bLength            );
	declogf(IND"  bDescriptorType    = 0x%02X (%s)"   EOL, p->bDescriptorType,
	                                  dec_tusb_desc_type_t(p->bDescriptorType)   );
	declogf(IND"  bInterfaceNumber   = %d"            EOL, p->bInterfaceNumber   );
	declogf(IND"  bAlternateSetting  = %d"            EOL, p->bAlternateSetting  );  // Value to select this setting
	declogf(IND"  bNumEndpoints      = %d"            EOL, p->bNumEndpoints      );  // Endpoint count (excl ep0)
	declogf(IND"  bInterfaceClass    = 0x%02X (%s)"   EOL, p->bInterfaceClass,
	                                 dec_tusb_class_code_t(p->bInterfaceClass)   );
	declogf(IND"  bInterfaceSubClass = 0x%02X (%s)"   EOL, p->bInterfaceSubClass,
	                      dec_subClass(p->bInterfaceClass, p->bInterfaceSubClass));
	declogf(IND"  bInterfaceProtocol = %d"            EOL, p->bInterfaceProtocol );
	declogf(IND"  iInterface         = %d \"%s\""     EOL, p->iInterface,
			                             dec_getStringDesc(p->iInterface)        );
	return true;
}

//+============================================================================ ========================================
//
//
bool  dec_TUSB_DESC_ENDPOINT (const void* const vp)
{
	const tusb_desc_endpoint_t* const  p = vp;
	typedef  struct { uint16_t :13;  uint16_t r:3; } top3bits;  // hack to pull out bits rsvd with __COUNTER__
	declogf("TUSB_DESC_ENDPOINT" EOL);
	declogf(IND"  bLength            = %d"               EOL, p->bLength                         );
	declogf(IND"  bDescriptorType    = %d"               EOL, p->bDescriptorType                 );
	declogf(IND"  bEndpointAddress   = 0x%02X"           EOL, p->bEndpointAddress                );
	declogf(IND"    Direction        +--[7  ] = %d (%s)" EOL, tu_edpt_dir(p->bEndpointAddress),
	                                                          tu_edpt_dir(p->bEndpointAddress) ? "in" : "out" );
	declogf(IND"    Reserved         +--[6:4] = %d"      EOL, (p->bEndpointAddress & 0x70) >> 4  );
	declogf(IND"    Endpoint number  `--[3:0] = %d"      EOL, tu_edpt_number(p->bEndpointAddress));
	declogf(IND"  bmAttributes       = 0x%02X"           EOL, p->bmAttributes                    );
	declogf(IND"    RESERVED         +--[7:6] = %d"      EOL, *(uint8_t*)&(p->bmAttributes) >> 6 );
	declogf(IND"    usage type       +--[5:4] = %d"      EOL, p->bmAttributes.usage              );
	declogf(IND"    sync type        +--[3:2] = %d"      EOL, p->bmAttributes.sync               );
	declogf(IND"    xfer type        `--[1:0] = %d"      EOL, p->bmAttributes.xfer               );
	declogf(IND"  wMaxPacketSize     = 0x%04x"           EOL, p->wMaxPacketSize                  );
	declogf(IND"    RESERVED         +--[15:13] = %d"    EOL, ((top3bits*)&p->wMaxPacketSize)->r );
	declogf(IND"    hs_period_mult   +--[12:11] = %d"    EOL, p->wMaxPacketSize.hs_period_mult   );
	declogf(IND"    size             `--[ 0:10] = %d"    EOL, p->wMaxPacketSize.size             );
	declogf(IND"  bInterval          = %d"               EOL, p->bInterval                       );
	declogf(IND"  bRefresh           = %d (unused)"      EOL, p->bRefresh                        );
	declogf(IND"  bSynchAddress      = %d (unused)"      EOL, p->bSynchAddress                   );
	return true;
}

//+============================================================================ ========================================
//
//
bool  dec_TUSB_DESC_FUNCTIONAL (const void* const vp)
{
	const tusb_desc_dfu_functional_t* const  p = vp;
	declogf("TUSB_DESC_FUNCTIONAL" EOL);
	declogf(IND"  bLength                    = %d"          EOL, p->bLength                              );
	declogf(IND"  bDescriptorType            = %0x02X"      EOL, p->bDescriptorType                      );
	declogf(IND"  bmAttributes               = %0x02X"      EOL, p->bmAttributes                         );
	declogf(IND"    bitCanDnload             +--[7  ] = %d" EOL, p->bmAttributes.bitCanDnload            );
	declogf(IND"    bitCanUpload             +--[6  ] = %d" EOL, p->bmAttributes.bitCanUpload            );
	declogf(IND"    bitManifestationTolerant +--[5  ] = %d" EOL, p->bmAttributes.bitManifestationTolerant);
	declogf(IND"    bitWillDetach            +--[4  ] = %d" EOL, p->bmAttributes.bitWillDetach           );
	declogf(IND"    RESERVED                 `--[3:0] = %d" EOL, p->bmAttributes.reserved & 0x0F         );
	declogf(IND"  wDetachTimeOut             = %d"          EOL, p->wDetachTimeOut                       );
	declogf(IND"  wTransferSize              = %d"          EOL, p->wTransferSize                        );
	declogf(IND"  bcdDFUVersion              = %04X (bcd)"  EOL, p->bcdDFUVersion                        );
	return true;
}

//+============================================================================ ========================================
// Decode a  descriptor
//
bool  dec_descriptor (const void* const vp)
{

	static uint16_t  ifClass    = 0x00;  // Interface Class    from [04] TUSB_DESC_INTERFACE
	static uint16_t  ifSubClass = 0x00;  // Interface SubClass from [04] TUSB_DESC_INTERFACE

	if (!declogf)  return true ;  // Disabled - always successful

	declogf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" EOL);
//	declogf(IND"Descriptor @ 0x%p" EOL, vp);
	dec_hexdump("<Descriptor", ((const uint8_t* const)vp)[0], vp);
	declogf(IND"[%02X] - ", tu_desc_type(vp));
	switch (((const uint8_t* const)vp)[1]) {  // bDescriptorType
		case TUSB_DESC_DEVICE                            :  // 0x01
			return dec_TUSB_DESC_DEVICE(vp);

		case TUSB_DESC_CONFIGURATION                     :  // 0x02
			return dec_TUSB_DESC_CONFIGURATION(vp);

		case TUSB_DESC_STRING                            :  // 0x03
			return dec_TUSB_DESC_STRING(vp);
//			return declogf("TUSB_DESC_STRING" EOL), false ;

		case TUSB_DESC_INTERFACE                         :  // 0x04
			ifClass    = ((tusb_desc_interface_t*)vp)->bInterfaceClass;     // We will need these for
			ifSubClass = ((tusb_desc_interface_t*)vp)->bInterfaceSubClass;  // ...TUSB_DESC_CS_INTERFACE
			return dec_TUSB_DESC_INTERFACE(vp);

		case TUSB_DESC_ENDPOINT                          :  // 0x05
			return dec_TUSB_DESC_ENDPOINT(vp);

		case TUSB_DESC_DEVICE_QUALIFIER                  :  // 0x06
			return declogf("TUSB_DESC_DEVICE_QUALIFIER" EOL), false ;

		case TUSB_DESC_OTHER_SPEED_CONFIG                :  // 0x07
			return declogf("TUSB_DESC_OTHER_SPEED_CONFIG" EOL), false ;

		case TUSB_DESC_INTERFACE_POWER                   :  // 0x08
			return declogf("TUSB_DESC_INTERFACE_POWER" EOL), false ;

		case TUSB_DESC_OTG                               :  // 0x09
			return declogf("TUSB_DESC_OTG" EOL), false ;

		case TUSB_DESC_DEBUG                             :  // 0x0A
			return declogf("TUSB_DESC_DEBUG" EOL), false ;

		case TUSB_DESC_INTERFACE_ASSOCIATION             :  // 0x0B
			return declogf("TUSB_DESC_INTERFACE_ASSOCIATION" EOL), false ;

		case TUSB_DESC_BOS                               :  // 0x0F
			return declogf("TUSB_DESC_BOS" EOL), false ;

		case TUSB_DESC_DEVICE_CAPABILITY                 :  // 0x10
			return declogf("TUSB_DESC_DEVICE_CAPABILITY" EOL), false ;

		case TUSB_DESC_FUNCTIONAL                        :  // 0x21
//		case TUSB_DESC_CS_DEVICE                         :  // 0x21
			return dec_TUSB_DESC_FUNCTIONAL(vp);

		case TUSB_DESC_CS_CONFIGURATION                  :  // 0x22
			return declogf("TUSB_DESC_CS_CONFIGURATION" EOL), false ;

		case TUSB_DESC_CS_STRING                         :  // 0x23
			return declogf("TUSB_DESC_CS_STRING" EOL), false ;

		case TUSB_DESC_CS_INTERFACE                      :  // 0x24
			declogf( "TUSB_DESC_CS_INTERFACE : %s/%s" EOL,
			         dec_tusb_class_code_t(ifClass), dec_subClass(ifClass, ifSubClass) );
			switch (ifClass) {
				case TUSB_CLASS_AUDIO:
					switch (ifSubClass) {
						case TUSB_CLASS_AUDIO_AUDIO_CONTROL  :  // 1
							return dec_IF_TUSB_CLASS_AUDIO_AUDIO_CONTROL(vp);

						case TUSB_CLASS_AUDIO_AUDIOSTREAMING :  // 2
							return dec_IF_TUSB_CLASS_AUDIO_AUDIOSTREAMING(vp);

						case TUSB_CLASS_AUDIO_MIDISTREAMING  :  // 3
							return dec_IF_TUSB_CLASS_AUDIO_MIDISTREAMING(vp);

						default:
							return declogf("-UNKNOWN-AUDIO-SUBCLASS-" EOL), false ;
					}//switch(ifSubClass)

				default:
					return declogf("-UNKNOWN-CLASS-" EOL), false ;
			}//switch(ifClass)

		case TUSB_DESC_CS_ENDPOINT                       :  // 0x25
			declogf( "TUSB_DESC_CS_ENDPOINT : %s/%s" EOL,
			         dec_tusb_class_code_t(ifClass), dec_subClass(ifClass, ifSubClass) );
			switch (ifClass) {
				case TUSB_CLASS_AUDIO:
					switch (ifSubClass) {
						case TUSB_CLASS_AUDIO_MIDISTREAMING  :  // 3
							return dec_EP_TUSB_CLASS_AUDIO_MIDISTREAMING(vp);

						default:
							return declogf("-UNKNOWN-AUDIO-SUBCLASS-" EOL), false ;
					}//switch(ifSubClass)

				default:
					return declogf("-UNKNOWN-CLASS-" EOL), false ;
			}//switch(ifClass)

			return declogf("" EOL), false ;

		case TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION     :  // 0x30
			return declogf("TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION" EOL), false ;

		case TUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION :  // 0x31
			return declogf("TUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION" EOL), false ;

		default:
			return declogf("-UNKNOWN-DESCRIPTOR-" EOL), false ;
	}//switch
}

#if 0

//	09  02  75 00 02 01 00 80 30   Where is this???
//	09  04  00 00 00 01 01 00 00
//	09  24  01 00 01 09 00 01 01
//	09  04  01 00 02 01 03 00 00
//	07  24  01 00 01 51 00
//	06  24  02 01 02 00
//	06  24  02 01 03 00
//	06  24  02 02 06 00
//	09  24  03 01 07 01 06 01 00
//	09  24  03 02 04 01 02 01 00
//	09  24  03 02 05 01 03 01 00
//	09  05  02 02 20 00 00 00 00
//	06  25  01 02 02 03
//	09  05  82 02 20 00 00 00 00
//	05  25  01 01 07
//
//
TinyUSB MIDI Data Driver example
# midi_init()
- enum_new_device()
+ hub 0x00, address 0x00, port 0x00
- enum_request_addr0_device_desc()
- usbh_edpt_control_open()
+ Get 8 bytes of Device Descriptor, to get max packet size...
> [0008]--Tx::Setup(pre-send)
>  0000:  00 00 00 00  00 00 00 00
> [0008]--Tx::Setup(post-send)
>  0000:  00 00 00 00  00 00 00 00
> [0008]--Tx::Data
>  0000:  12 01 10 01  00 00 00 08
> [0000]--Tx::Ack
- enum_get_addr0_device_desc_complete()
+ Got 8 bytes of device descriptor
+ Reset Device...
+ Issue set_address request...
- enum_request_set_addr()
+ New address: 0x01
+ Use new MaxPacketSize: 0x08   //!
> [0000]--Tx::Setup(pre-send)
> [0000]--Tx::Setup(post-send)
< [0000]--Rx::Data
> [0000]--Tx::Ack
- enum_set_address_complete()
+ set_address request sent
+ New address: 0x08
+ Close device 0
+ Open ep0 on the new device...
- usbh_edpt_control_open()
+ Tx device descriptor request to new ep0
> [0012]--Tx::Setup(pre-send)
>  0000:  12 01 10 01  00 00 00 08   00 00 00 00  00 00 00 00
>  0010:  00 00
> [0012]--Tx::Setup(post-send)
>  0000:  12 01 10 01  00 00 00 08   00 00 00 00  00 00 00 00
>  0010:  00 00
< [0012]--Rx::Data
<  0000:  12 01 10 01  00 00 00 08   00 00 00 00  00 00 00 00
<  0010:  00 00
> [0012]--Tx::Data
>  0000:  12 01 10 01  00 00 00 08   86 1a 2d 75  54 02 00 02
>  0010:  00 01
> [0000]--Tx::Ack
- enum_get_device_desc_complete()
+ Check device descriptor...
+ Length: 0x12 (18.) - VID/PID = 1A86:752D
+ Setup Complete
> [0009]--Tx::Setup(pre-send)
>  0000:  12 01 10 01  00 00 00 08   86
> [0009]--Tx::Setup(post-send)
>  0000:  12 01 10 01  00 00 00 08   86
< [0009]--Rx::Data
<  0000:  12 01 10 01  00 00 00 08   86
> [0009]--Tx::Data
>  0000:  09 02 75 00  02 01 00 80   30
> [0000]--Tx::Ack
- enum_get_9byte_config_desc_complete()
> [0075]--Tx::Setup(pre-send)
>  0000:  09 02 75 00  02 01 00 80   30 1a 2d 75  54 02 00 02
>  0010:  00 01 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0020:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0030:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0040:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0050:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0060:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0070:  00 00 00 00  00
> [0075]--Tx::Setup(post-send)
>  0000:  09 02 75 00  02 01 00 80   30 1a 2d 75  54 02 00 02
>  0010:  00 01 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0020:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0030:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0040:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0050:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0060:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
>  0070:  00 00 00 00  00
< [0075]--Rx::Data
<  0000:  09 02 75 00  02 01 00 80   30 1a 2d 75  54 02 00 02
<  0010:  00 01 00 00  00 00 00 00   00 00 00 00  00 00 00 00
<  0020:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
<  0030:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
<  0040:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
<  0050:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
<  0060:  00 00 00 00  00 00 00 00   00 00 00 00  00 00 00 00
<  0070:  00 00 00 00  00
> [0075]--Tx::Data
>  0000:  09 02 75 00  02 01 00 80   30 09 04 00  00 00 01 01
>  0010:  00 00 09 24  01 00 01 09   00 01 01 09  04 01 00 02
>  0020:  01 03 00 00  07 24 01 00   01 51 00 06  24 02 01 02
>  0030:  00 06 24 02  01 03 00 06   24 02 02 06  00 09 24 03
>  0040:  01 07 01 06  01 00 09 24   03 02 04 01  02 01 00 09
>  0050:  24 03 02 05  01 03 01 00   09 05 02 02  20 00 00 00
>  0060:  00 06 25 01  02 02 03 09   05 82 02 20  00 00 00 00
>  0070:  05 25 01 01  07
> [0000]--Tx::Ack
- enum_get_config_desc_complete()
- parse_configuration_descriptor()
# midi_open(port=0, dev=1, *desc=200005D1, max=108)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 04 00 00  00 01 01 00   00
? [04] - TUSB_DESC_INTERFACE
?   bLength            = 9
?   bDescriptorType    = 0x04 (INTERFACE)
?   bInterfaceNumber   = 0
?   bAlternateSetting  = 0
?   bNumEndpoints      = 0
?   bInterfaceClass    = 0x01 (AUDIO)
?   bInterfaceSubClass = 0x01 (AUDIO CONTROL)
?   bInterfaceProtocol = 0
?   iInterface         = 0 ""
# Found AUDIO interface @0:1...
# Subclass : AUDIOCONTROL -> not a MIDI interface. Rejecting...

vvv=================================================================vvv
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 04 00 00  00 01 01 00   00
? [04] - TUSB_DESC_INTERFACE
?   bLength            = 9
?   bDescriptorType    = 0x04 (INTERFACE)
?   bInterfaceNumber   = 0
?   bAlternateSetting  = 0
?   bNumEndpoints      = 0
?   bInterfaceClass    = 0x01 (AUDIO)
?   bInterfaceSubClass = 0x01 (AUDIO CONTROL)
?   bInterfaceProtocol = 0
?   iInterface         = 0 ""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 24 01 00  01 09 00 01   01
? [24] - TUSB_DESC_CS_INTERFACE : AUDIO/AUDIO CONTROL
?   bLength            = 9
?   bDescriptorType    = 0x24
?   bDescriptorSubType = 0x01 (AUDIO_DESC_CS_AC_INTERFACE)
?   bcdADC             = 0100 (bcd)
?   wTotalLength       = 9
?   bInCollection      = 1
?   baInterfaceNr      = {1}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 04 01 00  02 01 03 00   00
? [04] - TUSB_DESC_INTERFACE
?   bLength            = 9
?   bDescriptorType    = 0x04 (INTERFACE)
?   bInterfaceNumber   = 1
?   bAlternateSetting  = 0
?   bNumEndpoints      = 2
?   bInterfaceClass    = 0x01 (AUDIO)
?   bInterfaceSubClass = 0x03 (MIDISTREAMING)
?   bInterfaceProtocol = 0
?   iInterface         = 0 ""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0007]--Rx::Descriptor
<  0000:  07 24 01 00  01 51 00
? [24] - TUSB_DESC_CS_INTERFACE : AUDIO/MIDISTREAMING
?   bLength            = 7
?   bDescriptorType    = 0x24
?   bDescriptorSubType = 0x01 (MIDI_HEADER)
?   bcdMSC             = 0100 (bcd)
?   wTotalLength       = 81
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0006]--Rx::Descriptor
<  0000:  06 24 02 01  02 00
? [24] - TUSB_DESC_CS_INTERFACE : AUDIO/MIDISTREAMING
?   bLength            = 6
?   bDescriptorType    = 0x24
?   bDescriptorSubType = 0x02 (MIDI_IN_JACK)
?   bJackType          = 0x01 (EMBEDDED)
?   bJackID            = 2
?   iJack              = 0 ""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0006]--Rx::Descriptor
<  0000:  06 24 02 01  03 00
? [24] - TUSB_DESC_CS_INTERFACE : AUDIO/MIDISTREAMING
?   bLength            = 6
?   bDescriptorType    = 0x24
?   bDescriptorSubType = 0x02 (MIDI_IN_JACK)
?   bJackType          = 0x01 (EMBEDDED)
?   bJackID            = 3
?   iJack              = 0 ""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0006]--Rx::Descriptor
<  0000:  06 24 02 02  06 00
? [24] - TUSB_DESC_CS_INTERFACE : AUDIO/MIDISTREAMING
?   bLength            = 6
?   bDescriptorType    = 0x24
?   bDescriptorSubType = 0x02 (MIDI_IN_JACK)
?   bJackType          = 0x02 (EXTERNAL)
?   bJackID            = 6
?   iJack              = 0 ""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 24 03 01  07 01 06 01   00
? [24] - TUSB_DESC_CS_INTERFACE : AUDIO/MIDISTREAMING
?   bLength            = 9
?   bDescriptorType    = 0x24
?   bDescriptorSubType = 0x03 (MIDI_OUT_JACK)
?   bJackType          = 0x01 (EMBEDDED)
?   bJackID            = 7
?   bNrInputPins       = 1
?   baSourceID         = {6}
?   baSourcePin        = {1}
?   iJack              = 0 ""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 24 03 02  04 01 02 01   00
? [24] - TUSB_DESC_CS_INTERFACE : AUDIO/MIDISTREAMING
?   bLength            = 9
?   bDescriptorType    = 0x24
?   bDescriptorSubType = 0x03 (MIDI_OUT_JACK)
?   bJackType          = 0x02 (EXTERNAL)
?   bJackID            = 4
?   bNrInputPins       = 1
?   baSourceID         = {2}
?   baSourcePin        = {1}
?   iJack              = 0 ""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 24 03 02  05 01 03 01   00
? [24] - TUSB_DESC_CS_INTERFACE : AUDIO/MIDISTREAMING
?   bLength            = 9
?   bDescriptorType    = 0x24
?   bDescriptorSubType = 0x03 (MIDI_OUT_JACK)
?   bJackType          = 0x02 (EXTERNAL)
?   bJackID            = 5
?   bNrInputPins       = 1
?   baSourceID         = {3}
?   baSourcePin        = {1}
?   iJack              = 0 ""
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 05 02 02  20 00 00 00   00
? [05] - TUSB_DESC_ENDPOINT
?   bLength            = 9
?   bDescriptorType    = 5
?   bEndpointAddress   = 02h
?     Direction        +--[7  ] = 0 (out)
?     Reserved         +--[6:4] = 0
?     Endpoint number  `--[3:0] = 2
?   bmAttributes       = 02h
?     RESERVED         +--[7:6] = 0
?     usage type       +--[5:4] = 0
?     sync type        +--[3:2] = 0
?     xfer type        `--[1:0] = 2
?   wMaxPacketSize     = 0020h
?     RESERVED         +--[15:13] = 0
?     hs_period_mult   +--[12:11] = 0
?     size             `--[ 0:10] = 32
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0006]--Rx::Descriptor
<  0000:  06 25 01 02  02 03
? [25] - TUSB_DESC_CS_ENDPOINT : AUDIO/MIDISTREAMING
?   bLength            = 6
?   bDescriptorType    = 0x25
?   bDescriptorSubtype = 0x01 (MIDISTREAMING 'BULK' ENDPOINT)
?   bNrEmbMIDIJack     = 2
?   baAssocJackID      = {2,3}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0009]--Rx::Descriptor
<  0000:  09 05 82 02  20 00 00 00   00
? [05] - TUSB_DESC_ENDPOINT
?   bLength            = 9
?   bDescriptorType    = 5
?   bEndpointAddress   = 82h
?     Direction        +--[7  ] = 1 (in)
?     Reserved         +--[6:4] = 0
?     Endpoint number  `--[3:0] = 2
?   bmAttributes       = 02h
?     RESERVED         +--[7:6] = 0
?     usage type       +--[5:4] = 0
?     sync type        +--[3:2] = 0
?     xfer type        `--[1:0] = 2
?   wMaxPacketSize     = 0020h
?     RESERVED         +--[15:13] = 0
?     hs_period_mult   +--[12:11] = 0
?     size             `--[ 0:10] = 32
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
< [0005]--Rx::Descriptor
<  0000:  05 25 01 01  07
? [25] - TUSB_DESC_CS_ENDPOINT : AUDIO/MIDISTREAMING
?   bLength            = 5
?   bDescriptorType    = 0x25
?   bDescriptorSubtype = 0x01 (MIDISTREAMING 'BULK' ENDPOINT)
?   bNrEmbMIDIJack     = 1
?   baAssocJackID      = {7}
^^^=====================================================================^^^

> [0000]--Tx::Setup(pre-send)
> [0000]--Tx::Setup(post-send)
< [0000]--Rx::Data
> [0000]--Tx::Ack
- enum_set_config_complete()
# midi_set_config(dev=1, num=0) // VID:PID=1A86:752D
< [0000]--Rx::Data


Interface = Audio/AC #1
Interface = Audio/MS [81]
Interface = MIDI-In/Embedded #2 ---------------.
Interface = MIDI-In/Embedded #3 -------------. |
Interface = MIDI-In/External #6 -----------. | |
Interface = MIDI-Out/Embedded #7 <- #6:1 --^~|~|~~~~~~.
Interface = MIDI-Out/External #4 <- #2:1 ----|-^~~~.  |
Interface = MIDI-Out/External #5 <- #3:1 ----^~~~~~+  |
Endpoint:x02 (out) = #2, #3 ~~~~~~~~~~~~~~~~~~~~~~~^  |
Endpoint:x82 (in) = #7 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^


// https://docs.huihoo.com/doxygen/linux/kernel/3.7/sound_2usb_2midi_8c_source.html
#define  USB_ID(vid,pid)  ( (((uint32_t)(vid)) << 16) | (pid) )
printf("+ PID/VID = %04X:%04X\r\n", desc_device->idVendor, desc_device->idProduct);
  switch (USB_ID(desc_device->idVendor, desc_device->idProduct)) {
        /*
         * Various chips declare a packet size larger than 4 bytes, but
         * do not actually work with larger packets:
         */
    case USB_ID(0x0a92, 0x1020):  /* ESI M4U */
    case USB_ID(0x1430, 0x474b):  /* RedOctane GH MIDI INTERFACE */
    case USB_ID(0x15ca, 0x0101):  /* Textech USB Midi Cable */
    case USB_ID(0x15ca, 0x1806):  /* Textech USB Midi Cable */
    case USB_ID(0x1a86, 0x752d):  /* QinHeng CH345 "USB2.0-MIDI" */
    case USB_ID(0xfc08, 0x0101):  /* Unknown vendor Cable */
printf("* H/W Fix : Change MaxPacketSize from %d -> 4\r\n", desc_device->bMaxPacketSize0);
      new_dev->ep0_size = 4;
      break;
      /*
       * Some devices only work with 9 bytes packet size:
       */
    case USB_ID(0x0644, 0x800E):  /* Tascam US-122L */
    case USB_ID(0x0644, 0x800F):  /* Tascam US-144 */
printf("* H/W Fix : Change MaxPacketSize from %d -> 9\r\n", desc_device->bMaxPacketSize0);
      new_dev->ep0_size = 9;
      break;
    default:
printf("+ Use MaxPacketSize %d\r\n", desc_device->bMaxPacketSize0);
      new_dev->ep0_size = desc_device->bMaxPacketSize0;
      break;
  }
#undef USB_ID
//  new_dev->ep0_size = desc_device->bMaxPacketSize0;

#if CFG_TUH_MIDI
#define  VIDPID(vid,pid)  ( (((uint32_t)(vid)) << 16) | (pid) )
  uint16_t  vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  usbh_device_t const* dev = get_device(dev_addr);
printf("   + VID/PID = %04X:%04X\r\n", vid, pid);

  // Ref: https://docs.huihoo.com/doxygen/linux/kernel/3.7/sound_2usb_2midi_8c_source.html */
  switch (VIDPID(vid, pid)) {
    case VIDPID(0x0A92, 0x1020) :  // ESI M4U 
    case VIDPID(0x1430, 0x474B) :  // RedOctane GH MIDI INTERFACE 
    case VIDPID(0x15CA, 0x0101) :  // Textech USB Midi Cable 
    case VIDPID(0x15CA, 0x1806) :  // Textech USB Midi Cable 
    case VIDPID(0x1A86, 0x752D) :  // QinHeng CH345 "USB2.0-MIDI" 
    case VIDPID(0xFC08, 0x0101) :  // Unknown vendor Cable 
printf("* H/W Fix : Change MaxPacketSize from %d -> 4\r\n", max_packet_size);
      max_packet_size = 4;
      break;
    case VIDPID(0x0644, 0x800E) :  // Tascam US-122L
    case VIDPID(0x0644, 0x800F) :  // Tascam US-144 
printf("* H/W Fix : Change MaxPacketSize from %d -> 9\r\n", max_packet_size);
      max_packet_size = 9;
      break;
    default:
printf("+ Use MaxPacketSize %d\r\n", max_packet_size);
      break;
  }
#undef VIDPID
#endif

bool tuh_cdc_send(uint8_t dev_addr, void const * p_data, uint32_t length, bool is_notify)
{
  (void) is_notify;
  TU_VERIFY( tuh_cdc_mounted(dev_addr) );
  TU_VERIFY( p_data != NULL && length, TUSB_ERROR_INVALID_PARA);

  uint8_t const ep_out = cdch_data[dev_addr-1].ep_out;
  if ( usbh_edpt_busy(dev_addr, ep_out) ) return false;

  return usbh_edpt_xfer(dev_addr, ep_out, (void *) p_data, length);
}

#endif //0
