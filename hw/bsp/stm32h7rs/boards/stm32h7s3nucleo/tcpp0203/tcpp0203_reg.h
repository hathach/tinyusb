/**
  ******************************************************************************
  * @file    tcpp0203_reg.h
  * @author  MCD Application Team
  * @brief   Header of tcpp0203_reg.c
  *
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
#ifndef TCPP0203_REG_H
#define TCPP0203_REG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/** @addtogroup BSP
  * @{
  */

/** @addtogroup Component
  * @{
  */

/** @addtogroup TCPP0203
  * @{
  */


/** @defgroup TCPP0203_Exported_Constants TCPP0203 Exported Constants
  * @{
  */
/******************************************************************************/
/****************************** REGISTER MAPPING ******************************/
/******************************************************************************/
#define TCPP0203_WRITE_REG                   0x00U
#define TCPP0203_PROG_CTRL                   TCPP0203_WRITE_REG
#define TCPP0203_READ_REG1                   0x01U
#define TCPP0203_ACK_REG                     TCPP0203_READ_REG1
#define TCPP0203_READ_REG2                   0x02U
#define TCPP0203_FLAG_REG                    TCPP0203_READ_REG2

/**
  * @}
  */

/************** Generic Function  *******************/

typedef int32_t (*TCPP0203_Write_Func)(const void *, uint8_t, uint8_t *, uint8_t);
typedef int32_t (*TCPP0203_Read_Func)(const void *, uint8_t, uint8_t *, uint8_t);

typedef struct
{
  TCPP0203_Write_Func   WriteReg;
  TCPP0203_Read_Func    ReadReg;
  void                *handle;
} TCPP0203_ctx_t;

/*******************************************************************************
  * Register      : Generic - All
  * Address       : Generic - All
  * Bit Group Name: None
  * Permission    : W
  *******************************************************************************/
int32_t tcpp0203_write_reg(const TCPP0203_ctx_t *ctx, uint8_t reg, uint8_t *data, uint8_t length);
int32_t tcpp0203_read_reg(const TCPP0203_ctx_t *ctx, uint8_t reg, uint8_t *data, uint8_t length);

#ifdef __cplusplus
}
#endif

#endif /* TCPP0203_REG_H */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */


