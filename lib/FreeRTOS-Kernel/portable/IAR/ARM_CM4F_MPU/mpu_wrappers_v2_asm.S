/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* Including FreeRTOSConfig.h here will cause build errors if the header file
 * contains code not understood by the assembler - for example the 'extern' keyword.
 * To avoid errors place any such code inside a #ifdef __ICCARM__/#endif block so
 * the code is included in C files but excluded by the preprocessor in assembly
 * files (__ICCARM__ is defined by the IAR C compiler but not by the IAR assembler. */
#include "FreeRTOSConfig.h"
#include "mpu_syscall_numbers.h"

    SECTION freertos_system_calls:CODE:NOROOT(2)
    THUMB
/*-----------------------------------------------------------*/

#ifndef configUSE_MPU_WRAPPERS_V1
    #define configUSE_MPU_WRAPPERS_V1 0
#endif

/*-----------------------------------------------------------*/

#if ( configUSE_MPU_WRAPPERS_V1 == 0 )

    PUBLIC MPU_xTaskDelayUntil
MPU_xTaskDelayUntil:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskDelayUntil_Unpriv
    MPU_xTaskDelayUntil_Priv:
        b MPU_xTaskDelayUntilImpl
    MPU_xTaskDelayUntil_Unpriv:
        svc #SYSTEM_CALL_xTaskDelayUntil
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskAbortDelay
MPU_xTaskAbortDelay:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskAbortDelay_Unpriv
    MPU_xTaskAbortDelay_Priv:
        b MPU_xTaskAbortDelayImpl
    MPU_xTaskAbortDelay_Unpriv:
        svc #SYSTEM_CALL_xTaskAbortDelay
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTaskDelay
MPU_vTaskDelay:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTaskDelay_Unpriv
    MPU_vTaskDelay_Priv:
        b MPU_vTaskDelayImpl
    MPU_vTaskDelay_Unpriv:
        svc #SYSTEM_CALL_vTaskDelay
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxTaskPriorityGet
MPU_uxTaskPriorityGet:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxTaskPriorityGet_Unpriv
    MPU_uxTaskPriorityGet_Priv:
        b MPU_uxTaskPriorityGetImpl
    MPU_uxTaskPriorityGet_Unpriv:
        svc #SYSTEM_CALL_uxTaskPriorityGet
/*-----------------------------------------------------------*/

    PUBLIC MPU_eTaskGetState
MPU_eTaskGetState:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_eTaskGetState_Unpriv
    MPU_eTaskGetState_Priv:
        b MPU_eTaskGetStateImpl
    MPU_eTaskGetState_Unpriv:
        svc #SYSTEM_CALL_eTaskGetState
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTaskGetInfo
MPU_vTaskGetInfo:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTaskGetInfo_Unpriv
    MPU_vTaskGetInfo_Priv:
        b MPU_vTaskGetInfoImpl
    MPU_vTaskGetInfo_Unpriv:
        svc #SYSTEM_CALL_vTaskGetInfo
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskGetIdleTaskHandle
MPU_xTaskGetIdleTaskHandle:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskGetIdleTaskHandle_Unpriv
    MPU_xTaskGetIdleTaskHandle_Priv:
        b MPU_xTaskGetIdleTaskHandleImpl
    MPU_xTaskGetIdleTaskHandle_Unpriv:
        svc #SYSTEM_CALL_xTaskGetIdleTaskHandle
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTaskSuspend
MPU_vTaskSuspend:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTaskSuspend_Unpriv
    MPU_vTaskSuspend_Priv:
        b MPU_vTaskSuspendImpl
    MPU_vTaskSuspend_Unpriv:
        svc #SYSTEM_CALL_vTaskSuspend
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTaskResume
MPU_vTaskResume:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTaskResume_Unpriv
    MPU_vTaskResume_Priv:
        b MPU_vTaskResumeImpl
    MPU_vTaskResume_Unpriv:
        svc #SYSTEM_CALL_vTaskResume
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskGetTickCount
MPU_xTaskGetTickCount:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskGetTickCount_Unpriv
    MPU_xTaskGetTickCount_Priv:
        b MPU_xTaskGetTickCountImpl
    MPU_xTaskGetTickCount_Unpriv:
        svc #SYSTEM_CALL_xTaskGetTickCount
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxTaskGetNumberOfTasks
MPU_uxTaskGetNumberOfTasks:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxTaskGetNumberOfTasks_Unpriv
    MPU_uxTaskGetNumberOfTasks_Priv:
        b MPU_uxTaskGetNumberOfTasksImpl
    MPU_uxTaskGetNumberOfTasks_Unpriv:
        svc #SYSTEM_CALL_uxTaskGetNumberOfTasks
/*-----------------------------------------------------------*/

    PUBLIC MPU_ulTaskGetRunTimeCounter
MPU_ulTaskGetRunTimeCounter:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_ulTaskGetRunTimeCounter_Unpriv
    MPU_ulTaskGetRunTimeCounter_Priv:
        b MPU_ulTaskGetRunTimeCounterImpl
    MPU_ulTaskGetRunTimeCounter_Unpriv:
        svc #SYSTEM_CALL_ulTaskGetRunTimeCounter
/*-----------------------------------------------------------*/

    PUBLIC MPU_ulTaskGetRunTimePercent
MPU_ulTaskGetRunTimePercent:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_ulTaskGetRunTimePercent_Unpriv
    MPU_ulTaskGetRunTimePercent_Priv:
        b MPU_ulTaskGetRunTimePercentImpl
    MPU_ulTaskGetRunTimePercent_Unpriv:
        svc #SYSTEM_CALL_ulTaskGetRunTimePercent
/*-----------------------------------------------------------*/

    PUBLIC MPU_ulTaskGetIdleRunTimePercent
MPU_ulTaskGetIdleRunTimePercent:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_ulTaskGetIdleRunTimePercent_Unpriv
    MPU_ulTaskGetIdleRunTimePercent_Priv:
        b MPU_ulTaskGetIdleRunTimePercentImpl
    MPU_ulTaskGetIdleRunTimePercent_Unpriv:
        svc #SYSTEM_CALL_ulTaskGetIdleRunTimePercent
/*-----------------------------------------------------------*/

    PUBLIC MPU_ulTaskGetIdleRunTimeCounter
MPU_ulTaskGetIdleRunTimeCounter:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_ulTaskGetIdleRunTimeCounter_Unpriv
    MPU_ulTaskGetIdleRunTimeCounter_Priv:
        b MPU_ulTaskGetIdleRunTimeCounterImpl
    MPU_ulTaskGetIdleRunTimeCounter_Unpriv:
        svc #SYSTEM_CALL_ulTaskGetIdleRunTimeCounter
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTaskSetApplicationTaskTag
MPU_vTaskSetApplicationTaskTag:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTaskSetApplicationTaskTag_Unpriv
    MPU_vTaskSetApplicationTaskTag_Priv:
        b MPU_vTaskSetApplicationTaskTagImpl
    MPU_vTaskSetApplicationTaskTag_Unpriv:
        svc #SYSTEM_CALL_vTaskSetApplicationTaskTag
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskGetApplicationTaskTag
MPU_xTaskGetApplicationTaskTag:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskGetApplicationTaskTag_Unpriv
    MPU_xTaskGetApplicationTaskTag_Priv:
        b MPU_xTaskGetApplicationTaskTagImpl
    MPU_xTaskGetApplicationTaskTag_Unpriv:
        svc #SYSTEM_CALL_xTaskGetApplicationTaskTag
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTaskSetThreadLocalStoragePointer
MPU_vTaskSetThreadLocalStoragePointer:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTaskSetThreadLocalStoragePointer_Unpriv
    MPU_vTaskSetThreadLocalStoragePointer_Priv:
        b MPU_vTaskSetThreadLocalStoragePointerImpl
    MPU_vTaskSetThreadLocalStoragePointer_Unpriv:
        svc #SYSTEM_CALL_vTaskSetThreadLocalStoragePointer
/*-----------------------------------------------------------*/

    PUBLIC MPU_pvTaskGetThreadLocalStoragePointer
MPU_pvTaskGetThreadLocalStoragePointer:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_pvTaskGetThreadLocalStoragePointer_Unpriv
    MPU_pvTaskGetThreadLocalStoragePointer_Priv:
        b MPU_pvTaskGetThreadLocalStoragePointerImpl
    MPU_pvTaskGetThreadLocalStoragePointer_Unpriv:
        svc #SYSTEM_CALL_pvTaskGetThreadLocalStoragePointer
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxTaskGetSystemState
MPU_uxTaskGetSystemState:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxTaskGetSystemState_Unpriv
    MPU_uxTaskGetSystemState_Priv:
        b MPU_uxTaskGetSystemStateImpl
    MPU_uxTaskGetSystemState_Unpriv:
        svc #SYSTEM_CALL_uxTaskGetSystemState
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxTaskGetStackHighWaterMark
MPU_uxTaskGetStackHighWaterMark:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxTaskGetStackHighWaterMark_Unpriv
    MPU_uxTaskGetStackHighWaterMark_Priv:
        b MPU_uxTaskGetStackHighWaterMarkImpl
    MPU_uxTaskGetStackHighWaterMark_Unpriv:
        svc #SYSTEM_CALL_uxTaskGetStackHighWaterMark
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxTaskGetStackHighWaterMark2
MPU_uxTaskGetStackHighWaterMark2:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxTaskGetStackHighWaterMark2_Unpriv
    MPU_uxTaskGetStackHighWaterMark2_Priv:
        b MPU_uxTaskGetStackHighWaterMark2Impl
    MPU_uxTaskGetStackHighWaterMark2_Unpriv:
        svc #SYSTEM_CALL_uxTaskGetStackHighWaterMark2
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskGetCurrentTaskHandle
MPU_xTaskGetCurrentTaskHandle:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskGetCurrentTaskHandle_Unpriv
    MPU_xTaskGetCurrentTaskHandle_Priv:
        b MPU_xTaskGetCurrentTaskHandleImpl
    MPU_xTaskGetCurrentTaskHandle_Unpriv:
        svc #SYSTEM_CALL_xTaskGetCurrentTaskHandle
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskGetSchedulerState
MPU_xTaskGetSchedulerState:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskGetSchedulerState_Unpriv
    MPU_xTaskGetSchedulerState_Priv:
        b MPU_xTaskGetSchedulerStateImpl
    MPU_xTaskGetSchedulerState_Unpriv:
        svc #SYSTEM_CALL_xTaskGetSchedulerState
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTaskSetTimeOutState
MPU_vTaskSetTimeOutState:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTaskSetTimeOutState_Unpriv
    MPU_vTaskSetTimeOutState_Priv:
        b MPU_vTaskSetTimeOutStateImpl
    MPU_vTaskSetTimeOutState_Unpriv:
        svc #SYSTEM_CALL_vTaskSetTimeOutState
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskCheckForTimeOut
MPU_xTaskCheckForTimeOut:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskCheckForTimeOut_Unpriv
    MPU_xTaskCheckForTimeOut_Priv:
        b MPU_xTaskCheckForTimeOutImpl
    MPU_xTaskCheckForTimeOut_Unpriv:
        svc #SYSTEM_CALL_xTaskCheckForTimeOut
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskGenericNotifyEntry
MPU_xTaskGenericNotifyEntry:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskGenericNotify_Unpriv
    MPU_xTaskGenericNotify_Priv:
        b MPU_xTaskGenericNotifyImpl
    MPU_xTaskGenericNotify_Unpriv:
        svc #SYSTEM_CALL_xTaskGenericNotify
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskGenericNotifyWaitEntry
MPU_xTaskGenericNotifyWaitEntry:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskGenericNotifyWait_Unpriv
    MPU_xTaskGenericNotifyWait_Priv:
        b MPU_xTaskGenericNotifyWaitImpl
    MPU_xTaskGenericNotifyWait_Unpriv:
        svc #SYSTEM_CALL_xTaskGenericNotifyWait
/*-----------------------------------------------------------*/

    PUBLIC MPU_ulTaskGenericNotifyTake
MPU_ulTaskGenericNotifyTake:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_ulTaskGenericNotifyTake_Unpriv
    MPU_ulTaskGenericNotifyTake_Priv:
        b MPU_ulTaskGenericNotifyTakeImpl
    MPU_ulTaskGenericNotifyTake_Unpriv:
        svc #SYSTEM_CALL_ulTaskGenericNotifyTake
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTaskGenericNotifyStateClear
MPU_xTaskGenericNotifyStateClear:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTaskGenericNotifyStateClear_Unpriv
    MPU_xTaskGenericNotifyStateClear_Priv:
        b MPU_xTaskGenericNotifyStateClearImpl
    MPU_xTaskGenericNotifyStateClear_Unpriv:
        svc #SYSTEM_CALL_xTaskGenericNotifyStateClear
/*-----------------------------------------------------------*/

    PUBLIC MPU_ulTaskGenericNotifyValueClear
MPU_ulTaskGenericNotifyValueClear:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_ulTaskGenericNotifyValueClear_Unpriv
    MPU_ulTaskGenericNotifyValueClear_Priv:
        b MPU_ulTaskGenericNotifyValueClearImpl
    MPU_ulTaskGenericNotifyValueClear_Unpriv:
        svc #SYSTEM_CALL_ulTaskGenericNotifyValueClear
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueueGenericSend
MPU_xQueueGenericSend:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueueGenericSend_Unpriv
    MPU_xQueueGenericSend_Priv:
        b MPU_xQueueGenericSendImpl
    MPU_xQueueGenericSend_Unpriv:
        svc #SYSTEM_CALL_xQueueGenericSend
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxQueueMessagesWaiting
MPU_uxQueueMessagesWaiting:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxQueueMessagesWaiting_Unpriv
    MPU_uxQueueMessagesWaiting_Priv:
        b MPU_uxQueueMessagesWaitingImpl
    MPU_uxQueueMessagesWaiting_Unpriv:
        svc #SYSTEM_CALL_uxQueueMessagesWaiting
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxQueueSpacesAvailable
MPU_uxQueueSpacesAvailable:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxQueueSpacesAvailable_Unpriv
    MPU_uxQueueSpacesAvailable_Priv:
        b MPU_uxQueueSpacesAvailableImpl
    MPU_uxQueueSpacesAvailable_Unpriv:
        svc #SYSTEM_CALL_uxQueueSpacesAvailable
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueueReceive
MPU_xQueueReceive:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueueReceive_Unpriv
    MPU_xQueueReceive_Priv:
        b MPU_xQueueReceiveImpl
    MPU_xQueueReceive_Unpriv:
        svc #SYSTEM_CALL_xQueueReceive
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueuePeek
MPU_xQueuePeek:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueuePeek_Unpriv
    MPU_xQueuePeek_Priv:
        b MPU_xQueuePeekImpl
    MPU_xQueuePeek_Unpriv:
        svc #SYSTEM_CALL_xQueuePeek
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueueSemaphoreTake
MPU_xQueueSemaphoreTake:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueueSemaphoreTake_Unpriv
    MPU_xQueueSemaphoreTake_Priv:
        b MPU_xQueueSemaphoreTakeImpl
    MPU_xQueueSemaphoreTake_Unpriv:
        svc #SYSTEM_CALL_xQueueSemaphoreTake
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueueGetMutexHolder
MPU_xQueueGetMutexHolder:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueueGetMutexHolder_Unpriv
    MPU_xQueueGetMutexHolder_Priv:
        b MPU_xQueueGetMutexHolderImpl
    MPU_xQueueGetMutexHolder_Unpriv:
        svc #SYSTEM_CALL_xQueueGetMutexHolder
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueueTakeMutexRecursive
MPU_xQueueTakeMutexRecursive:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueueTakeMutexRecursive_Unpriv
    MPU_xQueueTakeMutexRecursive_Priv:
        b MPU_xQueueTakeMutexRecursiveImpl
    MPU_xQueueTakeMutexRecursive_Unpriv:
        svc #SYSTEM_CALL_xQueueTakeMutexRecursive
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueueGiveMutexRecursive
MPU_xQueueGiveMutexRecursive:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueueGiveMutexRecursive_Unpriv
    MPU_xQueueGiveMutexRecursive_Priv:
        b MPU_xQueueGiveMutexRecursiveImpl
    MPU_xQueueGiveMutexRecursive_Unpriv:
        svc #SYSTEM_CALL_xQueueGiveMutexRecursive
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueueSelectFromSet
MPU_xQueueSelectFromSet:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueueSelectFromSet_Unpriv
    MPU_xQueueSelectFromSet_Priv:
        b MPU_xQueueSelectFromSetImpl
    MPU_xQueueSelectFromSet_Unpriv:
        svc #SYSTEM_CALL_xQueueSelectFromSet
/*-----------------------------------------------------------*/

    PUBLIC MPU_xQueueAddToSet
MPU_xQueueAddToSet:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xQueueAddToSet_Unpriv
    MPU_xQueueAddToSet_Priv:
        b MPU_xQueueAddToSetImpl
    MPU_xQueueAddToSet_Unpriv:
        svc #SYSTEM_CALL_xQueueAddToSet
/*-----------------------------------------------------------*/

    PUBLIC MPU_vQueueAddToRegistry
MPU_vQueueAddToRegistry:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vQueueAddToRegistry_Unpriv
    MPU_vQueueAddToRegistry_Priv:
        b MPU_vQueueAddToRegistryImpl
    MPU_vQueueAddToRegistry_Unpriv:
        svc #SYSTEM_CALL_vQueueAddToRegistry
/*-----------------------------------------------------------*/

    PUBLIC MPU_vQueueUnregisterQueue
MPU_vQueueUnregisterQueue:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vQueueUnregisterQueue_Unpriv
    MPU_vQueueUnregisterQueue_Priv:
        b MPU_vQueueUnregisterQueueImpl
    MPU_vQueueUnregisterQueue_Unpriv:
        svc #SYSTEM_CALL_vQueueUnregisterQueue
/*-----------------------------------------------------------*/

    PUBLIC MPU_pcQueueGetName
MPU_pcQueueGetName:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_pcQueueGetName_Unpriv
    MPU_pcQueueGetName_Priv:
        b MPU_pcQueueGetNameImpl
    MPU_pcQueueGetName_Unpriv:
        svc #SYSTEM_CALL_pcQueueGetName
/*-----------------------------------------------------------*/

    PUBLIC MPU_pvTimerGetTimerID
MPU_pvTimerGetTimerID:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_pvTimerGetTimerID_Unpriv
    MPU_pvTimerGetTimerID_Priv:
        b MPU_pvTimerGetTimerIDImpl
    MPU_pvTimerGetTimerID_Unpriv:
        svc #SYSTEM_CALL_pvTimerGetTimerID
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTimerSetTimerID
MPU_vTimerSetTimerID:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTimerSetTimerID_Unpriv
    MPU_vTimerSetTimerID_Priv:
        b MPU_vTimerSetTimerIDImpl
    MPU_vTimerSetTimerID_Unpriv:
        svc #SYSTEM_CALL_vTimerSetTimerID
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTimerIsTimerActive
MPU_xTimerIsTimerActive:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTimerIsTimerActive_Unpriv
    MPU_xTimerIsTimerActive_Priv:
        b MPU_xTimerIsTimerActiveImpl
    MPU_xTimerIsTimerActive_Unpriv:
        svc #SYSTEM_CALL_xTimerIsTimerActive
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTimerGetTimerDaemonTaskHandle
MPU_xTimerGetTimerDaemonTaskHandle:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTimerGetTimerDaemonTaskHandle_Unpriv
    MPU_xTimerGetTimerDaemonTaskHandle_Priv:
        b MPU_xTimerGetTimerDaemonTaskHandleImpl
    MPU_xTimerGetTimerDaemonTaskHandle_Unpriv:
        svc #SYSTEM_CALL_xTimerGetTimerDaemonTaskHandle
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTimerGenericCommandFromTaskEntry
MPU_xTimerGenericCommandFromTaskEntry:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTimerGenericCommandFromTask_Unpriv
    MPU_xTimerGenericCommandFromTask_Priv:
        b MPU_xTimerGenericCommandFromTaskImpl
    MPU_xTimerGenericCommandFromTask_Unpriv:
        svc #SYSTEM_CALL_xTimerGenericCommandFromTask
/*-----------------------------------------------------------*/

    PUBLIC MPU_pcTimerGetName
MPU_pcTimerGetName:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_pcTimerGetName_Unpriv
    MPU_pcTimerGetName_Priv:
        b MPU_pcTimerGetNameImpl
    MPU_pcTimerGetName_Unpriv:
        svc #SYSTEM_CALL_pcTimerGetName
/*-----------------------------------------------------------*/

    PUBLIC MPU_vTimerSetReloadMode
MPU_vTimerSetReloadMode:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vTimerSetReloadMode_Unpriv
    MPU_vTimerSetReloadMode_Priv:
        b MPU_vTimerSetReloadModeImpl
    MPU_vTimerSetReloadMode_Unpriv:
        svc #SYSTEM_CALL_vTimerSetReloadMode
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTimerGetReloadMode
MPU_xTimerGetReloadMode:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTimerGetReloadMode_Unpriv
    MPU_xTimerGetReloadMode_Priv:
        b MPU_xTimerGetReloadModeImpl
    MPU_xTimerGetReloadMode_Unpriv:
        svc #SYSTEM_CALL_xTimerGetReloadMode
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxTimerGetReloadMode
MPU_uxTimerGetReloadMode:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxTimerGetReloadMode_Unpriv
    MPU_uxTimerGetReloadMode_Priv:
        b MPU_uxTimerGetReloadModeImpl
    MPU_uxTimerGetReloadMode_Unpriv:
        svc #SYSTEM_CALL_uxTimerGetReloadMode
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTimerGetPeriod
MPU_xTimerGetPeriod:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTimerGetPeriod_Unpriv
    MPU_xTimerGetPeriod_Priv:
        b MPU_xTimerGetPeriodImpl
    MPU_xTimerGetPeriod_Unpriv:
        svc #SYSTEM_CALL_xTimerGetPeriod
/*-----------------------------------------------------------*/

    PUBLIC MPU_xTimerGetExpiryTime
MPU_xTimerGetExpiryTime:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xTimerGetExpiryTime_Unpriv
    MPU_xTimerGetExpiryTime_Priv:
        b MPU_xTimerGetExpiryTimeImpl
    MPU_xTimerGetExpiryTime_Unpriv:
        svc #SYSTEM_CALL_xTimerGetExpiryTime
/*-----------------------------------------------------------*/

    PUBLIC MPU_xEventGroupWaitBitsEntry
MPU_xEventGroupWaitBitsEntry:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xEventGroupWaitBits_Unpriv
    MPU_xEventGroupWaitBits_Priv:
        b MPU_xEventGroupWaitBitsImpl
    MPU_xEventGroupWaitBits_Unpriv:
        svc #SYSTEM_CALL_xEventGroupWaitBits
/*-----------------------------------------------------------*/

    PUBLIC MPU_xEventGroupClearBits
MPU_xEventGroupClearBits:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xEventGroupClearBits_Unpriv
    MPU_xEventGroupClearBits_Priv:
        b MPU_xEventGroupClearBitsImpl
    MPU_xEventGroupClearBits_Unpriv:
        svc #SYSTEM_CALL_xEventGroupClearBits
/*-----------------------------------------------------------*/

    PUBLIC MPU_xEventGroupSetBits
MPU_xEventGroupSetBits:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xEventGroupSetBits_Unpriv
    MPU_xEventGroupSetBits_Priv:
        b MPU_xEventGroupSetBitsImpl
    MPU_xEventGroupSetBits_Unpriv:
        svc #SYSTEM_CALL_xEventGroupSetBits
/*-----------------------------------------------------------*/

    PUBLIC MPU_xEventGroupSync
MPU_xEventGroupSync:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xEventGroupSync_Unpriv
    MPU_xEventGroupSync_Priv:
        b MPU_xEventGroupSyncImpl
    MPU_xEventGroupSync_Unpriv:
        svc #SYSTEM_CALL_xEventGroupSync
/*-----------------------------------------------------------*/

    PUBLIC MPU_uxEventGroupGetNumber
MPU_uxEventGroupGetNumber:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_uxEventGroupGetNumber_Unpriv
    MPU_uxEventGroupGetNumber_Priv:
        b MPU_uxEventGroupGetNumberImpl
    MPU_uxEventGroupGetNumber_Unpriv:
        svc #SYSTEM_CALL_uxEventGroupGetNumber
/*-----------------------------------------------------------*/

    PUBLIC MPU_vEventGroupSetNumber
MPU_vEventGroupSetNumber:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_vEventGroupSetNumber_Unpriv
    MPU_vEventGroupSetNumber_Priv:
        b MPU_vEventGroupSetNumberImpl
    MPU_vEventGroupSetNumber_Unpriv:
        svc #SYSTEM_CALL_vEventGroupSetNumber
/*-----------------------------------------------------------*/

    PUBLIC MPU_xStreamBufferSend
MPU_xStreamBufferSend:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xStreamBufferSend_Unpriv
    MPU_xStreamBufferSend_Priv:
        b MPU_xStreamBufferSendImpl
    MPU_xStreamBufferSend_Unpriv:
        svc #SYSTEM_CALL_xStreamBufferSend
/*-----------------------------------------------------------*/

    PUBLIC MPU_xStreamBufferReceive
MPU_xStreamBufferReceive:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xStreamBufferReceive_Unpriv
    MPU_xStreamBufferReceive_Priv:
        b MPU_xStreamBufferReceiveImpl
    MPU_xStreamBufferReceive_Unpriv:
        svc #SYSTEM_CALL_xStreamBufferReceive
/*-----------------------------------------------------------*/

    PUBLIC MPU_xStreamBufferIsFull
MPU_xStreamBufferIsFull:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xStreamBufferIsFull_Unpriv
    MPU_xStreamBufferIsFull_Priv:
        b MPU_xStreamBufferIsFullImpl
    MPU_xStreamBufferIsFull_Unpriv:
        svc #SYSTEM_CALL_xStreamBufferIsFull
/*-----------------------------------------------------------*/

    PUBLIC MPU_xStreamBufferIsEmpty
MPU_xStreamBufferIsEmpty:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xStreamBufferIsEmpty_Unpriv
    MPU_xStreamBufferIsEmpty_Priv:
        b MPU_xStreamBufferIsEmptyImpl
    MPU_xStreamBufferIsEmpty_Unpriv:
        svc #SYSTEM_CALL_xStreamBufferIsEmpty
/*-----------------------------------------------------------*/

    PUBLIC MPU_xStreamBufferSpacesAvailable
MPU_xStreamBufferSpacesAvailable:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xStreamBufferSpacesAvailable_Unpriv
    MPU_xStreamBufferSpacesAvailable_Priv:
        b MPU_xStreamBufferSpacesAvailableImpl
    MPU_xStreamBufferSpacesAvailable_Unpriv:
        svc #SYSTEM_CALL_xStreamBufferSpacesAvailable
/*-----------------------------------------------------------*/

    PUBLIC MPU_xStreamBufferBytesAvailable
MPU_xStreamBufferBytesAvailable:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xStreamBufferBytesAvailable_Unpriv
    MPU_xStreamBufferBytesAvailable_Priv:
        b MPU_xStreamBufferBytesAvailableImpl
    MPU_xStreamBufferBytesAvailable_Unpriv:
        svc #SYSTEM_CALL_xStreamBufferBytesAvailable
/*-----------------------------------------------------------*/

    PUBLIC MPU_xStreamBufferSetTriggerLevel
MPU_xStreamBufferSetTriggerLevel:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xStreamBufferSetTriggerLevel_Unpriv
    MPU_xStreamBufferSetTriggerLevel_Priv:
        b MPU_xStreamBufferSetTriggerLevelImpl
    MPU_xStreamBufferSetTriggerLevel_Unpriv:
        svc #SYSTEM_CALL_xStreamBufferSetTriggerLevel
/*-----------------------------------------------------------*/

    PUBLIC MPU_xStreamBufferNextMessageLengthBytes
MPU_xStreamBufferNextMessageLengthBytes:
    push {r0}
    mrs r0, control
    tst r0, #1
    pop {r0}
    bne MPU_xStreamBufferNextMessageLengthBytes_Unpriv
    MPU_xStreamBufferNextMessageLengthBytes_Priv:
        b MPU_xStreamBufferNextMessageLengthBytesImpl
    MPU_xStreamBufferNextMessageLengthBytes_Unpriv:
        svc #SYSTEM_CALL_xStreamBufferNextMessageLengthBytes
/*-----------------------------------------------------------*/

/* Default weak implementations in case one is not available from
 * mpu_wrappers because of config options. */

    PUBWEAK MPU_xTaskDelayUntilImpl
MPU_xTaskDelayUntilImpl:
    b MPU_xTaskDelayUntilImpl

    PUBWEAK MPU_xTaskAbortDelayImpl
MPU_xTaskAbortDelayImpl:
    b MPU_xTaskAbortDelayImpl

    PUBWEAK MPU_vTaskDelayImpl
MPU_vTaskDelayImpl:
    b MPU_vTaskDelayImpl

    PUBWEAK MPU_uxTaskPriorityGetImpl
MPU_uxTaskPriorityGetImpl:
    b MPU_uxTaskPriorityGetImpl

    PUBWEAK MPU_eTaskGetStateImpl
MPU_eTaskGetStateImpl:
    b MPU_eTaskGetStateImpl

    PUBWEAK MPU_vTaskGetInfoImpl
MPU_vTaskGetInfoImpl:
    b MPU_vTaskGetInfoImpl

    PUBWEAK MPU_xTaskGetIdleTaskHandleImpl
MPU_xTaskGetIdleTaskHandleImpl:
    b MPU_xTaskGetIdleTaskHandleImpl

    PUBWEAK MPU_vTaskSuspendImpl
MPU_vTaskSuspendImpl:
    b MPU_vTaskSuspendImpl

    PUBWEAK MPU_vTaskResumeImpl
MPU_vTaskResumeImpl:
    b MPU_vTaskResumeImpl

    PUBWEAK MPU_xTaskGetTickCountImpl
MPU_xTaskGetTickCountImpl:
    b MPU_xTaskGetTickCountImpl

    PUBWEAK MPU_uxTaskGetNumberOfTasksImpl
MPU_uxTaskGetNumberOfTasksImpl:
    b MPU_uxTaskGetNumberOfTasksImpl

    PUBWEAK MPU_ulTaskGetRunTimeCounterImpl
MPU_ulTaskGetRunTimeCounterImpl:
    b MPU_ulTaskGetRunTimeCounterImpl

    PUBWEAK MPU_ulTaskGetRunTimePercentImpl
MPU_ulTaskGetRunTimePercentImpl:
    b MPU_ulTaskGetRunTimePercentImpl

    PUBWEAK MPU_ulTaskGetIdleRunTimePercentImpl
MPU_ulTaskGetIdleRunTimePercentImpl:
    b MPU_ulTaskGetIdleRunTimePercentImpl

    PUBWEAK MPU_ulTaskGetIdleRunTimeCounterImpl
MPU_ulTaskGetIdleRunTimeCounterImpl:
    b MPU_ulTaskGetIdleRunTimeCounterImpl

    PUBWEAK MPU_vTaskSetApplicationTaskTagImpl
MPU_vTaskSetApplicationTaskTagImpl:
    b MPU_vTaskSetApplicationTaskTagImpl

    PUBWEAK MPU_xTaskGetApplicationTaskTagImpl
MPU_xTaskGetApplicationTaskTagImpl:
    b MPU_xTaskGetApplicationTaskTagImpl

    PUBWEAK MPU_vTaskSetThreadLocalStoragePointerImpl
MPU_vTaskSetThreadLocalStoragePointerImpl:
    b MPU_vTaskSetThreadLocalStoragePointerImpl

    PUBWEAK MPU_pvTaskGetThreadLocalStoragePointerImpl
MPU_pvTaskGetThreadLocalStoragePointerImpl:
    b MPU_pvTaskGetThreadLocalStoragePointerImpl

    PUBWEAK MPU_uxTaskGetSystemStateImpl
MPU_uxTaskGetSystemStateImpl:
    b MPU_uxTaskGetSystemStateImpl

    PUBWEAK MPU_uxTaskGetStackHighWaterMarkImpl
MPU_uxTaskGetStackHighWaterMarkImpl:
    b MPU_uxTaskGetStackHighWaterMarkImpl

    PUBWEAK MPU_uxTaskGetStackHighWaterMark2Impl
MPU_uxTaskGetStackHighWaterMark2Impl:
    b MPU_uxTaskGetStackHighWaterMark2Impl

    PUBWEAK MPU_xTaskGetCurrentTaskHandleImpl
MPU_xTaskGetCurrentTaskHandleImpl:
    b MPU_xTaskGetCurrentTaskHandleImpl

    PUBWEAK MPU_xTaskGetSchedulerStateImpl
MPU_xTaskGetSchedulerStateImpl:
    b MPU_xTaskGetSchedulerStateImpl

    PUBWEAK MPU_vTaskSetTimeOutStateImpl
MPU_vTaskSetTimeOutStateImpl:
    b MPU_vTaskSetTimeOutStateImpl

    PUBWEAK MPU_xTaskCheckForTimeOutImpl
MPU_xTaskCheckForTimeOutImpl:
    b MPU_xTaskCheckForTimeOutImpl

    PUBWEAK MPU_xTaskGenericNotifyImpl
MPU_xTaskGenericNotifyImpl:
    b MPU_xTaskGenericNotifyImpl

    PUBWEAK MPU_xTaskGenericNotifyWaitImpl
MPU_xTaskGenericNotifyWaitImpl:
    b MPU_xTaskGenericNotifyWaitImpl

    PUBWEAK MPU_ulTaskGenericNotifyTakeImpl
MPU_ulTaskGenericNotifyTakeImpl:
    b MPU_ulTaskGenericNotifyTakeImpl

    PUBWEAK MPU_xTaskGenericNotifyStateClearImpl
MPU_xTaskGenericNotifyStateClearImpl:
    b MPU_xTaskGenericNotifyStateClearImpl

    PUBWEAK MPU_ulTaskGenericNotifyValueClearImpl
MPU_ulTaskGenericNotifyValueClearImpl:
    b MPU_ulTaskGenericNotifyValueClearImpl

    PUBWEAK MPU_xQueueGenericSendImpl
MPU_xQueueGenericSendImpl:
    b MPU_xQueueGenericSendImpl

    PUBWEAK MPU_uxQueueMessagesWaitingImpl
MPU_uxQueueMessagesWaitingImpl:
    b MPU_uxQueueMessagesWaitingImpl

    PUBWEAK MPU_uxQueueSpacesAvailableImpl
MPU_uxQueueSpacesAvailableImpl:
    b MPU_uxQueueSpacesAvailableImpl

    PUBWEAK MPU_xQueueReceiveImpl
MPU_xQueueReceiveImpl:
    b MPU_xQueueReceiveImpl

    PUBWEAK MPU_xQueuePeekImpl
MPU_xQueuePeekImpl:
    b MPU_xQueuePeekImpl

    PUBWEAK MPU_xQueueSemaphoreTakeImpl
MPU_xQueueSemaphoreTakeImpl:
    b MPU_xQueueSemaphoreTakeImpl

    PUBWEAK MPU_xQueueGetMutexHolderImpl
MPU_xQueueGetMutexHolderImpl:
    b MPU_xQueueGetMutexHolderImpl

    PUBWEAK MPU_xQueueTakeMutexRecursiveImpl
MPU_xQueueTakeMutexRecursiveImpl:
    b MPU_xQueueTakeMutexRecursiveImpl

    PUBWEAK MPU_xQueueGiveMutexRecursiveImpl
MPU_xQueueGiveMutexRecursiveImpl:
    b MPU_xQueueGiveMutexRecursiveImpl

    PUBWEAK MPU_xQueueSelectFromSetImpl
MPU_xQueueSelectFromSetImpl:
    b MPU_xQueueSelectFromSetImpl

    PUBWEAK MPU_xQueueAddToSetImpl
MPU_xQueueAddToSetImpl:
    b MPU_xQueueAddToSetImpl

    PUBWEAK MPU_vQueueAddToRegistryImpl
MPU_vQueueAddToRegistryImpl:
    b MPU_vQueueAddToRegistryImpl

    PUBWEAK MPU_vQueueUnregisterQueueImpl
MPU_vQueueUnregisterQueueImpl:
    b MPU_vQueueUnregisterQueueImpl

    PUBWEAK MPU_pcQueueGetNameImpl
MPU_pcQueueGetNameImpl:
    b MPU_pcQueueGetNameImpl

    PUBWEAK MPU_pvTimerGetTimerIDImpl
MPU_pvTimerGetTimerIDImpl:
    b MPU_pvTimerGetTimerIDImpl

    PUBWEAK MPU_vTimerSetTimerIDImpl
MPU_vTimerSetTimerIDImpl:
    b MPU_vTimerSetTimerIDImpl

    PUBWEAK MPU_xTimerIsTimerActiveImpl
MPU_xTimerIsTimerActiveImpl:
    b MPU_xTimerIsTimerActiveImpl

    PUBWEAK MPU_xTimerGetTimerDaemonTaskHandleImpl
MPU_xTimerGetTimerDaemonTaskHandleImpl:
    b MPU_xTimerGetTimerDaemonTaskHandleImpl

    PUBWEAK MPU_xTimerGenericCommandFromTaskImpl
MPU_xTimerGenericCommandFromTaskImpl:
    b MPU_xTimerGenericCommandFromTaskImpl

    PUBWEAK MPU_pcTimerGetNameImpl
MPU_pcTimerGetNameImpl:
    b MPU_pcTimerGetNameImpl

    PUBWEAK MPU_vTimerSetReloadModeImpl
MPU_vTimerSetReloadModeImpl:
    b MPU_vTimerSetReloadModeImpl

    PUBWEAK MPU_xTimerGetReloadModeImpl
MPU_xTimerGetReloadModeImpl:
    b MPU_xTimerGetReloadModeImpl

    PUBWEAK MPU_uxTimerGetReloadModeImpl
MPU_uxTimerGetReloadModeImpl:
    b MPU_uxTimerGetReloadModeImpl

    PUBWEAK MPU_xTimerGetPeriodImpl
MPU_xTimerGetPeriodImpl:
    b MPU_xTimerGetPeriodImpl

    PUBWEAK MPU_xTimerGetExpiryTimeImpl
MPU_xTimerGetExpiryTimeImpl:
    b MPU_xTimerGetExpiryTimeImpl

    PUBWEAK MPU_xEventGroupWaitBitsImpl
MPU_xEventGroupWaitBitsImpl:
    b MPU_xEventGroupWaitBitsImpl

    PUBWEAK MPU_xEventGroupClearBitsImpl
MPU_xEventGroupClearBitsImpl:
    b MPU_xEventGroupClearBitsImpl

    PUBWEAK MPU_xEventGroupSetBitsImpl
MPU_xEventGroupSetBitsImpl:
    b MPU_xEventGroupSetBitsImpl

    PUBWEAK MPU_xEventGroupSyncImpl
MPU_xEventGroupSyncImpl:
    b MPU_xEventGroupSyncImpl

    PUBWEAK MPU_uxEventGroupGetNumberImpl
MPU_uxEventGroupGetNumberImpl:
    b MPU_uxEventGroupGetNumberImpl

    PUBWEAK MPU_vEventGroupSetNumberImpl
MPU_vEventGroupSetNumberImpl:
    b MPU_vEventGroupSetNumberImpl

    PUBWEAK MPU_xStreamBufferSendImpl
MPU_xStreamBufferSendImpl:
    b MPU_xStreamBufferSendImpl

    PUBWEAK MPU_xStreamBufferReceiveImpl
MPU_xStreamBufferReceiveImpl:
    b MPU_xStreamBufferReceiveImpl

    PUBWEAK MPU_xStreamBufferIsFullImpl
MPU_xStreamBufferIsFullImpl:
    b MPU_xStreamBufferIsFullImpl

    PUBWEAK MPU_xStreamBufferIsEmptyImpl
MPU_xStreamBufferIsEmptyImpl:
    b MPU_xStreamBufferIsEmptyImpl

    PUBWEAK MPU_xStreamBufferSpacesAvailableImpl
MPU_xStreamBufferSpacesAvailableImpl:
    b MPU_xStreamBufferSpacesAvailableImpl

    PUBWEAK MPU_xStreamBufferBytesAvailableImpl
MPU_xStreamBufferBytesAvailableImpl:
    b MPU_xStreamBufferBytesAvailableImpl

    PUBWEAK MPU_xStreamBufferSetTriggerLevelImpl
MPU_xStreamBufferSetTriggerLevelImpl:
    b MPU_xStreamBufferSetTriggerLevelImpl

    PUBWEAK MPU_xStreamBufferNextMessageLengthBytesImpl
MPU_xStreamBufferNextMessageLengthBytesImpl:
    b MPU_xStreamBufferNextMessageLengthBytesImpl

/*-----------------------------------------------------------*/

#endif /* #if ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

    END
