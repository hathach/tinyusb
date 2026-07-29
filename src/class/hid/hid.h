/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/** \ingroup group_class
 *  \defgroup ClassDriver_HID Human Interface Device (HID)
 *  @{ */

#ifndef TUSB_HID_H_
#define TUSB_HID_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Common Definitions
//--------------------------------------------------------------------+
/** \defgroup ClassDriver_HID_Common Common Definitions
 *  @{ */

/// USB HID Descriptor
typedef struct TU_ATTR_PACKED
{
  uint8_t  bLength;         /**< Numeric expression that is the total size of the HID descriptor */
  uint8_t  bDescriptorType; /**< Constant name specifying type of HID descriptor. */

  uint16_t bcdHID;          /**< Numeric expression identifying the HID Class Specification release */
  uint8_t  bCountryCode;    /**< Numeric expression identifying country code of the localized hardware.  */
  uint8_t  bNumDescriptors; /**< Numeric expression specifying the number of class descriptors */

  uint8_t  bReportType;     /**< Type of HID class report. */
  uint16_t wReportLength;   /**< the total size of the Report descriptor. */
} tusb_hid_descriptor_hid_t;

/// HID Subclass
typedef enum
{
  HID_SUBCLASS_NONE = 0, ///< No Subclass
  HID_SUBCLASS_BOOT = 1  ///< Boot Interface Subclass
}hid_subclass_enum_t;

/// HID Interface Protocol
typedef enum
{
  HID_ITF_PROTOCOL_NONE     = 0, ///< None
  HID_ITF_PROTOCOL_KEYBOARD = 1, ///< Keyboard
  HID_ITF_PROTOCOL_MOUSE    = 2  ///< Mouse
}hid_interface_protocol_enum_t;

/// HID Descriptor Type
typedef enum
{
  HID_DESC_TYPE_HID      = 0x21, ///< HID Descriptor
  HID_DESC_TYPE_REPORT   = 0x22, ///< Report Descriptor
  HID_DESC_TYPE_PHYSICAL = 0x23  ///< Physical Descriptor
}hid_descriptor_enum_t;

/// HID Request Report Type
typedef enum
{
  HID_REPORT_TYPE_INVALID = 0,
  HID_REPORT_TYPE_INPUT,      ///< Input
  HID_REPORT_TYPE_OUTPUT,     ///< Output
  HID_REPORT_TYPE_FEATURE     ///< Feature
}hid_report_type_t;

/// HID Class Specific Control Request
typedef enum
{
  HID_REQ_CONTROL_GET_REPORT   = 0x01, ///< Get Report
  HID_REQ_CONTROL_GET_IDLE     = 0x02, ///< Get Idle
  HID_REQ_CONTROL_GET_PROTOCOL = 0x03, ///< Get Protocol
  HID_REQ_CONTROL_SET_REPORT   = 0x09, ///< Set Report
  HID_REQ_CONTROL_SET_IDLE     = 0x0a, ///< Set Idle
  HID_REQ_CONTROL_SET_PROTOCOL = 0x0b  ///< Set Protocol
}hid_request_enum_t;

/// HID Local Code
typedef enum
{
  HID_LOCAL_NotSupported = 0   , ///< NotSupported
  HID_LOCAL_Arabic             , ///< Arabic
  HID_LOCAL_Belgian            , ///< Belgian
  HID_LOCAL_Canadian_Bilingual , ///< Canadian_Bilingual
  HID_LOCAL_Canadian_French    , ///< Canadian_French
  HID_LOCAL_Czech_Republic     , ///< Czech_Republic
  HID_LOCAL_Danish             , ///< Danish
  HID_LOCAL_Finnish            , ///< Finnish
  HID_LOCAL_French             , ///< French
  HID_LOCAL_German             , ///< German
  HID_LOCAL_Greek              , ///< Greek
  HID_LOCAL_Hebrew             , ///< Hebrew
  HID_LOCAL_Hungary            , ///< Hungary
  HID_LOCAL_International      , ///< International
  HID_LOCAL_Italian            , ///< Italian
  HID_LOCAL_Japan_Katakana     , ///< Japan_Katakana
  HID_LOCAL_Korean             , ///< Korean
  HID_LOCAL_Latin_American     , ///< Latin_American
  HID_LOCAL_Netherlands_Dutch  , ///< Netherlands/Dutch
  HID_LOCAL_Norwegian          , ///< Norwegian
  HID_LOCAL_Persian_Farsi      , ///< Persian (Farsi)
  HID_LOCAL_Poland             , ///< Poland
  HID_LOCAL_Portuguese         , ///< Portuguese
  HID_LOCAL_Russia             , ///< Russia
  HID_LOCAL_Slovakia           , ///< Slovakia
  HID_LOCAL_Spanish            , ///< Spanish
  HID_LOCAL_Swedish            , ///< Swedish
  HID_LOCAL_Swiss_French       , ///< Swiss/French
  HID_LOCAL_Swiss_German       , ///< Swiss/German
  HID_LOCAL_Switzerland        , ///< Switzerland
  HID_LOCAL_Taiwan             , ///< Taiwan
  HID_LOCAL_Turkish_Q          , ///< Turkish-Q
  HID_LOCAL_UK                 , ///< UK
  HID_LOCAL_US                 , ///< US
  HID_LOCAL_Yugoslavia         , ///< Yugoslavia
  HID_LOCAL_Turkish_F            ///< Turkish-F
} hid_local_enum_t;

// HID protocol value used by GetProtocol / SetProtocol
typedef enum
{
  HID_PROTOCOL_BOOT = 0,
  HID_PROTOCOL_REPORT = 1
} hid_protocol_mode_enum_t;

/** @} */

//--------------------------------------------------------------------+
// GAMEPAD
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Gamepad Gamepad
 *  @{ */

/* From https://www.kernel.org/doc/html/latest/input/gamepad.html
          ____________________________              __
         / [__ZL__]          [__ZR__] \               |
        / [__ TL __]        [__ TR __] \              | Front Triggers
     __/________________________________\__         __|
    /                                  _   \          |
   /      /\           __             (N)   \         |
  /       ||      __  |MO|  __     _       _ \        | Main Pad
 |    <===DP===> |SE|      |ST|   (W) -|- (E) |       |
  \       ||    ___          ___       _     /        |
  /\      \/   /   \        /   \     (S)   /\      __|
 /  \________ | LS  | ____ |  RS | ________/  \       |
|         /  \ \___/ /    \ \___/ /  \         |      | Control Sticks
|        /    \_____/      \_____/    \        |    __|
|       /                              \       |
 \_____/                                \_____/

     |________|______|    |______|___________|
       D-Pad    Left       Right   Action Pad
               Stick       Stick

                 |_____________|
                    Menu Pad

  Most gamepads have the following features:
  - Action-Pad 4 buttons in diamonds-shape (on the right side) NORTH, SOUTH, WEST and EAST.
  - D-Pad (Direction-pad) 4 buttons (on the left side) that point up, down, left and right.
  - Menu-Pad Different constellations, but most-times 2 buttons: SELECT - START.
  - Analog-Sticks provide freely moveable sticks to control directions, Analog-sticks may also
  provide a digital button if you press them.
  - Triggers are located on the upper-side of the pad in vertical direction. The upper buttons
  are normally named Left- and Right-Triggers, the lower buttons Z-Left and Z-Right.
  - Rumble Many devices provide force-feedback features. But are mostly just simple rumble motors.
 */

/// HID Gamepad Protocol Report.
typedef struct TU_ATTR_PACKED
{
  int8_t  x;         ///< Delta x  movement of left analog-stick
  int8_t  y;         ///< Delta y  movement of left analog-stick
  int8_t  z;         ///< Delta z  movement of right analog-joystick
  int8_t  rz;        ///< Delta Rz movement of right analog-joystick
  int8_t  rx;        ///< Delta Rx movement of analog left trigger
  int8_t  ry;        ///< Delta Ry movement of analog right trigger
  uint8_t hat;       ///< Buttons mask for currently pressed buttons in the DPad/hat
  uint32_t buttons;  ///< Buttons mask for currently pressed buttons
}hid_gamepad_report_t;

/// Standard Gamepad Buttons Bitmap
typedef enum
{
  GAMEPAD_BUTTON_0  = TU_BIT(0),
  GAMEPAD_BUTTON_1  = TU_BIT(1),
  GAMEPAD_BUTTON_2  = TU_BIT(2),
  GAMEPAD_BUTTON_3  = TU_BIT(3),
  GAMEPAD_BUTTON_4  = TU_BIT(4),
  GAMEPAD_BUTTON_5  = TU_BIT(5),
  GAMEPAD_BUTTON_6  = TU_BIT(6),
  GAMEPAD_BUTTON_7  = TU_BIT(7),
  GAMEPAD_BUTTON_8  = TU_BIT(8),
  GAMEPAD_BUTTON_9  = TU_BIT(9),
  GAMEPAD_BUTTON_10 = TU_BIT(10),
  GAMEPAD_BUTTON_11 = TU_BIT(11),
  GAMEPAD_BUTTON_12 = TU_BIT(12),
  GAMEPAD_BUTTON_13 = TU_BIT(13),
  GAMEPAD_BUTTON_14 = TU_BIT(14),
  GAMEPAD_BUTTON_15 = TU_BIT(15),
  GAMEPAD_BUTTON_16 = TU_BIT(16),
  GAMEPAD_BUTTON_17 = TU_BIT(17),
  GAMEPAD_BUTTON_18 = TU_BIT(18),
  GAMEPAD_BUTTON_19 = TU_BIT(19),
  GAMEPAD_BUTTON_20 = TU_BIT(20),
  GAMEPAD_BUTTON_21 = TU_BIT(21),
  GAMEPAD_BUTTON_22 = TU_BIT(22),
  GAMEPAD_BUTTON_23 = TU_BIT(23),
  GAMEPAD_BUTTON_24 = TU_BIT(24),
  GAMEPAD_BUTTON_25 = TU_BIT(25),
  GAMEPAD_BUTTON_26 = TU_BIT(26),
  GAMEPAD_BUTTON_27 = TU_BIT(27),
  GAMEPAD_BUTTON_28 = TU_BIT(28),
  GAMEPAD_BUTTON_29 = TU_BIT(29),
  GAMEPAD_BUTTON_30 = TU_BIT(30),
  GAMEPAD_BUTTON_31 = TU_BIT(31),
}hid_gamepad_button_bm_t;

/// Standard Gamepad Buttons Naming from Linux input event codes
/// https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
#define GAMEPAD_BUTTON_A       GAMEPAD_BUTTON_0
#define GAMEPAD_BUTTON_SOUTH   GAMEPAD_BUTTON_0

#define GAMEPAD_BUTTON_B       GAMEPAD_BUTTON_1
#define GAMEPAD_BUTTON_EAST    GAMEPAD_BUTTON_1

#define GAMEPAD_BUTTON_C       GAMEPAD_BUTTON_2

#define GAMEPAD_BUTTON_X       GAMEPAD_BUTTON_3
#define GAMEPAD_BUTTON_NORTH   GAMEPAD_BUTTON_3

#define GAMEPAD_BUTTON_Y       GAMEPAD_BUTTON_4
#define GAMEPAD_BUTTON_WEST    GAMEPAD_BUTTON_4

#define GAMEPAD_BUTTON_Z       GAMEPAD_BUTTON_5
#define GAMEPAD_BUTTON_TL      GAMEPAD_BUTTON_6
#define GAMEPAD_BUTTON_TR      GAMEPAD_BUTTON_7
#define GAMEPAD_BUTTON_TL2     GAMEPAD_BUTTON_8
#define GAMEPAD_BUTTON_TR2     GAMEPAD_BUTTON_9
#define GAMEPAD_BUTTON_SELECT  GAMEPAD_BUTTON_10
#define GAMEPAD_BUTTON_START   GAMEPAD_BUTTON_11
#define GAMEPAD_BUTTON_MODE    GAMEPAD_BUTTON_12
#define GAMEPAD_BUTTON_THUMBL  GAMEPAD_BUTTON_13
#define GAMEPAD_BUTTON_THUMBR  GAMEPAD_BUTTON_14

/// Standard Gamepad HAT/DPAD Buttons (from Linux input event codes)
typedef enum
{
  GAMEPAD_HAT_CENTERED   = 0,  ///< DPAD_CENTERED
  GAMEPAD_HAT_UP         = 1,  ///< DPAD_UP
  GAMEPAD_HAT_UP_RIGHT   = 2,  ///< DPAD_UP_RIGHT
  GAMEPAD_HAT_RIGHT      = 3,  ///< DPAD_RIGHT
  GAMEPAD_HAT_DOWN_RIGHT = 4,  ///< DPAD_DOWN_RIGHT
  GAMEPAD_HAT_DOWN       = 5,  ///< DPAD_DOWN
  GAMEPAD_HAT_DOWN_LEFT  = 6,  ///< DPAD_DOWN_LEFT
  GAMEPAD_HAT_LEFT       = 7,  ///< DPAD_LEFT
  GAMEPAD_HAT_UP_LEFT    = 8,  ///< DPAD_UP_LEFT
}hid_gamepad_hat_t;

/// @}

//--------------------------------------------------------------------+
// MOUSE
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Mouse Mouse
 *  @{ */

/// Standard HID Boot Protocol Mouse Report.
typedef struct TU_ATTR_PACKED
{
  uint8_t buttons; /**< buttons mask for currently pressed buttons in the mouse. */
  int8_t  x;       /**< Current delta x movement of the mouse. */
  int8_t  y;       /**< Current delta y movement on the mouse. */
  int8_t  wheel;   /**< Current delta wheel movement on the mouse. */
  int8_t  pan;     // using AC Pan
} hid_mouse_report_t;


// Absolute Mouse: same as the Standard (relative) Mouse Report but
// with int16_t instead of int8_t for X and Y coordinates.
typedef struct TU_ATTR_PACKED
{
    uint8_t buttons; /**< buttons mask for currently pressed buttons in the mouse. */
    int16_t x;       /**< Current x position of the mouse. */
    int16_t y;       /**< Current y position of the mouse. */
    int8_t wheel;    /**< Current delta wheel movement on the mouse. */
    int8_t pan;      // using AC Pan
} hid_abs_mouse_report_t;


/// Standard Mouse Buttons Bitmap
typedef enum
{
  MOUSE_BUTTON_LEFT     = TU_BIT(0), ///< Left button
  MOUSE_BUTTON_RIGHT    = TU_BIT(1), ///< Right button
  MOUSE_BUTTON_MIDDLE   = TU_BIT(2), ///< Middle button
  MOUSE_BUTTON_BACKWARD = TU_BIT(3), ///< Backward button,
  MOUSE_BUTTON_FORWARD  = TU_BIT(4), ///< Forward button,
}hid_mouse_button_bm_t;

/// @}

//--------------------------------------------------------------------+
// Digitizer Stylus Pen
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Stylus Stylus
 *  @{ */

// Standard Stylus Pen Report.
typedef struct TU_ATTR_PACKED
{
  uint8_t attr;    /**< Attribute mask for describing current status of the stylus pen. */
  uint16_t x;      /**< Current x position of the mouse. */
  uint16_t y;      /**< Current y position of the mouse. */
} hid_stylus_report_t;

// Standard Stylus Pen Attributes Bitmap.
typedef enum
{
  STYLUS_ATTR_TIP_SWITCH = TU_BIT(0), ///< Tip switch
  STYLUS_ATTR_IN_RANGE   = TU_BIT(1), ///< In-range bit.
} hid_stylus_attr_bm_t;

/// @}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Keyboard Keyboard
 *  @{ */

/// Standard HID Boot Protocol Keyboard Report.
typedef struct TU_ATTR_PACKED
{
  uint8_t modifier;   /**< Keyboard modifier (KEYBOARD_MODIFIER_* masks). */
  uint8_t reserved;   /**< Reserved for OEM use, always set to 0. */
  uint8_t keycode[6]; /**< Key codes of the currently pressed keys. */
} hid_keyboard_report_t;

/// Keyboard modifier codes bitmap
typedef enum
{
  KEYBOARD_MODIFIER_LEFTCTRL   = TU_BIT(0), ///< Left Control
  KEYBOARD_MODIFIER_LEFTSHIFT  = TU_BIT(1), ///< Left Shift
  KEYBOARD_MODIFIER_LEFTALT    = TU_BIT(2), ///< Left Alt
  KEYBOARD_MODIFIER_LEFTGUI    = TU_BIT(3), ///< Left Window
  KEYBOARD_MODIFIER_RIGHTCTRL  = TU_BIT(4), ///< Right Control
  KEYBOARD_MODIFIER_RIGHTSHIFT = TU_BIT(5), ///< Right Shift
  KEYBOARD_MODIFIER_RIGHTALT   = TU_BIT(6), ///< Right Alt
  KEYBOARD_MODIFIER_RIGHTGUI   = TU_BIT(7)  ///< Right Window
}hid_keyboard_modifier_bm_t;

typedef enum
{
  KEYBOARD_LED_NUMLOCK    = TU_BIT(0), ///< Num Lock LED
  KEYBOARD_LED_CAPSLOCK   = TU_BIT(1), ///< Caps Lock LED
  KEYBOARD_LED_SCROLLLOCK = TU_BIT(2), ///< Scroll Lock LED
  KEYBOARD_LED_COMPOSE    = TU_BIT(3), ///< Composition Mode
  KEYBOARD_LED_KANA       = TU_BIT(4) ///< Kana mode
}hid_keyboard_led_bm_t;

/// @}

//--------------------------------------------------------------------+
// HID KEYCODE - defined by HID Usage Table: Keyboard/Keypad Page (0x07)
//--------------------------------------------------------------------+
#define HID_KEY_NONE                        0x00
#define HID_KEY_A                           0x04
#define HID_KEY_B                           0x05
#define HID_KEY_C                           0x06
#define HID_KEY_D                           0x07
#define HID_KEY_E                           0x08
#define HID_KEY_F                           0x09
#define HID_KEY_G                           0x0A
#define HID_KEY_H                           0x0B
#define HID_KEY_I                           0x0C
#define HID_KEY_J                           0x0D
#define HID_KEY_K                           0x0E
#define HID_KEY_L                           0x0F
#define HID_KEY_M                           0x10
#define HID_KEY_N                           0x11
#define HID_KEY_O                           0x12
#define HID_KEY_P                           0x13
#define HID_KEY_Q                           0x14
#define HID_KEY_R                           0x15
#define HID_KEY_S                           0x16
#define HID_KEY_T                           0x17
#define HID_KEY_U                           0x18
#define HID_KEY_V                           0x19
#define HID_KEY_W                           0x1A
#define HID_KEY_X                           0x1B
#define HID_KEY_Y                           0x1C
#define HID_KEY_Z                           0x1D
#define HID_KEY_1                           0x1E
#define HID_KEY_2                           0x1F
#define HID_KEY_3                           0x20
#define HID_KEY_4                           0x21
#define HID_KEY_5                           0x22
#define HID_KEY_6                           0x23
#define HID_KEY_7                           0x24
#define HID_KEY_8                           0x25
#define HID_KEY_9                           0x26
#define HID_KEY_0                           0x27
#define HID_KEY_ENTER                       0x28
#define HID_KEY_ESCAPE                      0x29
#define HID_KEY_BACKSPACE                   0x2A
#define HID_KEY_TAB                         0x2B
#define HID_KEY_SPACE                       0x2C
#define HID_KEY_MINUS                       0x2D
#define HID_KEY_EQUAL                       0x2E
#define HID_KEY_BRACKET_LEFT                0x2F
#define HID_KEY_BRACKET_RIGHT               0x30
#define HID_KEY_BACKSLASH                   0x31
#define HID_KEY_EUROPE_1                    0x32
#define HID_KEY_SEMICOLON                   0x33
#define HID_KEY_APOSTROPHE                  0x34
#define HID_KEY_GRAVE                       0x35
#define HID_KEY_COMMA                       0x36
#define HID_KEY_PERIOD                      0x37
#define HID_KEY_SLASH                       0x38
#define HID_KEY_CAPS_LOCK                   0x39
#define HID_KEY_F1                          0x3A
#define HID_KEY_F2                          0x3B
#define HID_KEY_F3                          0x3C
#define HID_KEY_F4                          0x3D
#define HID_KEY_F5                          0x3E
#define HID_KEY_F6                          0x3F
#define HID_KEY_F7                          0x40
#define HID_KEY_F8                          0x41
#define HID_KEY_F9                          0x42
#define HID_KEY_F10                         0x43
#define HID_KEY_F11                         0x44
#define HID_KEY_F12                         0x45
#define HID_KEY_PRINT_SCREEN                0x46
#define HID_KEY_SCROLL_LOCK                 0x47
#define HID_KEY_PAUSE                       0x48
#define HID_KEY_INSERT                      0x49
#define HID_KEY_HOME                        0x4A
#define HID_KEY_PAGE_UP                     0x4B
#define HID_KEY_DELETE                      0x4C
#define HID_KEY_END                         0x4D
#define HID_KEY_PAGE_DOWN                   0x4E
#define HID_KEY_ARROW_RIGHT                 0x4F
#define HID_KEY_ARROW_LEFT                  0x50
#define HID_KEY_ARROW_DOWN                  0x51
#define HID_KEY_ARROW_UP                    0x52
#define HID_KEY_NUM_LOCK                    0x53
#define HID_KEY_KEYPAD_DIVIDE               0x54
#define HID_KEY_KEYPAD_MULTIPLY             0x55
#define HID_KEY_KEYPAD_SUBTRACT             0x56
#define HID_KEY_KEYPAD_ADD                  0x57
#define HID_KEY_KEYPAD_ENTER                0x58
#define HID_KEY_KEYPAD_1                    0x59
#define HID_KEY_KEYPAD_2                    0x5A
#define HID_KEY_KEYPAD_3                    0x5B
#define HID_KEY_KEYPAD_4                    0x5C
#define HID_KEY_KEYPAD_5                    0x5D
#define HID_KEY_KEYPAD_6                    0x5E
#define HID_KEY_KEYPAD_7                    0x5F
#define HID_KEY_KEYPAD_8                    0x60
#define HID_KEY_KEYPAD_9                    0x61
#define HID_KEY_KEYPAD_0                    0x62
#define HID_KEY_KEYPAD_DECIMAL              0x63
#define HID_KEY_EUROPE_2                    0x64
#define HID_KEY_APPLICATION                 0x65
#define HID_KEY_POWER                       0x66
#define HID_KEY_KEYPAD_EQUAL                0x67
#define HID_KEY_F13                         0x68
#define HID_KEY_F14                         0x69
#define HID_KEY_F15                         0x6A
#define HID_KEY_F16                         0x6B
#define HID_KEY_F17                         0x6C
#define HID_KEY_F18                         0x6D
#define HID_KEY_F19                         0x6E
#define HID_KEY_F20                         0x6F
#define HID_KEY_F21                         0x70
#define HID_KEY_F22                         0x71
#define HID_KEY_F23                         0x72
#define HID_KEY_F24                         0x73
#define HID_KEY_EXECUTE                     0x74
#define HID_KEY_HELP                        0x75
#define HID_KEY_MENU                        0x76
#define HID_KEY_SELECT                      0x77
#define HID_KEY_STOP                        0x78
#define HID_KEY_AGAIN                       0x79
#define HID_KEY_UNDO                        0x7A
#define HID_KEY_CUT                         0x7B
#define HID_KEY_COPY                        0x7C
#define HID_KEY_PASTE                       0x7D
#define HID_KEY_FIND                        0x7E
#define HID_KEY_MUTE                        0x7F
#define HID_KEY_VOLUME_UP                   0x80
#define HID_KEY_VOLUME_DOWN                 0x81
#define HID_KEY_LOCKING_CAPS_LOCK           0x82
#define HID_KEY_LOCKING_NUM_LOCK            0x83
#define HID_KEY_LOCKING_SCROLL_LOCK         0x84
#define HID_KEY_KEYPAD_COMMA                0x85
#define HID_KEY_KEYPAD_EQUAL_SIGN           0x86
#define HID_KEY_KANJI1                      0x87
#define HID_KEY_KANJI2                      0x88
#define HID_KEY_KANJI3                      0x89
#define HID_KEY_KANJI4                      0x8A
#define HID_KEY_KANJI5                      0x8B
#define HID_KEY_KANJI6                      0x8C
#define HID_KEY_KANJI7                      0x8D
#define HID_KEY_KANJI8                      0x8E
#define HID_KEY_KANJI9                      0x8F
#define HID_KEY_LANG1                       0x90
#define HID_KEY_LANG2                       0x91
#define HID_KEY_LANG3                       0x92
#define HID_KEY_LANG4                       0x93
#define HID_KEY_LANG5                       0x94
#define HID_KEY_LANG6                       0x95
#define HID_KEY_LANG7                       0x96
#define HID_KEY_LANG8                       0x97
#define HID_KEY_LANG9                       0x98
#define HID_KEY_ALTERNATE_ERASE             0x99
#define HID_KEY_SYSREQ_ATTENTION            0x9A
#define HID_KEY_CANCEL                      0x9B
#define HID_KEY_CLEAR                       0x9C
#define HID_KEY_PRIOR                       0x9D
#define HID_KEY_RETURN                      0x9E
#define HID_KEY_SEPARATOR                   0x9F
#define HID_KEY_OUT                         0xA0
#define HID_KEY_OPER                        0xA1
#define HID_KEY_CLEAR_AGAIN                 0xA2
#define HID_KEY_CRSEL_PROPS                 0xA3
#define HID_KEY_EXSEL                       0xA4
// RESERVED					                        0xA5-AF
#define HID_KEY_KEYPAD_00                   0xB0
#define HID_KEY_KEYPAD_000                  0xB1
#define HID_KEY_THOUSANDS_SEPARATOR         0xB2
#define HID_KEY_DECIMAL_SEPARATOR           0xB3
#define HID_KEY_CURRENCY_UNIT               0xB4
#define HID_KEY_CURRENCY_SUBUNIT            0xB5
#define HID_KEY_KEYPAD_LEFT_PARENTHESIS     0xB6
#define HID_KEY_KEYPAD_RIGHT_PARENTHESIS    0xB7
#define HID_KEY_KEYPAD_LEFT_BRACE           0xB8
#define HID_KEY_KEYPAD_RIGHT_BRACE          0xB9
#define HID_KEY_KEYPAD_TAB                  0xBA
#define HID_KEY_KEYPAD_BACKSPACE            0xBB
#define HID_KEY_KEYPAD_A                    0xBC
#define HID_KEY_KEYPAD_B                    0xBD
#define HID_KEY_KEYPAD_C                    0xBE
#define HID_KEY_KEYPAD_D                    0xBF
#define HID_KEY_KEYPAD_E                    0xC0
#define HID_KEY_KEYPAD_F                    0xC1
#define HID_KEY_KEYPAD_XOR                  0xC2
#define HID_KEY_KEYPAD_CARET                0xC3
#define HID_KEY_KEYPAD_PERCENT              0xC4
#define HID_KEY_KEYPAD_LESS_THAN            0xC5
#define HID_KEY_KEYPAD_GREATER_THAN         0xC6
#define HID_KEY_KEYPAD_AMPERSAND            0xC7
#define HID_KEY_KEYPAD_DOUBLE_AMPERSAND     0xC8
#define HID_KEY_KEYPAD_VERTICAL_BAR         0xC9
#define HID_KEY_KEYPAD_DOUBLE_VERTICAL_BAR  0xCA
#define HID_KEY_KEYPAD_COLON                0xCB
#define HID_KEY_KEYPAD_HASH                 0xCC
#define HID_KEY_KEYPAD_SPACE                0xCD
#define HID_KEY_KEYPAD_AT                   0xCE
#define HID_KEY_KEYPAD_EXCLAMATION          0xCF
#define HID_KEY_KEYPAD_MEMORY_STORE         0xD0
#define HID_KEY_KEYPAD_MEMORY_RECALL        0xD1
#define HID_KEY_KEYPAD_MEMORY_CLEAR         0xD2
#define HID_KEY_KEYPAD_MEMORY_ADD           0xD3
#define HID_KEY_KEYPAD_MEMORY_SUBTRACT      0xD4
#define HID_KEY_KEYPAD_MEMORY_MULTIPLY      0xD5
#define HID_KEY_KEYPAD_MEMORY_DIVIDE        0xD6
#define HID_KEY_KEYPAD_PLUS_MINUS           0xD7
#define HID_KEY_KEYPAD_CLEAR                0xD8
#define HID_KEY_KEYPAD_CLEAR_ENTRY          0xD9
#define HID_KEY_KEYPAD_BINARY               0xDA
#define HID_KEY_KEYPAD_OCTAL                0xDB
#define HID_KEY_KEYPAD_DECIMAL_2            0xDC
#define HID_KEY_KEYPAD_HEXADECIMAL          0xDD
// RESERVED					                        0xDE-DF
#define HID_KEY_CONTROL_LEFT                0xE0
#define HID_KEY_SHIFT_LEFT                  0xE1
#define HID_KEY_ALT_LEFT                    0xE2
#define HID_KEY_GUI_LEFT                    0xE3
#define HID_KEY_CONTROL_RIGHT               0xE4
#define HID_KEY_SHIFT_RIGHT                 0xE5
#define HID_KEY_ALT_RIGHT                   0xE6
#define HID_KEY_GUI_RIGHT                   0xE7


//--------------------------------------------------------------------+
// REPORT DESCRIPTOR
//--------------------------------------------------------------------+

//------------- ITEM & TAG -------------//
#define HID_REPORT_DATA_0(data)
#define HID_REPORT_DATA_1(data) , data
#define HID_REPORT_DATA_2(data) , U16_TO_U8S_LE(data)
#define HID_REPORT_DATA_3(data) , U32_TO_U8S_LE(data)

#define HID_REPORT_ITEM(data, tag, type, size) \
  (((tag) << 4) | ((type) << 2) | (size)) HID_REPORT_DATA_##size(data)

// Report Item Types
enum {
  RI_TYPE_MAIN   = 0,
  RI_TYPE_GLOBAL = 1,
  RI_TYPE_LOCAL  = 2
};

//------------- Main Items - HID 1.11 section 6.2.2.4 -------------//

// Report Item Main group
enum {
  RI_MAIN_INPUT          = 8,
  RI_MAIN_OUTPUT         = 9,
  RI_MAIN_COLLECTION     = 10,
  RI_MAIN_FEATURE        = 11,
  RI_MAIN_COLLECTION_END = 12
};

#define HID_INPUT(x)           HID_REPORT_ITEM(x, RI_MAIN_INPUT         , RI_TYPE_MAIN, 1)
#define HID_OUTPUT(x)          HID_REPORT_ITEM(x, RI_MAIN_OUTPUT        , RI_TYPE_MAIN, 1)
#define HID_COLLECTION(x)      HID_REPORT_ITEM(x, RI_MAIN_COLLECTION    , RI_TYPE_MAIN, 1)
#define HID_FEATURE(x)         HID_REPORT_ITEM(x, RI_MAIN_FEATURE       , RI_TYPE_MAIN, 1)
#define HID_COLLECTION_END     HID_REPORT_ITEM(x, RI_MAIN_COLLECTION_END, RI_TYPE_MAIN, 0)

//------------- Input, Output, Feature - HID 1.11 section 6.2.2.5 -------------//
#define HID_DATA             (0<<0)
#define HID_CONSTANT         (1<<0)

#define HID_ARRAY            (0<<1)
#define HID_VARIABLE         (1<<1)

#define HID_ABSOLUTE         (0<<2)
#define HID_RELATIVE         (1<<2)

#define HID_WRAP_NO          (0<<3)
#define HID_WRAP             (1<<3)

#define HID_LINEAR           (0<<4)
#define HID_NONLINEAR        (1<<4)

#define HID_PREFERRED_STATE  (0<<5)
#define HID_PREFERRED_NO     (1<<5)

#define HID_NO_NULL_POSITION (0<<6)
#define HID_NULL_STATE       (1<<6)

#define HID_NON_VOLATILE     (0<<7)
#define HID_VOLATILE         (1<<7)

#define HID_BITFIELD         (0<<8)
#define HID_BUFFERED_BYTES   (1<<8)

//------------- Collection Item - HID 1.11 section 6.2.2.6 -------------//
enum {
  HID_COLLECTION_PHYSICAL = 0,
  HID_COLLECTION_APPLICATION,
  HID_COLLECTION_LOGICAL,
  HID_COLLECTION_REPORT,
  HID_COLLECTION_NAMED_ARRAY,
  HID_COLLECTION_USAGE_SWITCH,
  HID_COLLECTION_USAGE_MODIFIER
};

//------------- Global Items - HID 1.11 section 6.2.2.7 -------------//

// Report Item Global group
enum {
  RI_GLOBAL_USAGE_PAGE    = 0,
  RI_GLOBAL_LOGICAL_MIN   = 1,
  RI_GLOBAL_LOGICAL_MAX   = 2,
  RI_GLOBAL_PHYSICAL_MIN  = 3,
  RI_GLOBAL_PHYSICAL_MAX  = 4,
  RI_GLOBAL_UNIT_EXPONENT = 5,
  RI_GLOBAL_UNIT          = 6,
  RI_GLOBAL_REPORT_SIZE   = 7,
  RI_GLOBAL_REPORT_ID     = 8,
  RI_GLOBAL_REPORT_COUNT  = 9,
  RI_GLOBAL_PUSH          = 10,
  RI_GLOBAL_POP           = 11
};

#define HID_USAGE_PAGE(x)         HID_REPORT_ITEM(x, RI_GLOBAL_USAGE_PAGE, RI_TYPE_GLOBAL, 1)
#define HID_USAGE_PAGE_N(x, n)    HID_REPORT_ITEM(x, RI_GLOBAL_USAGE_PAGE, RI_TYPE_GLOBAL, n)

#define HID_LOGICAL_MIN(x)        HID_REPORT_ITEM(x, RI_GLOBAL_LOGICAL_MIN, RI_TYPE_GLOBAL, 1)
#define HID_LOGICAL_MIN_N(x, n)   HID_REPORT_ITEM(x, RI_GLOBAL_LOGICAL_MIN, RI_TYPE_GLOBAL, n)

#define HID_LOGICAL_MAX(x)        HID_REPORT_ITEM(x, RI_GLOBAL_LOGICAL_MAX, RI_TYPE_GLOBAL, 1)
#define HID_LOGICAL_MAX_N(x, n)   HID_REPORT_ITEM(x, RI_GLOBAL_LOGICAL_MAX, RI_TYPE_GLOBAL, n)

#define HID_PHYSICAL_MIN(x)       HID_REPORT_ITEM(x, RI_GLOBAL_PHYSICAL_MIN, RI_TYPE_GLOBAL, 1)
#define HID_PHYSICAL_MIN_N(x, n)  HID_REPORT_ITEM(x, RI_GLOBAL_PHYSICAL_MIN, RI_TYPE_GLOBAL, n)

#define HID_PHYSICAL_MAX(x)       HID_REPORT_ITEM(x, RI_GLOBAL_PHYSICAL_MAX, RI_TYPE_GLOBAL, 1)
#define HID_PHYSICAL_MAX_N(x, n)  HID_REPORT_ITEM(x, RI_GLOBAL_PHYSICAL_MAX, RI_TYPE_GLOBAL, n)

#define HID_UNIT_EXPONENT(x)      HID_REPORT_ITEM(x, RI_GLOBAL_UNIT_EXPONENT, RI_TYPE_GLOBAL, 1)
#define HID_UNIT_EXPONENT_N(x, n) HID_REPORT_ITEM(x, RI_GLOBAL_UNIT_EXPONENT, RI_TYPE_GLOBAL, n)

#define HID_UNIT(x)               HID_REPORT_ITEM(x, RI_GLOBAL_UNIT, RI_TYPE_GLOBAL, 1)
#define HID_UNIT_N(x, n)          HID_REPORT_ITEM(x, RI_GLOBAL_UNIT, RI_TYPE_GLOBAL, n)

#define HID_REPORT_SIZE(x)        HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_SIZE, RI_TYPE_GLOBAL, 1)
#define HID_REPORT_SIZE_N(x, n)   HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_SIZE, RI_TYPE_GLOBAL, n)

#define HID_REPORT_ID(x)          HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_ID, RI_TYPE_GLOBAL, 1),
#define HID_REPORT_ID_N(x, n)     HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_ID, RI_TYPE_GLOBAL, n),

#define HID_REPORT_COUNT(x)       HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_COUNT, RI_TYPE_GLOBAL, 1)
#define HID_REPORT_COUNT_N(x, n)  HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_COUNT, RI_TYPE_GLOBAL, n)

#define HID_PUSH                  HID_REPORT_ITEM(x, RI_GLOBAL_PUSH, RI_TYPE_GLOBAL, 0)
#define HID_POP                   HID_REPORT_ITEM(x, RI_GLOBAL_POP, RI_TYPE_GLOBAL, 0)

//------------- LOCAL ITEMS 6.2.2.8 -------------//

enum {
  RI_LOCAL_USAGE            = 0,
  RI_LOCAL_USAGE_MIN        = 1,
  RI_LOCAL_USAGE_MAX        = 2,
  RI_LOCAL_DESIGNATOR_INDEX = 3,
  RI_LOCAL_DESIGNATOR_MIN   = 4,
  RI_LOCAL_DESIGNATOR_MAX   = 5,
  // 6 is reserved
  RI_LOCAL_STRING_INDEX     = 7,
  RI_LOCAL_STRING_MIN       = 8,
  RI_LOCAL_STRING_MAX       = 9,
  RI_LOCAL_DELIMITER        = 10,
};

#define HID_USAGE(x)              HID_REPORT_ITEM(x, RI_LOCAL_USAGE, RI_TYPE_LOCAL, 1)
#define HID_USAGE_N(x, n)         HID_REPORT_ITEM(x, RI_LOCAL_USAGE, RI_TYPE_LOCAL, n)

#define HID_USAGE_MIN(x)          HID_REPORT_ITEM(x, RI_LOCAL_USAGE_MIN, RI_TYPE_LOCAL, 1)
#define HID_USAGE_MIN_N(x, n)     HID_REPORT_ITEM(x, RI_LOCAL_USAGE_MIN, RI_TYPE_LOCAL, n)

#define HID_USAGE_MAX(x)          HID_REPORT_ITEM(x, RI_LOCAL_USAGE_MAX, RI_TYPE_LOCAL, 1)
#define HID_USAGE_MAX_N(x, n)     HID_REPORT_ITEM(x, RI_LOCAL_USAGE_MAX, RI_TYPE_LOCAL, n)

//--------------------------------------------------------------------+
// Usage Table
/* Usage Types Data
    Sel  Selector               Array
    SV   Static Value           Constant, Variable, Absolute
    SF   Static Flag            Constant, Variable, Absolute
    DV   Dynamic Value          Constant, Variable, Absolute
    DF   Dynamic Flag           Constant, Variable, Absolute
*/
/* Usage Types Collection
    NAry  Named Array             Logical
    CA    Collection Application  Application
    CL    Collection Logical      Logical
    CP    Collection Physical     Physical
    US    Usage Switch            Logical
    UM    Usage Modifier          Logical
*/
//--------------------------------------------------------------------+

/// HID Usage Table - Table 1: Usage Page Summary
enum {
  HID_USAGE_PAGE_DESKTOP                   = 0x01,
  HID_USAGE_PAGE_SIMULATE                  = 0x02,
  HID_USAGE_PAGE_VIRTUAL_REALITY           = 0x03,
  HID_USAGE_PAGE_SPORT                     = 0x04,
  HID_USAGE_PAGE_GAME                      = 0x05,
  HID_USAGE_PAGE_GENERIC_DEVICE            = 0x06,
  HID_USAGE_PAGE_KEYBOARD                  = 0x07,
  HID_USAGE_PAGE_LED                       = 0x08,
  HID_USAGE_PAGE_BUTTON                    = 0x09,
  HID_USAGE_PAGE_ORDINAL                   = 0x0A,
  HID_USAGE_PAGE_TELEPHONY                 = 0x0B,
  HID_USAGE_PAGE_CONSUMER                  = 0x0C,
  HID_USAGE_PAGE_DIGITIZER                 = 0x0D,
  HID_USAGE_PAGE_HAPTIC                    = 0x0E,
  HID_USAGE_PAGE_PID                       = 0x0F,
  HID_USAGE_PAGE_UNICODE                   = 0x10,
  HID_USAGE_PAGE_SOC                       = 0x11,
  HID_USAGE_PAGE_EYE_AND_HEAD_TRACKERS     = 0x12,
  // 0x13 is reserved
  HID_USAGE_PAGE_AUXILIARY_DISPLAY         = 0x14,
  // 0x15 - 0x1f is reserved
  HID_USAGE_PAGE_SENSORS                   = 0x20,
  // 0x21 - 0x3f is reserved
  HID_USAGE_PAGE_MEDICAL_INSTRUMENT        = 0x40,
  HID_USAGE_PAGE_BRAILLE_DISPLAY           = 0x41,
  HID_USAGE_PAGE_LIGHTING_AND_ILLUMINATION = 0x59,
  HID_USAGE_PAGE_MONITOR                   = 0x80, // 0x80 - 0x83
  HID_USAGE_PAGE_POWER                     = 0x84,
  HID_USAGE_PAGE_BATTERY                   = 0x85,
  // 0x86 - 0x87 is reserved for Power Device
  HID_USAGE_PAGE_BARCODE_SCANNER           = 0x8C,
  HID_USAGE_PAGE_SCALE                     = 0x8D,
  HID_USAGE_PAGE_MSR                       = 0x8E,
  HID_USAGE_PAGE_CAMERA                    = 0x90,
  HID_USAGE_PAGE_ARCADE                    = 0x91,
  HID_USAGE_PAGE_GAMING                    = 0x92,   // Gaming Standards Association (GSA) HID usage page
  HID_USAGE_PAGE_FIDO                      = 0xF1D0, // FIDO alliance HID usage page
  HID_USAGE_PAGE_VENDOR                    = 0xFF00  // 0xFF00 - 0xFFFF
};

/// HID Usage Table - Table 6: Generic Desktop Page
enum {
  HID_USAGE_DESKTOP_POINTER                                    = 0x01, // CP
  HID_USAGE_DESKTOP_MOUSE                                      = 0x02, // CA
  // 03 Reserved

  HID_USAGE_DESKTOP_JOYSTICK                                   = 0x04, // CA
  HID_USAGE_DESKTOP_GAMEPAD                                    = 0x05, // CA
  HID_USAGE_DESKTOP_KEYBOARD                                   = 0x06, // CA
  HID_USAGE_DESKTOP_KEYPAD                                     = 0x07, // CA
  HID_USAGE_DESKTOP_MULTI_AXIS_CONTROLLER                      = 0x08, // CA
  HID_USAGE_DESKTOP_TABLET_PC_SYSTEM                           = 0x09, // CA
  HID_USAGE_DESKTOP_WATER_COOLING                              = 0x0A, // CA
  HID_USAGE_DESKTOP_COMPUTER_CHASSIS                           = 0x0B, // CA
  HID_USAGE_DESKTOP_WIRELESS_RADIO                             = 0x0C, // CA
  HID_USAGE_DESKTOP_PORTABLE_DEVICE                            = 0x0D, // CA
  HID_USAGE_DESKTOP_SYSTEM_MULTI_AXIS_CONTROLLER               = 0x0E, // CA
  HID_USAGE_DESKTOP_SPATIAL_CONTROLLER                         = 0x0F, // CA
  HID_USAGE_DESKTOP_ASSISTIVE                                  = 0x10, // CA
  HID_USAGE_DESKTOP_DEVICE_DOCK                                = 0x11, // CA
  HID_USAGE_DESKTOP_DOCKABLE_DEVICE                            = 0x12, // CA
  HID_USAGE_DESKTOP_CALL_STATE_MANAGEMENT                      = 0x13, // CA
  // 14-2F Reserved

  HID_USAGE_DESKTOP_X                                          = 0x30, // DV
  HID_USAGE_DESKTOP_Y                                          = 0x31, // DV
  HID_USAGE_DESKTOP_Z                                          = 0x32, // DV
  HID_USAGE_DESKTOP_RX                                         = 0x33, // DV
  HID_USAGE_DESKTOP_RY                                         = 0x34, // DV
  HID_USAGE_DESKTOP_RZ                                         = 0x35, // DV
  HID_USAGE_DESKTOP_SLIDER                                     = 0x36, // DV
  HID_USAGE_DESKTOP_DIAL                                       = 0x37, // DV
  HID_USAGE_DESKTOP_WHEEL                                      = 0x38, // DV
  HID_USAGE_DESKTOP_HAT_SWITCH                                 = 0x39, // DV
  HID_USAGE_DESKTOP_COUNTED_BUFFER                             = 0x3A, // CL
  HID_USAGE_DESKTOP_BYTE_COUNT                                 = 0x3B, // DV
  HID_USAGE_DESKTOP_MOTION_WAKEUP                              = 0x3C, // OSC/DF
  HID_USAGE_DESKTOP_START                                      = 0x3D, // OOC
  HID_USAGE_DESKTOP_SELECT                                     = 0x3E, // OOC
  // 3F Reserved

  HID_USAGE_DESKTOP_VX                                         = 0x40, // DV
  HID_USAGE_DESKTOP_VY                                         = 0x41, // DV
  HID_USAGE_DESKTOP_VZ                                         = 0x42, // DV
  HID_USAGE_DESKTOP_VBRX                                       = 0x43, // DV
  HID_USAGE_DESKTOP_VBRY                                       = 0x44, // DV
  HID_USAGE_DESKTOP_VBRZ                                       = 0x45, // DV
  HID_USAGE_DESKTOP_VNO                                        = 0x46, // DV
  HID_USAGE_DESKTOP_FEATURE_NOTIFICATION                       = 0x47, // DV/DF
  HID_USAGE_DESKTOP_RESOLUTION_MULTIPLIER                      = 0x48, // DV
  HID_USAGE_DESKTOP_QX                                         = 0x49, // DV
  HID_USAGE_DESKTOP_QY                                         = 0x4A, // DV
  HID_USAGE_DESKTOP_QZ                                         = 0x4B, // DV
  HID_USAGE_DESKTOP_QW                                         = 0x4C, // DV
  // 4D-7F Reserved

  HID_USAGE_DESKTOP_SYSTEM_CONTROL                             = 0x80, // CA
  HID_USAGE_DESKTOP_SYSTEM_POWER_DOWN                          = 0x81, // OSC
  HID_USAGE_DESKTOP_SYSTEM_SLEEP                               = 0x82, // OSC
  HID_USAGE_DESKTOP_SYSTEM_WAKE_UP                             = 0x83, // OSC
  HID_USAGE_DESKTOP_SYSTEM_CONTEXT_MENU                        = 0x84, // OSC
  HID_USAGE_DESKTOP_SYSTEM_MAIN_MENU                           = 0x85, // OSC
  HID_USAGE_DESKTOP_SYSTEM_APP_MENU                            = 0x86, // OSC
  HID_USAGE_DESKTOP_SYSTEM_MENU_HELP                           = 0x87, // OSC
  HID_USAGE_DESKTOP_SYSTEM_MENU_EXIT                           = 0x88, // OSC
  HID_USAGE_DESKTOP_SYSTEM_MENU_SELECT                         = 0x89, // OSC
  HID_USAGE_DESKTOP_SYSTEM_MENU_RIGHT                          = 0x8A, // RTC
  HID_USAGE_DESKTOP_SYSTEM_MENU_LEFT                           = 0x8B, // RTC
  HID_USAGE_DESKTOP_SYSTEM_MENU_UP                             = 0x8C, // RTC
  HID_USAGE_DESKTOP_SYSTEM_MENU_DOWN                           = 0x8D, // RTC
  HID_USAGE_DESKTOP_SYSTEM_COLD_RESTART                        = 0x8E, // OSC
  HID_USAGE_DESKTOP_SYSTEM_WARM_RESTART                        = 0x8F, // OSC
  HID_USAGE_DESKTOP_DPAD_UP                                    = 0x90, // OOC
  HID_USAGE_DESKTOP_DPAD_DOWN                                  = 0x91, // OOC
  HID_USAGE_DESKTOP_DPAD_RIGHT                                 = 0x92, // OOC
  HID_USAGE_DESKTOP_DPAD_LEFT                                  = 0x93, // OOC
  HID_USAGE_DESKTOP_INDEX_TRIGGER                              = 0x94, // MC/DV
  HID_USAGE_DESKTOP_PALM_TRIGGER                               = 0x95, // MC/DV
  HID_USAGE_DESKTOP_THUMBSTICK                                 = 0x96, // CP
  HID_USAGE_DESKTOP_SYSTEM_FUNCTION_SHIFT                      = 0x97, // MC
  HID_USAGE_DESKTOP_SYSTEM_FUNCTION_SHIFT_LOCK                 = 0x98, // OOC
  HID_USAGE_DESKTOP_SYSTEM_FUNCTION_SHIFT_LOCK_INDICATOR       = 0x99, // DV
  HID_USAGE_DESKTOP_SYSTEM_DISMISS_NOTIFICATION                = 0x9A, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DO_NOT_DISTURB                      = 0x9B, // OOC
  // 9C-9F Reserved

  HID_USAGE_DESKTOP_SYSTEM_DOCK                                = 0xA0, // OSC
  HID_USAGE_DESKTOP_SYSTEM_UNDOCK                              = 0xA1, // OSC
  HID_USAGE_DESKTOP_SYSTEM_SETUP                               = 0xA2, // OSC
  HID_USAGE_DESKTOP_SYSTEM_BREAK                               = 0xA3, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DEBUGGER_BREAK                      = 0xA4, // OSC
  HID_USAGE_DESKTOP_APPLICATION_BREAK                          = 0xA5, // OSC
  HID_USAGE_DESKTOP_APPLICATION_DEBUGGER_BREAK                 = 0xA6, // OSC
  HID_USAGE_DESKTOP_SYSTEM_SPEAKER_MUTE                        = 0xA7, // OSC
  HID_USAGE_DESKTOP_SYSTEM_HIBERNATE                           = 0xA8, // OSC
  HID_USAGE_DESKTOP_SYSTEM_MICROPHONE_MUTE                     = 0xA9, // OOC
  HID_USAGE_DESKTOP_SYSTEM_ACCESSIBILITY_BINDING               = 0xAA, // OOC
  // AB-AF Reserved

  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_INVERT                      = 0xB0, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_INTERNAL                    = 0xB1, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_EXTERNAL                    = 0xB2, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_BOTH                        = 0xB3, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_DUAL                        = 0xB4, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_TOGGLE_INT_EXT              = 0xB5, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_SWAP_PRIMARY_SECONDARY      = 0xB6, // OSC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_LCD_AUTOSCALE               = 0xB7, // OSC
  // B8-BF Reserved

  HID_USAGE_DESKTOP_SENSOR_ZONE                                = 0xC0, // CL
  HID_USAGE_DESKTOP_RPM                                        = 0xC1, // DV
  HID_USAGE_DESKTOP_COOLANT_LEVEL                              = 0xC2, // DV
  HID_USAGE_DESKTOP_COOLANT_CRITICAL_LEVEL                     = 0xC3, // SV
  HID_USAGE_DESKTOP_COOLANT_PUMP                               = 0xC4, // US
  HID_USAGE_DESKTOP_CHASSIS_ENCLOSURE                          = 0xC5, // CL
  HID_USAGE_DESKTOP_WIRELESS_RADIO_BUTTON                      = 0xC6, // OOC
  HID_USAGE_DESKTOP_WIRELESS_RADIO_LED                         = 0xC7, // OOC
  HID_USAGE_DESKTOP_WIRELESS_RADIO_SLIDER_SWITCH               = 0xC8, // OOC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_ROTATION_LOCK_BUTTON        = 0xC9, // OOC
  HID_USAGE_DESKTOP_SYSTEM_DISPLAY_ROTATION_LOCK_SLIDER_SWITCH = 0xCA, // OOC
  HID_USAGE_DESKTOP_CONTROL_ENABLE                             = 0xCB, // DF
  // CC-CF Reserved

  HID_USAGE_DESKTOP_DOCKABLE_DEVICE_UNIQUE_ID                  = 0xD0, // DV
  HID_USAGE_DESKTOP_DOCKABLE_DEVICE_VENDOR_ID                  = 0xD1, // DV
  HID_USAGE_DESKTOP_DOCKABLE_DEVICE_PRIMARY_USAGE_PAGE         = 0xD2, // DV
  HID_USAGE_DESKTOP_DOCKABLE_DEVICE_PRIMARY_USAGE_ID           = 0xD3, // DV
  HID_USAGE_DESKTOP_DOCKABLE_DEVICE_DOCKING_STATE              = 0xD4, // DF
  HID_USAGE_DESKTOP_DOCKABLE_DEVICE_DISPLAY_OCCLUSION          = 0xD5, // CL
  HID_USAGE_DESKTOP_DOCKABLE_DEVICE_OBJECT_TYPE                = 0xD6, // DV
  // D7-DF Reserved

  HID_USAGE_DESKTOP_CALL_ACTIVE_LED                            = 0xE0, // OOC
  HID_USAGE_DESKTOP_CALL_MUTE_TOGGLE                           = 0xE1, // OSC
  HID_USAGE_DESKTOP_CALL_MUTE_LED                              = 0xE2  // OOC
  // E3-FFFF Reserved
};

/// HID Usage Table: Simulation Controls Page (0x02)
enum {
  HID_USAGE_SIMULATION_CONTROLS_FLIGHT_SIMULATION_DEVICE       = 0x01, // CA
  HID_USAGE_SIMULATION_CONTROLS_AUTOMOBILE_SIMULATION_DEVICE   = 0x02, // CA
  HID_USAGE_SIMULATION_CONTROLS_TANK_SIMULATION_DEVICE         = 0x03, // CA
  HID_USAGE_SIMULATION_CONTROLS_SPACESHIP_SIMULATION_DEVICE    = 0x04, // CA
  HID_USAGE_SIMULATION_CONTROLS_SUBMARINE_SIMULATION_DEVICE    = 0x05, // CA
  HID_USAGE_SIMULATION_CONTROLS_SAILING_SIMULATION_DEVICE      = 0x06, // CA
  HID_USAGE_SIMULATION_CONTROLS_MOTORCYCLE_SIMULATION_DEVICE   = 0x07, // CA
  HID_USAGE_SIMULATION_CONTROLS_SPORTS_SIMULATION_DEVICE       = 0x08, // CA
  HID_USAGE_SIMULATION_CONTROLS_AIRPLANE_SIMULATION_DEVICE     = 0x09, // CA
  HID_USAGE_SIMULATION_CONTROLS_HELICOPTER_SIMULATION_DEVICE   = 0x0A, // CA
  HID_USAGE_SIMULATION_CONTROLS_MAGIC_CARPET_SIMULATION_DEVICE = 0x0B, // CA
  HID_USAGE_SIMULATION_CONTROLS_BICYCLE_SIMULATION_DEVICE      = 0x0C, // CA
  // 0D-1F Reserved

  HID_USAGE_SIMULATION_CONTROLS_FLIGHT_CONTROL_STICK           = 0x20, // CA
  HID_USAGE_SIMULATION_CONTROLS_FLIGHT_STICK                   = 0x21, // CA
  HID_USAGE_SIMULATION_CONTROLS_CYCLIC_CONTROL                 = 0x22, // CP
  HID_USAGE_SIMULATION_CONTROLS_CYCLIC_TRIM                    = 0x23, // CP
  HID_USAGE_SIMULATION_CONTROLS_FLIGHT_YOKE                    = 0x24, // CA
  HID_USAGE_SIMULATION_CONTROLS_TRACK_CONTROL                  = 0x25, // CP
  // 26-AF Reserved

  HID_USAGE_SIMULATION_CONTROLS_AILERON                        = 0xB0, // DV
  HID_USAGE_SIMULATION_CONTROLS_AILERON_TRIM                   = 0xB1, // DV
  HID_USAGE_SIMULATION_CONTROLS_ANTI_TORQUE_CONTROL            = 0xB2, // DV
  HID_USAGE_SIMULATION_CONTROLS_AUTOPILOT_ENABLE               = 0xB3, // OOC
  HID_USAGE_SIMULATION_CONTROLS_CHAFF_RELEASE                  = 0xB4, // OSC
  HID_USAGE_SIMULATION_CONTROLS_COLLECTIVE_CONTROL             = 0xB5, // DV
  HID_USAGE_SIMULATION_CONTROLS_DIVE_BRAKE                     = 0xB6, // DV
  HID_USAGE_SIMULATION_CONTROLS_ELECTRONIC_COUNTERMEASURES     = 0xB7, // OOC
  HID_USAGE_SIMULATION_CONTROLS_ELEVATOR                       = 0xB8, // DV
  HID_USAGE_SIMULATION_CONTROLS_ELEVATOR_TRIM                  = 0xB9, // DV
  HID_USAGE_SIMULATION_CONTROLS_RUDDER                         = 0xBA, // DV
  HID_USAGE_SIMULATION_CONTROLS_THROTTLE                       = 0xBB, // DV
  HID_USAGE_SIMULATION_CONTROLS_FLIGHT_COMMUNICATIONS          = 0xBC, // OOC
  HID_USAGE_SIMULATION_CONTROLS_FLARE_RELEASE                  = 0xBD, // OSC
  HID_USAGE_SIMULATION_CONTROLS_LANDING_GEAR                   = 0xBE, // OOC
  HID_USAGE_SIMULATION_CONTROLS_TOE_BRAKE                      = 0xBF, // DV
  HID_USAGE_SIMULATION_CONTROLS_TRIGGER                        = 0xC0, // MC
  HID_USAGE_SIMULATION_CONTROLS_WEAPONS_ARM                    = 0xC1, // OOC
  HID_USAGE_SIMULATION_CONTROLS_WEAPONS_SELECT                 = 0xC2, // OSC
  HID_USAGE_SIMULATION_CONTROLS_WING_FLAPS                     = 0xC3, // DV
  HID_USAGE_SIMULATION_CONTROLS_ACCELERATOR                    = 0xC4, // DV
  HID_USAGE_SIMULATION_CONTROLS_BRAKE                          = 0xC5, // DV
  HID_USAGE_SIMULATION_CONTROLS_CLUTCH                         = 0xC6, // DV
  HID_USAGE_SIMULATION_CONTROLS_SHIFTER                        = 0xC7, // DV
  HID_USAGE_SIMULATION_CONTROLS_STEERING                       = 0xC8, // DV
  HID_USAGE_SIMULATION_CONTROLS_TURRET_DIRECTION               = 0xC9, // DV
  HID_USAGE_SIMULATION_CONTROLS_BARREL_ELEVATION               = 0xCA, // DV
  HID_USAGE_SIMULATION_CONTROLS_DIVE_PLANE                     = 0xCB, // DV
  HID_USAGE_SIMULATION_CONTROLS_BALLAST                        = 0xCC, // DV
  HID_USAGE_SIMULATION_CONTROLS_BICYCLE_CRANK                  = 0xCD, // DV
  HID_USAGE_SIMULATION_CONTROLS_HANDLE_BARS                    = 0xCE, // DV
  HID_USAGE_SIMULATION_CONTROLS_FRONT_BRAKE                    = 0xCF, // DV
  HID_USAGE_SIMULATION_CONTROLS_REAR_BRAKE                     = 0xD0, // DV
  // D1-FFFF Reserved
};

/// HID Usage Table: VR Controls Page (0x03)
enum {
  HID_USAGE_VR_CONTROLS_BELT                 = 0x01, // CA
  HID_USAGE_VR_CONTROLS_BODY_SUIT            = 0x02, // CA
  HID_USAGE_VR_CONTROLS_FLEXOR               = 0x03, // CP
  HID_USAGE_VR_CONTROLS_GLOVE                = 0x04, // CA
  HID_USAGE_VR_CONTROLS_HEAD_TRACKER         = 0x05, // CP
  HID_USAGE_VR_CONTROLS_HEAD_MOUNTED_DISPLAY = 0x06, // CA
  HID_USAGE_VR_CONTROLS_HAND_TRACKER         = 0x07, // CA
  HID_USAGE_VR_CONTROLS_OCULOMETER           = 0x08, // CA
  HID_USAGE_VR_CONTROLS_VEST                 = 0x09, // CA
  HID_USAGE_VR_CONTROLS_ANIMATRONIC_DEVICE   = 0x0A, // CA
  // 0B-1F Reserved

  HID_USAGE_VR_CONTROLS_STEREO_ENABLE        = 0x20, // OOC
  HID_USAGE_VR_CONTROLS_DISPLAY_ENABLE       = 0x21  // OOC
  // 22-FFFF Reserved
};

/// HID Usage Table: Sports Controls Page (0x04)
enum {
  HID_USAGE_SPORTS_CONTROLS_BASEBALL_BAT         = 0x01, // CA
  HID_USAGE_SPORTS_CONTROLS_GOLF_CLUB            = 0x02, // CA
  HID_USAGE_SPORTS_CONTROLS_ROWING_MACHINE       = 0x03, // CA
  HID_USAGE_SPORTS_CONTROLS_TREADMILL            = 0x04, // CA
  // 05-2F Reserved

  HID_USAGE_SPORTS_CONTROLS_OAR                  = 0x30, // DV
  HID_USAGE_SPORTS_CONTROLS_SLOPE                = 0x31, // DV
  HID_USAGE_SPORTS_CONTROLS_RATE                 = 0x32, // DV
  HID_USAGE_SPORTS_CONTROLS_STICK_SPEED          = 0x33, // DV
  HID_USAGE_SPORTS_CONTROLS_STICK_FACE_ANGLE     = 0x34, // DV
  HID_USAGE_SPORTS_CONTROLS_STICK_HEEL_TOE       = 0x35, // DV
  HID_USAGE_SPORTS_CONTROLS_STICK_FOLLOW_THROUGH = 0x36, // DV
  HID_USAGE_SPORTS_CONTROLS_STICK_TEMPO          = 0x37, // DV
  HID_USAGE_SPORTS_CONTROLS_STICK_TYPE           = 0x38, // NAry
  HID_USAGE_SPORTS_CONTROLS_STICK_HEIGHT         = 0x39, // DV
  // 3A-4F Reserved

  HID_USAGE_SPORTS_CONTROLS_PUTTER               = 0x50, // Sel
  HID_USAGE_SPORTS_CONTROLS_1_IRON               = 0x51, // Sel
  HID_USAGE_SPORTS_CONTROLS_2_IRON               = 0x52, // Sel
  HID_USAGE_SPORTS_CONTROLS_3_IRON               = 0x53, // Sel
  HID_USAGE_SPORTS_CONTROLS_4_IRON               = 0x54, // Sel
  HID_USAGE_SPORTS_CONTROLS_5_IRON               = 0x55, // Sel
  HID_USAGE_SPORTS_CONTROLS_6_IRON               = 0x56, // Sel
  HID_USAGE_SPORTS_CONTROLS_7_IRON               = 0x57, // Sel
  HID_USAGE_SPORTS_CONTROLS_8_IRON               = 0x58, // Sel
  HID_USAGE_SPORTS_CONTROLS_9_IRON               = 0x59, // Sel
  HID_USAGE_SPORTS_CONTROLS_10_IRON              = 0x5A, // Sel
  HID_USAGE_SPORTS_CONTROLS_11_IRON              = 0x5B, // Sel
  HID_USAGE_SPORTS_CONTROLS_SAND_WEDGE           = 0x5C, // Sel
  HID_USAGE_SPORTS_CONTROLS_LOFT_WEDGE           = 0x5D, // Sel
  HID_USAGE_SPORTS_CONTROLS_POWER_WEDGE          = 0x5E, // Sel
  HID_USAGE_SPORTS_CONTROLS_1_WOOD               = 0x5F, // Sel
  HID_USAGE_SPORTS_CONTROLS_3_WOOD               = 0x60, // Sel
  HID_USAGE_SPORTS_CONTROLS_5_WOOD               = 0x61, // Sel
  HID_USAGE_SPORTS_CONTROLS_7_WOOD               = 0x62, // Sel
  HID_USAGE_SPORTS_CONTROLS_9_WOOD               = 0x63, // Sel
  // 64-FFFF Reserved
};

/// HID Usage Table: Game Controls Page (0x05)
enum {
  HID_USAGE_GAME_CONTROLS_3D_GAME_CONTROLLER     = 0x01, // CA
  HID_USAGE_GAME_CONTROLS_PINBALL_DEVICE         = 0x02, // CA
  HID_USAGE_GAME_CONTROLS_GUN_DEVICE             = 0x03, // CA
  // 04-1F Reserved

  HID_USAGE_GAME_CONTROLS_POINT_OF_VIEW          = 0x20, // CP
  HID_USAGE_GAME_CONTROLS_TURN_RIGHT_LEFT        = 0x21, // DV
  HID_USAGE_GAME_CONTROLS_PITCH_FORWARD_BACKWARD = 0x22, // DV
  HID_USAGE_GAME_CONTROLS_ROLL_RIGHT_LEFT        = 0x23, // DV
  HID_USAGE_GAME_CONTROLS_MOVE_RIGHT_LEFT        = 0x24, // DV
  HID_USAGE_GAME_CONTROLS_MOVE_FORWARD_BACKWARD  = 0x25, // DV
  HID_USAGE_GAME_CONTROLS_MOVE_UP_DOWN           = 0x26, // DV
  HID_USAGE_GAME_CONTROLS_LEAN_RIGHT_LEFT        = 0x27, // DV
  HID_USAGE_GAME_CONTROLS_LEAN_FORWARD_BACKWARD  = 0x28, // DV
  HID_USAGE_GAME_CONTROLS_HEIGHT_OF_POV          = 0x29, // DV
  HID_USAGE_GAME_CONTROLS_FLIPPER                = 0x2A, // MC
  HID_USAGE_GAME_CONTROLS_SECONDARY_FLIPPER      = 0x2B, // MC
  HID_USAGE_GAME_CONTROLS_BUMP                   = 0x2C, // MC
  HID_USAGE_GAME_CONTROLS_NEW_GAME               = 0x2D, // OSC
  HID_USAGE_GAME_CONTROLS_SHOOT_BALL             = 0x2E, // OSC
  HID_USAGE_GAME_CONTROLS_PLAYER                 = 0x2F, // OSC
  HID_USAGE_GAME_CONTROLS_GUN_BOLT               = 0x30, // OOC
  HID_USAGE_GAME_CONTROLS_GUN_CLIP               = 0x31, // OOC
  HID_USAGE_GAME_CONTROLS_GUN_SELECTOR           = 0x32, // NAry
  HID_USAGE_GAME_CONTROLS_GUN_SINGLE_SHOT        = 0x33, // Sel
  HID_USAGE_GAME_CONTROLS_GUN_BURST              = 0x34, // Sel
  HID_USAGE_GAME_CONTROLS_GUN_AUTOMATIC          = 0x35, // Sel
  HID_USAGE_GAME_CONTROLS_GUN_SAFETY             = 0x36, // OOC
  HID_USAGE_GAME_CONTROLS_GAMEPAD_FIRE_JUMP      = 0x37, // CL
  // 38 Reserved

  HID_USAGE_GAME_CONTROLS_GAMEPAD_TRIGGER        = 0x39, // CL
  HID_USAGE_GAME_CONTROLS_FORM_FITTING_GAMEPAD   = 0x3A  // SF
  // 3B-FFFF Reserved
};

/// HID Usage Table: Generic Device Controls Page (0x06)
enum {
  HID_USAGE_GENERIC_DEVICE_CONTROLS_BACKGROUND_NONUSER_CONTROLS     = 0x01, // CA
  // 02-1F Reserved

  HID_USAGE_GENERIC_DEVICE_CONTROLS_BATTERY_STRENGTH                = 0x20, // DV
  HID_USAGE_GENERIC_DEVICE_CONTROLS_WIRELESS_CHANNEL                = 0x21, // DV
  HID_USAGE_GENERIC_DEVICE_CONTROLS_WIRELESS_ID                     = 0x22, // DV
  HID_USAGE_GENERIC_DEVICE_CONTROLS_DISCOVER_WIRELESS_CONTROL       = 0x23, // OSC
  HID_USAGE_GENERIC_DEVICE_CONTROLS_SECURITY_CODE_CHARACTER_ENTERED = 0x24, // OSC
  HID_USAGE_GENERIC_DEVICE_CONTROLS_SECURITY_CODE_CHARACTER_ERASED  = 0x25, // OSC
  HID_USAGE_GENERIC_DEVICE_CONTROLS_SECURITY_CODE_CLEARED           = 0x26, // OSC
  HID_USAGE_GENERIC_DEVICE_CONTROLS_SEQUENCE_ID                     = 0x27, // DV
  HID_USAGE_GENERIC_DEVICE_CONTROLS_SEQUENCE_ID_RESET               = 0x28, // DF
  HID_USAGE_GENERIC_DEVICE_CONTROLS_RF_SIGNAL_STRENGTH              = 0x29, // DV
  HID_USAGE_GENERIC_DEVICE_CONTROLS_SOFTWARE_VERSION                = 0x2A, // CL
  HID_USAGE_GENERIC_DEVICE_CONTROLS_PROTOCOL_VERSION                = 0x2B, // CL
  HID_USAGE_GENERIC_DEVICE_CONTROLS_HARDWARE_VERSION                = 0x2C, // CL
  HID_USAGE_GENERIC_DEVICE_CONTROLS_MAJOR                           = 0x2D, // SV
  HID_USAGE_GENERIC_DEVICE_CONTROLS_MINOR                           = 0x2E, // SV
  HID_USAGE_GENERIC_DEVICE_CONTROLS_REVISION                        = 0x2F, // SV
  HID_USAGE_GENERIC_DEVICE_CONTROLS_HANDEDNESS                      = 0x30, // NAry
  HID_USAGE_GENERIC_DEVICE_CONTROLS_EITHER_HAND                     = 0x31, // Sel
  HID_USAGE_GENERIC_DEVICE_CONTROLS_LEFT_HAND                       = 0x32, // Sel
  HID_USAGE_GENERIC_DEVICE_CONTROLS_RIGHT_HAND                      = 0x33, // Sel
  HID_USAGE_GENERIC_DEVICE_CONTROLS_BOTH_HANDS                      = 0x34, // Sel
  // 35-3F Reserved

  HID_USAGE_GENERIC_DEVICE_CONTROLS_GRIP_POSE_OFFSET                = 0x40, // CP
  HID_USAGE_GENERIC_DEVICE_CONTROLS_POINTER_POSE_OFFSET             = 0x41  // CP
  // 42-FFFF Reserved
};

/// HID Usage Table: Keyboard/Keypad Page (0x07)
/// Defined above

/// HID Usage Table: LED Page (0x08)
enum {
  HID_USAGE_LED_NUM_LOCK                   = 0x01, // OOC
  HID_USAGE_LED_CAPS_LOCK                  = 0x02, // OOC
  HID_USAGE_LED_SCROLL_LOCK                = 0x03, // OOC
  HID_USAGE_LED_COMPOSE                    = 0x04, // OOC
  HID_USAGE_LED_KANA                       = 0x05, // OOC
  HID_USAGE_LED_POWER                      = 0x06, // OOC
  HID_USAGE_LED_SHIFT                      = 0x07, // OOC
  HID_USAGE_LED_DO_NOT_SHIFT               = 0x08, // OOC
  HID_USAGE_LED_MUTE                       = 0x09, // OOC
  HID_USAGE_LED_TONE_ENABLE                = 0x0A, // OOC
  HID_USAGE_LED_HIGH_CUT_FILTER            = 0x0B, // OOC
  HID_USAGE_LED_LOW_CUT_FILTER             = 0x0C, // OOC
  HID_USAGE_LED_EQUALIZER_ENABLE           = 0x0D, // OOC
  HID_USAGE_LED_SOUND_FIELD_ON             = 0x0E, // OOC
  HID_USAGE_LED_SURROUND_ON                = 0x0F, // OOC
  HID_USAGE_LED_REPEAT                     = 0x10, // OOC
  HID_USAGE_LED_STEREO                     = 0x11, // OOC
  HID_USAGE_LED_SAMPLING_RATE_DETECT       = 0x12, // OOC
  HID_USAGE_LED_SPINNING                   = 0x13, // OOC
  HID_USAGE_LED_CAV                        = 0x14, // OOC
  HID_USAGE_LED_CLV                        = 0x15, // OOC
  HID_USAGE_LED_RECORDING_FORMAT_DETECT    = 0x16, // OOC
  HID_USAGE_LED_OFF_HOOK                   = 0x17, // OOC
  HID_USAGE_LED_RING                       = 0x18, // OOC
  HID_USAGE_LED_MESSAGE_WAITING            = 0x19, // OOC
  HID_USAGE_LED_DATA_MODE                  = 0x1A, // OOC
  HID_USAGE_LED_BATTERY_OPERATION          = 0x1B, // OOC
  HID_USAGE_LED_BATTERY_OK                 = 0x1C, // OOC
  HID_USAGE_LED_BATTERY_LOW                = 0x1D, // OOC
  HID_USAGE_LED_SPEAKER                    = 0x1E, // OOC
  HID_USAGE_LED_HEADSET                    = 0x1F, // OOC
  HID_USAGE_LED_HOLD                       = 0x20, // OOC
  HID_USAGE_LED_MICROPHONE                 = 0x21, // OOC
  HID_USAGE_LED_COVERAGE                   = 0x22, // OOC
  HID_USAGE_LED_NIGHT_MODE                 = 0x23, // OOC
  HID_USAGE_LED_SEND_CALLS                 = 0x24, // OOC
  HID_USAGE_LED_CALL_PICKUPS               = 0x25, // OOC
  HID_USAGE_LED_CONFERENCE                 = 0x26, // OOC
  HID_USAGE_LED_STANDBY                    = 0x27, // OOC
  HID_USAGE_LED_CAMERA_ON                  = 0x28, // OOC
  HID_USAGE_LED_CAMERA_OFF                 = 0x29, // OOC
  HID_USAGE_LED_ON_LINE                    = 0x2A, // OOC
  HID_USAGE_LED_OFF_LINE                   = 0x2B, // OOC
  HID_USAGE_LED_BUSY                       = 0x2C, // OOC
  HID_USAGE_LED_READY                      = 0x2D, // OOC
  HID_USAGE_LED_PAPER_OUT                  = 0x2E, // OOC
  HID_USAGE_LED_PAPER_JAM                  = 0x2F, // OOC
  HID_USAGE_LED_REMOTE                     = 0x30, // OOC
  HID_USAGE_LED_FORWARD                    = 0x31, // OOC
  HID_USAGE_LED_REVERSE                    = 0x32, // OOC
  HID_USAGE_LED_STOP                       = 0x33, // OOC
  HID_USAGE_LED_REWIND                     = 0x34, // OOC
  HID_USAGE_LED_FAST_FORWARD               = 0x35, // OOC
  HID_USAGE_LED_PLAY                       = 0x36, // OOC
  HID_USAGE_LED_PAUSE                      = 0x37, // OOC
  HID_USAGE_LED_RECORD                     = 0x38, // OOC
  HID_USAGE_LED_ERROR                      = 0x39, // OOC
  HID_USAGE_LED_USAGE_SELECTED_INDICATOR   = 0x3A, // US
  HID_USAGE_LED_USAGE_IN_USE_INDICATOR     = 0x3B, // US
  HID_USAGE_LED_USAGE_MULTI_MODE_INDICATOR = 0x3C, // UM
  HID_USAGE_LED_INDICATOR_ON               = 0x3D, // Sel
  HID_USAGE_LED_INDICATOR_FLASH            = 0x3E, // Sel
  HID_USAGE_LED_INDICATOR_SLOW_BLINK       = 0x3F, // Sel
  HID_USAGE_LED_INDICATOR_FAST_BLINK       = 0x40, // Sel
  HID_USAGE_LED_INDICATOR_OFF              = 0x41, // Sel
  HID_USAGE_LED_FLASH_ON_TIME              = 0x42, // DV
  HID_USAGE_LED_SLOW_BLINK_ON_TIME         = 0x43, // DV
  HID_USAGE_LED_SLOW_BLINK_OFF_TIME        = 0x44, // DV
  HID_USAGE_LED_FAST_BLINK_ON_TIME         = 0x45, // DV
  HID_USAGE_LED_FAST_BLINK_OFF_TIME        = 0x46, // DV
  HID_USAGE_LED_USAGE_INDICATOR_COLOR      = 0x47, // UM
  HID_USAGE_LED_INDICATOR_RED              = 0x48, // Sel
  HID_USAGE_LED_INDICATOR_GREEN            = 0x49, // Sel
  HID_USAGE_LED_INDICATOR_AMBER            = 0x4A, // Sel
  HID_USAGE_LED_GENERIC_INDICATOR          = 0x4B, // OOC
  HID_USAGE_LED_SYSTEM_SUSPEND             = 0x4C, // OOC
  HID_USAGE_LED_EXTERNAL_POWER_CONNECTED   = 0x4D, // OOC
  HID_USAGE_LED_INDICATOR_BLUE             = 0x4E, // Sel
  HID_USAGE_LED_INDICATOR_ORANGE           = 0x4F, // Sel
  HID_USAGE_LED_GOOD_STATUS                = 0x50, // OOC
  HID_USAGE_LED_WARNING_STATUS             = 0x51, // OOC
  HID_USAGE_LED_RGB_LED                    = 0x52, // CL
  HID_USAGE_LED_RED_LED_CHANNEL            = 0x53, // DV
  HID_USAGE_LED_BLUE_LED_CHANNEL           = 0x54, // DV
  HID_USAGE_LED_GREEN_LED_CHANNEL          = 0x55, // DV
  HID_USAGE_LED_LED_INTENSITY              = 0x56, // DV
  HID_USAGE_LED_SYSTEM_MICROPHONE_MUTE     = 0x57, // OOC
  // 58-5F Reserved

  HID_USAGE_LED_PLAYER_INDICATOR           = 0x60, // NAry
  HID_USAGE_LED_PLAYER_1                   = 0x61, // Sel
  HID_USAGE_LED_PLAYER_2                   = 0x62, // Sel
  HID_USAGE_LED_PLAYER_3                   = 0x63, // Sel
  HID_USAGE_LED_PLAYER_4                   = 0x64, // Sel
  HID_USAGE_LED_PLAYER_5                   = 0x65, // Sel
  HID_USAGE_LED_PLAYER_6                   = 0x66, // Sel
  HID_USAGE_LED_PLAYER_7                   = 0x67, // Sel
  HID_USAGE_LED_PLAYER_8                   = 0x68, // Sel
  // 69-FFFF Reserved
};

/// HID Usage Table: Button Page (0x09)
/// Intentionally skipped

/// HID Usage Table: Ordinal Page (0x0A)
/// Intentionally skipped

/// HID Usage Table: Telephony Device Page (0x0B)
enum {
  HID_USAGE_TELEPHONY_PHONE                       = 0x0001, // CA
  HID_USAGE_TELEPHONY_ANSWERING_MACHINE           = 0x0002, // CA
  HID_USAGE_TELEPHONY_MESSAGE_CONTROLS            = 0x0003, // CL
  HID_USAGE_TELEPHONY_HANDSET                     = 0x0004, // CL
  HID_USAGE_TELEPHONY_HEADSET                     = 0x0005, // CL/CA
  HID_USAGE_TELEPHONY_TELEPHONY_KEY_PAD           = 0x0006, // NAry
  HID_USAGE_TELEPHONY_PROGRAMMABLE_BUTTON         = 0x0007, // NAry
  // 08-1F Reserved

  HID_USAGE_TELEPHONY_HOOK_SWITCH                 = 0x0020, // OOC
  HID_USAGE_TELEPHONY_FLASH                       = 0x0021, // MC
  HID_USAGE_TELEPHONY_FEATURE                     = 0x0022, // OSC
  HID_USAGE_TELEPHONY_HOLD                        = 0x0023, // OOC
  HID_USAGE_TELEPHONY_REDIAL                      = 0x0024, // OSC
  HID_USAGE_TELEPHONY_TRANSFER                    = 0x0025, // OSC
  HID_USAGE_TELEPHONY_DROP                        = 0x0026, // OSC
  HID_USAGE_TELEPHONY_PARK                        = 0x0027, // OOC
  HID_USAGE_TELEPHONY_FORWARD_CALLS               = 0x0028, // OOC
  HID_USAGE_TELEPHONY_ALTERNATE_FUNCTION          = 0x0029, // MC
  HID_USAGE_TELEPHONY_LINE                        = 0x002A, // OSC/NAry
  HID_USAGE_TELEPHONY_SPEAKER_PHONE               = 0x002B, // OOC
  HID_USAGE_TELEPHONY_CONFERENCE                  = 0x002C, // OOC
  HID_USAGE_TELEPHONY_RING_ENABLE                 = 0x002D, // OOC
  HID_USAGE_TELEPHONY_RING_SELECT                 = 0x002E, // OSC
  HID_USAGE_TELEPHONY_PHONE_MUTE                  = 0x002F, // OOC
  HID_USAGE_TELEPHONY_CALLER_ID                   = 0x0030, // MC
  HID_USAGE_TELEPHONY_SEND                        = 0x0031, // OOC
  // 32-4F Reserved

  HID_USAGE_TELEPHONY_SPEED_DIAL                  = 0x0050, // OSC
  HID_USAGE_TELEPHONY_STORE_NUMBER                = 0x0051, // OSC
  HID_USAGE_TELEPHONY_RECALL_NUMBER               = 0x0052, // OSC
  HID_USAGE_TELEPHONY_PHONE_DIRECTORY             = 0x0053, // OOC
  // 54-6F Reserved

  HID_USAGE_TELEPHONY_VOICE_MAIL                  = 0x0070, // OOC
  HID_USAGE_TELEPHONY_SCREEN_CALLS                = 0x0071, // OOC
  HID_USAGE_TELEPHONY_DO_NOT_DISTURB              = 0x0072, // OOC
  HID_USAGE_TELEPHONY_MESSAGE                     = 0x0073, // OSC
  HID_USAGE_TELEPHONY_ANSWER_ON_OFF               = 0x0074, // OOC
  // 75-8F Reserved

  HID_USAGE_TELEPHONY_INSIDE_DIAL_TONE            = 0x0090, // MC
  HID_USAGE_TELEPHONY_OUTSIDE_DIAL_TONE           = 0x0091, // MC
  HID_USAGE_TELEPHONY_INSIDE_RING_TONE            = 0x0092, // MC
  HID_USAGE_TELEPHONY_OUTSIDE_RING_TONE           = 0x0093, // MC
  HID_USAGE_TELEPHONY_PRIORITY_RING_TONE          = 0x0094, // MC
  HID_USAGE_TELEPHONY_INSIDE_RINGBACK             = 0x0095, // MC
  HID_USAGE_TELEPHONY_PRIORITY_RINGBACK           = 0x0096, // MC
  HID_USAGE_TELEPHONY_LINE_BUSY_TONE              = 0x0097, // MC
  HID_USAGE_TELEPHONY_REORDER_TONE                = 0x0098, // MC
  HID_USAGE_TELEPHONY_CALL_WAITING_TONE           = 0x0099, // MC
  HID_USAGE_TELEPHONY_CONFIRMATION_TONE_1         = 0x009A, // MC
  HID_USAGE_TELEPHONY_CONFIRMATION_TONE_2         = 0x009B, // MC
  HID_USAGE_TELEPHONY_TONES_OFF                   = 0x009C, // OOC
  HID_USAGE_TELEPHONY_OUTSIDE_RINGBACK            = 0x009D, // MC
  HID_USAGE_TELEPHONY_RINGER                      = 0x009E, // OOC
  // 9F-AF Reserved

  HID_USAGE_TELEPHONY_PHONE_KEY_0                 = 0x00B0, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_1                 = 0x00B1, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_2                 = 0x00B2, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_3                 = 0x00B3, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_4                 = 0x00B4, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_5                 = 0x00B5, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_6                 = 0x00B6, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_7                 = 0x00B7, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_8                 = 0x00B8, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_9                 = 0x00B9, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_STAR              = 0x00BA, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_POUND             = 0x00BB, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_A                 = 0x00BC, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_B                 = 0x00BD, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_C                 = 0x00BE, // Sel
  HID_USAGE_TELEPHONY_PHONE_KEY_D                 = 0x00BF, // Sel
  HID_USAGE_TELEPHONY_PHONE_CALL_HISTORY_KEY      = 0x00C0, // Sel
  HID_USAGE_TELEPHONY_PHONE_CALLER_ID_KEY         = 0x00C1, // Sel
  HID_USAGE_TELEPHONY_PHONE_SETTINGS_KEY          = 0x00C2, // Sel
  // C3-EF Reserved

  HID_USAGE_TELEPHONY_HOST_CONTROL                = 0x00F0, // OOC
  HID_USAGE_TELEPHONY_HOST_AVAILABLE              = 0x00F1, // OOC
  HID_USAGE_TELEPHONY_HOST_CALL_ACTIVE            = 0x00F2, // OOC
  HID_USAGE_TELEPHONY_ACTIVATE_HANDSET_AUDIO      = 0x00F3, // OOC
  HID_USAGE_TELEPHONY_RING_TYPE                   = 0x00F4, // NAry
  HID_USAGE_TELEPHONY_REDIALABLE_PHONE_NUMBER     = 0x00F5, // OOC
  // F6-F7 Reserved

  HID_USAGE_TELEPHONY_STOP_RING_TONE              = 0x00F8, // Sel
  HID_USAGE_TELEHONY_PSTN_RING_TONE               = 0x00F9, // Sel
  HID_USAGE_TELEPHONY_HOST_RING_TONE              = 0x00FA, // Sel
  HID_USAGE_TELEPHONY_ALERT_SOUND_ERROR           = 0x00FB, // Sel
  HID_USAGE_TELEPHONY_ALERT_SOUND_CONFIRM         = 0x00FC, // Sel
  HID_USAGE_TELEPHONY_ALERT_SOUND_NOTIFICATION    = 0x00FD, // Sel
  HID_USAGE_TELEPHONY_SILENT_RING                 = 0x00FE, // Sel
  // FF-107 Reserved

  HID_USAGE_TELEPHONY_EMAIL_MESSAGE_WAITING       = 0x0108, // OOC
  HID_USAGE_TELEPHONY_VOICEMAIL_MESSAGE_WAITING   = 0x0109, // OOC
  HID_USAGE_TELEPHONY_HOST_HOLD                   = 0x010A, // OOC
  // 10B-10F Reserved

  HID_USAGE_TELEPHONY_INCOMING_CALL_HISTORY_COUNT = 0x0110, // DV
  HID_USAGE_TELEPHONY_OUTGOING_CALL_HISTORY_COUNT = 0x0111, // DV
  HID_USAGE_TELEPHONY_INCOMING_CALL_HISTORY       = 0x0112, // CL
  HID_USAGE_TELEPHONY_OUTGOING_CALL_HISTORY       = 0x0113, // CL
  HID_USAGE_TELEPHONY_PHONE_LOCALE                = 0x0114, // DV
  // 115-13F Reserved

  HID_USAGE_TELEPHONY_PHONE_TIME_SECOND           = 0x0140, // DV
  HID_USAGE_TELEPHONY_PHONE_TIME_MINUTE           = 0x0141, // DV
  HID_USAGE_TELEPHONY_PHONE_TIME_HOUR             = 0x0142, // DV
  HID_USAGE_TELEPHONY_PHONE_DATE_DAY              = 0x0143, // DV
  HID_USAGE_TELEPHONY_PHONE_DATE_MONTH            = 0x0144, // DV
  HID_USAGE_TELEPHONY_PHONE_DATE_YEAR             = 0x0145, // DV
  HID_USAGE_TELEPHONY_HANDSET_NICKNAME            = 0x0146, // DV
  HID_USAGE_TELEPHONY_ADDRESS_BOOK_ID             = 0x0147, // DV
  // 148-149 Reserved

  HID_USAGE_TELEPHONY_CALL_DURATION               = 0x014A, // DV
  HID_USAGE_TELEPHONY_DUAL_MODE_PHONE             = 0x014B, // CA
  // 14C-FFFF Reserved
};

/// HID Usage Table: Consumer Page (0x0C)
enum {
  HID_USAGE_CONSUMER_UNASSIGNED                              = 0x0000,

  // Generic Control
  HID_USAGE_CONSUMER_CONTROL                                 = 0x0001, // CA
  HID_USAGE_CONSUMER_NUMERIC_KEY_PAD                         = 0x0002, // NAry
  HID_USAGE_CONSUMER_PROGRAMMABLE_BUTTONS                    = 0x0003, // NAry
  HID_USAGE_CONSUMER_MICROPHONE                              = 0x0004, // CA
  HID_USAGE_CONSUMER_HEADPHONE                               = 0x0005, // CA
  HID_USAGE_CONSUMER_GRAPHIC_EQUALIZER                       = 0x0006, // CA
  HID_USAGE_CONSUMER_KEYBOARD_BACKLIGHT                      = 0x0007, // CA
  // 08-1F Reserved

  HID_USAGE_CONSUMER_PLUS_10                                 = 0x0020, // OSC
  HID_USAGE_CONSUMER_PLUS_100                                = 0x0021, // OSC
  HID_USAGE_CONSUMER_AM_PM                                   = 0x0022, // OSC
  // 23-3F Reserved

  // Power Control
  HID_USAGE_CONSUMER_POWER                                   = 0x0030, // OOC
  HID_USAGE_CONSUMER_RESET                                   = 0x0031, // OSC
  HID_USAGE_CONSUMER_SLEEP                                   = 0x0032, // OSC

  HID_USAGE_CONSUMER_SLEEP_AFTER                             = 0x0033, // OSC
  HID_USAGE_CONSUMER_SLEEP_MODE                              = 0x0034, // RTC
  HID_USAGE_CONSUMER_ILLUMINATION                            = 0x0035, // OOC
  HID_USAGE_CONSUMER_FUNCTION_BUTTONS                        = 0x0036, // NAry
  // 37-3F Reserved
  HID_USAGE_CONSUMER_MENU                                    = 0x0040, // OOC
  HID_USAGE_CONSUMER_MENU_PICK                               = 0x0041, // OSC
  HID_USAGE_CONSUMER_MENU_UP                                 = 0x0042, // OSC
  HID_USAGE_CONSUMER_MENU_DOWN                               = 0x0043, // OSC
  HID_USAGE_CONSUMER_MENU_LEFT                               = 0x0044, // OSC
  HID_USAGE_CONSUMER_MENU_RIGHT                              = 0x0045, // OSC
  HID_USAGE_CONSUMER_MENU_ESCAPE                             = 0x0046, // OSC
  HID_USAGE_CONSUMER_MENU_VALUE_INCREASE                     = 0x0047, // OSC
  HID_USAGE_CONSUMER_MENU_VALUE_DECREASE                     = 0x0048, // OSC
  // 49-5F Reserved
  HID_USAGE_CONSUMER_DATA_ON_SCREEN                          = 0x0060, // OOC
  HID_USAGE_CONSUMER_CLOSED_CAPTION                          = 0x0061, // OOC
  HID_USAGE_CONSUMER_CLOSED_CAPTION_SELECT                   = 0x0062, // OSC
  HID_USAGE_CONSUMER_VCR_TV                                  = 0x0063, // OOC
  HID_USAGE_CONSUMER_BROADCAST_MODE                          = 0x0064, // OSC
  HID_USAGE_CONSUMER_SNAPSHOT                                = 0x0065, // OSC
  HID_USAGE_CONSUMER_STILL                                   = 0x0066, // OSC
  HID_USAGE_CONSUMER_PICTURE_IN_PICTURE_TOGGLE               = 0x0067, // OSC
  HID_USAGE_CONSUMER_PICTURE_IN_PICTURE_SWAP                 = 0x0068, // OSC
  HID_USAGE_CONSUMER_RED_MENU_BUTTON                         = 0x0069, // MC
  HID_USAGE_CONSUMER_GREEN_MENU_BUTTON                       = 0x006A, // MC
  HID_USAGE_CONSUMER_BLUE_MENU_BUTTON                        = 0x006B, // MC
  HID_USAGE_CONSUMER_YELLOW_MENU_BUTTON                      = 0x006C, // MC
  HID_USAGE_CONSUMER_ASPECT                                  = 0x006D, // OSC
  HID_USAGE_CONSUMER_3D_MODE_SELECT                          = 0x006E, // OSC
  HID_USAGE_CONSUMER_DISPLAY_BRIGHTNESS_INCREMENT            = 0x006F, // RTC
  HID_USAGE_CONSUMER_DISPLAY_BRIGHTNESS_DECREMENT            = 0x0070, // RTC
  HID_USAGE_CONSUMER_DISPLAY_BRIGHTNESS                      = 0x0071, // LC
  HID_USAGE_CONSUMER_DISPLAY_BACKLIGHT_TOGGLE                = 0x0072, // OOC
  HID_USAGE_CONSUMER_DISPLAY_SET_BRIGHTNESS_TO_MINIMUM       = 0x0073, // OSC
  HID_USAGE_CONSUMER_DISPLAY_SET_BRIGHTNESS_TO_MAXIMUM       = 0x0074, // OSC
  HID_USAGE_CONSUMER_DISPLAY_SET_AUTO_BRIGHTNESS             = 0x0075, // OOC
  HID_USAGE_CONSUMER_CAMERA_ACCESS_ENABLED                   = 0x0076, // OOC
  HID_USAGE_CONSUMER_CAMERA_ACCESS_DISABLED                  = 0x0077, // OOC
  HID_USAGE_CONSUMER_CAMERA_ACCESS_TOGGLE                    = 0x0078, // OOC
  HID_USAGE_CONSUMER_KEYBOARD_BRIGHTNESS_INCREMENT           = 0x0079, // OSC
  HID_USAGE_CONSUMER_KEYBOARD_BRIGHTNESS_DECREMENT           = 0x007A, // OSC
  HID_USAGE_CONSUMER_KEYBOARD_BACKLIGHT_SET_LEVEL            = 0x007B, // LC
  HID_USAGE_CONSUMER_KEYBOARD_BACKLIGHT_OOC                  = 0x007C, // OOC
  HID_USAGE_CONSUMER_KEYBOARD_BACKLIGHT_SET_MINIMUM          = 0x007D, // OSC
  HID_USAGE_CONSUMER_KEYBOARD_BACKLIGHT_SET_MAXIMUM          = 0x007E, // OSC
  HID_USAGE_CONSUMER_KEYBOARD_BACKLIGHT_AUTO                 = 0x007F, // OOC
  HID_USAGE_CONSUMER_SELECTION                               = 0x0080, // NAry
  HID_USAGE_CONSUMER_ASSIGN_SELECTION                        = 0x0081, // OSC
  HID_USAGE_CONSUMER_MODE_STEP                               = 0x0082, // OSC
  HID_USAGE_CONSUMER_RECALL_LAST                             = 0x0083, // OSC
  HID_USAGE_CONSUMER_ENTER_CHANNEL                           = 0x0084, // OSC
  HID_USAGE_CONSUMER_ORDER_MOVIE                             = 0x0085, // OSC
  HID_USAGE_CONSUMER_CHANNEL                                 = 0x0086, // LC
  HID_USAGE_CONSUMER_MEDIA_SELECTION                         = 0x0087, // NAry
  HID_USAGE_CONSUMER_MEDIA_SELECT_COMPUTER                   = 0x0088, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_TV                         = 0x0089, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_WWW                        = 0x008A, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_DVD                        = 0x008B, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_TELEPHONE                  = 0x008C, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_PROGRAM_GUIDE              = 0x008D, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_VIDEO_PHONE                = 0x008E, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_GAMES                      = 0x008F, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_MESSAGES                   = 0x0090, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_CD                         = 0x0091, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_VCR                        = 0x0092, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_TUNER                      = 0x0093, // Sel
  HID_USAGE_CONSUMER_QUIT                                    = 0x0094, // OSC
  HID_USAGE_CONSUMER_HELP                                    = 0x0095, // OOC
  HID_USAGE_CONSUMER_MEDIA_SELECT_TAPE                       = 0x0096, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_CABLE                      = 0x0097, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_SATELLITE                  = 0x0098, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_SECURITY                   = 0x0099, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_HOME                       = 0x009A, // Sel
  HID_USAGE_CONSUMER_MEDIA_SELECT_CALL                       = 0x009B, // Sel
  HID_USAGE_CONSUMER_CHANNEL_INCREMENT                       = 0x009C, // OSC
  HID_USAGE_CONSUMER_CHANNEL_DECREMENT                       = 0x009D, // OSC
  HID_USAGE_CONSUMER_MEDIA_SELECT_SAP                        = 0x009E, // Sel
  // 9F Reserved
  HID_USAGE_CONSUMER_VCR_PLUS                                = 0x00A0, // OSC
  HID_USAGE_CONSUMER_ONCE                                    = 0x00A1, // OSC
  HID_USAGE_CONSUMER_DAILY                                   = 0x00A2, // OSC
  HID_USAGE_CONSUMER_WEEKLY                                  = 0x00A3, // OSC
  HID_USAGE_CONSUMER_MONTHLY                                 = 0x00A4, // OSC
  // A5-AF Reserved

  HID_USAGE_CONSUMER_PLAY                                    = 0x00B0, // OOC
  HID_USAGE_CONSUMER_PAUSE                                   = 0x00B1, // OOC
  HID_USAGE_CONSUMER_RECORD                                  = 0x00B2, // OOC
  HID_USAGE_CONSUMER_FAST_FORWARD                            = 0x00B3, // OOC
  HID_USAGE_CONSUMER_REWIND                                  = 0x00B4, // OOC
  HID_USAGE_CONSUMER_SCAN_NEXT_TRACK                         = 0x00B5, // OSC
  HID_USAGE_CONSUMER_SCAN_PREVIOUS_TRACK                     = 0x00B6, // OSC
  HID_USAGE_CONSUMER_STOP                                    = 0x00B7, // OSC
  HID_USAGE_CONSUMER_EJECT                                   = 0x00B8, // OSC
  HID_USAGE_CONSUMER_RANDOM_PLAY                             = 0x00B9, // OOC
  HID_USAGE_CONSUMER_SELECT_DISC                             = 0x00BA, // NAry
  HID_USAGE_CONSUMER_ENTER_DISC                              = 0x00BB, // MC
  HID_USAGE_CONSUMER_REPEAT                                  = 0x00BC, // OSC
  HID_USAGE_CONSUMER_TRACKING                                = 0x00BD, // LC
  HID_USAGE_CONSUMER_TRACK_NORMAL                            = 0x00BE, // OSC
  HID_USAGE_CONSUMER_SLOW_TRACKING                           = 0x00BF, // LC
  HID_USAGE_CONSUMER_FRAME_FORWARD                           = 0x00C0, // RTC
  HID_USAGE_CONSUMER_FRAME_BACK                              = 0x00C1, // RTC
  HID_USAGE_CONSUMER_MARK                                    = 0x00C2, // OSC
  HID_USAGE_CONSUMER_CLEAR_MARK                              = 0x00C3, // OSC
  HID_USAGE_CONSUMER_REPEAT_FROM_MARK                        = 0x00C4, // OOC
  HID_USAGE_CONSUMER_RETURN_TO_MARK                          = 0x00C5, // OSC
  HID_USAGE_CONSUMER_SEARCH_MARK_FORWARD                     = 0x00C6, // OSC
  HID_USAGE_CONSUMER_SEARCH_MARK_BACKWARDS                   = 0x00C7, // OSC
  HID_USAGE_CONSUMER_COUNTER_RESET                           = 0x00C8, // OSC


  // These HID usages operate only on mobile systems (battery powered) and
  // require Windows 8 (build 8302 or greater).
  HID_USAGE_CONSUMER_WIRELESS_RADIO_CONTROLS                 = 0x000C,
  HID_USAGE_CONSUMER_WIRELESS_RADIO_BUTTONS                  = 0x00C6,
  HID_USAGE_CONSUMER_WIRELESS_RADIO_LED                      = 0x00C7,
  HID_USAGE_CONSUMER_WIRELESS_RADIO_SLIDER_SWITCH            = 0x00C8,


  HID_USAGE_CONSUMER_SHOW_COUNTER                            = 0x00C9, // OSC
  HID_USAGE_CONSUMER_TRACKING_INCREMENT                      = 0x00CA, // RTC
  HID_USAGE_CONSUMER_TRACKING_DECREMENT                      = 0x00CB, // RTC
  HID_USAGE_CONSUMER_STOP_EJECT                              = 0x00CC, // OSC
  HID_USAGE_CONSUMER_PLAY_PAUSE                              = 0x00CD, // OSC
  HID_USAGE_CONSUMER_PLAY_SKIP                               = 0x00CE, // OSC
  HID_USAGE_CONSUMER_VOICE_COMMAND                           = 0x00CF, // OSC
  HID_USAGE_CONSUMER_INVOKE_CAPTURE_INTERFACE                = 0x00D0, // Sel
  HID_USAGE_CONSUMER_START_OR_STOP_GAME_RECORDING            = 0x00D1, // Sel
  HID_USAGE_CONSUMER_HISTORICAL_GAME_CAPTURE                 = 0x00D2, // Sel
  HID_USAGE_CONSUMER_CAPTURE_GAME_SCREENSHOT                 = 0x00D3, // Sel
  HID_USAGE_CONSUMER_SHOW_OR_HIDE_RECORDING_INDICATOR        = 0x00D4, // Sel
  HID_USAGE_CONSUMER_START_OR_STOP_MICROPHONE_CAPTURE        = 0x00D5, // Sel
  HID_USAGE_CONSUMER_START_OR_STOP_CAMERA_CAPTURE            = 0x00D6, // Sel
  HID_USAGE_CONSUMER_START_OR_STOP_GAME_BROADCAST            = 0x00D7, // Sel
  HID_USAGE_CONSUMER_START_OR_STOP_VOICE_DICTATION_SESSION   = 0x00D8, // OOC
  HID_USAGE_CONSUMER_INVOKE_DISMISS_EMOJI_PICKER             = 0x00D9, // OOC
  // DA-DF Reserved
  HID_USAGE_CONSUMER_VOLUME                                  = 0x00E0, // LC
  HID_USAGE_CONSUMER_BALANCE                                 = 0x00E1, // LC
  HID_USAGE_CONSUMER_MUTE                                    = 0x00E2, // OOC
  HID_USAGE_CONSUMER_BASS                                    = 0x00E3, // LC
  HID_USAGE_CONSUMER_TREBLE                                  = 0x00E4, // LC
  HID_USAGE_CONSUMER_BASS_BOOST                              = 0x00E5, // OOC
  HID_USAGE_CONSUMER_SURROUND_MODE                           = 0x00E6, // OSC
  HID_USAGE_CONSUMER_LOUDNESS                                = 0x00E7, // OOC
  HID_USAGE_CONSUMER_MPX                                     = 0x00E8, // OOC
  HID_USAGE_CONSUMER_VOLUME_INCREMENT                        = 0x00E9, // RTC
  HID_USAGE_CONSUMER_VOLUME_DECREMENT                        = 0x00EA, // RTC
  // EB-EF Reserved
  HID_USAGE_CONSUMER_SPEED_SELECT                            = 0x00F0, // OSC
  HID_USAGE_CONSUMER_PLAYBACK_SPEED                          = 0x00F1, // NAry
  HID_USAGE_CONSUMER_STANDARD_PLAY                           = 0x00F2, // Sel
  HID_USAGE_CONSUMER_LONG_PLAY                               = 0x00F3, // Sel
  HID_USAGE_CONSUMER_EXTENDED_PLAY                           = 0x00F4, // Sel
  HID_USAGE_CONSUMER_SLOW                                    = 0x00F5, // OSC
  // F6-FF Reserved
  HID_USAGE_CONSUMER_FAN_ENABLE                              = 0x0100, // OOC
  HID_USAGE_CONSUMER_FAN_SPEED                               = 0x0101, // LC
  HID_USAGE_CONSUMER_LIGHT_ENABLE                            = 0x0102, // OOC
  HID_USAGE_CONSUMER_LIGHT_ILLUMINATION_LEVEL                = 0x0103, // LC
  HID_USAGE_CONSUMER_CLIMATE_CONTROL_ENABLE                  = 0x0104, // OOC
  HID_USAGE_CONSUMER_ROOM_TEMPERATURE                        = 0x0105, // LC
  HID_USAGE_CONSUMER_SECURITY_ENABLE                         = 0x0106, // OOC
  HID_USAGE_CONSUMER_FIRE_ALARM                              = 0x0107, // OSC
  HID_USAGE_CONSUMER_POLICE_ALARM                            = 0x0108, // OSC
  HID_USAGE_CONSUMER_PROXIMITY                               = 0x0109, // LC
  HID_USAGE_CONSUMER_MOTION                                  = 0x010A, // OSC
  HID_USAGE_CONSUMER_DURESS_ALARM                            = 0x010B, // OSC
  HID_USAGE_CONSUMER_HOLDUP_ALARM                            = 0x010C, // OSC
  HID_USAGE_CONSUMER_MEDICAL_ALARM                           = 0x010D, // OSC
  // 10E-14F Reserved
  HID_USAGE_CONSUMER_BALANCE_RIGHT                           = 0x0150, // RTC
  HID_USAGE_CONSUMER_BALANCE_LEFT                            = 0x0151, // RTC
  HID_USAGE_CONSUMER_BASS_INCREMENT                          = 0x0152, // RTC
  HID_USAGE_CONSUMER_BASS_DECREMENT                          = 0x0153, // RTC
  HID_USAGE_CONSUMER_TREBLE_INCREMENT                        = 0x0154, // RTC
  HID_USAGE_CONSUMER_TREBLE_DECREMENT                        = 0x0155, // RTC

  // 156-15F Reserved
  HID_USAGE_CONSUMER_SPEAKER_SYSTEM                          = 0x0160, // CL
  HID_USAGE_CONSUMER_CHANNEL_LEFT                            = 0x0161, // CL
  HID_USAGE_CONSUMER_CHANNEL_RIGHT                           = 0x0162, // CL
  HID_USAGE_CONSUMER_CHANNEL_CENTER                          = 0x0163, // CL
  HID_USAGE_CONSUMER_CHANNEL_FRONT                           = 0x0164, // CL
  HID_USAGE_CONSUMER_CHANNEL_CENTER_FRONT                    = 0x0165, // CL
  HID_USAGE_CONSUMER_CHANNEL_SIDE                            = 0x0166, // CL
  HID_USAGE_CONSUMER_CHANNEL_SURROUND                        = 0x0167, // CL
  HID_USAGE_CONSUMER_CHANNEL_LOW_FREQUENCY                   = 0x0168, // CL
  HID_USAGE_CONSUMER_CHANNEL_TOP                             = 0x0169, // CL
  HID_USAGE_CONSUMER_CHANNEL_UNKNOWN                         = 0x016A, // CL
  // 16B-16F Reserved
  HID_USAGE_CONSUMER_SUB_CHANNEL                             = 0x0170, // LC
  HID_USAGE_CONSUMER_SUB_CHANNEL_INCREMENT                   = 0x0171, // OSC
  HID_USAGE_CONSUMER_SUB_CHANNEL_DECREMENT                   = 0x0172, // OSC
  HID_USAGE_CONSUMER_ALTERNATE_AUDIO_INCREMENT               = 0x0173, // OSC
  HID_USAGE_CONSUMER_ALTERNATE_AUDIO_DECREMENT               = 0x0174, // OSC
  // 175-17F Reserved
  HID_USAGE_CONSUMER_APPLICATION_LAUNCH_BUTTONS              = 0x0180, // NAry
  HID_USAGE_CONSUMER_AL_LAUNCH_BUTTON_CONFIGURATION          = 0x0181, // Sel
  HID_USAGE_CONSUMER_AL_PROGRAMMABLE_BUTTON                  = 0x0182, // Sel
  HID_USAGE_CONSUMER_AL_CONSUMER_CONTROL_CONFIGURATION       = 0x0183, // Sel
  HID_USAGE_CONSUMER_AL_WORD_PROCESSOR                       = 0x0184, // Sel
  HID_USAGE_CONSUMER_AL_TEXT_EDITOR                          = 0x0185, // Sel
  HID_USAGE_CONSUMER_AL_SPREADSHEET                          = 0x0186, // Sel
  HID_USAGE_CONSUMER_AL_GRAPHICS_EDITOR                      = 0x0187, // Sel
  HID_USAGE_CONSUMER_AL_PRESENTATION_APP                     = 0x0188, // Sel
  HID_USAGE_CONSUMER_AL_DATABASE_APP                         = 0x0189, // Sel
  HID_USAGE_CONSUMER_AL_EMAIL_READER                         = 0x018A, // Sel
  HID_USAGE_CONSUMER_AL_NEWSREADER                           = 0x018B, // Sel
  HID_USAGE_CONSUMER_AL_VOICEMAIL                            = 0x018C, // Sel
  HID_USAGE_CONSUMER_AL_CONTACTS_ADDRESS_BOOK                = 0x018D, // Sel
  HID_USAGE_CONSUMER_AL_CALENDAR_SCHEDULE                    = 0x018E, // Sel
  HID_USAGE_CONSUMER_AL_TASK_PROJECT_MANAGER                 = 0x018F, // Sel
  HID_USAGE_CONSUMER_AL_LOG_JOURNAL_TIMECARD                 = 0x0190, // Sel
  HID_USAGE_CONSUMER_AL_CHECKBOOK_FINANCE                    = 0x0191, // Sel
  HID_USAGE_CONSUMER_AL_CALCULATOR                           = 0x0192, // Sel
  HID_USAGE_CONSUMER_AL_A_V_CAPTURE_PLAYBACK                 = 0x0193, // Sel
  HID_USAGE_CONSUMER_AL_LOCAL_MACHINE_BROWSER                = 0x0194, // Sel
  HID_USAGE_CONSUMER_AL_LAN_WAN_BROWSER                      = 0x0195, // Sel
  HID_USAGE_CONSUMER_AL_INTERNET_BROWSER                     = 0x0196, // Sel
  HID_USAGE_CONSUMER_AL_REMOTE_NETWORKING_ISP                = 0x0197, // Sel
  HID_USAGE_CONSUMER_AL_NETWORK_CONFERENCE                   = 0x0198, // Sel
  HID_USAGE_CONSUMER_AL_NETWORK_CHAT                         = 0x0199, // Sel
  HID_USAGE_CONSUMER_AL_TELEPHONY_DIALER                     = 0x019A, // Sel
  HID_USAGE_CONSUMER_AL_LOGON                                = 0x019B, // Sel
  HID_USAGE_CONSUMER_AL_LOGOFF                               = 0x019C, // Sel
  HID_USAGE_CONSUMER_AL_LOGON_LOGOFF                         = 0x019D, // Sel
  HID_USAGE_CONSUMER_AL_TERMINAL_LOCK_SCREENSAVER            = 0x019E, // Sel
  HID_USAGE_CONSUMER_AL_CONTROL_PANEL                        = 0x019F, // Sel
  HID_USAGE_CONSUMER_AL_COMMAND_LINE_PROCESSOR_RUN           = 0x01A0, // Sel
  HID_USAGE_CONSUMER_AL_PROCESS_TASK_MANAGER                 = 0x01A1, // Sel
  HID_USAGE_CONSUMER_AL_SELECT_TASK_APPLICATION              = 0x01A2, // Sel
  HID_USAGE_CONSUMER_AL_NEXT_TASK_APPLICATION                = 0x01A3, // Sel
  HID_USAGE_CONSUMER_AL_PREVIOUS_TASK_APPLICATION            = 0x01A4, // Sel
  HID_USAGE_CONSUMER_AL_PREEMPTIVE_HALT                      = 0x01A5, // Sel
  HID_USAGE_CONSUMER_AL_INTEGRATED_HELP_CENTER               = 0x01A6, // Sel
  HID_USAGE_CONSUMER_AL_DOCUMENTS                            = 0x01A7, // Sel
  HID_USAGE_CONSUMER_AL_THESAURUS                            = 0x01A8, // Sel
  HID_USAGE_CONSUMER_AL_DICTIONARY                           = 0x01A9, // Sel
  HID_USAGE_CONSUMER_AL_DESKTOP                              = 0x01AA, // Sel
  HID_USAGE_CONSUMER_AL_SPELL_CHECK                          = 0x01AB, // Sel
  HID_USAGE_CONSUMER_AL_GRAMMAR_CHECK                        = 0x01AC, // Sel
  HID_USAGE_CONSUMER_AL_WIRELESS_STATUS                      = 0x01AD, // Sel
  HID_USAGE_CONSUMER_AL_KEYBOARD_LAYOUT                      = 0x01AE, // Sel
  HID_USAGE_CONSUMER_AL_VIRUS_PROTECTION                     = 0x01AF, // Sel
  HID_USAGE_CONSUMER_AL_ENCRYPTION                           = 0x01B0, // Sel
  HID_USAGE_CONSUMER_AL_SCREEN_SAVER                         = 0x01B1, // Sel
  HID_USAGE_CONSUMER_AL_ALARMS                               = 0x01B2, // Sel
  HID_USAGE_CONSUMER_AL_CLOCK                                = 0x01B3, // Sel
  HID_USAGE_CONSUMER_AL_FILE_BROWSER                         = 0x01B4, // Sel
  HID_USAGE_CONSUMER_AL_POWER_STATUS                         = 0x01B5, // Sel
  HID_USAGE_CONSUMER_AL_IMAGE_BROWSER                        = 0x01B6, // Sel
  HID_USAGE_CONSUMER_AL_AUDIO_BROWSER                        = 0x01B7, // Sel
  HID_USAGE_CONSUMER_AL_MOVIE_BROWSER                        = 0x01B8, // Sel
  HID_USAGE_CONSUMER_AL_DIGITAL_RIGHTS_MANAGER               = 0x01B9, // Sel
  HID_USAGE_CONSUMER_AL_DIGITAL_WALLET                       = 0x01BA, // Sel
  // 1BB Reserved
  HID_USAGE_CONSUMER_AL_INSTANT_MESSAGING                    = 0x01BC, // Sel
  HID_USAGE_CONSUMER_AL_OEM_FEATURES_TIPS_TUTORIAL           = 0x01BD, // Sel
  HID_USAGE_CONSUMER_AL_OEM_HELP                             = 0x01BE, // Sel
  HID_USAGE_CONSUMER_AL_ONLINE_COMMUNITY                     = 0x01BF, // Sel
  HID_USAGE_CONSUMER_AL_ENTERTAINMENT_CONTENT                = 0x01C0, // Sel
  HID_USAGE_CONSUMER_AL_ONLINE_SHOPPING_BROWSER              = 0x01C1, // Sel
  HID_USAGE_CONSUMER_AL_SMARTCARD_INFORMATION_HELP           = 0x01C2, // Sel
  HID_USAGE_CONSUMER_AL_MARKET_MONITOR_FINANCE               = 0x01C3, // Sel
  HID_USAGE_CONSUMER_AL_CUSTOMIZED_CORPORATE_NEWS            = 0x01C4, // Sel
  HID_USAGE_CONSUMER_AL_ONLINE_ACTIVITY_BROWSER              = 0x01C5, // Sel
  HID_USAGE_CONSUMER_AL_RESEARCH_SEARCH_BROWSER              = 0x01C6, // Sel
  HID_USAGE_CONSUMER_AL_AUDIO_PLAYER                         = 0x01C7, // Sel
  HID_USAGE_CONSUMER_AL_MESSAGE_STATUS                       = 0x01C8, // Sel
  HID_USAGE_CONSUMER_AL_CONTACT_SYNC                         = 0x01C9, // Sel
  HID_USAGE_CONSUMER_AL_NAVIGATION                           = 0x01CA, // Sel
  HID_USAGE_CONSUMER_AL_CONTEXT_AWARE_DESKTOP_ASSISTANT      = 0x01CB, // Sel
  // 1CC-1FF Reserved
  HID_USAGE_CONSUMER_GENERIC_GUI_APPLICATION                 = 0x0200, // NAry
  HID_USAGE_CONSUMER_AC_NEW                                  = 0x0201, // Sel
  HID_USAGE_CONSUMER_AC_OPEN                                 = 0x0202, // Sel
  HID_USAGE_CONSUMER_AC_CLOSE                                = 0x0203, // Sel
  HID_USAGE_CONSUMER_AC_EXIT                                 = 0x0204, // Sel
  HID_USAGE_CONSUMER_AC_MAXIMIZE                             = 0x0205, // Sel
  HID_USAGE_CONSUMER_AC_MINIMIZE                             = 0x0206, // Sel
  HID_USAGE_CONSUMER_AC_SAVE                                 = 0x0207, // Sel
  HID_USAGE_CONSUMER_AC_PRINT                                = 0x0208, // Sel
  HID_USAGE_CONSUMER_AC_PROPERTIES                           = 0x0209, // Sel
  // 20A-219 Reserved
  HID_USAGE_CONSUMER_AC_UNDO                                 = 0x021A, // Sel
  HID_USAGE_CONSUMER_AC_COPY                                 = 0x021B, // Sel
  HID_USAGE_CONSUMER_AC_CUT                                  = 0x021C, // Sel
  HID_USAGE_CONSUMER_AC_PASTE                                = 0x021D, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_ALL                           = 0x021E, // Sel
  HID_USAGE_CONSUMER_AC_FIND                                 = 0x021F, // Sel
  HID_USAGE_CONSUMER_AC_FIND_AND_REPLACE                     = 0x0220, // Sel
  HID_USAGE_CONSUMER_AC_SEARCH                               = 0x0221, // Sel
  HID_USAGE_CONSUMER_AC_GO_TO                                = 0x0222, // Sel
  HID_USAGE_CONSUMER_AC_HOME                                 = 0x0223, // Sel
  HID_USAGE_CONSUMER_AC_BACK                                 = 0x0224, // Sel
  HID_USAGE_CONSUMER_AC_FORWARD                              = 0x0225, // Sel
  HID_USAGE_CONSUMER_AC_STOP                                 = 0x0226, // Sel
  HID_USAGE_CONSUMER_AC_REFRESH                              = 0x0227, // Sel
  HID_USAGE_CONSUMER_AC_PREVIOUS_LINK                        = 0x0228, // Sel
  HID_USAGE_CONSUMER_AC_NEXT_LINK                            = 0x0229, // Sel
  HID_USAGE_CONSUMER_AC_BOOKMARKS                            = 0x022A, // Sel
  HID_USAGE_CONSUMER_AC_HISTORY                              = 0x022B, // Sel
  HID_USAGE_CONSUMER_AC_SUBSCRIPTIONS                        = 0x022C, // Sel
  HID_USAGE_CONSUMER_AC_ZOOM_IN                              = 0x022D, // Sel
  HID_USAGE_CONSUMER_AC_ZOOM_OUT                             = 0x022E, // Sel
  HID_USAGE_CONSUMER_AC_ZOOM                                 = 0x022F, // LC
  HID_USAGE_CONSUMER_AC_FULL_SCREEN_VIEW                     = 0x0230, // Sel
  HID_USAGE_CONSUMER_AC_NORMAL_VIEW                          = 0x0231, // Sel
  HID_USAGE_CONSUMER_AC_VIEW_TOGGLE                          = 0x0232, // Sel
  HID_USAGE_CONSUMER_AC_SCROLL_UP                            = 0x0233, // Sel
  HID_USAGE_CONSUMER_AC_SCROLL_DOWN                          = 0x0234, // Sel
  HID_USAGE_CONSUMER_AC_SCROLL                               = 0x0235, // LC
  HID_USAGE_CONSUMER_AC_PAN_LEFT                             = 0x0236, // Sel
  HID_USAGE_CONSUMER_AC_PAN_RIGHT                            = 0x0237, // Sel
  HID_USAGE_CONSUMER_AC_PAN                                  = 0x0238, // LC
  HID_USAGE_CONSUMER_AC_NEW_WINDOW                           = 0x0239, // Sel
  HID_USAGE_CONSUMER_AC_TILE_HORIZONTALLY                    = 0x023A, // Sel
  HID_USAGE_CONSUMER_AC_TILE_VERTICALLY                      = 0x023B, // Sel
  HID_USAGE_CONSUMER_AC_FORMAT                               = 0x023C, // Sel
  HID_USAGE_CONSUMER_AC_EDIT                                 = 0x023D, // Sel
  HID_USAGE_CONSUMER_AC_BOLD                                 = 0x023E, // Sel
  HID_USAGE_CONSUMER_AC_ITALICS                              = 0x023F, // Sel
  HID_USAGE_CONSUMER_AC_UNDERLINE                            = 0x0240, // Sel
  HID_USAGE_CONSUMER_AC_STRIKETHROUGH                        = 0x0241, // Sel
  HID_USAGE_CONSUMER_AC_SUBSCRIPT                            = 0x0242, // Sel
  HID_USAGE_CONSUMER_AC_SUPERSCRIPT                          = 0x0243, // Sel
  HID_USAGE_CONSUMER_AC_ALL_CAPS                             = 0x0244, // Sel
  HID_USAGE_CONSUMER_AC_ROTATE                               = 0x0245, // Sel
  HID_USAGE_CONSUMER_AC_RESIZE                               = 0x0246, // Sel
  HID_USAGE_CONSUMER_AC_FLIP_HORIZONTAL                      = 0x0247, // Sel
  HID_USAGE_CONSUMER_AC_FLIP_VERTICAL                        = 0x0248, // Sel
  HID_USAGE_CONSUMER_AC_MIRROR_HORIZONTAL                    = 0x0249, // Sel
  HID_USAGE_CONSUMER_AC_MIRROR_VERTICAL                      = 0x024A, // Sel
  HID_USAGE_CONSUMER_AC_FONT_SELECT                          = 0x024B, // Sel
  HID_USAGE_CONSUMER_AC_FONT_COLOR                           = 0x024C, // Sel
  HID_USAGE_CONSUMER_AC_FONT_SIZE                            = 0x024D, // Sel
  HID_USAGE_CONSUMER_AC_JUSTIFY_LEFT                         = 0x024E, // Sel
  HID_USAGE_CONSUMER_AC_JUSTIFY_CENTER_H                     = 0x024F, // Sel
  HID_USAGE_CONSUMER_AC_JUSTIFY_RIGHT                        = 0x0250, // Sel
  HID_USAGE_CONSUMER_AC_JUSTIFY_BLOCK_H                      = 0x0251, // Sel
  HID_USAGE_CONSUMER_AC_JUSTIFY_TOP                          = 0x0252, // Sel
  HID_USAGE_CONSUMER_AC_JUSTIFY_CENTER_V                     = 0x0253, // Sel
  HID_USAGE_CONSUMER_AC_JUSTIFY_BOTTOM                       = 0x0254, // Sel
  HID_USAGE_CONSUMER_AC_JUSTIFY_BLOCK_V                      = 0x0255, // Sel
  HID_USAGE_CONSUMER_AC_INDENT_DECREASE                      = 0x0256, // Sel
  HID_USAGE_CONSUMER_AC_INDENT_INCREASE                      = 0x0257, // Sel
  HID_USAGE_CONSUMER_AC_NUMBERED_LIST                        = 0x0258, // Sel
  HID_USAGE_CONSUMER_AC_RESTART_NUMBERING                    = 0x0259, // Sel
  HID_USAGE_CONSUMER_AC_BULLETED_LIST                        = 0x025A, // Sel
  HID_USAGE_CONSUMER_AC_PROMOTE                              = 0x025B, // Sel
  HID_USAGE_CONSUMER_AC_DEMOTE                               = 0x025C, // Sel
  HID_USAGE_CONSUMER_AC_YES                                  = 0x025D, // Sel
  HID_USAGE_CONSUMER_AC_NO                                   = 0x025E, // Sel
  HID_USAGE_CONSUMER_AC_CANCEL                               = 0x025F, // Sel
  HID_USAGE_CONSUMER_AC_CATALOG                              = 0x0260, // Sel
  HID_USAGE_CONSUMER_AC_BUY_CHECKOUT                         = 0x0261, // Sel
  HID_USAGE_CONSUMER_AC_ADD_TO_CART                          = 0x0262, // Sel
  HID_USAGE_CONSUMER_AC_EXPAND                               = 0x0263, // Sel
  HID_USAGE_CONSUMER_AC_EXPAND_ALL                           = 0x0264, // Sel
  HID_USAGE_CONSUMER_AC_COLLAPSE                             = 0x0265, // Sel
  HID_USAGE_CONSUMER_AC_COLLAPSE_ALL                         = 0x0266, // Sel
  HID_USAGE_CONSUMER_AC_PRINT_PREVIEW                        = 0x0267, // Sel
  HID_USAGE_CONSUMER_AC_PASTE_SPECIAL                        = 0x0268, // Sel
  HID_USAGE_CONSUMER_AC_INSERT_MODE                          = 0x0269, // Sel
  HID_USAGE_CONSUMER_AC_DELETE                               = 0x026A, // Sel
  HID_USAGE_CONSUMER_AC_LOCK                                 = 0x026B, // Sel
  HID_USAGE_CONSUMER_AC_UNLOCK                               = 0x026C, // Sel
  HID_USAGE_CONSUMER_AC_PROTECT                              = 0x026D, // Sel
  HID_USAGE_CONSUMER_AC_UNPROTECT                            = 0x026E, // Sel
  HID_USAGE_CONSUMER_AC_ATTACH_COMMENT                       = 0x026F, // Sel
  HID_USAGE_CONSUMER_AC_DELETE_COMMENT                       = 0x0270, // Sel
  HID_USAGE_CONSUMER_AC_VIEW_COMMENT                         = 0x0271, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_WORD                          = 0x0272, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_SENTENCE                      = 0x0273, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_PARAGRAPH                     = 0x0274, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_COLUMN                        = 0x0275, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_ROW                           = 0x0276, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_TABLE                         = 0x0277, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_OBJECT                        = 0x0278, // Sel
  HID_USAGE_CONSUMER_AC_REDO_REPEAT                          = 0x0279, // Sel
  HID_USAGE_CONSUMER_AC_SORT                                 = 0x027A, // Sel
  HID_USAGE_CONSUMER_AC_SORT_ASCENDING                       = 0x027B, // Sel
  HID_USAGE_CONSUMER_AC_SORT_DESCENDING                      = 0x027C, // Sel
  HID_USAGE_CONSUMER_AC_FILTER                               = 0x027D, // Sel
  HID_USAGE_CONSUMER_AC_SET_CLOCK                            = 0x027E, // Sel
  HID_USAGE_CONSUMER_AC_VIEW_CLOCK                           = 0x027F, // Sel
  HID_USAGE_CONSUMER_AC_SELECT_TIME_ZONE                     = 0x0280, // Sel
  HID_USAGE_CONSUMER_AC_EDIT_TIME_ZONES                      = 0x0281, // Sel
  HID_USAGE_CONSUMER_AC_SET_ALARM                            = 0x0282, // Sel
  HID_USAGE_CONSUMER_AC_CLEAR_ALARM                          = 0x0283, // Sel
  HID_USAGE_CONSUMER_AC_SNOOZE_ALARM                         = 0x0284, // Sel
  HID_USAGE_CONSUMER_AC_RESET_ALARM                          = 0x0285, // Sel
  HID_USAGE_CONSUMER_AC_SYNCHRONIZE                          = 0x0286, // Sel
  HID_USAGE_CONSUMER_AC_SEND_RECEIVE                         = 0x0287, // Sel
  HID_USAGE_CONSUMER_AC_SEND_TO                              = 0x0288, // Sel
  HID_USAGE_CONSUMER_AC_REPLY                                = 0x0289, // Sel
  HID_USAGE_CONSUMER_AC_REPLY_ALL                            = 0x028A, // Sel
  HID_USAGE_CONSUMER_AC_FORWARD_MSG                          = 0x028B, // Sel
  HID_USAGE_CONSUMER_AC_SEND                                 = 0x028C, // Sel
  HID_USAGE_CONSUMER_AC_ATTACH_FILE                          = 0x028D, // Sel
  HID_USAGE_CONSUMER_AC_UPLOAD                               = 0x028E, // Sel
  HID_USAGE_CONSUMER_AC_DOWNLOAD_SAVE_TARGET_AS              = 0x028F, // Sel
  HID_USAGE_CONSUMER_AC_SET_BORDERS                          = 0x0290, // Sel
  HID_USAGE_CONSUMER_AC_INSERT_ROW                           = 0x0291, // Sel
  HID_USAGE_CONSUMER_AC_INSERT_COLUMN                        = 0x0292, // Sel
  HID_USAGE_CONSUMER_AC_INSERT_FILE                          = 0x0293, // Sel
  HID_USAGE_CONSUMER_AC_INSERT_PICTURE                       = 0x0294, // Sel
  HID_USAGE_CONSUMER_AC_INSERT_OBJECT                        = 0x0295, // Sel
  HID_USAGE_CONSUMER_AC_INSERT_SYMBOL                        = 0x0296, // Sel
  HID_USAGE_CONSUMER_AC_SAVE_AND_CLOSE                       = 0x0297, // Sel
  HID_USAGE_CONSUMER_AC_RENAME                               = 0x0298, // Sel
  HID_USAGE_CONSUMER_AC_MERGE                                = 0x0299, // Sel
  HID_USAGE_CONSUMER_AC_SPLIT                                = 0x029A, // Sel
  HID_USAGE_CONSUMER_AC_DISRIBUTE_HORIZONTALLY               = 0x029B, // Sel
  HID_USAGE_CONSUMER_AC_DISTRIBUTE_VERTICALLY                = 0x029C, // Sel
  HID_USAGE_CONSUMER_AC_NEXT_KEYBOARD_LAYOUT_SELECT          = 0x029D, // Sel
  HID_USAGE_CONSUMER_AC_NAVIGATION_GUIDANCE                  = 0x029E, // Sel
  HID_USAGE_CONSUMER_AC_DESKTOP_SHOW_ALL_WINDOWS             = 0x029F, // Sel
  HID_USAGE_CONSUMER_AC_SOFT_KEY_LEFT                        = 0x02A0, // Sel
  HID_USAGE_CONSUMER_AC_SOFT_KEY_RIGHT                       = 0x02A1, // Sel
  HID_USAGE_CONSUMER_AC_DESKTOP_SHOW_ALL_APPLICATIONS        = 0x02A2, // Sel
  // 2A3-2AF Reserved
  HID_USAGE_CONSUMER_AC_IDLE_KEEP_ALIVE                         = 0x02B0, // Sel
  // 2B1-2BF Reserved
  HID_USAGE_CONSUMER_EXTENDED_KEYBOARD_ATTRIBUTES_COLLECTION    = 0x02C0, // CL
  HID_USAGE_CONSUMER_KEYBOARD_FORM_FACTOR                       = 0x02C1, // SV
  HID_USAGE_CONSUMER_KEYBOARD_KEY_TYPE                          = 0x02C2, // SV
  HID_USAGE_CONSUMER_KEYBOARD_PHYSICAL_LAYOUT                   = 0x02C3, // SV
  HID_USAGE_CONSUMER_VENDOR_SPECIFIC_KEYBOARD_PHYSICAL_LAYOUT   = 0x02C4, // SV
  HID_USAGE_CONSUMER_KEYBOARD_IETF_LANGUAGE_TAG_INDEX           = 0x02C5, // SV
  HID_USAGE_CONSUMER_IMPLEMENTED_KEYBOARD_INPUT_ASSIST_CONTROLS = 0x02C6, // SV
  HID_USAGE_CONSUMER_KEYBOARD_INPUT_ASSIST_PREVIOUS             = 0x02C7, // Sel
  HID_USAGE_CONSUMER_KEYBOARD_INPUT_ASSIST_NEXT                 = 0x02C8, // Sel
  HID_USAGE_CONSUMER_KEYBOARD_INPUT_ASSIST_PREVIOUS_GROUP       = 0x02C9, // Sel
  HID_USAGE_CONSUMER_KEYBOARD_INPUT_ASSIST_NEXT_GROUP           = 0x02CA, // Sel
  HID_USAGE_CONSUMER_KEYBOARD_INPUT_ASSIST_ACCEPT               = 0x02CB, // Sel
  HID_USAGE_CONSUMER_KEYBOARD_INPUT_ASSIST_CANCEL               = 0x02CC, // Sel
  // 2CD-2CF Reserved
  HID_USAGE_CONSUMER_PRIVACY_SCREEN_TOGGLE                      = 0x02D0, // OOC
  HID_USAGE_CONSUMER_PRIVACY_SCREEN_LEVEL_DECREMENT             = 0x02D1, // RTC
  HID_USAGE_CONSUMER_PRIVACY_SCREEN_LEVEL_INCREMENT             = 0x02D2, // RTC
  HID_USAGE_CONSUMER_PRIVACY_SCREEN_LEVEL_MINIMUM               = 0x02D3, // OSC
  HID_USAGE_CONSUMER_PRIVACY_SCREEN_LEVEL_MAXIMUM               = 0x02D4, // OSC
  // 2D5-4FF Reserved
  HID_USAGE_CONSUMER_CONTACT_EDITED                             = 0x0500, // OOC
  HID_USAGE_CONSUMER_CONTACT_ADDED                              = 0x0501, // OOC
  HID_USAGE_CONSUMER_CONTACT_RECORD_ACTIVE                      = 0x0502, // OOC
  HID_USAGE_CONSUMER_CONTACT_INDEX                              = 0x0503, // DV
  HID_USAGE_CONSUMER_CONTACT_NICKNAME                           = 0x0504, // DV
  HID_USAGE_CONSUMER_CONTACT_FIRST_NAME                         = 0x0505, // DV
  HID_USAGE_CONSUMER_CONTACT_LAST_NAME                          = 0x0506, // DV
  HID_USAGE_CONSUMER_CONTACT_FULL_NAME                          = 0x0507, // DV
  HID_USAGE_CONSUMER_CONTACT_PHONE_NUMBER_PERSONAL              = 0x0508, // DV
  HID_USAGE_CONSUMER_CONTACT_PHONE_NUMBER_BUSINESS              = 0x0509, // DV
  HID_USAGE_CONSUMER_CONTACT_PHONE_NUMBER_MOBILE                = 0x050A, // DV
  HID_USAGE_CONSUMER_CONTACT_PHONE_NUMBER_PAGER                 = 0x050B, // DV
  HID_USAGE_CONSUMER_CONTACT_PHONE_NUMBER_FAX                   = 0x050C, // DV
  HID_USAGE_CONSUMER_CONTACT_PHONE_NUMBER_OTHER                 = 0x050D, // DV
  HID_USAGE_CONSUMER_CONTACT_EMAIL_PERSONAL                     = 0x050E, // DV
  HID_USAGE_CONSUMER_CONTACT_EMAIL_BUSINESS                     = 0x050F, // DV
  HID_USAGE_CONSUMER_CONTACT_EMAIL_OTHER                        = 0x0510, // DV
  HID_USAGE_CONSUMER_CONTACT_EMAIL_MAIN                         = 0x0511, // DV
  HID_USAGE_CONSUMER_CONTACT_SPEED_DIAL_NUMBER                  = 0x0512, // DV
  HID_USAGE_CONSUMER_CONTACT_STATUS_FLAG                        = 0x0513, // DV
  HID_USAGE_CONSUMER_CONTACT_MISC                               = 0x0514, // DV
  HID_USAGE_CONSUMER_KEYBOARD_BRIGHTNESS_NEXT                   = 0x0515, // OSC
  HID_USAGE_CONSUMER_KEYBOARD_BRIGHTNESS_PREVIOUS               = 0x0516, // OSC
  HID_USAGE_CONSUMER_KEYBOARD_BACKLIGHT_LEVEL_SUGGESTION        = 0x0517, // SV
  // 518-FFFF Reserved

  // For Backwards compatibility to prevent current builds from breaking
  HID_USAGE_CONSUMER_BRIGHTNESS_INCREMENT               = HID_USAGE_CONSUMER_DISPLAY_BRIGHTNESS_INCREMENT, // RTC
  HID_USAGE_CONSUMER_BRIGHTNESS_DECREMENT               = HID_USAGE_CONSUMER_DISPLAY_BRIGHTNESS_DECREMENT, // RTC
};

/// HID Usage Table: Digitizer Page (0x0D)
enum {
  HID_USAGE_DIGITIZER_UNDEFINED                           = 0x00,
  HID_USAGE_DIGITIZER_DIGITIZER                           = 0x01, // CA
  HID_USAGE_DIGITIZER_PEN                                 = 0x02, // CA
  HID_USAGE_DIGITIZER_LIGHT_PEN                           = 0x03, // CA
  HID_USAGE_DIGITIZER_TOUCH_SCREEN                        = 0x04, // CA
  HID_USAGE_DIGITIZER_TOUCH_PAD                           = 0x05, // CA
  HID_USAGE_DIGITIZER_WHITEBOARD                          = 0x06, // CA
  HID_USAGE_DIGITIZER_COORDINATE_MEASURING_MACHINE        = 0x07, // CA
  HID_USAGE_DIGITIZER_3D_DIGITIZER                        = 0x08, // CA
  HID_USAGE_DIGITIZER_STEREO_PLOTTER                      = 0x09, // CA
  HID_USAGE_DIGITIZER_ARTICULATED_ARM                     = 0x0A, // CA
  HID_USAGE_DIGITIZER_ARMATURE                            = 0x0B, // CA
  HID_USAGE_DIGITIZER_MULTIPLE_POINT_DIGITIZER            = 0x0C, // CA
  HID_USAGE_DIGITIZER_FREE_SPACE_WAND                     = 0x0D, // CA
  HID_USAGE_DIGITIZER_DEVICE_CONFIGURATION                = 0x0E, // CA
  HID_USAGE_DIGITIZER_CAPACITIVE_HEAT_MAP_DIGITIZER       = 0x0F, // CA
  // Reserved (0x10 - 0x1F)
  HID_USAGE_DIGITIZER_STYLUS                              = 0x20, // CA/CL
  HID_USAGE_DIGITIZER_PUCK                                = 0x21, // CL
  HID_USAGE_DIGITIZER_FINGER                              = 0x22, // CL
  HID_USAGE_DIGITIZER_DEVICE_SETTINGS                     = 0x23, // CL
  HID_USAGE_DIGITIZER_CHARACTER_GESTURE                   = 0x24, // CL
  // Reserved (0x25 - 0x2F)
  HID_USAGE_DIGITIZER_TIP_PRESSURE                        = 0x30, // DV
  HID_USAGE_DIGITIZER_BARREL_PRESSURE                     = 0x31, // DV
  HID_USAGE_DIGITIZER_IN_RANGE                            = 0x32, // MC
  HID_USAGE_DIGITIZER_TOUCH                               = 0x33, // MC
  HID_USAGE_DIGITIZER_UNTOUCH                             = 0x34, // OSC
  HID_USAGE_DIGITIZER_TAP                                 = 0x35, // OSC
  HID_USAGE_DIGITIZER_QUALITY                             = 0x36, // DV
  HID_USAGE_DIGITIZER_DATA_VALID                          = 0x37, // MC
  HID_USAGE_DIGITIZER_TRANSDUCER_INDEX                    = 0x38, // DV
  HID_USAGE_DIGITIZER_TABLET_FUNCTION_KEYS                = 0x39, // CL
  HID_USAGE_DIGITIZER_PROGRAM_CHANGE_KEYS                 = 0x3A, // CL
  HID_USAGE_DIGITIZER_BATTERY_STRENGTH                    = 0x3B, // DV
  HID_USAGE_DIGITIZER_INVERT                              = 0x3C, // MC
  HID_USAGE_DIGITIZER_X_TILT                              = 0x3D, // DV
  HID_USAGE_DIGITIZER_Y_TILT                              = 0x3E, // DV
  HID_USAGE_DIGITIZER_AZIMUTH                             = 0x3F, // DV
  HID_USAGE_DIGITIZER_ALTITUDE                            = 0x40, // DV
  HID_USAGE_DIGITIZER_TWIST                               = 0x41, // DV
  HID_USAGE_DIGITIZER_TIP_SWITCH                          = 0x42, // MC
  HID_USAGE_DIGITIZER_SECONDARY_TIP_SWITCH                = 0x43, // MC
  HID_USAGE_DIGITIZER_BARREL_SWITCH                       = 0x44, // MC
  HID_USAGE_DIGITIZER_ERASER                              = 0x45, // MC
  HID_USAGE_DIGITIZER_TABLET_PICK                         = 0x46, // MC
  HID_USAGE_DIGITIZER_TOUCH_VALID                         = 0x47, // MC
  HID_USAGE_DIGITIZER_WIDTH                               = 0x48, // DV
  HID_USAGE_DIGITIZER_HEIGHT                              = 0x49, // DV
  // Reserved (0x4A - 0x50)
  HID_USAGE_DIGITIZER_CONTACT_IDENTIFIER                  = 0x51, // DV
  HID_USAGE_DIGITIZER_DEVICE_MODE                         = 0x52, // DV
  HID_USAGE_DIGITIZER_DEVICE_IDENTIFIER                   = 0x53, // DV/SV
  HID_USAGE_DIGITIZER_CONTACT_COUNT                       = 0x54, // DV
  HID_USAGE_DIGITIZER_CONTACT_COUNT_MAXIMUM               = 0x55, // SV
  HID_USAGE_DIGITIZER_SCAN_TIME                           = 0x56, // DV
  HID_USAGE_DIGITIZER_SURFACE_SWITCH                      = 0x57, // DF
  HID_USAGE_DIGITIZER_BUTTON_SWITCH                       = 0x58, // DF
  HID_USAGE_DIGITIZER_PAD_TYPE                            = 0x59, // SF
  HID_USAGE_DIGITIZER_SECONDARY_BARREL_SWITCH             = 0x5A, // MC
  HID_USAGE_DIGITIZER_TRANSDUCER_SERIAL_NUMBER            = 0x5B, // SV
  HID_USAGE_DIGITIZER_PREFERRED_COLOR                     = 0x5C, // DV
  HID_USAGE_DIGITIZER_PREFERRED_COLOR_LOCKED              = 0x5D, // MC
  HID_USAGE_DIGITIZER_PREFERRED_LINE_WIDTH                = 0x5E, // DV
  HID_USAGE_DIGITIZER_PREFERRED_LINE_WIDTH_LOCKED         = 0x5F, // MC
  HID_USAGE_DIGITIZER_LATENCY_MODE                        = 0x60, // DF
  HID_USAGE_DIGITIZER_GESTURE_CHARACTER_QUALITY           = 0x61, // DV
  HID_USAGE_DIGITIZER_CHARACTER_GESTURE_DATA_LENGTH       = 0x62, // DV
  HID_USAGE_DIGITIZER_CHARACTER_GESTURE_DATA              = 0x63, // DV
  HID_USAGE_DIGITIZER_GESTURE_CHARACTER_ENCODING          = 0x64, // NAry
  HID_USAGE_DIGITIZER_UTF8_CHARACTER_GESTURE_ENCODING     = 0x65, // Sel
  HID_USAGE_DIGITIZER_UTF16_LE_CHARACTER_GESTURE_ENCODING = 0x66, // Sel
  HID_USAGE_DIGITIZER_UTF16_BE_CHARACTER_GESTURE_ENCODING = 0x67, // Sel
  HID_USAGE_DIGITIZER_UTF32_LE_CHARACTER_GESTURE_ENCODING = 0x68, // Sel
  HID_USAGE_DIGITIZER_UTF32_BE_CHARACTER_GESTURE_ENCODING = 0x69, // Sel
  HID_USAGE_DIGITIZER_CAPACITIVE_HEAT_MAP_VENDOR_ID       = 0x6A, // SV
  HID_USAGE_DIGITIZER_CAPACITIVE_HEAT_MAP_VERSION         = 0x6B, // SV
  HID_USAGE_DIGITIZER_CAPACITIVE_HEAT_MAP_FRAME_DATA      = 0x6C, // DV
  HID_USAGE_DIGITIZER_GESTURE_CHARACTER_ENABLE            = 0x6D, // DF
  HID_USAGE_DIGITIZER_TRANSDUCER_SERIAL_NUMBER_PART2      = 0x6E, // SV
  HID_USAGE_DIGITIZER_NO_PREFERRED_COLOR                  = 0x6F, // DF
  HID_USAGE_DIGITIZER_PREFERRED_LINE_STYLE                = 0x70, // NAry
  HID_USAGE_DIGITIZER_PREFERRED_LINE_STYLE_LOCKED         = 0x71, // MC
  HID_USAGE_DIGITIZER_INK                                 = 0x72, // Sel
  HID_USAGE_DIGITIZER_PENCIL                              = 0x73, // Sel
  HID_USAGE_DIGITIZER_HIGHLIGHTER                         = 0x74, // Sel
  HID_USAGE_DIGITIZER_CHISEL_MARKER                       = 0x75, // Sel
  HID_USAGE_DIGITIZER_BRUSH                               = 0x76, // Sel
  HID_USAGE_DIGITIZER_NO_PREFERENCE                       = 0x77, // Sel
  // Reserved (0x78 - 0x7F)
  HID_USAGE_DIGITIZER_DIGITIZER_DIAGNOSTIC                = 0x80, // CL
  HID_USAGE_DIGITIZER_DIGITIZER_ERROR                     = 0x81, // NAry
  HID_USAGE_DIGITIZER_ERR_NORMAL_STATUS                   = 0x82, // Sel
  HID_USAGE_DIGITIZER_ERR_TRANSDUCERS_EXCEEDED            = 0x83, // Sel
  HID_USAGE_DIGITIZER_ERR_FULL_TRANS_FEATURES_UNAVAILABLE = 0x84, // Sel
  HID_USAGE_DIGITIZER_ERR_CHARGE_LOW                      = 0x85, // Sel
  // Reserved (0x86 - 0x8F)
  HID_USAGE_DIGITIZER_TRANSDUCER_SOFTWARE_INFO            = 0x90, // CL
  HID_USAGE_DIGITIZER_TRANSDUCER_VENDOR_ID                = 0x91, // SV
  HID_USAGE_DIGITIZER_TRANSDUCER_PRODUCT_ID               = 0x92, // SV
  HID_USAGE_DIGITIZER_DEVICE_SUPPORTED_PROTOCOLS          = 0x93, // NAry/CL
  HID_USAGE_DIGITIZER_TRANSDUCER_SUPPORTED_PROTOCOLS      = 0x94, // NAry/CL
  HID_USAGE_DIGITIZER_NO_PROTOCOL                         = 0x95, // Sel
  HID_USAGE_DIGITIZER_WACOM_AES_PROTOCOL                  = 0x96, // Sel
  HID_USAGE_DIGITIZER_USI_PROTOCOL                        = 0x97, // Sel
  HID_USAGE_DIGITIZER_MICROSOFT_PEN_PROTOCOL              = 0x98, // Sel
  // Reserved (0x99 - 0x9F)
  HID_USAGE_DIGITIZER_SUPPORTED_REPORT_RATES              = 0xA0, // SV/CL
  HID_USAGE_DIGITIZER_REPORT_RATE                         = 0xA1, // DV
  HID_USAGE_DIGITIZER_TRANSDUCER_CONNECTED                = 0xA2, // SF
  HID_USAGE_DIGITIZER_SWITCH_DISABLED                     = 0xA3, // Sel
  HID_USAGE_DIGITIZER_SWITCH_UNIMPLEMENTED                = 0xA4, // Sel
  HID_USAGE_DIGITIZER_TRANSDUCER_SWITCHES                 = 0xA5, // CL
  HID_USAGE_DIGITIZER_TRANSDUCER_INDEX_SELECTOR           = 0xA6, // DV
  // Reserved (0xA7 - 0xAF)
  HID_USAGE_DIGITIZER_BUTTON_PRESS_THRESHOLD              = 0xB0, // DV

  // Reserved (0xB1 - 0xFFFF)
};

/// HID Usage Table: Haptics Page (0x0E)
enum{
  HID_USAGE_HAPTICS_SIMPLE_HAPTIC_CONTROLLER          = 0x0001, // CA/CL
  // Reserved (0x0002 - 0x000F)
  HID_USAGE_HAPTICS_WAVEFORM_LIST                     = 0x0010, // NAry
  HID_USAGE_HAPTICS_DURATION_LIST                     = 0x0011, // NAry
  // Reserved (0x0012 - 0x001F)
  HID_USAGE_HAPTICS_AUTO_TRIGGER                      = 0x0020, // DV
  HID_USAGE_HAPTICS_MANUAL_TRIGGER                    = 0x0021, // DV
  HID_USAGE_HAPTICS_AUTO_TRIGGER_ASSOCIATED_CONTROL   = 0x0022, // SV
  HID_USAGE_HAPTICS_INTENSITY                         = 0x0023, // DV
  HID_USAGE_HAPTICS_REPEAT_COUNT                      = 0x0024, // DV
  HID_USAGE_HAPTICS_RETRIGGER_PERIOD                  = 0x0025, // DV
  HID_USAGE_HAPTICS_WAVEFORM_VENDOR_PAGE              = 0x0026, // SV
  HID_USAGE_HAPTICS_WAVEFORM_VENDOR_ID                = 0x0027, // SV
  HID_USAGE_HAPTICS_WAVEFORM_CUTOFF_TIME              = 0x0028, // SV
  // Reserved (0x0029 - 0x1000)
  HID_USAGE_HAPTICS_WAVEFORM_NONE                     = 0x1001, // SV
  HID_USAGE_HAPTICS_WAVEFORM_STOP                     = 0x1002, // SV
  HID_USAGE_HAPTICS_WAVEFORM_CLICK                    = 0x1003, // SV
  HID_USAGE_HAPTICS_WAVEFORM_BUZZ_CONTINUOUS          = 0x1004, // SV
  HID_USAGE_HAPTICS_WAVEFORM_RUMBLE_CONTINUOUS        = 0x1005, // SV
  HID_USAGE_HAPTICS_WAVEFORM_PRESS                    = 0x1006, // SV
  HID_USAGE_HAPTICS_WAVEFORM_RELEASE                  = 0x1007, // SV
  HID_USAGE_HAPTICS_WAVEFORM_HOVER                    = 0x1008, // SV
  HID_USAGE_HAPTICS_WAVEFORM_SUCCESS                  = 0x1009, // SV
  HID_USAGE_HAPTICS_WAVEFORM_ERROR                    = 0x100A, // SV
  HID_USAGE_HAPTICS_WAVEFORM_INK_CONTINUOUS           = 0x100B, // SV
  HID_USAGE_HAPTICS_WAVEFORM_PENCIL_CONTINUOUS        = 0x100C, // SV
  HID_USAGE_HAPTICS_WAVEFORM_MARKER_CONTINUOUS        = 0x100D, // SV
  HID_USAGE_HAPTICS_WAVEFORM_CHISEL_MARKER_CONTINUOUS = 0x100E, // SV
  HID_USAGE_HAPTICS_WAVEFORM_BRUSH_CONTINUOUS         = 0x100F, // SV
  HID_USAGE_HAPTICS_WAVEFORM_ERASER_CONTINUOUS        = 0x1010, // SV
  HID_USAGE_HAPTICS_WAVEFORM_SPARKLE_CONTINUOUS       = 0x1011, // SV
  // Reserved (0x1012 - 0xFFFF)
};

/// HID Usage Table: Physical Input Device Page (0x0F)
enum {
  HID_USAGE_PID_UNDEFINED                                = 0x00,
  HID_USAGE_PID_PHYSICAL_INPUT_DEVICE                    = 0x01, // CA
  // Reserved (0x02 - 0x1F)
  HID_USAGE_PID_NORMAL                                   = 0x20, // DV
  HID_USAGE_PID_SET_EFFECT_REPORT                        = 0x21, // CL
  HID_USAGE_PID_EFFECT_PARAMETER_BLOCK_INDEX             = 0x22, // DV
  HID_USAGE_PID_PARAMETER_BLOCK_OFFSET                   = 0x23, // DV
  HID_USAGE_PID_ROM_FLAG                                 = 0x24, // DF
  HID_USAGE_PID_EFFECT_TYPE                              = 0x25, // NAry
  HID_USAGE_PID_ET_CONSTANTFORCE                         = 0x26, // Sel
  HID_USAGE_PID_ET_RAMP                                  = 0x27, // Sel
  HID_USAGE_PID_ET_CUSTOMFORCE                           = 0x28, // Sel
  // Reserved (0x29 - 0x2F)
  HID_USAGE_PID_ET_SQUARE                                = 0x30, // Sel
  HID_USAGE_PID_ET_SINE                                  = 0x31, // Sel
  HID_USAGE_PID_ET_TRIANGLE                              = 0x32, // Sel
  HID_USAGE_PID_ET_SAWTOOTH_UP                           = 0x33, // Sel
  HID_USAGE_PID_ET_SAWTOOTH_DOWN                         = 0x34, // Sel
  // Reserved (0x35 - 0x3F)
  HID_USAGE_PID_ET_SPRING                                = 0x40, // Sel
  HID_USAGE_PID_ET_DAMPER                                = 0x41, // Sel
  HID_USAGE_PID_ET_INERTIA                               = 0x42, // Sel
  HID_USAGE_PID_ET_FRICTION                              = 0x43, // Sel
  // Reserved (0x44 - 0x4F)
  HID_USAGE_PID_DURATION                                 = 0x50, // DV
  HID_USAGE_PID_SAMPLE_PERIOD                            = 0x51, // DV
  HID_USAGE_PID_GAIN                                     = 0x52, // DV
  HID_USAGE_PID_TRIGGER_BUTTON                           = 0x53, // DV
  HID_USAGE_PID_TRIGGER_REPEAT_INTERVAL                  = 0x54, // DV
  HID_USAGE_PID_AXES_ENABLE                              = 0x55, // US
  HID_USAGE_PID_DIRECTION_ENABLE                         = 0x56, // DF
  HID_USAGE_PID_DIRECTION                                = 0x57, // CL
  HID_USAGE_PID_TYPE_SPECIFIC_BLOCK_OFFSET               = 0x58, // CL
  HID_USAGE_PID_BLOCK_TYPE                               = 0x59, // NAry
  HID_USAGE_PID_SET_ENVELOPE_REPORT                      = 0x5A, // CL/SV
  HID_USAGE_PID_ATTACK_LEVEL                             = 0x5B, // DV
  HID_USAGE_PID_ATTACK_TIME                              = 0x5C, // DV
  HID_USAGE_PID_FADE_LEVEL                               = 0x5D, // DV
  HID_USAGE_PID_FADE_TIME                                = 0x5E, // DV
  HID_USAGE_PID_SET_CONDITION_REPORT                     = 0x5F, // CL/SV
  HID_USAGE_PID_CENTERPOINT_OFFSET                       = 0x60, // DV
  HID_USAGE_PID_POSITIVE_COEFFICIENT                     = 0x61, // DV
  HID_USAGE_PID_NEGATIVE_COEFFICIENT                     = 0x62, // DV
  HID_USAGE_PID_POSITIVE_SATURATION                      = 0x63, // DV
  HID_USAGE_PID_NEGATIVE_SATURATION                      = 0x64, // DV
  HID_USAGE_PID_DEAD_BAND                                = 0x65, // DV
  HID_USAGE_PID_DOWNLOAD_FORCE_SAMPLE                    = 0x66, // CL
  HID_USAGE_PID_ISOCH_CUSTOMFORCE_ENABLE                 = 0x67, // DF
  HID_USAGE_PID_CUSTOMFORCE_DATA_REPORT                  = 0x68, // CL
  HID_USAGE_PID_CUSTOMFORCE_DATA                         = 0x69, // DV
  HID_USAGE_PID_CUSTOMFORCE_VENDOR_DEFINED_DATA          = 0x6A, // DV
  HID_USAGE_PID_SET_CUSTOMFORCE_REPORT                   = 0x6B, // CL/SV
  HID_USAGE_PID_CUSTOMFORCE_DATA_OFFSET                  = 0x6C, // DV
  HID_USAGE_PID_SAMPLE_COUNT                             = 0x6D, // DV
  HID_USAGE_PID_SET_PERIODIC_REPORT                      = 0x6E, // CL/SV
  HID_USAGE_PID_OFFSET                                   = 0x6F, // DV
  HID_USAGE_PID_MAGNITUDE                                = 0x70, // DV
  HID_USAGE_PID_PHASE                                    = 0x71, // DV
  HID_USAGE_PID_PERIOD                                   = 0x72, // DV
  HID_USAGE_PID_SET_CONSTANTFORCE_REPORT                 = 0x73, // CL/SV
  HID_USAGE_PID_SET_RAMPFORCE_REPORT                     = 0x74, // CL/SV
  HID_USAGE_PID_RAMP_START                               = 0x75, // DV
  HID_USAGE_PID_RAMP_END                                 = 0x76, // DV
  HID_USAGE_PID_EFFECT_OPERATION_REPORT                  = 0x77, // CL
  HID_USAGE_PID_EFFECT_OPERATION                         = 0x78, // NAry
  HID_USAGE_PID_OP_EFFECT_START                          = 0x79, // Sel
  HID_USAGE_PID_OP_EFFECT_START_SOLO                     = 0x7A, // Sel
  HID_USAGE_PID_OP_EFFECT_STOP                           = 0x7B, // Sel
  HID_USAGE_PID_LOOP_COUNT                               = 0x7C, // DV
  HID_USAGE_PID_DEVICE_GAIN_REPORT                       = 0x7D, // CL
  HID_USAGE_PID_DEVICE_GAIN                              = 0x7E, // DV
  HID_USAGE_PID_PARAMETER_BLOCK_POOLS_REPORT             = 0x7F, // CL
  HID_USAGE_PID_RAM_POOL_SIZE                            = 0x80, // DV
  HID_USAGE_PID_ROM_POOL_SIZE                            = 0x81, // SV
  HID_USAGE_PID_ROM_EFFECT_BLOCK_COUNT                   = 0x82, // SV
  HID_USAGE_PID_SIMULTANEOUS_EFFECTS_MAX                 = 0x83, // SV
  HID_USAGE_PID_POOL_ALIGNMENT                           = 0x84, // SV
  HID_USAGE_PID_PARAMETER_BLOCK_MOVE_REPORT              = 0x85, // CL
  HID_USAGE_PID_MOVE_SOURCE                              = 0x86, // DV
  HID_USAGE_PID_MOVE_DESTINATION                         = 0x87, // DV
  HID_USAGE_PID_MOVE_LENGTH                              = 0x88, // DV
  HID_USAGE_PID_EFFECT_PARAMETER_BLOCK_LOAD_REPORT       = 0x89, // CL
  // Reserved (0x8A)
  HID_USAGE_PID_EFFECT_PARAMETER_BLOCK_LOAD_STATUS       = 0x8B, // NAry
  HID_USAGE_PID_BLOCK_LOAD_SUCCESS                       = 0x8C, // Sel
  HID_USAGE_PID_BLOCK_LOAD_FULL                          = 0x8D, // Sel
  HID_USAGE_PID_BLOCK_LOAD_ERROR                         = 0x8E, // Sel
  HID_USAGE_PID_BLOCK_HANDLE                             = 0x8F, // DV
  HID_USAGE_PID_EFFECT_PARAMETER_BLOCK_FREE_REPORT       = 0x90, // CL
  HID_USAGE_PID_TYPE_SPECIFIC_BLOCK_HANDLE               = 0x91, // CL
  HID_USAGE_PID_PID_STATE_REPORT                         = 0x92, // CL
  // Reserved (0x93)
  HID_USAGE_PID_EFFECT_PLAYING                           = 0x94, // DF
  HID_USAGE_PID_PID_DEVICE_CONTROL_REPORT                = 0x95, // CL
  HID_USAGE_PID_PID_DEVICE_CONTROL                       = 0x96, // NAry
  HID_USAGE_PID_DC_ENABLE_ACTUATORS                      = 0x97, // Sel
  HID_USAGE_PID_DC_DISABLE_ACTUATORS                     = 0x98, // Sel
  HID_USAGE_PID_DC_STOP_ALL_EFFECTS                      = 0x99, // Sel
  HID_USAGE_PID_DC_RESET                                 = 0x9A, // Sel
  HID_USAGE_PID_DC_PAUSE                                 = 0x9B, // Sel
  HID_USAGE_PID_DC_CONTINUE                              = 0x9C, // Sel
  // Reserved (0x9D - 0x9E)
  HID_USAGE_PID_DEVICE_PAUSED                            = 0x9F, // DF
  HID_USAGE_PID_ACTUATORS_ENABLED                        = 0xA0, // DF
  // Reserved (0xA1 - 0xA3)
  HID_USAGE_PID_SAFETY_SWITCH                            = 0xA4, // DF
  HID_USAGE_PID_ACTUATOR_OVERRIDE_SWITCH                 = 0xA5, // DF
  HID_USAGE_PID_ACTUATOR_POWER                           = 0xA6, // OOC
  HID_USAGE_PID_START_DELAY                              = 0xA7, // DV
  HID_USAGE_PID_PARAMETER_BLOCK_SIZE                     = 0xA8, // CL
  HID_USAGE_PID_DEVICEMANAGED_POOL                       = 0xA9, // SF
  HID_USAGE_PID_SHARED_PARAMETER_BLOCKS                  = 0xAA, // SF
  HID_USAGE_PID_CREATE_NEW_EFFECT_PARAMETER_BLOCK_REPORT = 0xAB, // CL
  HID_USAGE_PID_RAM_POOL_AVAILABLE                       = 0xAC, // DV
  // Reserved (0xAD - 0xFFFF)
};

/// HID Usage Table: Unicode Page (0x10)
/// Intentionally skipped

/// HID Usage Table: SoC Page (0x11)
enum {
  HID_USAGE_SOC_SOC_CONTROL                      = 0x01, // CA
  HID_USAGE_SOC_FIRMWARE_TRANSFER                = 0x02, // CL
  HID_USAGE_SOC_FIRMWARE_FILE_ID                 = 0x03, // DV
  HID_USAGE_SOC_FILE_OFFSET_IN_BYTES             = 0x04, // DV
  HID_USAGE_SOC_FILE_TRANSFER_SIZE_MAX_IN_BYTES  = 0x05, // DV
  HID_USAGE_SOC_FILE_PAYLOAD                     = 0x06, // DV
  HID_USAGE_SOC_FILE_PAYLOAD_SIZE_IN_BYTES       = 0x07, // DV
  HID_USAGE_SOC_FILE_PAYLOAD_CONTAINS_LAST_BYTES = 0x08, // DF
  HID_USAGE_SOC_FILE_TRANSFER_STOP               = 0x09, // DF
  HID_USAGE_SOC_FILE_TRANSFER_TILL_END           = 0x0A  // DF
  // Reserved (0x0B - 0xFFFF)
};

/// HID Usage Table: Eye and Head Trackers Page (0x12)
enum {
  HID_USAGE_EYE_AND_HEAD_TRACKER_EYE_TRACKER                 = 0x0001, // CA
  HID_USAGE_EYE_AND_HEAD_TRACKER_HEAD_TRACKER                = 0x0002, // CA
  // Reserved (0x0003 - 0x000F)
  HID_USAGE_EYE_AND_HEAD_TRACKER_TRACKING_DATA               = 0x0010, // CP
  HID_USAGE_EYE_AND_HEAD_TRACKER_CAPABILITIES                = 0x0011, // CL
  HID_USAGE_EYE_AND_HEAD_TRACKER_CONFIGURATION               = 0x0012, // CL
  HID_USAGE_EYE_AND_HEAD_TRACKER_STATUS                      = 0x0013, // CL
  HID_USAGE_EYE_AND_HEAD_TRACKER_CONTROL                     = 0x0014, // CL
  // Reserved (0x0015 - 0x001F)
  HID_USAGE_EYE_AND_HEAD_TRACKER_SENSOR_TIMESTAMP            = 0x0020, // DV
  HID_USAGE_EYE_AND_HEAD_TRACKER_POSITION_X                  = 0x0021, // DV
  HID_USAGE_EYE_AND_HEAD_TRACKER_POSITION_Y                  = 0x0022, // DV
  HID_USAGE_EYE_AND_HEAD_TRACKER_POSITION_Z                  = 0x0023, // DV
  HID_USAGE_EYE_AND_HEAD_TRACKER_GAZE_POINT                  = 0x0024, // CP
  HID_USAGE_EYE_AND_HEAD_TRACKER_LEFT_EYE_POSITION           = 0x0025, // CP
  HID_USAGE_EYE_AND_HEAD_TRACKER_RIGHT_EYE_POSITION          = 0x0026, // CP
  HID_USAGE_EYE_AND_HEAD_TRACKER_HEAD_POSITION               = 0x0027, // CP
  HID_USAGE_EYE_AND_HEAD_TRACKER_HEAD_DIRECTION_POINT        = 0x0028, // CP
  HID_USAGE_EYE_AND_HEAD_TRACKER_ROTATION_ABOUT_X_AXIS       = 0x0029, // DV
  HID_USAGE_EYE_AND_HEAD_TRACKER_ROTATION_ABOUT_Y_AXIS       = 0x002A, // DV
  HID_USAGE_EYE_AND_HEAD_TRACKER_ROTATION_ABOUT_Z_AXIS       = 0x002B, // DV
  // Reserved (0x002C - 0x00FF)
  HID_USAGE_EYE_AND_HEAD_TRACKER_TRACKER_QUALITY             = 0x0100, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_MINIMUM_TRACKING_DISTANCE   = 0x0101, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_OPTIMUM_TRACKING_DISTANCE   = 0x0102, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_MAXIMUM_TRACKING_DISTANCE   = 0x0103, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_MAXIMUM_SCREEN_PLANE_WIDTH  = 0x0104, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_MAXIMUM_SCREEN_PLANE_HEIGHT = 0x0105, // SV
  // Reserved (0x0106 - 0x01FF)
  HID_USAGE_EYE_AND_HEAD_TRACKER_DISPLAY_MANUFACTURER_ID     = 0x0200, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_DISPLAY_PRODUCT_ID          = 0x0201, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_DISPLAY_SERIAL_NUMBER       = 0x0202, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_DISPLAY_MANUFACTURER_DATE   = 0x0203, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_CALIBRATED_SCREEN_WIDTH     = 0x0204, // SV
  HID_USAGE_EYE_AND_HEAD_TRACKER_CALIBRATED_SCREEN_HEIGHT    = 0x0205, // SV
  // Reserved (0x0206 - 0x02FF)
  HID_USAGE_EYE_AND_HEAD_TRACKER_SAMPLING_FREQUENCY          = 0x0300, // DV
  HID_USAGE_EYE_AND_HEAD_TRACKER_CONFIGURATION_STATUS        = 0x0301, // DV
  // Reserved (0x0302 - 0x03FF)
  HID_USAGE_EYE_AND_HEAD_TRACKER_DEVICE_MODE_REQUEST         = 0x0400, // DV
  // Reserved (0x0401 - 0xFFFF)
};

/// HID Usage Table - Auxiliary Display Page (0x14)
enum {
  HID_USAGE_AUX_DISPLAY_ALPHANUMERIC_DISPLAY         = 0x01, // CA
  HID_USAGE_AUX_DISPLAY_AUXILIARY_DISPLAY            = 0x02, // CA
  // Reserved (0x03 - 0x1F)
  HID_USAGE_AUX_DISPLAY_DISPLAY_ATTRIBUTES_REPORT    = 0x20, // CL
  HID_USAGE_AUX_DISPLAY_ASCII_CHARACTER_SET          = 0x21, // SF
  HID_USAGE_AUX_DISPLAY_DATA_READ_BACK               = 0x22, // SF
  HID_USAGE_AUX_DISPLAY_FONT_READ_BACK               = 0x23, // SF
  HID_USAGE_AUX_DISPLAY_DISPLAY_CONTROL_REPORT       = 0x24, // CL
  HID_USAGE_AUX_DISPLAY_CLEAR_DISPLAY                = 0x25, // DF
  HID_USAGE_AUX_DISPLAY_DISPLAY_ENABLE               = 0x26, // DF
  HID_USAGE_AUX_DISPLAY_SCREEN_SAVER_DELAY           = 0x27, // SV/DV
  HID_USAGE_AUX_DISPLAY_SCREEN_SAVER_ENABLE          = 0x28, // DF
  HID_USAGE_AUX_DISPLAY_VERTICAL_SCROLL              = 0x29, // SF/DF
  HID_USAGE_AUX_DISPLAY_HORIZONTAL_SCROLL            = 0x2A, // SF/DF
  HID_USAGE_AUX_DISPLAY_CHARACTER_REPORT             = 0x2B, // CL
  HID_USAGE_AUX_DISPLAY_DISPLAY_DATA                 = 0x2C, // DV
  HID_USAGE_AUX_DISPLAY_DISPLAY_STATUS               = 0x2D, // CL
  HID_USAGE_AUX_DISPLAY_STAT_NOT_READY               = 0x2E, // Sel
  HID_USAGE_AUX_DISPLAY_STAT_READY                   = 0x2F, // Sel
  HID_USAGE_AUX_DISPLAY_ERR_NOT_A_LOADABLE_CHARACTER = 0x30, // Sel
  HID_USAGE_AUX_DISPLAY_ERR_FONT_DATA_CANNOT_BE_READ = 0x31, // Sel
  HID_USAGE_AUX_DISPLAY_CURSOR_POSITION_REPORT       = 0x32, // Sel
  HID_USAGE_AUX_DISPLAY_ROW                          = 0x33, // DV
  HID_USAGE_AUX_DISPLAY_COLUMN                       = 0x34, // DV
  HID_USAGE_AUX_DISPLAY_ROWS                         = 0x35, // SV
  HID_USAGE_AUX_DISPLAY_COLUMNS                      = 0x36, // SV
  HID_USAGE_AUX_DISPLAY_CURSOR_PIXEL_POSITIONING     = 0x37, // SF
  HID_USAGE_AUX_DISPLAY_CURSOR_MODE                  = 0x38, // DF
  HID_USAGE_AUX_DISPLAY_CURSOR_ENABLE                = 0x39, // DF
  HID_USAGE_AUX_DISPLAY_CURSOR_BLINK                 = 0x3A, // DF
  HID_USAGE_AUX_DISPLAY_FONT_REPORT                  = 0x3B, // CL
  HID_USAGE_AUX_DISPLAY_FONT_DATA                    = 0x3C, // Buffered Bytes
  HID_USAGE_AUX_DISPLAY_CHARACTER_WIDTH              = 0x3D, // SV
  HID_USAGE_AUX_DISPLAY_CHARACTER_HEIGHT             = 0x3E, // SV
  HID_USAGE_AUX_DISPLAY_CHARACTER_SPACING_HORIZONTAL = 0x3F, // SV
  HID_USAGE_AUX_DISPLAY_CHARACTER_SPACING_VERTICAL   = 0x40, // SV
  HID_USAGE_AUX_DISPLAY_UNICODE_CHARACTER_SET        = 0x41, // SF
  HID_USAGE_AUX_DISPLAY_FONT_7_SEGMENT               = 0x42, // SF
  HID_USAGE_AUX_DISPLAY_7_SEGMENT_DIRECT_MAP         = 0x43, // SF
  HID_USAGE_AUX_DISPLAY_FONT_14_SEGMENT              = 0x44, // SF
  HID_USAGE_AUX_DISPLAY_14_SEGMENT_DIRECT_MAP        = 0x45, // SF
  HID_USAGE_AUX_DISPLAY_DISPLAY_BRIGHTNESS           = 0x46, // DV
  HID_USAGE_AUX_DISPLAY_DISPLAY_CONTRAST             = 0x47, // DV
  HID_USAGE_AUX_DISPLAY_CHARACTER_ATTRIBUTE          = 0x48, // CL
  HID_USAGE_AUX_DISPLAY_ATTRIBUTE_READBACK           = 0x49, // SF
  HID_USAGE_AUX_DISPLAY_ATTRIBUTE_DATA               = 0x4A, // DV
  HID_USAGE_AUX_DISPLAY_CHAR_ATTR_ENHANCE            = 0x4B, // OOC
  HID_USAGE_AUX_DISPLAY_CHAR_ATTR_UNDERLINE          = 0x4C, // OOC
  HID_USAGE_AUX_DISPLAY_CHAR_ATTR_BLINK              = 0x4D, // OOC
  // Reserved (0x4E - 0x7F)
  HID_USAGE_AUX_DISPLAY_BITMAP_SIZE_X                = 0x80, // SV
  HID_USAGE_AUX_DISPLAY_BITMAP_SIZE_Y                = 0x81, // SV
  HID_USAGE_AUX_DISPLAY_MAX_BLIT_SIZE                = 0x82, // SV
  HID_USAGE_AUX_DISPLAY_BIT_DEPTH_FORMAT             = 0x83, // SV
  HID_USAGE_AUX_DISPLAY_DISPLAY_ORIENTATION          = 0x84, // DV
  HID_USAGE_AUX_DISPLAY_PALETTE_REPORT               = 0x85, // CL
  HID_USAGE_AUX_DISPLAY_PALETTE_DATA_SIZE            = 0x86, // SV
  HID_USAGE_AUX_DISPLAY_PALETTE_DATA_OFFSET          = 0x87, // SV
  HID_USAGE_AUX_DISPLAY_PALETTE_DATA                 = 0x88, // Buffered Bytes
  // Reserved (0x89)
  HID_USAGE_AUX_DISPLAY_BLIT_REPORT                  = 0x8A, // CL
  HID_USAGE_AUX_DISPLAY_BLIT_RECTANGLE_X1            = 0x8B, // SV
  HID_USAGE_AUX_DISPLAY_BLIT_RECTANGLE_Y1            = 0x8C, // SV
  HID_USAGE_AUX_DISPLAY_BLIT_RECTANGLE_X2            = 0x8D, // SV
  HID_USAGE_AUX_DISPLAY_BLIT_RECTANGLE_Y2            = 0x8E, // SV
  HID_USAGE_AUX_DISPLAY_BLIT_DATA                    = 0x8F, // Buffered Bytes
  HID_USAGE_AUX_DISPLAY_SOFT_BUTTON                  = 0x90, // CL
  HID_USAGE_AUX_DISPLAY_SOFT_BUTTON_ID               = 0x91, // SV
  HID_USAGE_AUX_DISPLAY_SOFT_BUTTON_SIDE             = 0x92, // SV
  HID_USAGE_AUX_DISPLAY_SOFT_BUTTON_OFFSET_1         = 0x93, // SV
  HID_USAGE_AUX_DISPLAY_SOFT_BUTTON_OFFSET_2         = 0x94, // SV
  HID_USAGE_AUX_DISPLAY_SOFT_BUTTON_REPORT           = 0x95, // SV
  // Reserved (0x96 - 0xC1)
  HID_USAGE_AUX_DISPLAY_SOFT_KEYS                    = 0xC2, // SV
  // Reserved (0xC3 - 0xCB)
  HID_USAGE_AUX_DISPLAY_DATA_EXTENSIONS              = 0xCC, // SF
  // Reserved (0xCD - 0xCE)
  HID_USAGE_AUX_DISPLAY_CHARACTER_MAPPING            = 0xCF, // SV
  // Reserved (0xD0 - 0xDC)
  HID_USAGE_AUX_DISPLAY_UNICODE_EQUIVALENT           = 0xDD, // SV
  // Reserved (0xDE)
  HID_USAGE_AUX_DISPLAY_CHARACTER_PAGE_MAPPING       = 0xDF, // SV
  // Reserved (0xE0 - 0xFE)
  HID_USAGE_AUX_DISPLAY_REQUEST_REPORT               = 0xFF  // DV
  // Reserved (0x100 - 0xFFFF)
};

/// HID Usage Table - Medical Instrument Page (0x40)
enum {
  HID_USAGE_MEDICAL_INSTRUMENT_MEDICAL_ULTRASOUND           = 0x01, // CA
  // Reserved (0x02 - 0x1F)
  HID_USAGE_MEDICAL_INSTRUMENT_VCR_ACQUISITION              = 0x20, // OOC
  HID_USAGE_MEDICAL_INSTRUMENT_FREEZE_THAW                  = 0x21, // OOC
  HID_USAGE_MEDICAL_INSTRUMENT_CLIP_STORE                   = 0x22, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_UPDATE                       = 0x23, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_NEXT                         = 0x24, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_SAVE                         = 0x25, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_PRINT                        = 0x26, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_MICROPHONE_ENABLE            = 0x27, // OSC
  // Reserved (0x28 - 0x3F)
  HID_USAGE_MEDICAL_INSTRUMENT_CINE                         = 0x40, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_TRANSMIT_POWER               = 0x41, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_VOLUME                       = 0x42, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_FOCUS                        = 0x43, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_DEPTH                        = 0x44, // LC
  // Reserved (0x45 - 0x5F)
  HID_USAGE_MEDICAL_INSTRUMENT_SOFT_STEP_PRIMARY            = 0x60, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_SOFT_STEP_SECONDARY          = 0x61, // LC
  // Reserved (0x62 - 0x6F)
  HID_USAGE_MEDICAL_INSTRUMENT_DEPTH_GAIN_COMPENSATION      = 0x70, // LC
  // Reserved (0x71 - 0x7F)
  HID_USAGE_MEDICAL_INSTRUMENT_ZOOM_SELECT                  = 0x80, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_ZOOM_ADJUST                  = 0x81, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_SPECTRAL_DOPPLER_MODE_SELECT = 0x82, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_SPECTRAL_DOPPLER_ADJUST      = 0x83, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_COLOR_DOPPLER_MODE_SELECT    = 0x84, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_COLOR_DOPPLER_ADJUST         = 0x85, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_MOTION_MODE_SELECT           = 0x86, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_MOTION_MODE_ADJUST           = 0x87, // LC
  HID_USAGE_MEDICAL_INSTRUMENT_2D_MODE_SELECT               = 0x88, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_2D_MODE_ADJUST               = 0x89, // LC
  // Reserved (0x8A - 0x9F)
  HID_USAGE_MEDICAL_INSTRUMENT_SOFT_CONTROL_SELECT          = 0xA0, // OSC
  HID_USAGE_MEDICAL_INSTRUMENT_SOFT_CONTROL_ADJUST          = 0xA1, // LC
  // Reserved (0xA2 - 0xFFFF)
};

/// HID Usage Table - Lighting And Illumination Page (0x59)
enum {
  HID_USAGE_LIGHTING_LAMP_ARRAY                          = 0x01, // CA
  HID_USAGE_LIGHTING_LAMP_ARRAY_ATTRIBUTES_REPORT        = 0x02, // CL
  HID_USAGE_LIGHTING_LAMP_COUNT                          = 0x03, // SV/DV
  HID_USAGE_LIGHTING_BOUNDING_BOX_WIDTH_IN_MICROMETERS   = 0x04, // SV
  HID_USAGE_LIGHTING_BOUNDING_BOX_HEIGHT_IN_MICROMETERS  = 0x05, // SV
  HID_USAGE_LIGHTING_BOUNDING_BOX_DEPTH_IN_MICROMETERS   = 0x06, // SV
  HID_USAGE_LIGHTING_LAMP_ARRAY_KIND                     = 0x07, // SV
  HID_USAGE_LIGHTING_MIN_UPDATE_INTERVAL_IN_MICROSECONDS = 0x08, // SV
  // Reserved (0x09 - 0x1F)
  HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_REQUEST_REPORT      = 0x20, // CL
  HID_USAGE_LIGHTING_LAMP_ID                             = 0x21, // SV/DV
  HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_RESPONSE_REPORT     = 0x22, // CL
  HID_USAGE_LIGHTING_POSITION_X_IN_MICROMETERS           = 0x23, // DV
  HID_USAGE_LIGHTING_POSITION_Y_IN_MICROMETERS           = 0x24, // DV
  HID_USAGE_LIGHTING_POSITION_Z_IN_MICROMETERS           = 0x25, // DV
  HID_USAGE_LIGHTING_LAMP_PURPOSES                       = 0x26, // DV
  HID_USAGE_LIGHTING_UPDATE_LATENCY_IN_MICROSECONDS      = 0x27, // DV
  HID_USAGE_LIGHTING_RED_LEVEL_COUNT                     = 0x28, // DV
  HID_USAGE_LIGHTING_GREEN_LEVEL_COUNT                   = 0x29, // DV
  HID_USAGE_LIGHTING_BLUE_LEVEL_COUNT                    = 0x2A, // DV
  HID_USAGE_LIGHTING_INTENSITY_LEVEL_COUNT               = 0x2B, // DV
  HID_USAGE_LIGHTING_IS_PROGRAMMABLE                     = 0x2C, // DV
  HID_USAGE_LIGHTING_INPUT_BINDING                       = 0x2D, // DV
  // Reserved (0x2E - 0x4F)
  HID_USAGE_LIGHTING_LAMP_MULTI_UPDATE_REPORT            = 0x50, // CL
  HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL                  = 0x51, // DV
  HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL                = 0x52, // DV
  HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL                 = 0x53, // DV
  HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL            = 0x54, // DV
  HID_USAGE_LIGHTING_LAMP_UPDATE_FLAGS                   = 0x55, // DV
  // Reserved (0x56 - 0x5F)
  HID_USAGE_LIGHTING_LAMP_RANGE_UPDATE_REPORT            = 0x60, // CL
  HID_USAGE_LIGHTING_LAMP_ID_START                       = 0x61, // DV
  HID_USAGE_LIGHTING_LAMP_ID_END                         = 0x62, // DV
  // Reserved (0x63 - 0x6F)
  HID_USAGE_LIGHTING_LAMP_ARRAY_CONTROL_REPORT           = 0x70, // CL
  HID_USAGE_LIGHTING_AUTONOMOUS_MODE                     = 0x71, // DV
  // Reserved (0x72 - 0xFFFF)
};

/// HID Usage Table: Monitor Page (0x80)
enum {
  HID_USAGE_MONITOR_MONITOR_CONTROL  = 0x01, // CA
  HID_USAGE_MONITOR_EDID_INFORMATION = 0x02, // SV
  HID_USAGE_MONITOR_VDIF_INFORMATION = 0x03, // SV
  HID_USAGE_MONITOR_VESA_VERSION     = 0x04  // SV
  // Reserved (0x05 - 0xFFFF)
};

/// HID Usage Table: Monitor Enumerated Page (0x81)
/// Intentionally skipped

/// HID Usage Table: VESA Virtual Controls Page (0x82)
enum {
  HID_USAGE_VESA_VIRTUAL_CONTROLS_DEGAUSS                          = 0x01, // DV
  // Reserved (0x02 - 0x0F)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_BRIGHTNESS                       = 0x10, // DV
  // Reserved (0x11)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_CONTRAST                         = 0x12, // DV
  // Reserved (0x13 - 0x15)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_RED_VIDEO_GAIN                   = 0x16, // DV
  // Reserved (0x17)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_GREEN_VIDEO_GAIN                 = 0x18, // DV
  // Reserved (0x19)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_BLUE_VIDEO_GAIN                  = 0x1A, // DV
  // Reserved (0x1B)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_FOCUS                            = 0x1C, // DV
  // Reserved (0x1D - 0x1F)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_POSITION              = 0x20, // DV
  // Reserved (0x21)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_SIZE                  = 0x22, // DV
  // Reserved (0x23)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_PINCUSHION            = 0x24, // DV
  // Reserved (0x25)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_PINCUSHION_BALANCE    = 0x26, // DV
  // Reserved (0x27)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_MISCONVERGENCE        = 0x28, // DV
  // Reserved (0x29)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_LINEARITY             = 0x2A, // DV
  // Reserved (0x2B)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_LINEARITY_BALANCE     = 0x2C, // DV
  // Reserved (0x2D - 0x2F)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_POSITION                = 0x30, // DV
  // Reserved (0x31)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_SIZE                    = 0x32, // DV
  // Reserved (0x33)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_PINCUSHION              = 0x34, // DV
  // Reserved (0x35)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_PINCUSHION_BALANCE      = 0x36, // DV
  // Reserved (0x37)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_MISCONVERGENCE          = 0x38, // DV
  // Reserved (0x39)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_LINEARITY               = 0x3A, // DV
  // Reserved (0x3B)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_LINEARITY_BALANCE       = 0x3C, // DV
  // Reserved (0x3D - 0x3F)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_PARALLELOGRAM_DISTORTION         = 0x40, // DV
  // Reserved (0x41)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_TRAPEZOIDAL_DISTORTION           = 0x42, // DV
  // Reserved (0x43)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_TILT                             = 0x44, // DV
  // Reserved (0x45)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_TOP_CORNER_DISTORTION_CONTROL    = 0x46, // DV
  // Reserved (0x47)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_TOP_CORNER_DISTORTION_BALANCE    = 0x48, // DV
  // Reserved (0x49)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_BOTTOM_CORNER_DISTORTION_CONTROL = 0x4A, // DV
  // Reserved (0x4B)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_BOTTOM_CORNER_DISTORTION_BALANCE = 0x4C, // DV
  // Reserved (0x4D - 0x55)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_MOIRE                 = 0x56, // DV
  // Reserved (0x57)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_MOIRE                   = 0x58, // DV
  // Reserved (0x59 - 0x5D)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_INPUT_LEVEL_SELECT               = 0x5E, // NAry
  // Reserved (0x5F)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_INPUT_SOURCE_SELECT              = 0x60, // NAry
  // Reserved (0x61 - 0x6B)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_RED_VIDEO_BLACK_LEVEL            = 0x6C, // DV
  // Reserved (0x6D)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_GREEN_VIDEO_BLACK_LEVEL          = 0x6E, // DV
  // Reserved (0x6F)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_BLUE_VIDEO_BLACK_LEVEL           = 0x70, // DV
  // Reserved (0x71 - 0xA1)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_AUTO_SIZE_CENTER                 = 0xA2, // NAry
  // Reserved (0xA3)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_POLARITY_HORIZONTAL_SYNC         = 0xA4, // NAry
  // Reserved (0xA5)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_POLARITY_VERTICAL_SYNC           = 0xA6, // NAry
  // Reserved (0xA7)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_SYNC_TYPE                        = 0xA8, // NAry
  // Reserved (0xA9)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_SCREEN_ORIENTATION               = 0xAA, // NAry
  // Reserved (0xAB)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_HORIZONTAL_FREQUENCY             = 0xAC, // DV
  // Reserved (0xAD)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_VERTICAL_FREQUENCY               = 0xAE, // DV
  // Reserved (0xAF)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_SETTINGS                         = 0xB0, // NAry
  // Reserved (0xB1 - 0xC9)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_ON_SCREEN_DISPLAY                = 0xCA, // NAry
  // Reserved (0xCB - 0xD3)
  HID_USAGE_VESA_VIRTUAL_CONTROLS_STEREO_MODE                      = 0xD4, // NAry
  // Reserved (0xD5 - 0xFFFF)
};

/// HID Usage Table: Power Device Page (0x84)
enum {
  HID_USAGE_POWER_UNDEFINED              = 0x00,
  HID_USAGE_POWER_I_NAME                 = 0x01,
  HID_USAGE_POWER_PRESENT_STATUS         = 0x02,
  HID_USAGE_POWER_CHANGED_STATUS         = 0x03,
  HID_USAGE_POWER_UPS                    = 0x04,
  HID_USAGE_POWER_POWER_SUPPLY           = 0x05,
  // 06-0F Reserved
  HID_USAGE_POWER_BATTERY_SYSTEM         = 0x10,
  HID_USAGE_POWER_BATTERY_SYSTEM_ID      = 0x11,
  HID_USAGE_POWER_BATTERY                = 0x12,
  HID_USAGE_POWER_BATTERY_ID             = 0x13,
  HID_USAGE_POWER_CHARGER                = 0x14,
  HID_USAGE_POWER_CHARGER_ID             = 0x15,
  HID_USAGE_POWER_POWER_CONVERTER        = 0x16,
  HID_USAGE_POWER_POWER_CONVERTER_ID     = 0x17,
  HID_USAGE_POWER_OUTLET_SYSTEM          = 0x18,
  HID_USAGE_POWER_OUTLET_SYSTEM_ID       = 0x19,
  HID_USAGE_POWER_INPUT                  = 0x1A,
  HID_USAGE_POWER_INPUT_ID               = 0x1B,
  HID_USAGE_POWER_OUTPUT                 = 0x1C,
  HID_USAGE_POWER_OUTPUT_ID              = 0x1D,
  HID_USAGE_POWER_FLOW                   = 0x1E,
  HID_USAGE_POWER_FLOW_ID                = 0x1F,
  HID_USAGE_POWER_OUTLET                 = 0x20,
  HID_USAGE_POWER_OUTLET_ID              = 0x21,
  HID_USAGE_POWER_GANG                   = 0x22,
  HID_USAGE_POWER_GANG_ID                = 0x23,
  HID_USAGE_POWER_POWER_SUMMARY          = 0x24,
  HID_USAGE_POWER_POWER_SUMMARY_ID       = 0x25,
  // 26-2F Reserved
  HID_USAGE_POWER_VOLTAGE                = 0x30,
  HID_USAGE_POWER_CURRENT                = 0x31,
  HID_USAGE_POWER_FREQUENCY              = 0x32,
  HID_USAGE_POWER_APPARENT_POWER         = 0x33,
  HID_USAGE_POWER_ACTIVE_POWER           = 0x34,
  HID_USAGE_POWER_PERCENT_LOAD           = 0x35,
  HID_USAGE_POWER_TEMPERATURE            = 0x36,
  HID_USAGE_POWER_HUMIDITY               = 0x37,
  HID_USAGE_POWER_BAD_COUNT              = 0x38,
  // 39-3F Reserved
  HID_USAGE_POWER_CONFIG_VOLTAGE         = 0x40,
  HID_USAGE_POWER_CONFIG_CURRENT         = 0x41,
  HID_USAGE_POWER_CONFIG_FREQUENCY       = 0x42,
  HID_USAGE_POWER_CONFIG_APPARENT_POWER  = 0x43,
  HID_USAGE_POWER_CONFIG_ACTIVE_POWER    = 0x44,
  HID_USAGE_POWER_CONFIG_PERCENT_LOAD    = 0x45,
  HID_USAGE_POWER_CONFIG_TEMPERATURE     = 0x46,
  HID_USAGE_POWER_CONFIG_HUMIDITY        = 0x47,
  // 48-4F Reserved
  HID_USAGE_POWER_SWITCH_ON_CONTROL      = 0x50,
  HID_USAGE_POWER_SWITCH_OFF_CONTROL     = 0x51,
  HID_USAGE_POWER_TOGGLE_CONTROL         = 0x52,
  HID_USAGE_POWER_LOW_VOLTAGE_TRANSFER   = 0x53,
  HID_USAGE_POWER_HIGH_VOLTAGE_TRANSFER  = 0x54,
  HID_USAGE_POWER_DELAY_BEFORE_REBOOT    = 0x55,
  HID_USAGE_POWER_DELAY_BEFORE_STARTUP   = 0x56,
  HID_USAGE_POWER_DELAY_BEFORE_SHUTDOWN  = 0x57,
  HID_USAGE_POWER_TEST                   = 0x58,
  HID_USAGE_POWER_MODULE_RESET           = 0x59,
  HID_USAGE_POWER_AUDIBLE_ALARM_CONTROL  = 0x5A,
  // 5B-5F Reserved
  HID_USAGE_POWER_PRESENT                = 0x60,
  HID_USAGE_POWER_GOOD                   = 0x61,
  HID_USAGE_POWER_INTERNAL_FAILURE       = 0x62,
  HID_USAGE_POWER_VOLTAGE_OUT_OF_RANGE   = 0x63,
  HID_USAGE_POWER_FREQUENCY_OUT_OF_RANGE = 0x64,
  HID_USAGE_POWER_OVERLOAD               = 0x65,
  HID_USAGE_POWER_OVER_CHARGED           = 0x66,
  HID_USAGE_POWER_OVER_TEMPERATURE       = 0x67,
  HID_USAGE_POWER_SHUTDOWN_REQUESTED     = 0x68,
  HID_USAGE_POWER_SHUTDOWN_IMMINENT      = 0x69,
  // 6A Reserved
  HID_USAGE_POWER_SWITCH_ON_OFF          = 0x6B,
  HID_USAGE_POWER_SWITCHABLE             = 0x6C,
  HID_USAGE_POWER_USED                   = 0x6D,
  HID_USAGE_POWER_BOOST                  = 0x6E,
  HID_USAGE_POWER_BUCK                   = 0x6F,
  HID_USAGE_POWER_INITIALIZED            = 0x70,
  HID_USAGE_POWER_TESTED                 = 0x71,
  HID_USAGE_POWER_AWAITING_POWER         = 0x72,
  HID_USAGE_POWER_COMMUNICATION_LOST     = 0x73,
  // 74-FC Reserved
  HID_USAGE_POWER_I_MANUFACTURER         = 0xFD,
  HID_USAGE_POWER_I_PRODUCT              = 0xFE,
  HID_USAGE_POWER_I_SERIAL_NUMBER        = 0xFF
  // Reserved (0x100 - 0xFFFF)
};

/// HID Usage Table: Battery System Page (0x85)
enum {
  HID_USAGE_BATTERY_UNDEFINED                      = 0x00,
  HID_USAGE_BATTERY_SMB_BATTERY_MODE               = 0x01,
  HID_USAGE_BATTERY_SMB_BATTERY_STATUS             = 0x02,
  HID_USAGE_BATTERY_SMB_ALARM_WARNING              = 0x03,
  HID_USAGE_BATTERY_SMB_CHARGER_MODE               = 0x04,
  HID_USAGE_BATTERY_SMB_CHARGER_STATUS             = 0x05,
  HID_USAGE_BATTERY_SMB_CHARGER_SPEC_INFO          = 0x06,
  HID_USAGE_BATTERY_SMB_SELECTOR_STATE             = 0x07,
  HID_USAGE_BATTERY_SMB_SELECTOR_PRESETS           = 0x08,
  HID_USAGE_BATTERY_SMB_SELECTOR_INFO              = 0x09,
  // 0A-0F Reserved
  HID_USAGE_BATTERY_OPTIONAL_MFG_FUNCTION_1        = 0x10,
  HID_USAGE_BATTERY_OPTIONAL_MFG_FUNCTION_2        = 0x11,
  HID_USAGE_BATTERY_OPTIONAL_MFG_FUNCTION_3        = 0x12,
  HID_USAGE_BATTERY_OPTIONAL_MFG_FUNCTION_4        = 0x13,
  HID_USAGE_BATTERY_OPTIONAL_MFG_FUNCTION_5        = 0x14,
  HID_USAGE_BATTERY_CONNECTION_TO_SMBUS            = 0x15,
  HID_USAGE_BATTERY_OUTPUT_CONNECTION              = 0x16,
  HID_USAGE_BATTERY_CHARGER_CONNECTION             = 0x17,
  HID_USAGE_BATTERY_BATTERY_INSERTION              = 0x18,
  HID_USAGE_BATTERY_USE_NEXT                       = 0x19,
  HID_USAGE_BATTERY_OK_TO_USE                      = 0x1A,
  HID_USAGE_BATTERY_BATTERY_SUPPORTED              = 0x1B,
  HID_USAGE_BATTERY_SELECTOR_REVISION              = 0x1C,
  HID_USAGE_BATTERY_CHARGING_INDICATOR             = 0x1D,
  // 1E-27 Reserved
  HID_USAGE_BATTERY_MANUFACTURER_ACCESS            = 0x28,
  HID_USAGE_BATTERY_REMAINING_CAPACITY_LIMIT       = 0x29,
  HID_USAGE_BATTERY_REMAINING_TIME_LIMIT           = 0x2A,
  HID_USAGE_BATTERY_AT_RATE                        = 0x2B,
  HID_USAGE_BATTERY_CAPACITY_MODE                  = 0x2C,
  HID_USAGE_BATTERY_BROADCAST_TO_CHARGER           = 0x2D,
  HID_USAGE_BATTERY_PRIMARY_BATTERY                = 0x2E,
  HID_USAGE_BATTERY_CHARGE_CONTROLLER              = 0x2F,
  // 30-3F Reserved
  HID_USAGE_BATTERY_TERMINATE_CHARGE               = 0x40,
  HID_USAGE_BATTERY_TERMINATE_DISCHARGE            = 0x41,
  HID_USAGE_BATTERY_BELOW_REMAINING_CAPACITY_LIMIT = 0x42,
  HID_USAGE_BATTERY_REMAINING_TIME_LIMIT_EXPIRED   = 0x43,
  HID_USAGE_BATTERY_CHARGING                       = 0x44,
  HID_USAGE_BATTERY_DISCHARGING                    = 0x45,
  HID_USAGE_BATTERY_FULLY_CHARGED                  = 0x46,
  HID_USAGE_BATTERY_FULLY_DISCHARGED               = 0x47,
  HID_USAGE_BATTERY_CONDITIONING_FLAG              = 0x48,
  HID_USAGE_BATTERY_AT_RATE_OK                     = 0x49,
  HID_USAGE_BATTERY_SMB_ERROR_CODE                 = 0x4A,
  HID_USAGE_BATTERY_NEED_REPLACEMENT               = 0x4B,
  // 4C-5F Reserved
  HID_USAGE_BATTERY_AT_RATE_TIME_TO_FULL           = 0x60,
  HID_USAGE_BATTERY_AT_RATE_TIME_TO_EMPTY          = 0x61,
  HID_USAGE_BATTERY_AVERAGE_CURRENT                = 0x62,
  HID_USAGE_BATTERY_MAX_ERROR                      = 0x63,
  HID_USAGE_BATTERY_RELATIVE_STATE_OF_CHARGE       = 0x64,
  HID_USAGE_BATTERY_ABSOLUTE_STATE_OF_CHARGE       = 0x65,
  HID_USAGE_BATTERY_REMAINING_CAPACITY             = 0x66,
  HID_USAGE_BATTERY_FULL_CHARGE_CAPACITY           = 0x67,
  HID_USAGE_BATTERY_RUN_TIME_TO_EMPTY              = 0x68,
  HID_USAGE_BATTERY_AVERAGE_TIME_TO_EMPTY          = 0x69,
  HID_USAGE_BATTERY_AVERAGE_TIME_TO_FULL           = 0x6A,
  HID_USAGE_BATTERY_CYCLE_COUNT                    = 0x6B,
  // 6C-7F Reserved
  HID_USAGE_BATTERY_BATT_PACK_MODEL_LEVEL          = 0x80,
  HID_USAGE_BATTERY_INTERNAL_CHARGE_CONTROLLER     = 0x81,
  HID_USAGE_BATTERY_PRIMARY_BATTERY_SUPPORT        = 0x82,
  HID_USAGE_BATTERY_DESIGN_CAPACITY                = 0x83,
  HID_USAGE_BATTERY_SPECIFICATION_INFO             = 0x84,
  HID_USAGE_BATTERY_MANUFACTURER_DATE              = 0x85,
  HID_USAGE_BATTERY_SERIAL_NUMBER                  = 0x86,
  HID_USAGE_BATTERY_I_MANUFACTURER_NAME            = 0x87,
  HID_USAGE_BATTERY_I_DEVICE_NAME                  = 0x88,
  HID_USAGE_BATTERY_I_DEVICE_CHEMISTRY             = 0x89,
  HID_USAGE_BATTERY_MANUFACTURER_DATA              = 0x8A,
  HID_USAGE_BATTERY_RECHARGEABLE                   = 0x8B,
  HID_USAGE_BATTERY_WARNING_CAPACITY_LIMIT         = 0x8C,
  HID_USAGE_BATTERY_CAPACITY_GRANULARITY_1         = 0x8D,
  HID_USAGE_BATTERY_CAPACITY_GRANULARITY_2         = 0x8E,
  HID_USAGE_BATTERY_I_OEMINFORMATION               = 0x8F,
  // 90-BF Reserved
  HID_USAGE_BATTERY_INHIBIT_CHARGE                 = 0xC0,
  HID_USAGE_BATTERY_ENABLE_POLLING                 = 0xC1,
  HID_USAGE_BATTERY_RESET_TO_ZERO                  = 0xC2,
  // C3-CF Reserved
  HID_USAGE_BATTERY_AC_PRESENT                     = 0xD0,
  HID_USAGE_BATTERY_BATTERY_PRESENT                = 0xD1,
  HID_USAGE_BATTERY_POWER_FAIL                     = 0xD2,
  HID_USAGE_BATTERY_ALARM_INHIBITED                = 0xD3,
  HID_USAGE_BATTERY_THERMISTOR_UNDER_RANGE         = 0xD4,
  HID_USAGE_BATTERY_THERMISTOR_HOT                 = 0xD5,
  HID_USAGE_BATTERY_THERMISTOR_COLD                = 0xD6,
  HID_USAGE_BATTERY_THERMISTOR_OVER_RANGE          = 0xD7,
  HID_USAGE_BATTERY_VOLTAGE_OUT_OF_RANGE           = 0xD8,
  HID_USAGE_BATTERY_CURRENT_OUT_OF_RANGE           = 0xD9,
  HID_USAGE_BATTERY_CURRENT_NOT_REGULATED          = 0xDA,
  HID_USAGE_BATTERY_VOLTAGE_NOT_REGULATED          = 0xDB,
  HID_USAGE_BATTERY_MASTER_MODE                    = 0xDC,
  // DD-EF Reserved
  HID_USAGE_BATTERY_CHARGER_SELECTOR_SUPPORT       = 0xF0,
  HID_USAGE_BATTERY_CHARGER_SPEC                   = 0xF1,
  HID_USAGE_BATTERY_LEVEL_2                        = 0xF2,
  HID_USAGE_BATTERY_LEVEL_3                        = 0xF3
  // F4-FF Reserved
};

/// HID Usage Table: Camera Control Page (0x90)
enum {
  // Reserved (0x01 - 0x1F)
  HID_USAGE_CAMERA_CONTROL_CAMERA_AUTO_FOCUS = 0x20, // OSC
  HID_USAGE_CAMERA_CONTROL_CAMERA_SHUTTER    = 0x21  // OSC
  // Reserved (0x22 - 0xFFFF)
};

/// HID Usage Table: Arcade Page (0x91)
enum {
  HID_USAGE_ARCADE_GENERAL_PURPOSE_IO_CARD              = 0x01, // CA
  HID_USAGE_ARCADE_COIN_DOOR                            = 0x02, // CA
  HID_USAGE_ARCADE_WATCHDOG_TIMER                       = 0x03, // CA
  // Reserved (0x04 - 0x2F)
  HID_USAGE_ARCADE_GENERAL_PURPOSE_ANALOG_INPUT_STATE   = 0x30, // DV
  HID_USAGE_ARCADE_GENERAL_PURPOSE_DIGITAL_INPUT_STATE  = 0x31, // DV
  HID_USAGE_ARCADE_GENERAL_PURPOSE_OPTICAL_INPUT_STATE  = 0x32, // DV
  HID_USAGE_ARCADE_GENERAL_PURPOSE_DIGITAL_OUTPUT_STATE = 0x33, // DV
  HID_USAGE_ARCADE_NUMBER_OF_COIN_DOORS                 = 0x34, // DV
  HID_USAGE_ARCADE_COIN_DRAWER_DROP_COUNT               = 0x35, // DV
  HID_USAGE_ARCADE_COIN_DRAWER_START                    = 0x36, // OOC
  HID_USAGE_ARCADE_COIN_DRAWER_SERVICE                  = 0x37, // OOC
  HID_USAGE_ARCADE_COIN_DRAWER_TILT                     = 0x38, // OOC
  HID_USAGE_ARCADE_COIN_DOOR_TEST                       = 0x39, // OOC
  // Reserved (0x3A - 0x3F)
  HID_USAGE_ARCADE_COIN_DOOR_LOCKOUT                    = 0x40, // OOC
  HID_USAGE_ARCADE_WATCHDOG_TIMEOUT                     = 0x41, // DV
  HID_USAGE_ARCADE_WATCHDOG_ACTION                      = 0x42, // NAry
  HID_USAGE_ARCADE_WATCHDOG_REBOOT                      = 0x43, // Sel
  HID_USAGE_ARCADE_WATCHDOG_RESTART                     = 0x44, // Sel
  HID_USAGE_ARCADE_ALARM_INPUT                          = 0x45, // DV
  HID_USAGE_ARCADE_COIN_DOOR_COUNTER                    = 0x46, // OOC
  HID_USAGE_ARCADE_IO_DIRECTION_MAPPING                 = 0x47, // DV
  HID_USAGE_ARCADE_SET_IO_DIRECTION_MAPPING             = 0x48, // DV
  HID_USAGE_ARCADE_EXTENDED_OPTICAL_INPUT_STATE         = 0x49, // DV
  HID_USAGE_ARCADE_PIN_PAD_INPUT_STATE                  = 0x4A, // DV
  HID_USAGE_ARCADE_PIN_PAD_STATUS                       = 0x4B, // DV
  HID_USAGE_ARCADE_PIN_PAD_OUTPUT                       = 0x4C, // OOC
  HID_USAGE_ARCADE_PIN_PAD_COMMAND                      = 0x4D, // DV
  // Reserved (0x4E - 0xFFFF)
};

/// HID Usage Table: FIDO Alliance Page (0xF1D0)
enum {
  HID_USAGE_FIDO_U2FHID   = 0x01, // U2FHID usage for top-level collection
  HID_USAGE_FIDO_DATA_IN  = 0x20, // Raw IN data report
  HID_USAGE_FIDO_DATA_OUT = 0x21  // Raw OUT data report
};

/*--------------------------------------------------------------------
 * ASCII to KEYCODE Conversion
 *  Expand to array of [128][2] (shift, keycode)
 *
 * Usage: example to convert input chr into keyboard report (modifier + keycode)
 *
 *  uint8_t const conv_table[128][2] =  { HID_ASCII_TO_KEYCODE };
 *
 *  uint8_t keycode[6] = { 0 };
 *  uint8_t modifier   = 0;
 *
 *  if ( conv_table[chr][0] ) modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
 *  keycode[0] = conv_table[chr][1];
 *  tud_hid_keyboard_report(report_id, modifier, keycode);
 *
 *--------------------------------------------------------------------*/
#define HID_ASCII_TO_KEYCODE \
    {0, 0                     }, /* 0x00 Null      */ \
    {0, 0                     }, /* 0x01           */ \
    {0, 0                     }, /* 0x02           */ \
    {0, 0                     }, /* 0x03           */ \
    {0, 0                     }, /* 0x04           */ \
    {0, 0                     }, /* 0x05           */ \
    {0, 0                     }, /* 0x06           */ \
    {0, 0                     }, /* 0x07           */ \
    {0, HID_KEY_BACKSPACE     }, /* 0x08 Backspace */ \
    {0, HID_KEY_TAB           }, /* 0x09 Tab       */ \
    {0, HID_KEY_ENTER         }, /* 0x0A Line Feed */ \
    {0, 0                     }, /* 0x0B           */ \
    {0, 0                     }, /* 0x0C           */ \
    {0, HID_KEY_ENTER         }, /* 0x0D CR        */ \
    {0, 0                     }, /* 0x0E           */ \
    {0, 0                     }, /* 0x0F           */ \
    {0, 0                     }, /* 0x10           */ \
    {0, 0                     }, /* 0x11           */ \
    {0, 0                     }, /* 0x12           */ \
    {0, 0                     }, /* 0x13           */ \
    {0, 0                     }, /* 0x14           */ \
    {0, 0                     }, /* 0x15           */ \
    {0, 0                     }, /* 0x16           */ \
    {0, 0                     }, /* 0x17           */ \
    {0, 0                     }, /* 0x18           */ \
    {0, 0                     }, /* 0x19           */ \
    {0, 0                     }, /* 0x1A           */ \
    {0, HID_KEY_ESCAPE        }, /* 0x1B Escape    */ \
    {0, 0                     }, /* 0x1C           */ \
    {0, 0                     }, /* 0x1D           */ \
    {0, 0                     }, /* 0x1E           */ \
    {0, 0                     }, /* 0x1F           */ \
                                                      \
    {0, HID_KEY_SPACE         }, /* 0x20           */ \
    {1, HID_KEY_1             }, /* 0x21 !         */ \
    {1, HID_KEY_APOSTROPHE    }, /* 0x22 "         */ \
    {1, HID_KEY_3             }, /* 0x23 #         */ \
    {1, HID_KEY_4             }, /* 0x24 $         */ \
    {1, HID_KEY_5             }, /* 0x25 %         */ \
    {1, HID_KEY_7             }, /* 0x26 &         */ \
    {0, HID_KEY_APOSTROPHE    }, /* 0x27 '         */ \
    {1, HID_KEY_9             }, /* 0x28 (         */ \
    {1, HID_KEY_0             }, /* 0x29 )         */ \
    {1, HID_KEY_8             }, /* 0x2A *         */ \
    {1, HID_KEY_EQUAL         }, /* 0x2B +         */ \
    {0, HID_KEY_COMMA         }, /* 0x2C ,         */ \
    {0, HID_KEY_MINUS         }, /* 0x2D -         */ \
    {0, HID_KEY_PERIOD        }, /* 0x2E .         */ \
    {0, HID_KEY_SLASH         }, /* 0x2F /         */ \
    {0, HID_KEY_0             }, /* 0x30 0         */ \
    {0, HID_KEY_1             }, /* 0x31 1         */ \
    {0, HID_KEY_2             }, /* 0x32 2         */ \
    {0, HID_KEY_3             }, /* 0x33 3         */ \
    {0, HID_KEY_4             }, /* 0x34 4         */ \
    {0, HID_KEY_5             }, /* 0x35 5         */ \
    {0, HID_KEY_6             }, /* 0x36 6         */ \
    {0, HID_KEY_7             }, /* 0x37 7         */ \
    {0, HID_KEY_8             }, /* 0x38 8         */ \
    {0, HID_KEY_9             }, /* 0x39 9         */ \
    {1, HID_KEY_SEMICOLON     }, /* 0x3A :         */ \
    {0, HID_KEY_SEMICOLON     }, /* 0x3B ;         */ \
    {1, HID_KEY_COMMA         }, /* 0x3C <         */ \
    {0, HID_KEY_EQUAL         }, /* 0x3D =         */ \
    {1, HID_KEY_PERIOD        }, /* 0x3E >         */ \
    {1, HID_KEY_SLASH         }, /* 0x3F ?         */ \
                                                      \
    {1, HID_KEY_2             }, /* 0x40 @         */ \
    {1, HID_KEY_A             }, /* 0x41 A         */ \
    {1, HID_KEY_B             }, /* 0x42 B         */ \
    {1, HID_KEY_C             }, /* 0x43 C         */ \
    {1, HID_KEY_D             }, /* 0x44 D         */ \
    {1, HID_KEY_E             }, /* 0x45 E         */ \
    {1, HID_KEY_F             }, /* 0x46 F         */ \
    {1, HID_KEY_G             }, /* 0x47 G         */ \
    {1, HID_KEY_H             }, /* 0x48 H         */ \
    {1, HID_KEY_I             }, /* 0x49 I         */ \
    {1, HID_KEY_J             }, /* 0x4A J         */ \
    {1, HID_KEY_K             }, /* 0x4B K         */ \
    {1, HID_KEY_L             }, /* 0x4C L         */ \
    {1, HID_KEY_M             }, /* 0x4D M         */ \
    {1, HID_KEY_N             }, /* 0x4E N         */ \
    {1, HID_KEY_O             }, /* 0x4F O         */ \
    {1, HID_KEY_P             }, /* 0x50 P         */ \
    {1, HID_KEY_Q             }, /* 0x51 Q         */ \
    {1, HID_KEY_R             }, /* 0x52 R         */ \
    {1, HID_KEY_S             }, /* 0x53 S         */ \
    {1, HID_KEY_T             }, /* 0x55 T         */ \
    {1, HID_KEY_U             }, /* 0x55 U         */ \
    {1, HID_KEY_V             }, /* 0x56 V         */ \
    {1, HID_KEY_W             }, /* 0x57 W         */ \
    {1, HID_KEY_X             }, /* 0x58 X         */ \
    {1, HID_KEY_Y             }, /* 0x59 Y         */ \
    {1, HID_KEY_Z             }, /* 0x5A Z         */ \
    {0, HID_KEY_BRACKET_LEFT  }, /* 0x5B [         */ \
    {0, HID_KEY_BACKSLASH     }, /* 0x5C '\'       */ \
    {0, HID_KEY_BRACKET_RIGHT }, /* 0x5D ]         */ \
    {1, HID_KEY_6             }, /* 0x5E ^         */ \
    {1, HID_KEY_MINUS         }, /* 0x5F _         */ \
                                                      \
    {0, HID_KEY_GRAVE         }, /* 0x60 `         */ \
    {0, HID_KEY_A             }, /* 0x61 a         */ \
    {0, HID_KEY_B             }, /* 0x62 b         */ \
    {0, HID_KEY_C             }, /* 0x63 c         */ \
    {0, HID_KEY_D             }, /* 0x66 d         */ \
    {0, HID_KEY_E             }, /* 0x65 e         */ \
    {0, HID_KEY_F             }, /* 0x66 f         */ \
    {0, HID_KEY_G             }, /* 0x67 g         */ \
    {0, HID_KEY_H             }, /* 0x68 h         */ \
    {0, HID_KEY_I             }, /* 0x69 i         */ \
    {0, HID_KEY_J             }, /* 0x6A j         */ \
    {0, HID_KEY_K             }, /* 0x6B k         */ \
    {0, HID_KEY_L             }, /* 0x6C l         */ \
    {0, HID_KEY_M             }, /* 0x6D m         */ \
    {0, HID_KEY_N             }, /* 0x6E n         */ \
    {0, HID_KEY_O             }, /* 0x6F o         */ \
    {0, HID_KEY_P             }, /* 0x70 p         */ \
    {0, HID_KEY_Q             }, /* 0x71 q         */ \
    {0, HID_KEY_R             }, /* 0x72 r         */ \
    {0, HID_KEY_S             }, /* 0x73 s         */ \
    {0, HID_KEY_T             }, /* 0x75 t         */ \
    {0, HID_KEY_U             }, /* 0x75 u         */ \
    {0, HID_KEY_V             }, /* 0x76 v         */ \
    {0, HID_KEY_W             }, /* 0x77 w         */ \
    {0, HID_KEY_X             }, /* 0x78 x         */ \
    {0, HID_KEY_Y             }, /* 0x79 y         */ \
    {0, HID_KEY_Z             }, /* 0x7A z         */ \
    {1, HID_KEY_BRACKET_LEFT  }, /* 0x7B {         */ \
    {1, HID_KEY_BACKSLASH     }, /* 0x7C |         */ \
    {1, HID_KEY_BRACKET_RIGHT }, /* 0x7D }         */ \
    {1, HID_KEY_GRAVE         }, /* 0x7E ~         */ \
    {0, HID_KEY_DELETE        }  /* 0x7F Delete    */ \

/*--------------------------------------------------------------------
 * KEYCODE to Ascii Conversion
 *  Expand to array of [128][2] (ascii without shift, ascii with shift)
 *
 * Usage: example to convert ascii from keycode (key) and shift modifier (shift).
 * Here we assume key < 128 ( printable )
 *
 *  uint8_t const conv_table[128][2] =  { HID_KEYCODE_TO_ASCII };
 *  char ch = shift ? conv_table[chr][1] : conv_table[chr][0];
 *
 *--------------------------------------------------------------------*/
#define HID_KEYCODE_TO_ASCII    \
    {0     , 0      }, /* 0x00 */ \
    {0     , 0      }, /* 0x01 */ \
    {0     , 0      }, /* 0x02 */ \
    {0     , 0      }, /* 0x03 */ \
    {'a'   , 'A'    }, /* 0x04 */ \
    {'b'   , 'B'    }, /* 0x05 */ \
    {'c'   , 'C'    }, /* 0x06 */ \
    {'d'   , 'D'    }, /* 0x07 */ \
    {'e'   , 'E'    }, /* 0x08 */ \
    {'f'   , 'F'    }, /* 0x09 */ \
    {'g'   , 'G'    }, /* 0x0a */ \
    {'h'   , 'H'    }, /* 0x0b */ \
    {'i'   , 'I'    }, /* 0x0c */ \
    {'j'   , 'J'    }, /* 0x0d */ \
    {'k'   , 'K'    }, /* 0x0e */ \
    {'l'   , 'L'    }, /* 0x0f */ \
    {'m'   , 'M'    }, /* 0x10 */ \
    {'n'   , 'N'    }, /* 0x11 */ \
    {'o'   , 'O'    }, /* 0x12 */ \
    {'p'   , 'P'    }, /* 0x13 */ \
    {'q'   , 'Q'    }, /* 0x14 */ \
    {'r'   , 'R'    }, /* 0x15 */ \
    {'s'   , 'S'    }, /* 0x16 */ \
    {'t'   , 'T'    }, /* 0x17 */ \
    {'u'   , 'U'    }, /* 0x18 */ \
    {'v'   , 'V'    }, /* 0x19 */ \
    {'w'   , 'W'    }, /* 0x1a */ \
    {'x'   , 'X'    }, /* 0x1b */ \
    {'y'   , 'Y'    }, /* 0x1c */ \
    {'z'   , 'Z'    }, /* 0x1d */ \
    {'1'   , '!'    }, /* 0x1e */ \
    {'2'   , '@'    }, /* 0x1f */ \
    {'3'   , '#'    }, /* 0x20 */ \
    {'4'   , '$'    }, /* 0x21 */ \
    {'5'   , '%'    }, /* 0x22 */ \
    {'6'   , '^'    }, /* 0x23 */ \
    {'7'   , '&'    }, /* 0x24 */ \
    {'8'   , '*'    }, /* 0x25 */ \
    {'9'   , '('    }, /* 0x26 */ \
    {'0'   , ')'    }, /* 0x27 */ \
    {'\r'  , '\r'   }, /* 0x28 */ \
    {'\x1b', '\x1b' }, /* 0x29 */ \
    {'\b'  , '\b'   }, /* 0x2a */ \
    {'\t'  , '\t'   }, /* 0x2b */ \
    {' '   , ' '    }, /* 0x2c */ \
    {'-'   , '_'    }, /* 0x2d */ \
    {'='   , '+'    }, /* 0x2e */ \
    {'['   , '{'    }, /* 0x2f */ \
    {']'   , '}'    }, /* 0x30 */ \
    {'\\'  , '|'    }, /* 0x31 */ \
    {'#'   , '~'    }, /* 0x32 */ \
    {';'   , ':'    }, /* 0x33 */ \
    {'\''  , '\"'   }, /* 0x34 */ \
    {'`'   , '~'    }, /* 0x35 */ \
    {','   , '<'    }, /* 0x36 */ \
    {'.'   , '>'    }, /* 0x37 */ \
    {'/'   , '?'    }, /* 0x38 */ \
                                  \
    {0     , 0      }, /* 0x39 */ \
    {0     , 0      }, /* 0x3a */ \
    {0     , 0      }, /* 0x3b */ \
    {0     , 0      }, /* 0x3c */ \
    {0     , 0      }, /* 0x3d */ \
    {0     , 0      }, /* 0x3e */ \
    {0     , 0      }, /* 0x3f */ \
    {0     , 0      }, /* 0x40 */ \
    {0     , 0      }, /* 0x41 */ \
    {0     , 0      }, /* 0x42 */ \
    {0     , 0      }, /* 0x43 */ \
    {0     , 0      }, /* 0x44 */ \
    {0     , 0      }, /* 0x45 */ \
    {0     , 0      }, /* 0x46 */ \
    {0     , 0      }, /* 0x47 */ \
    {0     , 0      }, /* 0x48 */ \
    {0     , 0      }, /* 0x49 */ \
    {0     , 0      }, /* 0x4a */ \
    {0     , 0      }, /* 0x4b */ \
    {0     , 0      }, /* 0x4c */ \
    {0     , 0      }, /* 0x4d */ \
    {0     , 0      }, /* 0x4e */ \
    {0     , 0      }, /* 0x4f */ \
    {0     , 0      }, /* 0x50 */ \
    {0     , 0      }, /* 0x51 */ \
    {0     , 0      }, /* 0x52 */ \
    {0     , 0      }, /* 0x53 */ \
                                  \
    {'/'   , '/'    }, /* 0x54 */ \
    {'*'   , '*'    }, /* 0x55 */ \
    {'-'   , '-'    }, /* 0x56 */ \
    {'+'   , '+'    }, /* 0x57 */ \
    {'\r'  , '\r'   }, /* 0x58 */ \
    {'1'   , 0      }, /* 0x59 */ \
    {'2'   , 0      }, /* 0x5a */ \
    {'3'   , 0      }, /* 0x5b */ \
    {'4'   , 0      }, /* 0x5c */ \
    {'5'   , '5'    }, /* 0x5d */ \
    {'6'   , 0      }, /* 0x5e */ \
    {'7'   , 0      }, /* 0x5f */ \
    {'8'   , 0      }, /* 0x60 */ \
    {'9'   , 0      }, /* 0x61 */ \
    {'0'   , 0      }, /* 0x62 */ \
    {'.'   , 0      }, /* 0x63 */ \
    {0     , 0      }, /* 0x64 */ \
    {0     , 0      }, /* 0x65 */ \
    {0     , 0      }, /* 0x66 */ \
    {'='   , '='    }, /* 0x67 */ \


#ifdef __cplusplus
 }
#endif

#endif /* TUSB_HID_H__ */

/// @}
