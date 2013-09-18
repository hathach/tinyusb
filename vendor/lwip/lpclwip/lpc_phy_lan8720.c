/**********************************************************************
* $Id$		lpc_phy_lan8720.c			2011-11-20
*//**
* @file		lpc_phy_lan8720.c
* @brief	LAN8720 PHY status and control.
* @version	1.0
* @date		20 Nov. 2011
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

#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/tcpip.h"
#include "lwip/snmp.h"

#include "boards/board.h"
#include "arch/lpc_arch.h" // FIXME Ethernet
#include "lpc_phy.h"

/** @defgroup lan8720_phy	PHY status and control for the LAN8720.
 * @ingroup lwip_phy
 *
 * Various functions for controlling and monitoring the status of the
 * LAN8720 PHY. In polled (standalone) systems, the PHY state must be
 * monitored as part of the application. In a threaded (RTOS) system,
 * the PHY state is monitored by the PHY handler thread. The MAC
 * driver will not transmit unless the PHY link is active.
 * @{
 */

/** \brief  LAN8720 PHY register offsets */
#define LAN8_BCR_REG        0x0  /**< Basic Control Register */
#define LAN8_BSR_REG        0x1  /**< Basic Status Reg */
#define LAN8_PHYID1_REG     0x2  /**< PHY ID 1 Reg  */
#define LAN8_PHYID2_REG     0x3  /**< PHY ID 2 Reg */
#define LAN8_PHYSPLCTL_REG  0x1F /**< PHY special control/status Reg */

/* LAN8720 BCR register definitions */
#define LAN8_RESET          (1 << 15)  /**< 1= S/W Reset */
#define LAN8_LOOPBACK       (1 << 14)  /**< 1=loopback Enabled */
#define LAN8_SPEED_SELECT   (1 << 13)  /**< 1=Select 100MBps */
#define LAN8_AUTONEG        (1 << 12)  /**< 1=Enable auto-negotiation */
#define LAN8_POWER_DOWN     (1 << 11)  /**< 1=Power down PHY */
#define LAN8_ISOLATE        (1 << 10)  /**< 1=Isolate PHY */
#define LAN8_RESTART_AUTONEG (1 << 9)  /**< 1=Restart auto-negoatiation */
#define LAN8_DUPLEX_MODE    (1 << 8)   /**< 1=Full duplex mode */

/* LAN8720 BSR register definitions */
#define LAN8_100BASE_T4     (1 << 15)  /**< T4 mode */
#define LAN8_100BASE_TX_FD  (1 << 14)  /**< 100MBps full duplex */
#define LAN8_100BASE_TX_HD  (1 << 13)  /**< 100MBps half duplex */
#define LAN8_10BASE_T_FD    (1 << 12)  /**< 100Bps full duplex */
#define LAN8_10BASE_T_HD    (1 << 11)  /**< 10MBps half duplex */
#define LAN8_AUTONEG_COMP   (1 << 5)   /**< Auto-negotation complete */
#define LAN8_RMT_FAULT      (1 << 4)   /**< Fault */
#define LAN8_AUTONEG_ABILITY (1 << 3)  /**< Auto-negotation supported */
#define LAN8_LINK_STATUS    (1 << 2)   /**< 1=Link active */
#define LAN8_JABBER_DETECT  (1 << 1)   /**< Jabber detect */
#define LAN8_EXTEND_CAPAB   (1 << 0)   /**< Supports extended capabilities */

/* LAN8720 PHYSPLCTL status definitions */
#define LAN8_SPEEDMASK      (7 << 2)   /**< Speed and duplex mask */
#define LAN8_SPEED100F      (6 << 2)   /**< 100BT full duplex */
#define LAN8_SPEED10F       (5 << 2)   /**< 10BT full duplex */
#define LAN8_SPEED100H      (2 << 2)   /**< 100BT half duplex */
#define LAN8_SPEED10H       (1 << 2)   /**< 10BT half duplex */

/* LAN8720 PHY ID 1/2 register definitions */
#define LAN8_PHYID1_OUI     0x0007     /**< Expected PHY ID1 */
#define LAN8_PHYID2_OUI     0xC0F0     /**< Expected PHY ID2, except last 4 bits */

/**
 * @brief PHY status structure used to indicate current status of PHY.
 */
typedef struct {
	u32_t     phy_speed_100mbs:2; /**< 10/100 MBS connection speed flag. */
	u32_t     phy_full_duplex:2;  /**< Half/full duplex connection speed flag. */
	u32_t     phy_link_active:2;  /**< Phy link active flag. */
} PHY_STATUS_TYPE;

/** \brief  PHY update flags */
static PHY_STATUS_TYPE physts;

/** \brief  Last PHY update flags, used for determing if something has changed */
static PHY_STATUS_TYPE olddphysts;

/** \brief  PHY update counter for state machine */
static s32_t phyustate;

/** \brief  Update PHY status from passed value
 *
 *  This function updates the current PHY status based on the
 *  passed PHY status word. The PHY status indicate if the link
 *  is active, the connection speed, and duplex.
 *
 *  \param[in]    netif   NETIF structure
 *  \param[in]    linksts Status word with link state
 *  \param[in]    sdsts   Status word with speed and duplex states
 *  \return        1 if the status has changed, otherwise 0
 */
