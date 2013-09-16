/**********************************************************************
* $Id$		lpc18xx_43xx_emac.h			2011-11-20
*//**
* @file		lpc18xx_43xx_emac.h
* @brief	LPC18xx/43xx ethernet driver header file for LWIP
* @version	1.0
* @date		20. Nov. 2011
* @author	NXP MCU SW Application Team
* 
* Copyright(C) 2011, NXP Semiconductor
* All rights reserved.
*
***********************************************************************
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
**********************************************************************/

#ifndef __LPC18XX_43XX_EMAC_H
#define __LPC18XX_43XX_EMAC_H

#include "lwip/opt.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* These functions are only visible when not using an RTOS */ 
#if NO_SYS == 1
void lpc_enetif_input(struct netif *netif);
s32_t lpc_tx_ready(struct netif *netif);
s32_t lpc_rx_queue(struct netif *netif);
void lpc_tx_reclaim(struct netif *netif);
#endif

err_t lpc_enetif_init(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* __LPC18XX_43XX_EMAC_H */
