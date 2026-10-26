/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// Host-side stand-in for lib/SystemView/SYSVIEW/SEGGER_SYSVIEW.h, used ONLY by
// test_tu_sysview.c (the only unit test that forces CFG_TUD_SYSVIEW nonzero -- every other
// test leaves it at the tusb_option.h default of 0, so common/tusb_debug.h's
// `#include "SEGGER_SYSVIEW.h"` never triggers for them and this file is never reached).
// The real vendor header pulls in its own SEGGER.h/SEGGER_SYSVIEW_ConfDefaults.h chain, written
// for an embedded target -- not worth dragging onto the host just to unit-test a depth counter.
// Declares exactly the surface src/tusb.c references; test_tu_sysview.c
// provides the definitions (hand-written stubs, not CMock-generated).
#ifndef SEGGER_SYSVIEW_H
#define SEGGER_SYSVIEW_H

#include <stdint.h>

typedef uint32_t U32;

typedef struct SEGGER_SYSVIEW_OS_API SEGGER_SYSVIEW_OS_API; // opaque: only ever used by pointer
typedef void (SEGGER_SYSVIEW_SEND_SYS_DESC_FUNC)(void);

void SEGGER_SYSVIEW_Init(U32 SysFreq, U32 CPUFreq, const SEGGER_SYSVIEW_OS_API *pOSAPI,
                          SEGGER_SYSVIEW_SEND_SYS_DESC_FUNC pfSendSysDesc);
void SEGGER_SYSVIEW_SetRAMBase(U32 RAMBaseAddress);
void SEGGER_SYSVIEW_Start(void);
void SEGGER_SYSVIEW_DisableEvents(U32 DisableMask);
void SEGGER_SYSVIEW_SendSysDesc(const char *sSysDesc);
void SEGGER_SYSVIEW_RecordEnterISR(void);
void SEGGER_SYSVIEW_RecordExitISR(void);
void SEGGER_SYSVIEW_RecordVoid(unsigned int Id);
void SEGGER_SYSVIEW_RecordEndCall(unsigned int Id);
void SEGGER_SYSVIEW_NameResource(U32 ResourceId, const char *sName);

#endif
