/**
  ******************************************************************************
  * @file    tcpp0203_reg.c
  * @author  MCD Application Team
  * @brief   This file provides unitary register function to control the TCPP02-03
  *          Type-C port protection driver.
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

/* Includes ------------------------------------------------------------------*/
#include "tcpp0203_reg.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup Components
  * @{
  */

/** @addtogroup TCPP0203
  * @brief     This file provides a set of functions needed to drive the
  *            TCPP02/03 Type-C port protection codec.
  * @{
  */

/************** Generic Function  *******************/
/*******************************************************************************
  * Function Name : tcpp0203_read_reg
  * Description   : Generic Reading function. It must be fulfilled with either
  *                 I2C or SPI reading functions
  * Input         : Register Address, length of buffer
  * Output        : data Read
  *******************************************************************************/
int32_t tcpp0203_read_reg(const TCPP0203_ctx_t *ctx, uint8_t reg, uint8_t *data, uint8_t length)
{
  return ctx->ReadReg(ctx->handle, reg, data, length);
}

/*******************************************************************************
  * Function Name : tcpp0203_write_reg
  * Description   : Generic Writing function. It must be fulfilled with either
  *                 I2C or SPI writing function
  * Input         : Register Address, data to be written, length of buffer
  * Output        : None
  *******************************************************************************/
int32_t tcpp0203_write_reg(const TCPP0203_ctx_t *ctx, uint8_t reg, uint8_t *data, uint8_t length)
{
  return ctx->WriteReg(ctx->handle, reg, data, length);
}

/******************************************************************************/
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */


