/**
  ******************************************************************************
  * @file    tcpp0203.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the
  *          tcpp0203.c driver.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef TCPP0203_H
#define TCPP0203_H

/* Includes ------------------------------------------------------------------*/
#include "tcpp0203_reg.h"
#include <stddef.h>

/** @addtogroup BSP
  * @{
  */

/** @addtogroup Component
  * @{
  */

/** @addtogroup TCPP0203
  * @{
  */

/* Exported types ------------------------------------------------------------*/

/** @defgroup TCPP0203_Exported_Types TCPP0203 Exported Types
  * @{
  */
typedef int32_t (*TCPP0203_Init_Func)(void);
typedef int32_t (*TCPP0203_DeInit_Func)(void);
typedef int32_t (*TCPP0203_GetTick_Func)(void);
typedef int32_t (*TCPP0203_WriteReg_Func)(uint16_t, uint16_t, uint8_t *, uint16_t);
typedef int32_t (*TCPP0203_ReadReg_Func)(uint16_t, uint16_t, uint8_t *, uint16_t);

typedef struct
{
  TCPP0203_Init_Func          Init;
  TCPP0203_DeInit_Func        DeInit;
  uint16_t                    Address;
  TCPP0203_WriteReg_Func      WriteReg;
  TCPP0203_ReadReg_Func       ReadReg;
  TCPP0203_GetTick_Func       GetTick;
} TCPP0203_IO_t;


typedef struct
{
  TCPP0203_IO_t         IO;
  TCPP0203_ctx_t        Ctx;
  uint8_t               IsInitialized;
} TCPP0203_Object_t;

typedef struct
{
  int32_t (*Init)(TCPP0203_Object_t *);
  int32_t (*DeInit)(TCPP0203_Object_t *);
  int32_t (*Reset)(TCPP0203_Object_t *);
  int32_t (*SetVConnSwitch)(TCPP0203_Object_t *, uint8_t);
  int32_t (*SetGateDriverProvider)(TCPP0203_Object_t *, uint8_t);
  int32_t (*SetGateDriverConsumer)(TCPP0203_Object_t *, uint8_t);
  int32_t (*SetPowerMode)(TCPP0203_Object_t *, uint8_t);
  int32_t (*SetVBusDischarge)(TCPP0203_Object_t *, uint8_t);
  int32_t (*SetVConnDischarge)(TCPP0203_Object_t *, uint8_t);
  int32_t (*GetVConnSwitchAck)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetGateDriverProviderAck)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetGateDriverConsumerAck)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetPowerModeAck)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetVBusDischargeAck)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetVConnDischargeAck)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetOCPVConnFlag)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetOCPVBusFlag)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetOVPVBusFlag)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetOVPCCFlag)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetOTPFlag)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*GetVBusOkFlag)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*ReadTCPPType)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*ReadVCONNPower)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*WriteCtrlRegister)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*ReadAckRegister)(TCPP0203_Object_t *, uint8_t *);
  int32_t (*ReadFlagRegister)(TCPP0203_Object_t *, uint8_t *);
} TCPP0203_Drv_t;

/**
  * @}
  */

/** @defgroup TCPP0203_Exported_Constants TCPP0203 Exported Constants
  * @{
  */
/**
  * @brief  TCPP0203 Driver Response codes
  */
#define TCPP0203_OK                (0)
#define TCPP0203_ERROR             (-1)

/**
  * @brief  TCPP0203 possible I2C Addresses
  */
#define TCPP0203_I2C_ADDRESS_X68                (0x68U)
#define TCPP0203_I2C_ADDRESS_X6A                (0x6AU)

/**
  * @brief  TCPP0203 Reg0 Reset Value
  */
#define TCPP0203_REG0_RST_VALUE                   TCPP0203_GD_CONSUMER_SWITCH_CLOSED

/**
  * @brief  TCPP0203 VCONN Switch
  */
#define TCPP0203_VCONN_SWITCH_POS                 (0U)
#define TCPP0203_VCONN_SWITCH_MSK                 (0x03U << TCPP0203_VCONN_SWITCH_POS)
#define TCPP0203_VCONN_SWITCH_OPEN                (0x00U)
#define TCPP0203_VCONN_SWITCH_CC1                 (0x01U << TCPP0203_VCONN_SWITCH_POS)
#define TCPP0203_VCONN_SWITCH_CC2                 (0x02U << TCPP0203_VCONN_SWITCH_POS)

/**
  * @brief  TCPP0203 Gate Driver Provider values
  */
