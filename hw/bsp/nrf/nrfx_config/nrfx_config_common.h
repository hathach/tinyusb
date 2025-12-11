/*
 * Copyright (c) 2022 - 2025, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NRFX_CONFIG_COMMON_H__
#define NRFX_CONFIG_COMMON_H__

#ifndef NRFX_CONFIG_H__
#error "This file should not be included directly. Include nrfx_config.h instead."
#endif

/** @brief Symbol specifying major version of the nrfx API to be used. */
#ifndef NRFX_CONFIG_API_VER_MAJOR
#define NRFX_CONFIG_API_VER_MAJOR 3
#endif

/** @brief Symbol specifying minor version of the nrfx API to be used. */
#ifndef NRFX_CONFIG_API_VER_MINOR
#define NRFX_CONFIG_API_VER_MINOR 12
#endif

/** @brief Symbol specifying micro version of the nrfx API to be used. */
#ifndef NRFX_CONFIG_API_VER_MICRO
#define NRFX_CONFIG_API_VER_MICRO 0
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
#define NRFX_CLOCK_ENABLED   0
#define NRFX_UARTE_ENABLED   1

#if defined(NRF54H20_XXAA)
#define NRFX_UARTE120_ENABLED  1

#else

#define NRFX_POWER_ENABLED   1
#define NRFX_POWER_DEFAULT_CONFIG_IRQ_PRIORITY  7

#define NRFX_UARTE0_ENABLED  1

#define NRFX_GPIOTE_ENABLED  1
#define NRFX_GPIOTE0_ENABLED 1

#define NRFX_SPIM_ENABLED    1
#define NRFX_SPIM1_ENABLED   1 // use SPI1 since nrf5340 share uart with spi
#endif

#define NRFX_PRS_ENABLED     0
#define NRFX_USBREG_ENABLED  1

#define NRF_STATIC_INLINE static inline

#endif /* NRFX_CONFIG_COMMON_H__ */
