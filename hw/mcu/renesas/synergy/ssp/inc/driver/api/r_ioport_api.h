/***********************************************************************************************************************
 * Copyright [2015-2025] Renesas Electronics Corporation and/or its licensors. All Rights Reserved.
 * 
 * This file is part of Renesas SynergyTM Software Package (SSP)
 *
 * The contents of this file (the "contents") are proprietary and confidential to Renesas Electronics Corporation
 * and/or its licensors ("Renesas") and subject to statutory and contractual protections.
 *
 * This file is subject to a Renesas SSP license agreement. Unless otherwise agreed in an SSP license agreement with
 * Renesas: 1) you may not use, copy, modify, distribute, display, or perform the contents; 2) you may not use any name
 * or mark of Renesas for advertising or publicity purposes or in connection with your use of the contents; 3) RENESAS
 * MAKES NO WARRANTY OR REPRESENTATIONS ABOUT THE SUITABILITY OF THE CONTENTS FOR ANY PURPOSE; THE CONTENTS ARE PROVIDED
 * "AS IS" WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, AND NON-INFRINGEMENT; AND 4) RENESAS SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, OR
 * CONSEQUENTIAL DAMAGES, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA, OR PROJECTS, WHETHER IN AN ACTION OF
 * CONTRACT OR TORT, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE CONTENTS. Third-party contents
 * included in this file may be subject to different terms.
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * File Name    : r_ioport_api.h
 * Description  : IOPORT driver interface.
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @defgroup IOPORT_API I/O Port Interface
 * @brief Interface  for accessing I/O ports and configuring I/O functionality.
 *
 * The IOPort shared interface provides the ability to access the IOPorts of a device at both bit and port level.
 * Port and pin direction can be changed.
 *
 * Related SSP architecture topics:
 *  - @ref ssp-interfaces
 *  - @ref ssp-predefined-layers
 *  - @ref using-ssp-modules
 *
 * IOPORT Interface description: @ref HALIOPORTInterface
 *
 * @{
 **********************************************************************************************************************/

#ifndef DRV_IOPORT_API_H
#define DRV_IOPORT_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/* Common error codes and definitions. */
#include "bsp_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define IOPORT_API_VERSION_MAJOR (2U)
#define IOPORT_API_VERSION_MINOR (0U)

/* Private definition to set enumeration values. */
#define IOPORT_PRV_PFS_PSEL_OFFSET     (24)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** IO port type used with ports */
typedef uint16_t ioport_size_t;                     ///< IO port size on this device

/** Levels that can be set and read for individual pins */
typedef enum e_ioport_level
{
    IOPORT_LEVEL_LOW = 0,               ///< Low
    IOPORT_LEVEL_HIGH                   ///< High
} ioport_level_t;

/** Direction of individual pins */
typedef enum e_ioport_dir
{
    IOPORT_DIRECTION_INPUT = 0,         ///< Input
    IOPORT_DIRECTION_OUTPUT             ///< Output
} ioport_direction_t;

/** Superset list of all possible IO ports. */
typedef enum e_ioport_port
{
    IOPORT_PORT_00 = 0x0000,                            ///< IO port 0
    IOPORT_PORT_01 = 0x0100,                            ///< IO port 1
    IOPORT_PORT_02 = 0x0200,                            ///< IO port 2
    IOPORT_PORT_03 = 0x0300,                            ///< IO port 3
    IOPORT_PORT_04 = 0x0400,                            ///< IO port 4
    IOPORT_PORT_05 = 0x0500,                            ///< IO port 5
    IOPORT_PORT_06 = 0x0600,                            ///< IO port 6
    IOPORT_PORT_07 = 0x0700,                            ///< IO port 7
    IOPORT_PORT_08 = 0x0800,                            ///< IO port 8
    IOPORT_PORT_09 = 0x0900,                            ///< IO port 9
    IOPORT_PORT_10 = 0x0A00,                            ///< IO port 10
    IOPORT_PORT_11 = 0x0B00,                            ///< IO port 11
} ioport_port_t;