#define TCPP0203_GD_PROVIDER_SWITCH_POS           (2U)
#define TCPP0203_GD_PROVIDER_SWITCH_MSK           (0x01U << TCPP0203_GD_PROVIDER_SWITCH_POS)
#define TCPP0203_GD_PROVIDER_SWITCH_OPEN          (0x00U)
#define TCPP0203_GD_PROVIDER_SWITCH_CLOSED        (TCPP0203_GD_PROVIDER_SWITCH_MSK)

/**
  * @brief  TCPP0203 Gate Driver Consumer values
  */
#define TCPP0203_GD_CONSUMER_SWITCH_POS           (3U)
#define TCPP0203_GD_CONSUMER_SWITCH_MSK           (0x01U << TCPP0203_GD_CONSUMER_SWITCH_POS)
#define TCPP0203_GD_CONSUMER_SWITCH_CLOSED        (0x00U)
#define TCPP0203_GD_CONSUMER_SWITCH_OPEN          (TCPP0203_GD_CONSUMER_SWITCH_MSK)

/**
  * @brief  TCPP0203 Power Mode values
  */
#define TCPP0203_POWER_MODE_POS                   (4U)
#define TCPP0203_POWER_MODE_MSK                   (0x03U << TCPP0203_POWER_MODE_POS)
#define TCPP0203_POWER_MODE_HIBERNATE             (0x00U)
#define TCPP0203_POWER_MODE_LOWPOWER              (0x02U << TCPP0203_POWER_MODE_POS)
#define TCPP0203_POWER_MODE_NORMAL                (0x01U << TCPP0203_POWER_MODE_POS)

/**
  * @brief  TCPP0203 VBUS Discharge management
  */
#define TCPP0203_VBUS_DISCHARGE_POS               (6U)
#define TCPP0203_VBUS_DISCHARGE_MSK               (0x01U << TCPP0203_VBUS_DISCHARGE_POS)
#define TCPP0203_VBUS_DISCHARGE_OFF               (0x00U)
#define TCPP0203_VBUS_DISCHARGE_ON                (TCPP0203_VBUS_DISCHARGE_MSK)

/**
  * @brief  TCPP0203 VConn Discharge management
  */
#define TCPP0203_VCONN_DISCHARGE_POS              (7U)
#define TCPP0203_VCONN_DISCHARGE_MSK              (0x01U << TCPP0203_VCONN_DISCHARGE_POS)
#define TCPP0203_VCONN_DISCHARGE_OFF              (0x00U)
#define TCPP0203_VCONN_DISCHARGE_ON               (TCPP0203_VCONN_DISCHARGE_MSK)

/**
  * @brief  TCPP0203 VCONN Switch Acknowledge
  */
#define TCPP0203_VCONN_SWITCH_ACK_POS             (0U)
#define TCPP0203_VCONN_SWITCH_ACK_MSK             (0x03U << TCPP0203_VCONN_SWITCH_ACK_POS)
#define TCPP0203_VCONN_SWITCH_ACK_OPEN            (0x00U)
#define TCPP0203_VCONN_SWITCH_ACK_CC1             (0x02U << TCPP0203_VCONN_SWITCH_ACK_POS)
#define TCPP0203_VCONN_SWITCH_ACK_CC2             (0x01U << TCPP0203_VCONN_SWITCH_ACK_POS)

/**
  * @brief  TCPP0203 Gate Driver Provider Acknowledge
  */
#define TCPP0203_GD_PROVIDER_SWITCH_ACK_POS       (2U)
#define TCPP0203_GD_PROVIDER_SWITCH_ACK_MSK       (0x01U << TCPP0203_GD_PROVIDER_SWITCH_ACK_POS)
#define TCPP0203_GD_PROVIDER_SWITCH_ACK_OPEN      (0x00U)
#define TCPP0203_GD_PROVIDER_SWITCH_ACK_CLOSED    (TCPP0203_GD_PROVIDER_SWITCH_ACK_MSK)

/**
  * @brief  TCPP0203 Gate Driver Consumer Acknowledge
  */
#define TCPP0203_GD_CONSUMER_SWITCH_ACK_POS       (3U)
#define TCPP0203_GD_CONSUMER_SWITCH_ACK_MSK       (0x01U << TCPP0203_GD_CONSUMER_SWITCH_ACK_POS)
#define TCPP0203_GD_CONSUMER_SWITCH_ACK_CLOSED    (0x00U)
#define TCPP0203_GD_CONSUMER_SWITCH_ACK_OPEN      (TCPP0203_GD_CONSUMER_SWITCH_ACK_MSK)

/**
  * @brief  TCPP0203 Power Mode Acknowledge
  */
#define TCPP0203_POWER_MODE_ACK_POS               (4U)
#define TCPP0203_POWER_MODE_ACK_MSK               (0x03U << TCPP0203_POWER_MODE_ACK_POS)
#define TCPP0203_POWER_MODE_ACK_HIBERNATE         (0x00U)
#define TCPP0203_POWER_MODE_ACK_LOWPOWER          (0x01U << TCPP0203_POWER_MODE_ACK_POS)
#define TCPP0203_POWER_MODE_ACK_NORMAL            (0x02U << TCPP0203_POWER_MODE_ACK_POS)

