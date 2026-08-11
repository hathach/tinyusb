/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Host-side stand-in for lib/SystemView/SYSVIEW/SEGGER_SYSVIEW.h, used ONLY by
// test_tusb_sysview.c (the only unit test that forces CFG_TUD_SYSVIEW nonzero -- every other
// test leaves it at the tusb_option.h default of 0, so common/tusb_sysview.h's
// `#include "SEGGER_SYSVIEW.h"` never triggers for them and this file is never reached).
// The real vendor header pulls in its own SEGGER.h/SEGGER_SYSVIEW_ConfDefaults.h chain, written
// for an embedded target -- not worth dragging onto the host just to unit-test a depth counter.
// Declares exactly the surface src/common/tusb_sysview.c references; test_tusb_sysview.c
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
int  SEGGER_SYSVIEW_IsStarted(void);
void SEGGER_SYSVIEW_RecordEnterISR(void);
void SEGGER_SYSVIEW_RecordExitISR(void);
void SEGGER_SYSVIEW_RecordVoid(unsigned int Id);
void SEGGER_SYSVIEW_RecordEndCall(unsigned int Id);
void SEGGER_SYSVIEW_HeapDefine(void *pHeap, void *pBase, unsigned int HeapSize, unsigned int MetadataSize);
void SEGGER_SYSVIEW_HeapAlloc(void *pHeap, void *pUserData, unsigned int UserDataLen);
void SEGGER_SYSVIEW_HeapFree(void *pHeap, void *pUserData);
void SEGGER_SYSVIEW_NameResource(U32 ResourceId, const char *sName);

#endif
