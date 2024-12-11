;/*
; * FreeRTOS Kernel <DEVELOPMENT BRANCH>
; * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
; *
; * SPDX-License-Identifier: MIT
; *
; * Permission is hereby granted, free of charge, to any person obtaining a copy of
; * this software and associated documentation files (the "Software"), to deal in
; * the Software without restriction, including without limitation the rights to
; * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
; * the Software, and to permit persons to whom the Software is furnished to do so,
; * subject to the following conditions:
; *
; * The above copyright notice and this permission notice shall be included in all
; * copies or substantial portions of the Software.
; *
; * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
; * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
; * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
; * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
; * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
; *
; * https://www.FreeRTOS.org
; * https://github.com/FreeRTOS
; *
; */

#include "portmacro.h"

    EXTERN    _vTaskSwitchContext
    EXTERN    _xTaskIncrementTick

    EXTERN    _interrupt_vector_table

    PUBLIC    _vPortYield
    PUBLIC    _vPortStartFirstTask
    PUBLIC    _vPortTickISR

#if !defined(__IASMRL78__) || (__VER__ < 310)
    #error "This port requires the IAR Assembler for RL78 version 3.10 or later."
#endif

;-------------------------------------------------------------------------------
;   FreeRTOS yield handler.  This is installed as the BRK software interrupt
;   handler.
;-------------------------------------------------------------------------------
    SECTION  `.text`:CODE:ROOT(1)
_vPortYield:
    portSAVE_CONTEXT               ; Save the context of the current task.
    RCALL    (_vTaskSwitchContext) ; Call the scheduler to select the next task.
    portRESTORE_CONTEXT            ; Restore the context of the next task to run.
    RETB
;-------------------------------------------------------------------------------


;-------------------------------------------------------------------------------
;   Starts the scheduler by restoring the context of the task that will execute
;   first.
;-------------------------------------------------------------------------------
    SECTION  `.text`:CODE:ROOT(1)
_vPortStartFirstTask:
    portRESTORE_CONTEXT            ; Restore the context of whichever task the ...
    RETI                           ; An interrupt stack frame is used so the
                                   ; task is started using a RETI instruction.
;-------------------------------------------------------------------------------


;-------------------------------------------------------------------------------
;   FreeRTOS Timer Tick handler.
;   This is installed as the interval timer interrupt handler.
;-------------------------------------------------------------------------------
    SECTION  `.text`:CODE:ROOT(1)
_vPortTickISR:
    portSAVE_CONTEXT               ; Save the context of the current task.
    RCALL    (_xTaskIncrementTick) ; Call the timer tick function.
    CMPW    AX, #0x00
    SKZ
    RCALL    (_vTaskSwitchContext) ; Call the scheduler to select the next task.
    portRESTORE_CONTEXT            ; Restore the context of the next task to run.
    RETI
;-------------------------------------------------------------------------------

    END
