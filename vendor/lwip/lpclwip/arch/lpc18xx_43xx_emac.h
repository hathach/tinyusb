/*
 * @brief LPC18xx/43xx LWIP EMAC driver
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

#ifndef __LPC18XX_43XX_EMAC_H_
#define __LPC18XX_43XX_EMAC_H_

#include "lwip/opt.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @defgroup NET_LWIP_LPC18XX43XX_EMAC_DRIVER 18xx/43xx EMAC driver for LWIP
 * @ingroup NET_LWIP
 * This is the LPC18xx/43xx EMAC driver for LWIP. This driver supports both
 * RTOS-based and no-RTOS operation with LWIP. WHen using an RTOS, several
 * threads will be created for handling RX and TX packet fucntions.
 *
 * Note that some LWIP examples may not necessarily use all the provided
 * LWIP driver functions or may contain overriden versions of the functions.
 * (For example, PHY drives may have their own implementation of the MII
 * read/write functions).
 * @{
 */

/**
 * @brief	Attempt to read a packet from the EMAC interface
 * @param	netif	: lwip network interface structure pointer
 * @return	Nothing
 */
void lpc_enetif_input(struct netif *netif);

/**
 * @brief	Attempt to allocate and requeue a new pbuf for RX
 * @param	netif	: lwip network interface structure pointer
 * @return	The number of new descriptors queued
 */
s32_t lpc_rx_queue(struct netif *netif);

/**
 * @brief	Polls if an available TX descriptor is ready
 * @param	netif	: lwip network interface structure pointer
 * @return	0 if no descriptors are read, or >0
 * @note	Can be used to determine if the low level transmit function will block
 */
s32_t lpc_tx_ready(struct netif *netif);

/**
 * @brief	Call for freeing TX buffers that are complete
 * @param	netif	: lwip network interface structure pointer
 * @return	Nothing
 */
void lpc_tx_reclaim(struct netif *netif);

/**
 * @brief	LWIP 18xx/43xx EMAC initialization function
 * @param	netif	: lwip network interface structure pointer
 * @return	ERR_OK if the loopif is initialized, or ERR_* on other errors
 * @note	Should be called at the beginning of the program to set up the
 * network interface. This function should be passed as a parameter to
 * netif_add().
 */
err_t lpc_enetif_init(struct netif *netif);

/**
 * @brief	Set up the MAC interface duplex
 * @param	full_duplex	: 0 = half duplex, 1 = full duplex
 * @return	Nothing
 * @note	This function provides a method for the PHY to setup the EMAC
 * for the PHY negotiated duplex mode.
 */
void lpc_emac_set_duplex(int full_duplex);

/**
 * @brief	Set up the MAC interface speed
 * @param	mbs_100	: 0 = 10mbs mode, 1 = 100mbs mode
 * @return	Nothing
 * @note	This function provides a method for the PHY to setup the EMAC
 * for the PHY negotiated bit rate.
 */
void lpc_emac_set_speed(int mbs_100);

/**
 * @brief	Millisecond Delay function
 * @param	ms		: Milliseconds to wait
 * @return	None
 */
extern void msDelay(uint32_t ms);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __LPC18XX_43XX_EMAC_H_ */
