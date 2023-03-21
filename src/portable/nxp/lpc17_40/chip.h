/*
	Name: chip.h
	Description: TinyUSB CDC for µCNC. This file adds the needed definitions and convertions to make tinyUSB compile on Arduino.

	Copyright: Copyright (c) João Martins
	Author: João Martins
	Date: 21-03-2023

	µCNC is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. Please see <http://www.gnu.org/licenses/>

	µCNC is distributed WITHOUT ANY WARRANTY;
	Also without the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the	GNU General Public License for more details.
*/

#ifndef _CHIP_H_
#define _CHIP_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "LPC17xx.h"

#define EoTIntClr USBEoTIntClr
#define EoTIntSt USBEoTIntSt
#define EpInd USBEpInd
#define EpDMADis USBEpDMADis
#define EpDMAEn USBEpDMAEn
#define EpIntClr USBEpIntClr
#define EpIntSt USBEpIntSt
#define EpIntPri USBEpIntPri
#define EpIntEn USBEpIntEn
#define DevIntClr USBDevIntClr
#define DevIntEn USBDevIntEn
#define DevIntSt USBDevIntSt
#define DMARSet USBDMARSet
#define DMAIntEn USBDMAIntEn
#define DMAIntSt USBDMAIntSt
#define DMARClr USBDMARClr
#define RxData USBRxData
#define RxPLen USBRxPLen
#define Ctrl USBCtrl
#define TxData USBTxData
#define TxPLen USBTxPLen
#define UDCAH USBUDCAH
#define NDDRIntClr USBNDDRIntClr
#define SysErrIntClr USBSysErrIntClr
#define MaxPSize USBMaxPSize
#define ReEp USBReEp
#define CmdCode USBCmdCode
#define CmdData USBCmdData

#ifdef __cplusplus
}
#endif

#endif