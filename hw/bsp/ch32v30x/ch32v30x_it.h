/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32v30x_it.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/06/06
* Description        : This file contains the headers of the interrupt handlers.
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* SPDX-License-Identifier: Apache-2.0
*******************************************************************************/
#ifndef __CH32V30x_IT_H
#define __CH32V30x_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v30x.h"

void USBHS_IRQHandler(void);
void OTG_FS_IRQHandler(void);
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif


#endif /* __CH32V30x_IT_H */
