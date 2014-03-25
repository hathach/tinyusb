/**************************************************************************/
/*!
    @file     msc_device_app.h
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup group_demo
 *  \defgroup Mass Storage Device App
 *  @{ */

#ifndef _TUSB_MSCD_DEVICE_APP_H_
#define _TUSB_MSCD_DEVICE_APP_H_

#include "board.h"
#include "tusb.h"

#ifdef __cplusplus
 extern "C" {
#endif

#if TUSB_CFG_DEVICE_MSC

enum
{
  DISK_BLOCK_NUM  = 16, // 8KB is the smallest size that windows allow to mount
  DISK_BLOCK_SIZE = 512
};

#define README_CONTENTS \
"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

#if TUSB_CFG_MCU==MCU_LPC11UXX || TUSB_CFG_MCU==MCU_LPC13UXX
  #define MSCD_APP_ROMDISK
#else // defaults is ram disk
  #define MSCD_APP_RAMDISK
#endif

void msc_device_app_init(void);
OSAL_TASK_FUNCTION( msc_device_app_task , p_task_para);

extern scsi_sense_fixed_data_t mscd_sense_data;

#else

#define msc_device_app_init()
#define msc_device_app_task(x)

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSCD_DEVICE_APP_H_ */

/** @} */
