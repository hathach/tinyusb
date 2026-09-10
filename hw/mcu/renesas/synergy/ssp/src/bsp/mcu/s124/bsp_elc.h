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
/***********************************************************************************************************************
* File Name    : bsp_elc.h
* Description  : ELC Interface.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @addtogroup BSP_MCU_S124
 *
 * @{
 **********************************************************************************************************************/

#ifndef BSP_ELCDEFS_H_
#define BSP_ELCDEFS_H_

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/
/** Possible peripherals to be linked to event signals */
typedef enum e_elc_peripheral
{
    ELC_PERIPHERAL_GPT_A                                                        =    (0),
    ELC_PERIPHERAL_GPT_B                                                        =    (1),
    ELC_PERIPHERAL_GPT_C                                                        =    (2),
    ELC_PERIPHERAL_GPT_D                                                        =    (3),
    ELC_PERIPHERAL_ADC0                                                         =    (8),
    ELC_PERIPHERAL_ADC0_B                                                       =    (9),
    ELC_PERIPHERAL_DAC0                                                         =   (12),
    ELC_PERIPHERAL_IOPORT1                                                      =   (14),
    ELC_PERIPHERAL_IOPORT2                                                      =   (15),
    ELC_PERIPHERAL_CTSU                                                         =   (18),
} elc_peripheral_t;

/** Sources of event signals to be linked to other peripherals or the CPU1
 * @note This list may change based on device. This list is for S124.
 * */
