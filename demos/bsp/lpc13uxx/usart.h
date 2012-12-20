/****************************************************************************
 *   $Id:: usart.h 6172 2011-01-13 18:22:51Z usb00423                       $
 *   Project: NXP LPC13Uxx software example
 *
 *   Description:
 *     This file contains definition and prototype for UART configuration.
 *
 ****************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
****************************************************************************/
#ifndef __USART_H 
#define __USART_H

/* Synchronous mode control register definition. */
#define SYNC_ON             (0x1<<0)
#define SYNC_OFF            (0x0<<0)

#define SYNC_MASTER         (0x1<<1)
#define SYNC_SLAVE          (0x0<<1)

#define SYNC_RE             (0x0<<2)
#define SYNC_FE             (0x1<<2)

#define SYNC_CONT_CLK_EN    (0x1<<4)
#define SYNC_CONT_CLK_DIS   (0x0<<4)

#define SYNC_STARTSTOPOFF   (0x1<<5)
#define SYNC_STARTSTOPON    (0x0<<5)

#define SYNC_CON_CLK_CLR    (0x1<<6)

#endif /* end __USART_H */
/*****************************************************************************
**                            End Of File
******************************************************************************/