/**
  * @brief  TCPP0203 VBUS Discharge Acknowledge
  */
#define TCPP0203_VBUS_DISCHARGE_ACK_POS           (6U)
#define TCPP0203_VBUS_DISCHARGE_ACK_MSK           (0x01U << TCPP0203_VBUS_DISCHARGE_ACK_POS)
#define TCPP0203_VBUS_DISCHARGE_ACK_OFF           (0x00U)
#define TCPP0203_VBUS_DISCHARGE_ACK_ON            (TCPP0203_VBUS_DISCHARGE_ACK_MSK)

/**
  * @brief  TCPP0203 VConn Discharge Acknowledge
  */
#define TCPP0203_VCONN_DISCHARGE_ACK_POS          (7U)
#define TCPP0203_VCONN_DISCHARGE_ACK_MSK          (0x01U << TCPP0203_VCONN_DISCHARGE_ACK_POS)
#define TCPP0203_VCONN_DISCHARGE_ACK_OFF          (0x00U)
#define TCPP0203_VCONN_DISCHARGE_ACK_ON           (TCPP0203_VCONN_DISCHARGE_ACK_MSK)

/**
  * @brief  TCPP0203 OCP Vconn Flag management
  */
#define TCPP0203_FLAG_OCP_VCONN_POS               (0U)
#define TCPP0203_FLAG_OCP_VCONN_MSK               (0x01U << TCPP0203_FLAG_OCP_VCONN_POS)
#define TCPP0203_FLAG_OCP_VCONN_SET               (TCPP0203_FLAG_OCP_VCONN_MSK)
#define TCPP0203_FLAG_OCP_VCONN_RESET             (0x00U)

/**
  * @brief  TCPP0203 OCP VBUS Flag management
  */
#define TCPP0203_FLAG_OCP_VBUS_POS                (1U)
#define TCPP0203_FLAG_OCP_VBUS_MSK                (0x01U << TCPP0203_FLAG_OCP_VBUS_POS)
#define TCPP0203_FLAG_OCP_VBUS_SET                (TCPP0203_FLAG_OCP_VBUS_MSK)
#define TCPP0203_FLAG_OCP_VBUS_RESET              (0x00U)

/**
  * @brief  TCPP0203 OVP VBUS Flag management
  */
#define TCPP0203_FLAG_OVP_VBUS_POS                (2U)
#define TCPP0203_FLAG_OVP_VBUS_MSK                (0x01U << TCPP0203_FLAG_OVP_VBUS_POS)
#define TCPP0203_FLAG_OVP_VBUS_SET                (TCPP0203_FLAG_OVP_VBUS_MSK)
#define TCPP0203_FLAG_OVP_VBUS_RESET              (0x00U)

/**
  * @brief  TCPP0203 OVP CC Flag management
  */
#define TCPP0203_FLAG_OVP_CC_POS                  (3U)
#define TCPP0203_FLAG_OVP_CC_MSK                  (0x01U << TCPP0203_FLAG_OVP_CC_POS)
#define TCPP0203_FLAG_OVP_CC_SET                  (TCPP0203_FLAG_OVP_CC_MSK)
#define TCPP0203_FLAG_OVP_CC_RESET                (0x00U)

/**
  * @brief  TCPP0203 OTP Flag management
  */
#define TCPP0203_FLAG_OTP_POS                     (4U)
#define TCPP0203_FLAG_OTP_MSK                     (0x01U << TCPP0203_FLAG_OTP_POS)
#define TCPP0203_FLAG_OTP_SET                     (TCPP0203_FLAG_OTP_MSK)
#define TCPP0203_FLAG_OTP_RESET                   (0x00U)

/**
  * @brief  TCPP0203 VBUS OK Flag management
  */
#define TCPP0203_FLAG_VBUS_OK_POS                 (5U)
#define TCPP0203_FLAG_VBUS_OK_MSK                 (0x01U << TCPP0203_FLAG_VBUS_OK_POS)
#define TCPP0203_FLAG_VBUS_OK_SET                 (TCPP0203_FLAG_VBUS_OK_MSK)
#define TCPP0203_FLAG_VBUS_OK_RESET               (0x00U)

/**
  * @brief  TCPP0203 VConn Power
  */
#define TCPP0203_FLAG_VCONN_PWR_POS               (6U)
#define TCPP0203_FLAG_VCONN_PWR_MSK               (0x01U << TCPP0203_FLAG_VCONN_PWR_POS)
#define TCPP0203_FLAG_VCONN_PWR_1W                (TCPP0203_FLAG_VCONN_PWR_MSK)
#define TCPP0203_FLAG_VCONN_PWR_0_1W              (0x00U)

