/*
 * @brief LPC43xx dual-core example configuration file
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#ifndef __LPC43XX_DUALCORE_CONFIG_H_
#define __LPC43XX_DUALCORE_CONFIG_H_

#include "board.h"

/*
 * Users can enable any of the following macros so that the examples will be
 * built with that functionality.
 */
#define EXAMPLE_BLINKY
#undef  EXAMPLE_USB_DEVICE
#undef  EXAMPLE_USB_HOST
#undef  EXAMPLE_LWIP
#undef  EXAMPLE_EMWIN

/*
 * Users can define any one of the following macros to use the appropriate OS.
 * For standalone executables both the macros should be disabled.
 */
#define OS_FREE_RTOS
#undef  OS_UCOS_III

/*
 * The Stack sizes are currently used by Code Red LPCXpresso tool chain
 * only. In future, this will be used by Keil & IAR tool chains also
 */
#define STACK_SIZE  0x200
#define HEAP_SIZE   0x4000

/*
 * The following defines are used by the M4 image to locate the M0 image, and
 * one of the below macro must be defined based on your linker definition file.
 * If the linker file is made to generate image in SPIFI area then define
 * TARGET_SPIFI macro (so that M4 image will locate the corresponding M0 in the
 * SPIFI region). If none of them r defined the following defaults will be used
 * For LPC4330 NGX XPLORER BOARD:      TARGET_SPIFI
 * For LPC4357 KEIL MCB BOARD:         TARGET_IFLASH
 * For LPC4350 HITEX EVALUATION BOARD: TARGET_SPIFI
 */
#undef TARGET_SPIFI     /* SPIFI Flash */
#undef TARGET_IFLASH    /* Internal Flash */
#undef TARGET_XFLASH    /* External NOR Flash */

/* Priority of various tasks in dual-core examples */
/* LWIP thread priority should always be greater than the
 * MAC priority ie., greater than TASK_PRIO_ETHERNET
 */
#ifdef OS_FREE_RTOS
/* higher the number higher the priority */
#define TASK_PRIO_LCD             (tskIDLE_PRIORITY + 0UL)
#define TASK_PRIO_TOUCHSCREEN     (tskIDLE_PRIORITY + 1UL)
#define TASK_PRIO_BLINKY_EVENT    (tskIDLE_PRIORITY + 1UL)
#define TASK_PRIO_ETHERNET        (tskIDLE_PRIORITY + 2UL)
#define TASK_PRIO_IPC_DISPATCH    (tskIDLE_PRIORITY + 3UL)
#define TASK_PRIO_USBDEVICE       (tskIDLE_PRIORITY + 4UL)
#define TASK_PRIO_LWIP_THREAD     (tskIDLE_PRIORITY + 5UL)

#elif defined(OS_UCOS_III)
/* lower the number higher the priority */
#define TASK_PRIO_BLINKY_EVENT    14
#define TASK_PRIO_LCD             14
#define TASK_PRIO_TOUCHSCREEN     13
#define TASK_PRIO_ETHERNET        13
#define TASK_PRIO_IPC_DISPATCH    12
#define TASK_PRIO_USBDEVICE       11
#define TASK_PRIO_LWIP_THREAD     10
#endif

/* Priority of various IRQs used in dual-core examples */
/* lower the number higher the priority */
#define IRQ_PRIO_IPC              7
#define IRQ_PRIO_ETHERNET         6
#define IRQ_PRIO_USBDEV           5

/* Minimum stack size for UCOS-III Tasks */
#define UCOS_MIN_STACK_SZ         128

/*
 * Offset of M0 image from the starting of M4 image
 * Usually the size allocated for M0 image in scatter
 * file/ Linker Definition file / Memory configuration
 * in the IDE.
 * ####Don't change this value unless you are sure about what you are doing ####
 */
#define M0_IMAGE_OFFSET      0x40000

/*
 * Absolute addresses used by both cores.
 * ####Don't change these values unless you are sure about what you are doing ####
 */
#define SHARED_MEM_M0          0x20000000
#define SHARED_MEM_M4          0x20000020

/* Size & location of RAM DISK used by FAT Filesystem */
#define RAMDISK_LOCATION    0x20002000
#define RAMDISK_SIZE        0x2000

#ifdef CORE_M4
/* Delay and LED to be blinked by M4 Core */
#define BLINKY_DEFAULT_DELAY         1000
#define BLINK_LED                    1
#endif /* CORE_M4 */

#ifdef CORE_M0
/* Delay and LED to be blinked by M4 Core */
#define BLINK_LED                    0
#define BLINKY_DEFAULT_DELAY         500
#endif /* CORE_M0 */

/* Base address of various flashes */
#define SPIFI_BASE_ADDR      0x14000000
#define XFLASH_BASE_ADDR     0x1C000000
#define IFLASH_BASE_ADDR     0x1A000000

/* Include Common header here */
#include "dualcore_common.h"

#endif /* ifndef __LPC43XX_DUALCORE_CONFIG_H_ */