static s32_t lpc_update_phy_sts(struct netif *netif, u32_t linksts, u32_t sdsts)
{
	s32_t changed = 0;

	/* Update link active status */
	if (linksts & LAN8_LINK_STATUS)
		physts.phy_link_active = 1;
	else
		physts.phy_link_active = 0;

	switch (sdsts & LAN8_SPEEDMASK) {
		case LAN8_SPEED100F:
		default:
			physts.phy_speed_100mbs = 1;
			physts.phy_full_duplex = 1;
			break;

		case LAN8_SPEED10F:
			physts.phy_speed_100mbs = 0;
			physts.phy_full_duplex = 1;
			break;

		case LAN8_SPEED100H:
			physts.phy_speed_100mbs = 1;
			physts.phy_full_duplex = 0;
			break;

		case LAN8_SPEED10H:
			physts.phy_speed_100mbs = 0;
			physts.phy_full_duplex = 0;
			break;
	}

	if (physts.phy_speed_100mbs != olddphysts.phy_speed_100mbs) {
		changed = 1;
		if (physts.phy_speed_100mbs) {
			/* 100MBit mode. */
			lpc_emac_set_speed(1);

			NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100000000);
		}
		else {
			/* 10MBit mode. */
			lpc_emac_set_speed(0);

			NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 10000000);
		}

		olddphysts.phy_speed_100mbs = physts.phy_speed_100mbs;
	}

	if (physts.phy_full_duplex != olddphysts.phy_full_duplex) {
		changed = 1;
		if (physts.phy_full_duplex)
			lpc_emac_set_duplex(1);
		else
			lpc_emac_set_duplex(0);

		olddphysts.phy_full_duplex = physts.phy_full_duplex;
	}

	if (physts.phy_link_active != olddphysts.phy_link_active) {
		changed = 1;
#if NO_SYS == 1
		if (physts.phy_link_active)
            netif_set_link_up(netif);
		else
			netif_set_link_down(netif);
#else
		if (physts.phy_link_active)
            tcpip_callback_with_block((tcpip_callback_fn) netif_set_link_up,
                (void*) netif, 1);
         else
            tcpip_callback_with_block((tcpip_callback_fn) netif_set_link_down,
                (void*) netif, 1);
#endif

		olddphysts.phy_link_active = physts.phy_link_active;
	}

	return changed;
}

/** \brief  Initialize the LAN8720 PHY.
 *
 *  This function initializes the LAN8720 PHY. It will block until
 *  complete. This function is called as part of the EMAC driver
 *  initialization. Configuration of the PHY at startup is
 *  controlled by setting up configuration defines in
 *  lpc_emac_config.h.
 *
 *  \param[in]     netif   NETIF structure
 *  \param[in]     rmii    If set, configures the PHY for RMII mode
 *  \return        ERR_OK if the setup was successful, otherwise ERR_TIMEOUT
 */
err_t lpc_phy_init(struct netif *netif, int rmii)
{
	u32_t tmp, tmp1;
	s32_t i;

	physts.phy_speed_100mbs = olddphysts.phy_speed_100mbs = 2;
	physts.phy_full_duplex = olddphysts.phy_full_duplex = 2;
	physts.phy_link_active = olddphysts.phy_link_active = 2;
	phyustate = 0;

	/* Only first read and write are checked for failure */
	/* Put the LAN8720 in reset mode and wait for completion */
	if (lpc_mii_write(LAN8_BCR_REG, LAN8_RESET) != 0)
		return ERR_TIMEOUT;
	i = 400;
	while (i > 0) {
		msDelay(1);   /* 1 ms */
		if (lpc_mii_read(LAN8_BCR_REG, &tmp) != 0)
			return ERR_TIMEOUT;

		if (!(tmp & (LAN8_RESET | LAN8_POWER_DOWN)))
			i = -1;
		else
			i--;
	}
	/* Timeout? */
	if (i == 0)
		return ERR_TIMEOUT;

	/* Setup link based on configuration options */
#if PHY_USE_AUTONEG==1
	tmp = LAN8_AUTONEG;
#else
	tmp = 0;
#endif
#if PHY_USE_100MBS==1
	tmp |= LAN8_SPEED_SELECT;
#endif
#if PHY_USE_FULL_DUPLEX==1
	tmp |= LAN8_DUPLEX_MODE;
#endif
	lpc_mii_write(LAN8_BCR_REG, tmp);

	/* The link is not set active at this point, but will be detected
       later */

	return ERR_OK;
}

/* Phy status update state machine */
s32_t lpc_phy_sts_sm(struct netif *netif)
{
	static u32_t sts;
	s32_t changed = 0;

	switch (phyustate) {
		default:
		case 0:
			/* Read BMSR to clear faults */
			lpc_mii_read_noblock(LAN8_BSR_REG);
			phyustate = 1;
			break;

		case 1:
			/* Wait for read status state */
			if (!lpc_mii_is_busy()) {
				/* Get PHY status with link state */
				sts = lpc_mii_read_data();
				lpc_mii_read_noblock(LAN8_PHYSPLCTL_REG);
				phyustate = 2;
			}
			break;

		case 2:
			/* Wait for read status state */
			if (!lpc_mii_is_busy()) {
				/* Update PHY status */
				changed = lpc_update_phy_sts(netif, sts, lpc_mii_read_data());
				phyustate = 0;
			}
			break;
	}

	return changed;
}

/**
 * @}
 */

/* --------------------------------- End Of File ------------------------------ */
