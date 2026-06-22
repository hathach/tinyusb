/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v10x_it.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2022/08/20
 * Description        : This file contains the headers of the interrupt handlers.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#ifndef __CH32V10x_IT_H
#define __CH32V10x_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v10x.h"

void USBHD_IRQHandler(void);
void USBWakeUp_IRQHandler(void);
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __CH32V10x_IT_H */