/**
  * @brief  TCPP0203 Device Type
  */
#define TCPP0203_DEVICE_TYPE_POS                  (7U)
#define TCPP0203_DEVICE_TYPE_MSK                  (0x01U << TCPP0203_DEVICE_TYPE_POS)
#define TCPP0203_DEVICE_TYPE_02                   (TCPP0203_DEVICE_TYPE_MSK)
#define TCPP0203_DEVICE_TYPE_03                   (0x00U)

/**
  * @}
  */

/** @defgroup TCPP0203_Exported_Macros TCPP0203 Exported Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup TCPP0203_Exported_Functions TCPP0203 Exported Functions
  * @{
  */

/*------------------------------------------------------------------------------
                TCPP02/03 Type-C port protection functions
------------------------------------------------------------------------------*/
/* High Layer codec functions */
int32_t TCPP0203_RegisterBusIO(TCPP0203_Object_t *pObj, TCPP0203_IO_t *pIO);
int32_t TCPP0203_Init(TCPP0203_Object_t *pObj);
int32_t TCPP0203_DeInit(TCPP0203_Object_t *pObj);
int32_t TCPP0203_Reset(TCPP0203_Object_t *pObj);
int32_t TCPP0203_SetVConnSwitch(TCPP0203_Object_t *pObj, uint8_t VConnSwitch);
int32_t TCPP0203_SetGateDriverProvider(TCPP0203_Object_t *pObj, uint8_t GateDriverProvider);
int32_t TCPP0203_SetGateDriverConsumer(TCPP0203_Object_t *pObj, uint8_t GateDriverConsumer);
int32_t TCPP0203_SetPowerMode(TCPP0203_Object_t *pObj, uint8_t PowerMode);
int32_t TCPP0203_SetVBusDischarge(TCPP0203_Object_t *pObj, uint8_t VBusDischarge);
int32_t TCPP0203_SetVConnDischarge(TCPP0203_Object_t *pObj, uint8_t VConnDischarge);
int32_t TCPP0203_GetVConnSwitchAck(TCPP0203_Object_t *pObj, uint8_t *pVConnSwitchAck);
int32_t TCPP0203_GetGateDriverProviderAck(TCPP0203_Object_t *pObj, uint8_t *pGateDriverProviderAck);
int32_t TCPP0203_GetGateDriverConsumerAck(TCPP0203_Object_t *pObj, uint8_t *pGateDriverConsumerAck);
int32_t TCPP0203_GetPowerModeAck(TCPP0203_Object_t *pObj, uint8_t *pPowerModeAck);
int32_t TCPP0203_GetVBusDischargeAck(TCPP0203_Object_t *pObj, uint8_t *pVBusDischargeAck);
int32_t TCPP0203_GetVConnDischargeAck(TCPP0203_Object_t *pObj, uint8_t *pVConnDischargeAck);
int32_t TCPP0203_GetOCPVConnFlag(TCPP0203_Object_t *pObj, uint8_t *pOCPVConnFlag);
int32_t TCPP0203_GetOCPVBusFlag(TCPP0203_Object_t *pObj, uint8_t *pGetOCPVBusFlag);
int32_t TCPP0203_GetOVPVBusFlag(TCPP0203_Object_t *pObj, uint8_t *pOVPVBusFlag);
int32_t TCPP0203_GetOVPCCFlag(TCPP0203_Object_t *pObj, uint8_t *pOVPCCFlag);
int32_t TCPP0203_GetOTPFlag(TCPP0203_Object_t *pObj, uint8_t *pOTPFlag);
int32_t TCPP0203_GetVBusOkFlag(TCPP0203_Object_t *pObj, uint8_t *pVBusOkFlag);
int32_t TCPP0203_ReadTCPPType(TCPP0203_Object_t *pObj, uint8_t *pTCPPType);
int32_t TCPP0203_ReadVCONNPower(TCPP0203_Object_t *pObj, uint8_t *pVCONNPower);
int32_t TCPP0203_WriteCtrlRegister(TCPP0203_Object_t *pObj, uint8_t *pCtrlRegister);
int32_t TCPP0203_ReadAckRegister(TCPP0203_Object_t *pObj, uint8_t *pAckRegister);
int32_t TCPP0203_ReadFlagRegister(TCPP0203_Object_t *pObj, uint8_t *pFlagRegister);

/**
  * @}
  */

/* TCPP02/03 Type-C port protection driver structure */
extern TCPP0203_Drv_t TCPP0203_Driver;

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#endif /* TCPP0203_H */