/** Superset list of all possible IO port pins. */
typedef enum e_ioport_port_pin_t
{
    IOPORT_PORT_00_PIN_00 = 0x0000,                        ///< IO port 0 pin 0
    IOPORT_PORT_00_PIN_01 = 0x0001,                        ///< IO port 0 pin 1
    IOPORT_PORT_00_PIN_02 = 0x0002,                        ///< IO port 0 pin 2
    IOPORT_PORT_00_PIN_03 = 0x0003,                        ///< IO port 0 pin 3
    IOPORT_PORT_00_PIN_04 = 0x0004,                        ///< IO port 0 pin 4
    IOPORT_PORT_00_PIN_05 = 0x0005,                        ///< IO port 0 pin 5
    IOPORT_PORT_00_PIN_06 = 0x0006,                        ///< IO port 0 pin 6
    IOPORT_PORT_00_PIN_07 = 0x0007,                        ///< IO port 0 pin 7
    IOPORT_PORT_00_PIN_08 = 0x0008,                        ///< IO port 0 pin 8
    IOPORT_PORT_00_PIN_09 = 0x0009,                        ///< IO port 0 pin 9
    IOPORT_PORT_00_PIN_10 = 0x000A,                        ///< IO port 0 pin 10
    IOPORT_PORT_00_PIN_11 = 0x000B,                        ///< IO port 0 pin 11
    IOPORT_PORT_00_PIN_12 = 0x000C,                        ///< IO port 0 pin 12
    IOPORT_PORT_00_PIN_13 = 0x000D,                        ///< IO port 0 pin 13
    IOPORT_PORT_00_PIN_14 = 0x000E,                        ///< IO port 0 pin 14
    IOPORT_PORT_00_PIN_15 = 0x000F,                        ///< IO port 0 pin 15

    IOPORT_PORT_01_PIN_00 = 0x0100,                        ///< IO port 1 pin 0
    IOPORT_PORT_01_PIN_01 = 0x0101,                        ///< IO port 1 pin 1
    IOPORT_PORT_01_PIN_02 = 0x0102,                        ///< IO port 1 pin 2
    IOPORT_PORT_01_PIN_03 = 0x0103,                        ///< IO port 1 pin 3
    IOPORT_PORT_01_PIN_04 = 0x0104,                        ///< IO port 1 pin 4
    IOPORT_PORT_01_PIN_05 = 0x0105,                        ///< IO port 1 pin 5
    IOPORT_PORT_01_PIN_06 = 0x0106,                        ///< IO port 1 pin 6
    IOPORT_PORT_01_PIN_07 = 0x0107,                        ///< IO port 1 pin 7
    IOPORT_PORT_01_PIN_08 = 0x0108,                        ///< IO port 1 pin 8
    IOPORT_PORT_01_PIN_09 = 0x0109,                        ///< IO port 1 pin 9
    IOPORT_PORT_01_PIN_10 = 0x010A,                        ///< IO port 1 pin 10
    IOPORT_PORT_01_PIN_11 = 0x010B,                        ///< IO port 1 pin 11
    IOPORT_PORT_01_PIN_12 = 0x010C,                        ///< IO port 1 pin 12
    IOPORT_PORT_01_PIN_13 = 0x010D,                        ///< IO port 1 pin 13
    IOPORT_PORT_01_PIN_14 = 0x010E,                        ///< IO port 1 pin 14
    IOPORT_PORT_01_PIN_15 = 0x010F,                        ///< IO port 1 pin 15

    IOPORT_PORT_02_PIN_00 = 0x0200,                        ///< IO port 2 pin 0
    IOPORT_PORT_02_PIN_01 = 0x0201,                        ///< IO port 2 pin 1
    IOPORT_PORT_02_PIN_02 = 0x0202,                        ///< IO port 2 pin 2
    IOPORT_PORT_02_PIN_03 = 0x0203,                        ///< IO port 2 pin 3
    IOPORT_PORT_02_PIN_04 = 0x0204,                        ///< IO port 2 pin 4
    IOPORT_PORT_02_PIN_05 = 0x0205,                        ///< IO port 2 pin 5
    IOPORT_PORT_02_PIN_06 = 0x0206,                        ///< IO port 2 pin 6
    IOPORT_PORT_02_PIN_07 = 0x0207,                        ///< IO port 2 pin 7
    IOPORT_PORT_02_PIN_08 = 0x0208,                        ///< IO port 2 pin 8
    IOPORT_PORT_02_PIN_09 = 0x0209,                        ///< IO port 2 pin 9
    IOPORT_PORT_02_PIN_10 = 0x020A,                        ///< IO port 2 pin 10
    IOPORT_PORT_02_PIN_11 = 0x020B,                        ///< IO port 2 pin 11
    IOPORT_PORT_02_PIN_12 = 0x020C,                        ///< IO port 2 pin 12
    IOPORT_PORT_02_PIN_13 = 0x020D,                        ///< IO port 2 pin 13
    IOPORT_PORT_02_PIN_14 = 0x020E,                        ///< IO port 2 pin 14
    IOPORT_PORT_02_PIN_15 = 0x020F,                        ///< IO port 2 pin 15

    IOPORT_PORT_03_PIN_00 = 0x0300,                        ///< IO port 3 pin 0
    IOPORT_PORT_03_PIN_01 = 0x0301,                        ///< IO port 3 pin 1
    IOPORT_PORT_03_PIN_02 = 0x0302,                        ///< IO port 3 pin 2
    IOPORT_PORT_03_PIN_03 = 0x0303,                        ///< IO port 3 pin 3
    IOPORT_PORT_03_PIN_04 = 0x0304,                        ///< IO port 3 pin 4
    IOPORT_PORT_03_PIN_05 = 0x0305,                        ///< IO port 3 pin 5
    IOPORT_PORT_03_PIN_06 = 0x0306,                        ///< IO port 3 pin 6
    IOPORT_PORT_03_PIN_07 = 0x0307,                        ///< IO port 3 pin 7
    IOPORT_PORT_03_PIN_08 = 0x0308,                        ///< IO port 3 pin 8
    IOPORT_PORT_03_PIN_09 = 0x0309,                        ///< IO port 3 pin 9
    IOPORT_PORT_03_PIN_10 = 0x030A,                        ///< IO port 3 pin 10
    IOPORT_PORT_03_PIN_11 = 0x030B,                        ///< IO port 3 pin 11
    IOPORT_PORT_03_PIN_12 = 0x030C,                        ///< IO port 3 pin 12
    IOPORT_PORT_03_PIN_13 = 0x030D,                        ///< IO port 3 pin 13
    IOPORT_PORT_03_PIN_14 = 0x030E,                        ///< IO port 3 pin 14
    IOPORT_PORT_03_PIN_15 = 0x030F,                        ///< IO port 3 pin 15

    IOPORT_PORT_04_PIN_00 = 0x0400,                        ///< IO port 4 pin 0
    IOPORT_PORT_04_PIN_01 = 0x0401,                        ///< IO port 4 pin 1
    IOPORT_PORT_04_PIN_02 = 0x0402,                        ///< IO port 4 pin 2
    IOPORT_PORT_04_PIN_03 = 0x0403,                        ///< IO port 4 pin 3
    IOPORT_PORT_04_PIN_04 = 0x0404,                        ///< IO port 4 pin 4
    IOPORT_PORT_04_PIN_05 = 0x0405,                        ///< IO port 4 pin 5
    IOPORT_PORT_04_PIN_06 = 0x0406,                        ///< IO port 4 pin 6
    IOPORT_PORT_04_PIN_07 = 0x0407,                        ///< IO port 4 pin 7
    IOPORT_PORT_04_PIN_08 = 0x0408,                        ///< IO port 4 pin 8
    IOPORT_PORT_04_PIN_09 = 0x0409,                        ///< IO port 4 pin 9
    IOPORT_PORT_04_PIN_10 = 0x040A,                        ///< IO port 4 pin 10
    IOPORT_PORT_04_PIN_11 = 0x040B,                        ///< IO port 4 pin 11
    IOPORT_PORT_04_PIN_12 = 0x040C,                        ///< IO port 4 pin 12
    IOPORT_PORT_04_PIN_13 = 0x040D,                        ///< IO port 4 pin 13
    IOPORT_PORT_04_PIN_14 = 0x040E,                        ///< IO port 4 pin 14
    IOPORT_PORT_04_PIN_15 = 0x040F,                        ///< IO port 4 pin 15

    IOPORT_PORT_05_PIN_00 = 0x0500,                        ///< IO port 5 pin 0
    IOPORT_PORT_05_PIN_01 = 0x0501,                        ///< IO port 5 pin 1
    IOPORT_PORT_05_PIN_02 = 0x0502,                        ///< IO port 5 pin 2
    IOPORT_PORT_05_PIN_03 = 0x0503,                        ///< IO port 5 pin 3
    IOPORT_PORT_05_PIN_04 = 0x0504,                        ///< IO port 5 pin 4
    IOPORT_PORT_05_PIN_05 = 0x0505,                        ///< IO port 5 pin 5
    IOPORT_PORT_05_PIN_06 = 0x0506,                        ///< IO port 5 pin 6
    IOPORT_PORT_05_PIN_07 = 0x0507,                        ///< IO port 5 pin 7
    IOPORT_PORT_05_PIN_08 = 0x0508,                        ///< IO port 5 pin 8
    IOPORT_PORT_05_PIN_09 = 0x0509,                        ///< IO port 5 pin 9
    IOPORT_PORT_05_PIN_10 = 0x050A,                        ///< IO port 5 pin 10
    IOPORT_PORT_05_PIN_11 = 0x050B,                        ///< IO port 5 pin 11
    IOPORT_PORT_05_PIN_12 = 0x050C,                        ///< IO port 5 pin 12
    IOPORT_PORT_05_PIN_13 = 0x050D,                        ///< IO port 5 pin 13
    IOPORT_PORT_05_PIN_14 = 0x050E,                        ///< IO port 5 pin 14
    IOPORT_PORT_05_PIN_15 = 0x050F,                        ///< IO port 5 pin 15

    IOPORT_PORT_06_PIN_00 = 0x0600,                        ///< IO port 6 pin 0
    IOPORT_PORT_06_PIN_01 = 0x0601,                        ///< IO port 6 pin 1
    IOPORT_PORT_06_PIN_02 = 0x0602,                        ///< IO port 6 pin 2
    IOPORT_PORT_06_PIN_03 = 0x0603,                        ///< IO port 6 pin 3
    IOPORT_PORT_06_PIN_04 = 0x0604,                        ///< IO port 6 pin 4
    IOPORT_PORT_06_PIN_05 = 0x0605,                        ///< IO port 6 pin 5
    IOPORT_PORT_06_PIN_06 = 0x0606,                        ///< IO port 6 pin 6
    IOPORT_PORT_06_PIN_07 = 0x0607,                        ///< IO port 6 pin 7
    IOPORT_PORT_06_PIN_08 = 0x0608,                        ///< IO port 6 pin 8
    IOPORT_PORT_06_PIN_09 = 0x0609,                        ///< IO port 6 pin 9
    IOPORT_PORT_06_PIN_10 = 0x060A,                        ///< IO port 6 pin 10
    IOPORT_PORT_06_PIN_11 = 0x060B,                        ///< IO port 6 pin 11
    IOPORT_PORT_06_PIN_12 = 0x060C,                        ///< IO port 6 pin 12
    IOPORT_PORT_06_PIN_13 = 0x060D,                        ///< IO port 6 pin 13
    IOPORT_PORT_06_PIN_14 = 0x060E,                        ///< IO port 6 pin 14
    IOPORT_PORT_06_PIN_15 = 0x060F,                        ///< IO port 6 pin 15

    IOPORT_PORT_07_PIN_00 = 0x0700,                        ///< IO port 7 pin 0
    IOPORT_PORT_07_PIN_01 = 0x0701,                        ///< IO port 7 pin 1
    IOPORT_PORT_07_PIN_02 = 0x0702,                        ///< IO port 7 pin 2
    IOPORT_PORT_07_PIN_03 = 0x0703,                        ///< IO port 7 pin 3
    IOPORT_PORT_07_PIN_04 = 0x0704,                        ///< IO port 7 pin 4
    IOPORT_PORT_07_PIN_05 = 0x0705,                        ///< IO port 7 pin 5
    IOPORT_PORT_07_PIN_06 = 0x0706,                        ///< IO port 7 pin 6
    IOPORT_PORT_07_PIN_07 = 0x0707,                        ///< IO port 7 pin 7
    IOPORT_PORT_07_PIN_08 = 0x0708,                        ///< IO port 7 pin 8
    IOPORT_PORT_07_PIN_09 = 0x0709,                        ///< IO port 7 pin 9
    IOPORT_PORT_07_PIN_10 = 0x070A,                        ///< IO port 7 pin 10
    IOPORT_PORT_07_PIN_11 = 0x070B,                        ///< IO port 7 pin 11
    IOPORT_PORT_07_PIN_12 = 0x070C,                        ///< IO port 7 pin 12
    IOPORT_PORT_07_PIN_13 = 0x070D,                        ///< IO port 7 pin 13
    IOPORT_PORT_07_PIN_14 = 0x070E,                        ///< IO port 7 pin 14
    IOPORT_PORT_07_PIN_15 = 0x070F,                        ///< IO port 7 pin 15

    IOPORT_PORT_08_PIN_00 = 0x0800,                        ///< IO port 8 pin 0
    IOPORT_PORT_08_PIN_01 = 0x0801,                        ///< IO port 8 pin 1
    IOPORT_PORT_08_PIN_02 = 0x0802,                        ///< IO port 8 pin 2
    IOPORT_PORT_08_PIN_03 = 0x0803,                        ///< IO port 8 pin 3
    IOPORT_PORT_08_PIN_04 = 0x0804,                        ///< IO port 8 pin 4
    IOPORT_PORT_08_PIN_05 = 0x0805,                        ///< IO port 8 pin 5
    IOPORT_PORT_08_PIN_06 = 0x0806,                        ///< IO port 8 pin 6
    IOPORT_PORT_08_PIN_07 = 0x0807,                        ///< IO port 8 pin 7
    IOPORT_PORT_08_PIN_08 = 0x0808,                        ///< IO port 8 pin 8
    IOPORT_PORT_08_PIN_09 = 0x0809,                        ///< IO port 8 pin 9
    IOPORT_PORT_08_PIN_10 = 0x080A,                        ///< IO port 8 pin 10
    IOPORT_PORT_08_PIN_11 = 0x080B,                        ///< IO port 8 pin 11
    IOPORT_PORT_08_PIN_12 = 0x080C,                        ///< IO port 8 pin 12
    IOPORT_PORT_08_PIN_13 = 0x080D,                        ///< IO port 8 pin 13
    IOPORT_PORT_08_PIN_14 = 0x080E,                        ///< IO port 8 pin 14
    IOPORT_PORT_08_PIN_15 = 0x080F,                        ///< IO port 8 pin 15

    IOPORT_PORT_09_PIN_00 = 0x0900,                        ///< IO port 9 pin 0
    IOPORT_PORT_09_PIN_01 = 0x0901,                        ///< IO port 9 pin 1
    IOPORT_PORT_09_PIN_02 = 0x0902,                        ///< IO port 9 pin 2
    IOPORT_PORT_09_PIN_03 = 0x0903,                        ///< IO port 9 pin 3
    IOPORT_PORT_09_PIN_04 = 0x0904,                        ///< IO port 9 pin 4
    IOPORT_PORT_09_PIN_05 = 0x0905,                        ///< IO port 9 pin 5
    IOPORT_PORT_09_PIN_06 = 0x0906,                        ///< IO port 9 pin 6
    IOPORT_PORT_09_PIN_07 = 0x0907,                        ///< IO port 9 pin 7
    IOPORT_PORT_09_PIN_08 = 0x0908,                        ///< IO port 9 pin 8
    IOPORT_PORT_09_PIN_09 = 0x0909,                        ///< IO port 9 pin 9
    IOPORT_PORT_09_PIN_10 = 0x090A,                        ///< IO port 9 pin 10
    IOPORT_PORT_09_PIN_11 = 0x090B,                        ///< IO port 9 pin 11
    IOPORT_PORT_09_PIN_12 = 0x090C,                        ///< IO port 9 pin 12
    IOPORT_PORT_09_PIN_13 = 0x090D,                        ///< IO port 9 pin 13
    IOPORT_PORT_09_PIN_14 = 0x090E,                        ///< IO port 9 pin 14
    IOPORT_PORT_09_PIN_15 = 0x090F,                        ///< IO port 9 pin 15

    IOPORT_PORT_10_PIN_00 = 0x0A00,                        ///< IO port 10 pin 0
    IOPORT_PORT_10_PIN_01 = 0x0A01,                        ///< IO port 10 pin 1
    IOPORT_PORT_10_PIN_02 = 0x0A02,                        ///< IO port 10 pin 2
    IOPORT_PORT_10_PIN_03 = 0x0A03,                        ///< IO port 10 pin 3
    IOPORT_PORT_10_PIN_04 = 0x0A04,                        ///< IO port 10 pin 4
    IOPORT_PORT_10_PIN_05 = 0x0A05,                        ///< IO port 10 pin 5
    IOPORT_PORT_10_PIN_06 = 0x0A06,                        ///< IO port 10 pin 6
    IOPORT_PORT_10_PIN_07 = 0x0A07,                        ///< IO port 10 pin 7
    IOPORT_PORT_10_PIN_08 = 0x0A08,                        ///< IO port 10 pin 8
    IOPORT_PORT_10_PIN_09 = 0x0A09,                        ///< IO port 10 pin 9
    IOPORT_PORT_10_PIN_10 = 0x0A0A,                        ///< IO port 10 pin 10
    IOPORT_PORT_10_PIN_11 = 0x0A0B,                        ///< IO port 10 pin 11
    IOPORT_PORT_10_PIN_12 = 0x0A0C,                        ///< IO port 10 pin 12
    IOPORT_PORT_10_PIN_13 = 0x0A0D,                        ///< IO port 10 pin 13
    IOPORT_PORT_10_PIN_14 = 0x0A0E,                        ///< IO port 10 pin 14
    IOPORT_PORT_10_PIN_15 = 0x0A0F,                        ///< IO port 10 pin 15

    IOPORT_PORT_11_PIN_00 = 0x0B00,                        ///< IO port 11 pin 0
    IOPORT_PORT_11_PIN_01 = 0x0B01,                        ///< IO port 11 pin 1
    IOPORT_PORT_11_PIN_02 = 0x0B02,                        ///< IO port 11 pin 2
    IOPORT_PORT_11_PIN_03 = 0x0B03,                        ///< IO port 11 pin 3
    IOPORT_PORT_11_PIN_04 = 0x0B04,                        ///< IO port 11 pin 4
    IOPORT_PORT_11_PIN_05 = 0x0B05,                        ///< IO port 11 pin 5
    IOPORT_PORT_11_PIN_06 = 0x0B06,                        ///< IO port 11 pin 6
    IOPORT_PORT_11_PIN_07 = 0x0B07,                        ///< IO port 11 pin 7
    IOPORT_PORT_11_PIN_08 = 0x0B08,                        ///< IO port 11 pin 8
    IOPORT_PORT_11_PIN_09 = 0x0B09,                        ///< IO port 11 pin 9
    IOPORT_PORT_11_PIN_10 = 0x0B0A,                        ///< IO port 11 pin 10
    IOPORT_PORT_11_PIN_11 = 0x0B0B,                        ///< IO port 11 pin 11
    IOPORT_PORT_11_PIN_12 = 0x0B0C,                        ///< IO port 11 pin 12
    IOPORT_PORT_11_PIN_13 = 0x0B0D,                        ///< IO port 11 pin 13
    IOPORT_PORT_11_PIN_14 = 0x0B0E,                        ///< IO port 11 pin 14
    IOPORT_PORT_11_PIN_15 = 0x0B0F,                        ///< IO port 11 pin 15
} ioport_port_pin_t;

