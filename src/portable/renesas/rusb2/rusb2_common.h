/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */
#pragma once

#include "common/tusb_common.h"
#include "rusb2_type.h"

#if TU_CHECK_MCU(OPT_MCU_RX63X, OPT_MCU_RX65X, OPT_MCU_RX72N)
  #include "rusb2_rx.h"
#elif TU_CHECK_MCU(OPT_MCU_RAXXX)
  #include "rusb2_ra.h"

  // Hack for D0FIFO definitions on RA Cortex-M23
  #if defined(RENESAS_CORTEX_M23)
    #define D0FIFO      CFIFO
    #define D0FIFOSEL   CFIFOSEL
    #define D0FIFOSEL_b CFIFOSEL_b
    #define D1FIFOSEL   CFIFOSEL
    #define D1FIFOSEL_b CFIFOSEL_b
    #define D0FIFOCTR   CFIFOCTR
    #define D0FIFOCTR_b CFIFOCTR_b
  #endif

#else
  #error "Unsupported MCU"
#endif


//--------------------------------------------------------------------+
// Common
//--------------------------------------------------------------------+

enum {
  FIFOSEL_BIGEND = (TU_BYTE_ORDER == TU_BIG_ENDIAN ? RUSB2_FIFOSEL_BIGEND : 0)
};
