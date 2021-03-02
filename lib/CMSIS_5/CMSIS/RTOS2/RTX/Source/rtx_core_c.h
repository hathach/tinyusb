/*
 * Copyright (c) 2013-2020 Arm Limited. All rights reserved.
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
 * Title:       Cortex Core definitions
 *
 * -----------------------------------------------------------------------------
 */

#ifndef RTX_CORE_C_H_
#define RTX_CORE_C_H_

//lint -emacro((923,9078),SCB) "cast from unsigned long to pointer" [MISRA Note 9]
#include "RTE_Components.h"
#include CMSIS_device_header

#if ((!defined(__ARM_ARCH_6M__))        && \
     (!defined(__ARM_ARCH_7A__))        && \
     (!defined(__ARM_ARCH_7M__))        && \
     (!defined(__ARM_ARCH_7EM__))       && \
     (!defined(__ARM_ARCH_8M_BASE__))   && \
     (!defined(__ARM_ARCH_8M_MAIN__))   && \
     (!defined(__ARM_ARCH_8_1M_MAIN__)))
#error "Unknown Arm Architecture!"
#endif

#if   (defined(__ARM_ARCH_7A__) && (__ARM_ARCH_7A__ != 0))
#include "rtx_core_ca.h"
#else
#include "rtx_core_cm.h"
#endif

#endif  // RTX_CORE_C_H_
