/*
 * Copyright (c) 2013-2019 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------------
 *
 * Project:     CMSIS-RTOS RTX
 * Title:       Delay functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  ==== Service Calls ====

/// Wait for Timeout (Time Delay).
/// \note API identical to osDelay
static osStatus_t svcRtxDelay (uint32_t ticks) {

  if (ticks != 0U) {
    if (osRtxThreadWaitEnter(osRtxThreadWaitingDelay, ticks)) {
      EvrRtxDelayStarted(ticks);
    } else {
      EvrRtxDelayCompleted(osRtxThreadGetRunning());
    }
  }

  return osOK;
}

/// Wait until specified time.
/// \note API identical to osDelayUntil
static osStatus_t svcRtxDelayUntil (uint32_t ticks) {

  ticks -= osRtxInfo.kernel.tick;
  if ((ticks == 0U) || (ticks > 0x7FFFFFFFU)) {
    EvrRtxDelayError((int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  if (osRtxThreadWaitEnter(osRtxThreadWaitingDelay, ticks)) {
    EvrRtxDelayUntilStarted(ticks);
  } else {
    EvrRtxDelayCompleted(osRtxThreadGetRunning());
  }

  return osOK;
}

//  Service Calls definitions
//lint ++flb "Library Begin" [MISRA Note 11]
SVC0_1(Delay,      osStatus_t, uint32_t)
SVC0_1(DelayUntil, osStatus_t, uint32_t)
//lint --flb "Library End"


//  ==== Public API ====

/// Wait for Timeout (Time Delay).
osStatus_t osDelay (uint32_t ticks) {
  osStatus_t status;

  EvrRtxDelay(ticks);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxDelayError((int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcDelay(ticks);
  }
  return status;
}

/// Wait until specified time.
osStatus_t osDelayUntil (uint32_t ticks) {
  osStatus_t status;

  EvrRtxDelayUntil(ticks);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxDelayError((int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcDelayUntil(ticks);
  }
  return status;
}
