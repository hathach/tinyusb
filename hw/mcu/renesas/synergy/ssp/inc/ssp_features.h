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
* File Name    : ssp_features.h
* Description  : Contains common feature types and functions used by the SSP.
***********************************************************************************************************************/

#ifndef SSP_FEATURES_H_
#define SSP_FEATURES_H_

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
/* C99 includes. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
/* Different compiler support. */
#include "ssp_common_api.h"
#include "../src/bsp/mcu/all/bsp_compiler_support.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/** Number of Cortex processor exceptions, used as an offset from XPSR value for the IRQn_Type macro. */
#define SSP_PRIV_CORTEX_PROCESSOR_EXCEPTIONS   (16U)

/** Used to signify that the requested IRQ vector is not defined in this system. */
#define SSP_INVALID_VECTOR                     ((IRQn_Type) -33)

/** Used to allocated vector table and vector information array for peripherals with a single channel.  Parameters
 * are as follows:
 *    1. ISR function name
 *    2. IP name (ssp_ip_t enum without the SSP_IP_ prefix).
 *    3. Signal name (ssp_signal_t enum without the SSP_SIGNAL_\<IP_NAME>_ prefix), where \<IP_NAME> is the IP name
 *       from (2) above.
 */
/* Parentheses cannot be added around macro parameters since they are used to construct variable
 * and section names that do not allow parentheses. */