/** Superset of all peripheral functions.  */
typedef enum e_ioport_peripheral
{
    IOPORT_PERIPHERAL_IO              = 0x00,                                   ///< Pin will functions as an IO pin
    IOPORT_PERIPHERAL_DEBUG           = (0x00UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a DEBUG pin
    IOPORT_PERIPHERAL_AGT             = (0x01UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as an AGT
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_GPT0            = (0x02UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a GPT
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_GPT1            = (0x03UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a GPT
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_SCI0_2_4_6_8    = (0x04UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as an SCI
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_SCI1_3_5_7_9    = (0x05UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as an SCI
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_RSPI            = (0x06UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a RSPI
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_RIIC            = (0x07UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a RIIC
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_KEY             = (0x08UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a KEY
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_CLKOUT_COMP_RTC = (0x09UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a
                                                                                // clock/comparator/RTC peripheral pin
    IOPORT_PERIPHERAL_CAC_AD          = (0x0AUL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a CAC/ADC
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_BUS             = (0x0BUL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a BUS
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_CTSU            = (0x0CUL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a CTSU
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_LCDC            = (0x0DUL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a segment LCD
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_DALI            = (0x0EUL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a DALI
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_CAN             = (0x10UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a CAN
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_QSPI            = (0x11UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a QSPI
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_SSI             = (0x12UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as an SSI
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_USB_FS          = (0x13UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a USB
                                                                                // full speed peripheral pin
    IOPORT_PERIPHERAL_USB_HS          = (0x14UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a USB
                                                                                // high speed peripheral pin
    IOPORT_PERIPHERAL_SDHI_MMC        = (0x15UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as an SD/MMC
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_ETHER_MII       = (0x16UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as an Ethernet
                                                                                // MMI peripheral pin
    IOPORT_PERIPHERAL_ETHER_RMII      = (0x17UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as an Ethernet
                                                                                // RMMI peripheral pin
    IOPORT_PERIPHERAL_PDC             = (0x18UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a PDC
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_LCD_GRAPHICS    = (0x19UL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a graphics
                                                                                // LCD peripheral pin
    IOPORT_PERIPHERAL_TRACE           = (0x1AUL << IOPORT_PRV_PFS_PSEL_OFFSET), ///< Pin will function as a debug trace
                                                                                // peripheral pin
    IOPORT_PERIPHERAL_END                                                       ///< Marks end of enum - used by
                                                                                // parameter checking
} ioport_peripheral_t;

/** Superset of Ethernet channels. */
typedef enum e_ioport_eth_ch
{
    IOPORT_ETHERNET_CHANNEL_0 = 0x10,                   ///< Used to select Ethernet channel 0
    IOPORT_ETHERNET_CHANNEL_1 = 0x20,                   ///< Used to select Ethernet channel 1
    IOPORT_ETHERNET_CHANNEL_END                         ///< Marks end of enum - used by parameter checking
} ioport_ethernet_channel_t;

/** Superset of Ethernet PHY modes. */
typedef enum e_ioport_eth_mode
{
    IOPORT_ETHERNET_MODE_MII = 0,                       ///< Ethernet PHY mode set to MII
    IOPORT_ETHERNET_MODE_RMII,                          ///< Ethernet PHY mode set to RMII
    IOPORT_ETHERNET_MODE_END                            ///< Marks end of enum - used by parameter checking
} ioport_ethernet_mode_t;

/** Options to configure pin functions  */
typedef enum e_ioport_cfg_options
{
    IOPORT_CFG_PORT_DIRECTION_INPUT  = 0x00000000,      ///< Sets the pin direction to input (default)
    IOPORT_CFG_PORT_DIRECTION_OUTPUT = 0x00000004,      ///< Sets the pin direction to output
    IOPORT_CFG_PORT_OUTPUT_LOW       = 0x00000000,      ///< Sets the pin level to low
    IOPORT_CFG_PORT_OUTPUT_HIGH      = 0x00000001,      ///< Sets the pin level to high
    IOPORT_CFG_PULLUP_ENABLE         = 0x00000010,      ///< Enables the pin's internal pull-up
    IOPORT_CFG_PIM_TTL               = 0x00000020,      ///< Enables the pin's input mode
    IOPORT_CFG_NMOS_ENABLE           = 0x00000040,      ///< Enables the pin's NMOS open-drain output
    IOPORT_CFG_PMOS_ENABLE           = 0x00000080,      ///< Enables the pin's PMOS open-drain ouput
    IOPORT_CFG_DRIVE_MID             = 0x00000400,      ///< Sets pin drive output to medium
    IOPORT_CFG_DRIVE_MID_IIC         = 0x00000C00,      ///< Sets pin to drive output needed for IIC on a 20mA port
    IOPORT_CFG_DRIVE_HIGH            = 0x00000C00,      ///< Sets pin drive output to high
    IOPORT_CFG_EVENT_RISING_EDGE     = 0x00001000,      ///< Sets pin event trigger to rising edge
    IOPORT_CFG_EVENT_FALLING_EDGE    = 0x00002000,      ///< Sets pin event trigger to falling edge
    IOPORT_CFG_EVENT_BOTH_EDGES      = 0x00003000,      ///< Sets pin event trigger to both edges
    IOPORT_CFG_IRQ_ENABLE            = 0x00004000,      ///< Sets pin as an IRQ pin
    IOPORT_CFG_ANALOG_ENABLE         = 0x00008000,      ///< Enables pin to operate as an analog pin
    IOPORT_CFG_PERIPHERAL_PIN        = 0x00010000       ///< Enables pin to operate as a peripheral pin
} ioport_cfg_options_t;

/** Pin identifier and pin PFS pin configuration value */
typedef struct st_ioport_pin_cfg
{
    uint32_t           pin_cfg;         ///< Pin PFS configuration - Use ioport_cfg_options_t parameters to configure
    ioport_port_pin_t  pin;             ///< Pin identifier
} ioport_pin_cfg_t;

/** Multiple pin configuration data for loading into PFS registers by R_IOPORT_Init()  */
typedef struct st_ioport_cfg
{
    uint16_t                 number_of_pins; ///< Number of pins for which there is configuration data
    ioport_pin_cfg_t const * p_pin_cfg_data; ///< Pin configuration data
} ioport_cfg_t;

/** IOPort driver structure. IOPort functions implemented at the HAL layer will follow this API. */
typedef struct st_ioport_api
{
    /** Initialize internal driver data and initial pin configurations.  Called during startup.  Do
     * not call this API during runtime.  Use ioport_api_t::pinsCfg for runtime reconfiguration of
     * multiple pins.
     * @par Implemented as
     * - R_IOPORT_Init()
     * @param[in]  p_cfg				Pointer to pin configuration data array.
     */
    ssp_err_t (* init)(const ioport_cfg_t * p_cfg);

    /** Configure multiple pins.
     * @par Implemented as
     * - R_IOPORT_PinsCfg()
     * @param[in]  p_cfg                Pointer to pin configuration data array.
     */
    ssp_err_t (* pinsCfg)(const ioport_cfg_t * p_cfg);

    /** Configure settings for an individual pin.
     * @par Implemented as
     * - R_IOPORT_PinCfg()
     * @param[in]  pin                  Pin to be read.
     * @param[in]  cfg                  Configuration options for the pin.
     */
    ssp_err_t (* pinCfg)(ioport_port_pin_t pin,     uint32_t cfg);

    /** Set the pin direction of a pin.
     * @par Implemented as
     * - R_IOPORT_PinDirectionSet()
     * @param[in]  pin                  Pin being configured.
     * @param[in]  direction            Direction to set pin to which is a member of ioport_direction_t.
     */
    ssp_err_t (* pinDirectionSet)(ioport_port_pin_t pin,    ioport_direction_t direction);

    /** Read the event input data of the specified pin and return the level.
     * @par Implemented as
     * - R_IOPORT_PinEventInputRead()
     * @param[in]  pin                  Pin to be read.
     * @param[in]  p_pin_event         Pointer to return the event data.
     */
    ssp_err_t (* pinEventInputRead)(ioport_port_pin_t pin,    ioport_level_t * p_pin_event);

    /** Write pin event data.
     * @par Implemented as
     * - R_IOPORT_PinEventOutputWrite()
     * @param[in]  pin 					Pin event data is to be written to.
     * @param[in]  pin_value			Level to be written to pin output event.
     */
    ssp_err_t (* pinEventOutputWrite)(ioport_port_pin_t pin,    ioport_level_t pin_value);

    /** Configure the PHY mode of the Ethernet channels.
     * @par Implemented as
     * - R_IOPORT_EthernetModeCfg()
     * @param[in]  channel              Channel configuration will be set for.
     * @param[in]  mode                 PHY mode to set the channel to.
     */
    ssp_err_t (* pinEthernetModeCfg)(ioport_ethernet_channel_t channel,     ioport_ethernet_mode_t mode);

    /** Read level of a pin.
     * @par Implemented as
     * - R_IOPORT_PinRead()
     * @param[in]  pin                  Pin to be read.
     * @param[in]  p_pin_value          Pointer to return the pin level.
     */
    ssp_err_t (* pinRead)(ioport_port_pin_t pin,    ioport_level_t * p_pin_value);

    /** Write specified level to a pin.
     * @par Implemented as
     * - R_IOPORT_PinWrite()
     * @param[in]  pin                  Pin to be written to.
     * @param[in]  level                State to be written to the pin.
     */
    ssp_err_t (* pinWrite)(ioport_port_pin_t pin,    ioport_level_t level);

    /** Set the direction of one or more pins on a port.
     * @par Implemented as
     * - R_IOPORT_PortDirectionSet()
     * @param[in]  port                 Port being configured.
     * @param[in]  direction_values     Value controlling direction of pins on port (1 - output, 0 - input).
     * @param[in]  mask                 Mask controlling which pins on the port are to be configured.
     */
    ssp_err_t (* portDirectionSet)(ioport_port_t port,   ioport_size_t direction_values, ioport_size_t mask);

    /** Read captured event data for a port.
     * @par Implemented as
     * - R_IOPORT_PortEventInputRead()
     * @param[in]  port                 Port to be read.
     * @param[in]  p_event_data         Pointer to return the event data.
     */
    ssp_err_t (* portEventInputRead)(ioport_port_t port,   ioport_size_t * p_event_data);

    /** Write event output data for a port.
     * @par Implemented as
     * - R_IOPORT_PortEventOutputWrite()
     * @param[in]  port                 Port event data will be written to.
     * @param[in]  event_data           Data to be written as event data to specified port.
     * @param[in]  mask_value           Each bit set to 1 in the mask corresponds to that bit's value in event data.
     * being written to port.
     */
    ssp_err_t (* portEventOutputWrite)(ioport_port_t port,   ioport_size_t event_data, ioport_size_t mask_value);

    /** Read states of pins on the specified port.
     * @par Implemented as
     * - R_IOPORT_PortRead()
     * @param[in]  port                 Port to be read.
     * @param[in]  p_port_value         Pointer to return the port value.
     */
    ssp_err_t (* portRead)(ioport_port_t port,   ioport_size_t * p_port_value);

    /** Write to multiple pins on a port.
     * @par Implemented as
     * - R_IOPORT_PortWrite()
     * @param[in]  port                 Port to be written to.
     * @param[in]  value                Value to be written to the port.
     * @param[in]  mask                 Mask controlling which pins on the port are written to.
     */
    ssp_err_t (* portWrite)(ioport_port_t port,   ioport_size_t value, ioport_size_t mask);

    /** Return the version of the IOPort driver.
     * @par Implemented as
     * - R_IOPORT_VersionGet()
     * @param[out]  p_data              Memory address to return version information to.
     */
    ssp_err_t (* versionGet)(ssp_version_t * p_data);
} ioport_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_ioport_instance
{
    ioport_cfg_t const * p_cfg;     ///< Pointer to the configuration structure for this instance
    ioport_api_t const * p_api;     ///< Pointer to the API structure for this instance
} ioport_instance_t;

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* DRV_IOPORT_API_H */

/*******************************************************************************************************************//**
 * @} (end defgroup IOPORT_API)
 **********************************************************************************************************************/
