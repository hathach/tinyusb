/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32x035_it.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/04/06
 * Description        : This file contains the headers of the interrupt handlers.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#ifndef __CH32X035_IT_H
#define __CH32X035_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32x035.h"

void USBFS_IRQHandler(void);
void USBFSWakeUp_IRQHandler(void);
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif

#endif