/*LDRA_INSPECTWINDOW 50 */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define SSP_VECTOR_DEFINE(isr,ip,signal) \
    void isr (void); \
    static void * gp_ctrl_##ip##_##signal; \
    const ssp_vector_t g_vector_##ip##_##signal BSP_PLACE_IN_SECTION_V2(".vector." #ip"_"#signal )=isr; \
    const ssp_vector_info_t g_vector_info_##ip##_##signal  BSP_PLACE_IN_SECTION_V2(".vector_info."#ip"_"#signal)= \
        {.event_number=ELC_EVENT_##ip##_##signal, .ip_id = SSP_IP_##ip, .ip_channel=0U, \
      .ip_unit=0U, .ip_signal=SSP_SIGNAL_##ip##_##signal, .pp_ctrl = &gp_ctrl_##ip##_##signal};

/** Used to allocated vector table and vector information array for peripherals with multiple channels.  Parameters
 * are as follows:
 *    1. ISR function name
 *    2. IP name (ssp_ip_t enum without the SSP_IP_ prefix).
 *    3. Signal name (ssp_signal_t enum without the SSP_SIGNAL_\<IP_NAME>_ prefix), where \<IP_NAME> is the IP name
 *       from (2) above.
 *    4. Channel number
 */
/* Parentheses cannot be added around macro parameters since they are used to construct variable
 * and section names that do not allow parentheses. */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define SSP_VECTOR_DEFINE_CHAN(isr,ip,signal,channel) \
    void isr (void); \
    static void * gp_ctrl_##ip##_##channel##_##signal; \
    const ssp_vector_t g_vector_##ip##_##channel##_##signal \
      BSP_PLACE_IN_SECTION_V2(".vector." #ip"_"#channel"_"#signal )=isr; \
    const ssp_vector_info_t g_vector_info_##ip##_##channel##_##signal  \
      BSP_PLACE_IN_SECTION_V2(".vector_info."#ip"_"#channel"_"#signal)= \
        {.event_number=ELC_EVENT_##ip##channel##_##signal, \
      .ip_id = SSP_IP_##ip, .ip_channel=(channel), .ip_unit=0U, \
    .ip_signal=SSP_SIGNAL_##ip##_##signal, .pp_ctrl = &gp_ctrl_##ip##_##channel##_##signal};

/** Used to allocated vector table and vector information array for peripherals with multiple units.  Parameters
 * are as follows:
 *    1. ISR function name
 *    2. IP name (ssp_ip_t enum without the SSP_IP_ prefix).
 *    3. Signal name (ssp_signal_t enum without the SSP_SIGNAL_\<IP_NAME>_ prefix), where \<IP_NAME> is the IP name
 *       from (2) above.
 *    4. Channel number
 *    4. Unit name (ssp_ip_unit_t enum without the SSP_IP_UNIT_\<IP_NAME> prefix), where \<IP_NAME> is the IP name
 *       from (2) above.
 */
/* Parentheses cannot be added around macro parameters since they are used to construct variable
 * and section names that do not allow parentheses. */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define SSP_VECTOR_DEFINE_UNIT(isr,ip,unit_name,signal,channel) \
    void isr (void); \
    static void * gp_ctrl_##ip##_##unit_name##_##channel##_##signal; \
    const ssp_vector_t g_vector_##ip##_##unit_name##_##channel##_##signal \
        BSP_PLACE_IN_SECTION_V2(".vector."#ip"_"#unit_name"_"#channel"_"#signal )=isr; \
    const ssp_vector_info_t g_vector_info_##ip##_##unit_name##_##channel##_##signal  \
        BSP_PLACE_IN_SECTION_V2(".vector_info."#ip"_"#unit_name"_"#channel"_"#signal)= \
        {.event_number=ELC_EVENT_##ip##unit_name##_##signal, \
        .ip_id = SSP_IP_##ip, .ip_channel=(channel), .ip_unit=SSP_IP_UNIT_##ip##unit_name, \
        .ip_signal=SSP_SIGNAL_##ip##_##signal, .pp_ctrl = &gp_ctrl_##ip##_##unit_name##_##channel##_##signal};

/** Used to allocated hardware locks.  Parameters are as follows:
 *    1. IP name (ssp_ip_t enum without the SSP_IP_ prefix).
 *    2. Unit number (used for blocks with variations like USB, not to be confused with ADC unit).
 *    3. Channel number
 */
/* Parentheses cannot be added around macro parameters since they are used to construct variable
 * and section names that do not allow parentheses. */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S *//*LDRA_INSPECTED 78 S */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define SSP_HW_LOCK_DEFINE(ip,unit_number,channel_number) \
    bsp_lock_t g_hw_lock_##ip##_##unit_number##_##channel_number BSP_PACKED \
        BSP_PLACE_IN_SECTION_V2(".hw_lock."#ip"_"#unit_number"_"#channel_number); \
    const ssp_feature_t g_lock_lookup_##ip##_##unit_number##_##channel_number  \
        BSP_PLACE_IN_SECTION_V2(".hw_lock_lookup."#ip"_"#unit_number"_"#channel_number) = \
        {.id = SSP_IP_##ip, .channel=channel_number, .unit=unit_number};

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

typedef enum e_ssp_ip
{
    SSP_IP_CFLASH=0,
    SSP_IP_DFLASH=1,
    SSP_IP_RAM=2,
    SSP_IP_SYSTEM=3,    SSP_IP_LVD=3,    SSP_IP_CGC=3,    SSP_IP_LPM=3,
    SSP_IP_FCU=4,
    SSP_IP_DEBUG=5,
    SSP_IP_ICU=6,
    SSP_IP_DMAC=7,
    SSP_IP_DTC=8,
    SSP_IP_IOPORT=9,
    SSP_IP_PFS=10,
    SSP_IP_ELC=11,
    SSP_IP_BSC=12,
    SSP_IP_MPU=13,
    SSP_IP_MSTP=14,
    SSP_IP_MMF=15,
    SSP_IP_KEY=16,
    SSP_IP_CAC=17,
    SSP_IP_DOC=18,
    SSP_IP_CRC=19,
    SSP_IP_SCI=20,
    SSP_IP_IIC=21,
    SSP_IP_SPI=22,
    SSP_IP_CTSU=23,
    SSP_IP_SCE=24,
    SSP_IP_SLCDC=25,
    SSP_IP_AES=26,
    SSP_IP_TRNG=27,
    SSP_IP_ROMC=30,
    SSP_IP_SRAM=31,
    SSP_IP_ADC=32,
    SSP_IP_DAC=33,
    SSP_IP_TSN=34,
    SSP_IP_DAAD=35,
    SSP_IP_COMP_HS=36,
    SSP_IP_COMP_LP=37,
    SSP_IP_OPAMP=38,
    SSP_IP_SDADC=39,
    SSP_IP_RTC=40,
    SSP_IP_WDT=41,
    SSP_IP_IWDT=42,
    SSP_IP_GPT=43,
    SSP_IP_POEG=44,
    SSP_IP_OPS=45,
    SSP_IP_PSD=46,
    SSP_IP_AGT=47,
    SSP_IP_CAN=48,
    SSP_IP_IRDA=49,
    SSP_IP_QSPI=50,
    SSP_IP_USB=51,
    SSP_IP_SDHIMMC=52,
    SSP_IP_SRC=53,
    SSP_IP_SSI=54,
    SSP_IP_DALI=55,
    SSP_IP_ETHER=64,    SSP_IP_EDMAC=64,
    SSP_IP_EPTPC=65,
    SSP_IP_PDC=66,
    SSP_IP_GLCDC=67,
    SSP_IP_DRW=68,
    SSP_IP_JPEG=69,
    SSP_IP_MAX
} ssp_ip_t;


typedef enum e_ssp_signal
{
    SSP_SIGNAL_ADC_COMPARE_MATCH=0,
    SSP_SIGNAL_ADC_COMPARE_MISMATCH,
    SSP_SIGNAL_ADC_SCAN_END,
    SSP_SIGNAL_ADC_SCAN_END_B,
    SSP_SIGNAL_ADC_WINDOW_A,
    SSP_SIGNAL_ADC_WINDOW_B,
    SSP_SIGNAL_AES_RDREQ=0,
    SSP_SIGNAL_AES_WRREQ,
    SSP_SIGNAL_AGT_COMPARE_A=0,
    SSP_SIGNAL_AGT_COMPARE_B,
    SSP_SIGNAL_AGT_INT,
    SSP_SIGNAL_CAC_FREQUENCY_ERROR=0,
    SSP_SIGNAL_CAC_MEASUREMENT_END,
    SSP_SIGNAL_CAC_OVERFLOW,
    SSP_SIGNAL_CAN_ERROR=0,
    SSP_SIGNAL_CAN_FIFO_RX,
    SSP_SIGNAL_CAN_FIFO_TX,
    SSP_SIGNAL_CAN_MAILBOX_RX,
    SSP_SIGNAL_CAN_MAILBOX_TX,
    SSP_SIGNAL_CGC_MOSC_STOP=0,
    SSP_SIGNAL_LPM_SNOOZE_REQUEST,
    SSP_SIGNAL_LVD_LVD1,
    SSP_SIGNAL_LVD_LVD2,
    SSP_SIGNAL_VBATT_LVD,
    SSP_SIGNAL_LVD_VBATT = SSP_SIGNAL_VBATT_LVD,
    SSP_SIGNAL_COMP_HS_INT=0,
    SSP_SIGNAL_COMP_LP=0,
    SSP_SIGNAL_COMP_LP_INT=0,
    SSP_SIGNAL_CTSU_END=0,
    SSP_SIGNAL_CTSU_READ,
    SSP_SIGNAL_CTSU_WRITE,
    SSP_SIGNAL_DALI_DEI=0,
    SSP_SIGNAL_DALI_CLI,
    SSP_SIGNAL_DALI_SDI,
    SSP_SIGNAL_DALI_BPI,
    SSP_SIGNAL_DALI_FEI,
    SSP_SIGNAL_DALI_SDI_OR_BPI,
    SSP_SIGNAL_DMAC_INT=0,
    SSP_SIGNAL_DOC_INT=0,
    SSP_SIGNAL_DRW_INT=0,
    SSP_SIGNAL_DTC_COMPLETE=0,
    SSP_SIGNAL_DTC_END,
    SSP_SIGNAL_EDMAC_EINT=0,
    SSP_SIGNAL_ELC_SOFTWARE_EVENT_0=0,
    SSP_SIGNAL_ELC_SOFTWARE_EVENT_1,
    SSP_SIGNAL_EPTPC_IPLS=0,
    SSP_SIGNAL_EPTPC_MINT,
    SSP_SIGNAL_EPTPC_PINT,
    SSP_SIGNAL_EPTPC_TIMER0_FALL,
    SSP_SIGNAL_EPTPC_TIMER0_RISE,
    SSP_SIGNAL_EPTPC_TIMER1_FALL,
    SSP_SIGNAL_EPTPC_TIMER1_RISE,
    SSP_SIGNAL_EPTPC_TIMER2_FALL,
    SSP_SIGNAL_EPTPC_TIMER2_RISE,
    SSP_SIGNAL_EPTPC_TIMER3_FALL,
    SSP_SIGNAL_EPTPC_TIMER3_RISE,
    SSP_SIGNAL_EPTPC_TIMER4_FALL,
    SSP_SIGNAL_EPTPC_TIMER4_RISE,
    SSP_SIGNAL_EPTPC_TIMER5_FALL,
    SSP_SIGNAL_EPTPC_TIMER5_RISE,
    SSP_SIGNAL_FCU_FIFERR=0,
    SSP_SIGNAL_FCU_FRDYI,
    SSP_SIGNAL_GLCDC_LINE_DETECT=0,
    SSP_SIGNAL_GLCDC_UNDERFLOW_1,
    SSP_SIGNAL_GLCDC_UNDERFLOW_2,
    SSP_SIGNAL_GPT_CAPTURE_COMPARE_A=0,
    SSP_SIGNAL_GPT_CAPTURE_COMPARE_B,
    SSP_SIGNAL_GPT_COMPARE_C,
    SSP_SIGNAL_GPT_COMPARE_D,
    SSP_SIGNAL_GPT_COMPARE_E,
    SSP_SIGNAL_GPT_COMPARE_F,
    SSP_SIGNAL_GPT_COUNTER_OVERFLOW,
    SSP_SIGNAL_GPT_COUNTER_UNDERFLOW,
    SSP_SIGNAL_GPT_AD_TRIG_A,
    SSP_SIGNAL_GPT_AD_TRIG_B,
    SSP_SIGNAL_OPS_UVW_EDGE,
    SSP_SIGNAL_ICU_IRQ0=0,
    SSP_SIGNAL_ICU_IRQ1,
    SSP_SIGNAL_ICU_IRQ2,
    SSP_SIGNAL_ICU_IRQ3,
    SSP_SIGNAL_ICU_IRQ4,
    SSP_SIGNAL_ICU_IRQ5,
    SSP_SIGNAL_ICU_IRQ6,
    SSP_SIGNAL_ICU_IRQ7,
    SSP_SIGNAL_ICU_IRQ8,
    SSP_SIGNAL_ICU_IRQ9,
    SSP_SIGNAL_ICU_IRQ10,
    SSP_SIGNAL_ICU_IRQ11,
    SSP_SIGNAL_ICU_IRQ12,
    SSP_SIGNAL_ICU_IRQ13,
    SSP_SIGNAL_ICU_IRQ14,
    SSP_SIGNAL_ICU_IRQ15,
    SSP_SIGNAL_ICU_SNOOZE_CANCEL,
    SSP_SIGNAL_IIC_ERI=0,
    SSP_SIGNAL_IIC_RXI,
    SSP_SIGNAL_IIC_TEI,
    SSP_SIGNAL_IIC_TXI,
    SSP_SIGNAL_IIC_WUI,
    SSP_SIGNAL_IOPORT_EVENT_1=0,
    SSP_SIGNAL_IOPORT_EVENT_2,
    SSP_SIGNAL_IOPORT_EVENT_3,
    SSP_SIGNAL_IOPORT_EVENT_4,
    SSP_SIGNAL_IWDT_UNDERFLOW=0,
    SSP_SIGNAL_JPEG_JDTI=0,
    SSP_SIGNAL_JPEG_JEDI,
    SSP_SIGNAL_KEY_INT=0,
    SSP_SIGNAL_PDC_FRAME_END=0,
    SSP_SIGNAL_PDC_INT,
    SSP_SIGNAL_PDC_RECEIVE_DATA_READY,
    SSP_SIGNAL_POEG_EVENT=0,
    SSP_SIGNAL_QSPI_INT=0,
    SSP_SIGNAL_RTC_ALARM=0,
    SSP_SIGNAL_RTC_PERIOD,
    SSP_SIGNAL_RTC_CARRY,
    SSP_SIGNAL_SCE_INTEGRATE_RDRDY=0,
    SSP_SIGNAL_SCE_INTEGRATE_WRRDY,
    SSP_SIGNAL_SCE_LONG_PLG,
    SSP_SIGNAL_SCE_PROC_BUSY,
    SSP_SIGNAL_SCE_RDRDY_0,
    SSP_SIGNAL_SCE_RDRDY_1,
    SSP_SIGNAL_SCE_ROMOK,
    SSP_SIGNAL_SCE_TEST_BUSY,
    SSP_SIGNAL_SCE_WRRDY_0,
    SSP_SIGNAL_SCE_WRRDY_1,
    SSP_SIGNAL_SCE_WRRDY_4,
    SSP_SIGNAL_SCI_AM=0,
    SSP_SIGNAL_SCI_ERI,
    SSP_SIGNAL_SCI_RXI,
    SSP_SIGNAL_SCI_RXI_OR_ERI,
    SSP_SIGNAL_SCI_TEI,
    SSP_SIGNAL_SCI_TXI,
    SSP_SIGNAL_SDADC_ADI=0,
    SSP_SIGNAL_SDADC_SCANEND,
    SSP_SIGNAL_SDADC_CALIEND,
    SSP_SIGNAL_SDHIMMC_ACCS=0,
    SSP_SIGNAL_SDHIMMC_CARD,
    SSP_SIGNAL_SDHIMMC_DMA_REQ,
    SSP_SIGNAL_SDHIMMC_SDIO,
    SSP_SIGNAL_SPI_ERI=0,
    SSP_SIGNAL_SPI_IDLE,
    SSP_SIGNAL_SPI_RXI,
    SSP_SIGNAL_SPI_TEI,
    SSP_SIGNAL_SPI_TXI,
    SSP_SIGNAL_SRC_CONVERSION_END=0,
    SSP_SIGNAL_SRC_INPUT_FIFO_EMPTY,
    SSP_SIGNAL_SRC_OUTPUT_FIFO_FULL,
    SSP_SIGNAL_SRC_OUTPUT_FIFO_OVERFLOW,
    SSP_SIGNAL_SRC_OUTPUT_FIFO_UNDERFLOW,
    SSP_SIGNAL_SSI_INT=0,
    SSP_SIGNAL_SSI_RXI,
    SSP_SIGNAL_SSI_TXI,
    SSP_SIGNAL_SSI_TXI_RXI,
    SSP_SIGNAL_TRNG_RDREQ=0,
    SSP_SIGNAL_USB_FIFO_0=0,
    SSP_SIGNAL_USB_FIFO_1,
    SSP_SIGNAL_USB_INT,
    SSP_SIGNAL_USB_RESUME,
    SSP_SIGNAL_USB_USB_INT_RESUME,
    SSP_SIGNAL_WDT_UNDERFLOW=0,
} ssp_signal_t;

typedef enum e_ssp_ip_unit
{
    SSP_IP_UNIT_USBFS = 0,
    SSP_IP_UNIT_USBHS = 1,
} ssp_ip_unit_t;

typedef struct st_ssp_vector_info
{
    void ** pp_ctrl;   /* pointer to control handle for usage by ISR */
    uint32_t event_number: 10;
    uint32_t ip_channel:6;
    uint32_t ip_id:8;
    uint32_t ip_unit:3;
    uint32_t ip_signal:5;
} ssp_vector_info_t;

typedef void (* ssp_vector_t)(void);

typedef union st_ssp_feature
{
    /*LDRA_INSPECTED 381 S Anonymous structures and unions are allowed in SSP code. */
    struct {
        ssp_ip_t id      :8;
        uint32_t unit    :8;
        uint32_t channel :16;
    };
    uint32_t word;
} ssp_feature_t;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Inline Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief      Return active interrupt vector number value
 *
 * @return     Active interrupt vector number value
 **********************************************************************************************************************/
__STATIC_INLINE IRQn_Type R_SSP_CurrentIrqGet(void)
{
    uint32_t xpsr_value = __get_xPSR();
    xPSR_Type * p_xpsr = (xPSR_Type *) &xpsr_value;
    return (IRQn_Type) (p_xpsr->b.ISR - SSP_PRIV_CORTEX_PROCESSOR_EXCEPTIONS);
}

/*******************************************************************************************************************//**
 * @brief      Finds the vector information associated with the requested IRQ.
 *
 * @param[in]  irq            IRQ number (parameter checking must ensure the IRQ number is valid before calling this
 *                            function.
 * @param[out] pp_vector_info Pointer to pointer to vector information for IRQ.
 **********************************************************************************************************************/
__STATIC_INLINE void R_SSP_VectorInfoGet(IRQn_Type const irq, ssp_vector_info_t ** pp_vector_info)
{
    SSP_PARAMETER_NOT_USED(pp_vector_info);

    /* This provides access to the vector information array defined in bsp_irq.c. This is an inline function instead of
     * being part of bsp_irq.c for performance considerations because it is used in interrupt service routines. */
    extern ssp_vector_info_t * const gp_vector_information;

    *pp_vector_info = &gp_vector_information[irq];
}

#endif /* SSP_FEATURES_H_ */