typedef enum e_elc_event
{
    ELC_EVENT_ICU_IRQ0                                                          =    (1),
    ELC_EVENT_ICU_IRQ1                                                          =    (2),
    ELC_EVENT_ICU_IRQ2                                                          =    (3),
    ELC_EVENT_ICU_IRQ3                                                          =    (4),
    ELC_EVENT_ICU_IRQ4                                                          =    (5),
    ELC_EVENT_ICU_IRQ5                                                          =    (6),
    ELC_EVENT_ICU_IRQ6                                                          =    (7),
    ELC_EVENT_ICU_IRQ7                                                          =    (8),
    ELC_EVENT_DTC_COMPLETE                                                      =    (9),
    ELC_EVENT_DTC_END                                                           =   (10),
    ELC_EVENT_ICU_SNOOZE_CANCEL                                                 =   (11),
    ELC_EVENT_FCU_FRDYI                                                         =   (12),
    ELC_EVENT_LVD_LVD1                                                          =   (13),
    ELC_EVENT_LVD_LVD2                                                          =   (14),
    ELC_EVENT_CGC_MOSC_STOP                                                     =   (15),
    ELC_EVENT_LPM_SNOOZE_REQUEST                                                =   (16),
    ELC_EVENT_AGT0_INT                                                          =   (17),
    ELC_EVENT_AGT0_COMPARE_A                                                    =   (18),
    ELC_EVENT_AGT0_COMPARE_B                                                    =   (19),
    ELC_EVENT_AGT1_INT                                                          =   (20),
    ELC_EVENT_AGT1_COMPARE_A                                                    =   (21),
    ELC_EVENT_AGT1_COMPARE_B                                                    =   (22),
    ELC_EVENT_IWDT_UNDERFLOW                                                    =   (23),
    ELC_EVENT_WDT_UNDERFLOW                                                     =   (24),
    ELC_EVENT_RTC_ALARM                                                         =   (25),
    ELC_EVENT_RTC_PERIOD                                                        =   (26),
    ELC_EVENT_RTC_CARRY                                                         =   (27),
    ELC_EVENT_ADC0_SCAN_END                                                     =   (28),
    ELC_EVENT_ADC0_SCAN_END_B                                                   =   (29),
    ELC_EVENT_ADC0_WINDOW_A                                                     =   (30),
    ELC_EVENT_ADC0_WINDOW_B                                                     =   (31),
    ELC_EVENT_ADC0_COMPARE_MATCH                                                =   (32),
    ELC_EVENT_ADC0_COMPARE_MISMATCH                                             =   (33),
    ELC_EVENT_COMP_LP_0                                                         =   (34),
    ELC_EVENT_COMP_LP0_INT                                                      =   (34),
    ELC_EVENT_COMP_LP_1                                                         =   (35),
    ELC_EVENT_COMP_LP1_INT                                                      =   (35),
    ELC_EVENT_USBFS_INT                                                         =   (36),
    ELC_EVENT_USBFS_RESUME                                                      =   (37),
    ELC_EVENT_IIC0_RXI                                                          =   (38),
    ELC_EVENT_IIC0_TXI                                                          =   (39),
    ELC_EVENT_IIC0_TEI                                                          =   (40),
    ELC_EVENT_IIC0_ERI                                                          =   (41),
    ELC_EVENT_IIC0_WUI                                                          =   (42),
    ELC_EVENT_IIC1_RXI                                                          =   (43),
    ELC_EVENT_IIC1_TXI                                                          =   (44),
    ELC_EVENT_IIC1_TEI                                                          =   (45),
    ELC_EVENT_IIC1_ERI                                                          =   (46),
    ELC_EVENT_CTSU_WRITE                                                        =   (47),
    ELC_EVENT_CTSU_READ                                                         =   (48),
    ELC_EVENT_CTSU_END                                                          =   (49),
    ELC_EVENT_KEY_INT                                                           =   (50),
    ELC_EVENT_DOC_INT                                                           =   (51),
    ELC_EVENT_CAC_FREQUENCY_ERROR                                               =   (52),
    ELC_EVENT_CAC_MEASUREMENT_END                                               =   (53),
    ELC_EVENT_CAC_OVERFLOW                                                      =   (54),
    ELC_EVENT_CAN0_ERROR                                                        =   (55),
    ELC_EVENT_CAN0_FIFO_RX                                                      =   (56),
    ELC_EVENT_CAN0_FIFO_TX                                                      =   (57),
    ELC_EVENT_CAN0_MAILBOX_RX                                                   =   (58),
    ELC_EVENT_CAN0_MAILBOX_TX                                                   =   (59),
    ELC_EVENT_IOPORT_EVENT_1                                                    =   (60),
    ELC_EVENT_IOPORT_EVENT_2                                                    =   (61),
    ELC_EVENT_ELC_SOFTWARE_EVENT_0                                              =   (62),
    ELC_EVENT_ELC_SOFTWARE_EVENT_1                                              =   (63),
    ELC_EVENT_POEG0_EVENT                                                       =   (64),
    ELC_EVENT_POEG1_EVENT                                                       =   (65),
    ELC_EVENT_GPT0_CAPTURE_COMPARE_A                                            =   (66),
    ELC_EVENT_GPT0_CAPTURE_COMPARE_B                                            =   (67),
    ELC_EVENT_GPT0_COMPARE_C                                                    =   (68),
    ELC_EVENT_GPT0_COMPARE_D                                                    =   (69),
    ELC_EVENT_GPT0_COUNTER_OVERFLOW                                             =   (70),
    ELC_EVENT_GPT0_COUNTER_UNDERFLOW                                            =   (71),
    ELC_EVENT_GPT1_CAPTURE_COMPARE_A                                            =   (72),
    ELC_EVENT_GPT1_CAPTURE_COMPARE_B                                            =   (73),
    ELC_EVENT_GPT1_COMPARE_C                                                    =   (74),
    ELC_EVENT_GPT1_COMPARE_D                                                    =   (75),
    ELC_EVENT_GPT1_COUNTER_OVERFLOW                                             =   (76),
    ELC_EVENT_GPT1_COUNTER_UNDERFLOW                                            =   (77),
    ELC_EVENT_GPT2_CAPTURE_COMPARE_A                                            =   (78),
    ELC_EVENT_GPT2_CAPTURE_COMPARE_B                                            =   (79),
    ELC_EVENT_GPT2_COMPARE_C                                                    =   (80),
    ELC_EVENT_GPT2_COMPARE_D                                                    =   (81),
    ELC_EVENT_GPT2_COUNTER_OVERFLOW                                             =   (82),
    ELC_EVENT_GPT2_COUNTER_UNDERFLOW                                            =   (83),
    ELC_EVENT_GPT3_CAPTURE_COMPARE_A                                            =   (84),
    ELC_EVENT_GPT3_CAPTURE_COMPARE_B                                            =   (85),
    ELC_EVENT_GPT3_COMPARE_C                                                    =   (86),
    ELC_EVENT_GPT3_COMPARE_D                                                    =   (87),
    ELC_EVENT_GPT3_COUNTER_OVERFLOW                                             =   (88),
    ELC_EVENT_GPT3_COUNTER_UNDERFLOW                                            =   (89),
    ELC_EVENT_GPT4_CAPTURE_COMPARE_A                                            =   (90),
    ELC_EVENT_GPT4_CAPTURE_COMPARE_B                                            =   (91),
    ELC_EVENT_GPT4_COMPARE_C                                                    =   (92),
    ELC_EVENT_GPT4_COMPARE_D                                                    =   (93),
    ELC_EVENT_GPT4_COUNTER_OVERFLOW                                             =   (94),
    ELC_EVENT_GPT4_COUNTER_UNDERFLOW                                            =   (95),
    ELC_EVENT_GPT5_CAPTURE_COMPARE_A                                            =   (96),
    ELC_EVENT_GPT5_CAPTURE_COMPARE_B                                            =   (97),
    ELC_EVENT_GPT5_COMPARE_C                                                    =   (98),
    ELC_EVENT_GPT5_COMPARE_D                                                    =   (99),
    ELC_EVENT_GPT5_COUNTER_OVERFLOW                                             =  (100),
    ELC_EVENT_GPT5_COUNTER_UNDERFLOW                                            =  (101),
    ELC_EVENT_GPT6_CAPTURE_COMPARE_A                                            =  (102),
    ELC_EVENT_GPT6_CAPTURE_COMPARE_B                                            =  (103),
    ELC_EVENT_GPT6_COMPARE_C                                                    =  (104),
    ELC_EVENT_GPT6_COMPARE_D                                                    =  (105),
    ELC_EVENT_GPT6_COUNTER_OVERFLOW                                             =  (106),
    ELC_EVENT_GPT6_COUNTER_UNDERFLOW                                            =  (107),
    ELC_EVENT_OPS_UVW_EDGE                                                      =  (108),
    ELC_EVENT_SCI0_RXI                                                          =  (109),
    ELC_EVENT_SCI0_TXI                                                          =  (110),
    ELC_EVENT_SCI0_TEI                                                          =  (111),
    ELC_EVENT_SCI0_ERI                                                          =  (112),
    ELC_EVENT_SCI0_AM                                                           =  (113),
    ELC_EVENT_SCI0_RXI_OR_ERI                                                   =  (114),
    ELC_EVENT_SCI1_RXI                                                          =  (115),
    ELC_EVENT_SCI1_TXI                                                          =  (116),
    ELC_EVENT_SCI1_TEI                                                          =  (117),
    ELC_EVENT_SCI1_ERI                                                          =  (118),
    ELC_EVENT_SCI1_AM                                                           =  (119),
    ELC_EVENT_SCI9_RXI                                                          =  (120),
    ELC_EVENT_SCI9_TXI                                                          =  (121),
    ELC_EVENT_SCI9_TEI                                                          =  (122),
    ELC_EVENT_SCI9_ERI                                                          =  (123),
    ELC_EVENT_SCI9_AM                                                           =  (124),
    ELC_EVENT_SPI0_RXI                                                          =  (125),
    ELC_EVENT_SPI0_TXI                                                          =  (126),
    ELC_EVENT_SPI0_IDLE                                                         =  (127),
    ELC_EVENT_SPI0_ERI                                                          =  (128),
    ELC_EVENT_SPI0_TEI                                                          =  (129),
    ELC_EVENT_SPI1_RXI                                                          =  (130),
    ELC_EVENT_SPI1_TXI                                                          =  (131),
    ELC_EVENT_SPI1_IDLE                                                         =  (132),
    ELC_EVENT_SPI1_ERI                                                          =  (133),
    ELC_EVENT_SPI1_TEI                                                          =  (134),
    ELC_EVENT_AES_WRREQ                                                         =  (135),
    ELC_EVENT_AES_RDREQ                                                         =  (136),
    ELC_EVENT_TRNG_RDREQ                                                        =  (137),
} elc_event_t;

#endif /* BSP_ELCDEFS_H_ */

/** @} (end addtogroup BSP_MCU_S124) */
