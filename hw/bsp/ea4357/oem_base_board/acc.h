/*****************************************************************************
 *
 *   Copyright(C) 2011, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * Embedded Artists AB assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. Embedded Artists AB
 * reserves the right to make changes in the software without
 * notification. Embedded Artists AB also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 *****************************************************************************/
#ifndef __ACC_H
#define __ACC_H


typedef enum
{
    ACC_MODE_STANDBY,
    ACC_MODE_MEASURE,
    ACC_MODE_LEVEL, /* level detection */
    ACC_MODE_PULSE, /* pulse detection */
} acc_mode_t;

typedef enum
{
    ACC_RANGE_8G,
    ACC_RANGE_2G,
    ACC_RANGE_4G,
} acc_range_t;


void acc_init (void);

void acc_read (int8_t *x, int8_t *y, int8_t *z);
void acc_setRange(acc_range_t range);
void acc_setMode(acc_mode_t mode);



#endif /* end __LIGHT_H */
/****************************************************************************
**                            End Of File
*****************************************************************************/
