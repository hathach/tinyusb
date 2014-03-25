/**************************************************************************/
/*!
    @file     ansi_esc_code.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup group_board
 *  \defgroup group_ansi_esc ANSI Esacpe Code
 *  @{ */

#ifndef _TUSB_ANSI_ESC_CODE_H_
#define _TUSB_ANSI_ESC_CODE_H_


#ifdef __cplusplus
 extern "C" {
#endif

#define CSI_CODE(seq)   "\33[" seq
#define CSI_SGR(x)      CSI_CODE(#x) "m"

//------------- Cursor movement -------------//
/** \defgroup group_ansi_cursor Cursor Movement
 *  @{ */
#define ANSI_CURSOR_UP(n)        CSI_CODE(#n "A")          ///< Move cursor up
#define ANSI_CURSOR_DOWN(n)      CSI_CODE(#n "B")          ///< Move cursor down
#define ANSI_CURSOR_FORWARD(n)   CSI_CODE(#n "C")          ///< Move cursor forward
#define ANSI_CURSOR_BACKWARD(n)  CSI_CODE(#n "D")          ///< Move cursor backward
#define ANSI_CURSOR_LINE_DOWN(n) CSI_CODE(#n "E")          ///< Move cursor to the beginning of the line (n) down
#define ANSI_CURSOR_LINE_UP(n)   CSI_CODE(#n "F")          ///< Move cursor to the beginning of the line (n) up
#define ANSI_CURSOR_POSITION(n, m) CSI_CODE(#n ";" #m "H") ///< Move cursor to position (n, m)
/** @} */

//------------- Screen -------------//
/** \defgroup group_ansi_screen Screen Control
 *  @{ */
#define ANSI_ERASE_SCREEN(n)     CSI_CODE(#n "J") ///< Erase the screen
#define ANSI_ERASE_LINE(n)       CSI_CODE(#n "K") ///< Erase the line (n)
#define ANSI_SCROLL_UP(n)        CSI_CODE(#n "S") ///< Scroll the whole page up (n) lines
#define ANSI_SCROLL_DOWN(n)      CSI_CODE(#n "T") ///< Scroll the whole page down (n) lines
/** @} */

//------------- Text Color -------------//
/** \defgroup group_ansi_text Text Color
 *  @{ */
#define ANSI_TEXT_BLACK          CSI_SGR(30)
#define ANSI_TEXT_RED            CSI_SGR(31)
#define ANSI_TEXT_GREEN          CSI_SGR(32)
#define ANSI_TEXT_YELLOW         CSI_SGR(33)
#define ANSI_TEXT_BLUE           CSI_SGR(34)
#define ANSI_TEXT_MAGENTA        CSI_SGR(35)
#define ANSI_TEXT_CYAN           CSI_SGR(36)
#define ANSI_TEXT_WHITE          CSI_SGR(37)
#define ANSI_TEXT_DEFAULT        CSI_SGR(39)
/** @} */

//------------- Background Color -------------//
/** \defgroup group_ansi_background Background Color
 *  @{ */
#define ANSI_BG_BLACK            CSI_SGR(40)
#define ANSI_BG_RED              CSI_SGR(41)
#define ANSI_BG_GREEN            CSI_SGR(42)
#define ANSI_BG_YELLOW           CSI_SGR(43)
#define ANSI_BG_BLUE             CSI_SGR(44)
#define ANSI_BG_MAGENTA          CSI_SGR(45)
#define ANSI_BG_CYAN             CSI_SGR(46)
#define ANSI_BG_WHITE            CSI_SGR(47)
#define ANSI_BG_DEFAULT          CSI_SGR(49)
/** @} */

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_ANSI_ESC_CODE_H_ */

/** @} */
