/**************************************************************************/
/*!
    @file     mcu_capacity.h
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

#ifndef _TUSB_MCU_CAPACITY_H_
#define _TUSB_MCU_CAPACITY_H_

#ifdef __cplusplus
 extern "C" {
#endif

// CAP is abbreviation for Capacity

//--------------------------------------------------------------------+
// ARCHTECTURE
//--------------------------------------------------------------------+



//--------------------------------------------------------------------+
// Controller
//--------------------------------------------------------------------+
#if TUSB_CFG_MCU == MCU_LPC43XX || TUSB_CFG_MCU == MCU_LPC18XX
  #define CAP_CONTROLLER_NUMBER 2
#else
  #define CAP_CONTROLLER_NUMBER 1
#endif

#define CAP_MODE_DEVICE
#if TUSB_CFG_MCU == MCU_LPC43XX || TUSB_CFG_MCU == MCU_LPC18XX || TUSB_CFG_MCU == MCU_LPC175X_6X
  #define CAP_MODE_HOST
#endif

//--------------------------------------------------------------------+
// Validation
//--------------------------------------------------------------------+
#if (CAP_CONTROLLER_NUMBER == 1) && ( defined TUSB_CFG_CONTROLLER_1_MODE)
 #error current MCU does not have the required number of controllers
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MCU_CAPACITY_H_ */
