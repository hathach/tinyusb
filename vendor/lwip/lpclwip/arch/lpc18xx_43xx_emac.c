/**********************************************************************
* $Id$		lpc18xx_43xx_emac.c			2011-11-20
*//**
* @file		lpc18xx_43xx_emac.c
* @brief	LPC18xx/43xx ethernet driver for LWIP
* @version	1.0
* @date		20. Nov. 2011
* @author	NXP MCU SW Application Team
* 
* Copyright(C) 2012, NXP Semiconductor
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
#include "lwip/sys.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "netif/etharp.h"
#include "netif/ppp_oe.h"

#include "boards/board.h"
#include "lpc18xx_43xx_mac_regs.h"
#include "lpc18xx_43xx_emac.h"
#include "lpc_phy.h"

// FIXME - still to do
// Checksum offloading for packets using 43xx hardware

#ifndef LPC_EMAC_RMII
#error LPC_EMAC_RMII is not defined!
#endif

#if LPC_NUM_BUFF_TXDESCS < 2
#error LPC_NUM_BUFF_TXDESCS must be at least 2
#endif

#if LPC_NUM_BUFF_RXDESCS < 3
#error LPC_NUM_BUFF_RXDESCS must be at least 3
#endif

#ifndef LPC_CHECK_SLOWMEM
#error LPC_CHECK_SLOWMEM must be 0 or 1
#endif

/** @defgroup lwip18xx_43xx_emac_DRIVER	lpc18xx/43xx EMAC driver for LWIP
 * @ingroup lwip_emac
 *
 * @{
 */

#if NO_SYS == 0
/** \brief  Driver transmit and receive thread priorities
 * 
 * Thread priorities for receive thread and TX cleanup thread. Alter
 * to prioritize receive or transmit bandwidth. In a heavily loaded
 * system or with LWIP_DEBUG enabled, the priorities might be better
 * the same. */
#define tskTXCLEAN_PRIORITY  (TCPIP_THREAD_PRIO - 1)
#define tskRECPKT_PRIORITY   (TCPIP_THREAD_PRIO - 1)
#endif

/** \brief  Debug output formatter lock define
 * 
 * When using FreeRTOS and with LWIP_DEBUG enabled, enabling this
 * define will allow RX debug messages to not interleave with the
 * TX messages (so they are actually readable). Not enabling this
 * define when the system is under load will cause the output to
 * be unreadable. There is a small tradeoff in performance for this
 * so use it only for debug. */
//#define LOCK_RX_THREAD

/* LPC EMAC driver data structure */
struct lpc_enetdata {
	struct netif *netif;        /**< Reference back to LWIP parent netif */
	TRAN_DESC_ENH_T ptdesc[LPC_NUM_BUFF_TXDESCS]; /**< TX descriptor list */
	REC_DESC_ENH_T prdesc[LPC_NUM_BUFF_RXDESCS]; /**< RX descriptor list */
	struct pbuf *txpbufs[LPC_NUM_BUFF_TXDESCS]; /**< Saved pbuf pointers, for free after TX */
	volatile u32_t tx_free_descs; /**< Number of free TX descriptors */
	u32_t tx_fill_idx;  /**< Current free TX descriptor index */
	u32_t tx_reclaim_idx; /**< Next incoming TX packet descriptor index */
	struct pbuf *rxpbufs[LPC_NUM_BUFF_RXDESCS]; /**< Saved pbuf pointers for RX */
	volatile u32_t rx_free_descs; /**< Number of free RX descriptors */
	volatile u32_t rx_get_idx; /**< Index to next RX descriptor that id to be received */
	u32_t rx_next_idx; /**< Index to next RX descriptor that needs a pbuf */
#if NO_SYS == 0
	sys_sem_t RxSem; /**< RX receive thread wakeup semaphore */
	sys_sem_t TxCleanSem; /**< TX cleanup thread wakeup semaphore */
	sys_mutex_t TXLockMutex; /**< TX critical section mutex */
	xSemaphoreHandle xTXDCountSem; /**< TX free buffer counting semaphore */
#endif
};

/** \brief  LPC EMAC driver work data
 */
static struct lpc_enetdata lpc_enetdata;

#if LPC_CHECK_SLOWMEM == 1
struct lpc_slowmem_array_t {
	u32_t start;
	u32_t end;
};
const struct lpc_slowmem_array_t slmem[] = LPC_SLOWMEM_ARRAY;
#endif

/* Write a value via the MII link (non-blocking) */
void lpc_mii_write_noblock(u32_t PhyReg, u32_t Value)
{
	/* Write value at PHY address and register */
	LPC_ETHERNET->MAC_MII_ADDR = MAC_MIIA_PA(LPC_PHYDEF_PHYADDR) |
		MAC_MIIA_GR(PhyReg) | MAC_MIIA_CR(4) | MAC_MIIA_W;
	LPC_ETHERNET->MAC_MII_DATA = Value;
	LPC_ETHERNET->MAC_MII_ADDR |= MAC_MIIA_GB;
}

/* Write a value via the MII link (blocking) */
err_t lpc_mii_write(u32_t PhyReg, u32_t Value)
{
	u32_t mst = 250;
	err_t sts = ERR_OK;

	/* Write value at PHY address and register */
	lpc_mii_write_noblock(PhyReg, Value);

	/* Wait for unbusy status */
	while (mst > 0) {
		sts = LPC_ETHERNET->MAC_MII_ADDR & MAC_MIIA_GB;
		if (sts == 0)
			mst = 0;
		else {
			mst--;
			msDelay(1);
		}
	}

	if (sts != 0)
		sts = ERR_TIMEOUT;

	return sts;
}

/* Reads current MII link busy status */
u32_t lpc_mii_is_busy(void)
{
	return (LPC_ETHERNET->MAC_MII_ADDR & MAC_MIIA_GB);
}

/* Starts a read operation via the MII link (non-blocking) */
u32_t lpc_mii_read_data(void)
{
	return LPC_ETHERNET->MAC_MII_DATA;
}

/* Starts a read operation via the MII link (non-blocking) */
void lpc_mii_read_noblock(u32_t PhyReg) 
{
	/* Read value at PHY address and register */
	LPC_ETHERNET->MAC_MII_ADDR = MAC_MIIA_PA(LPC_PHYDEF_PHYADDR) |
		MAC_MIIA_GR(PhyReg) | MAC_MIIA_CR(4);
	LPC_ETHERNET->MAC_MII_ADDR |= MAC_MIIA_GB;
}

/* Read a value via the MII link (blocking) */
err_t lpc_mii_read(u32_t PhyReg, u32_t *data) 
{
	u32_t mst = 250;
	err_t sts = ERR_OK;

	/* Read value at PHY address and register */
	lpc_mii_read_noblock(PhyReg);

	/* Wait for unbusy status */
	while (mst > 0) {
		sts = LPC_ETHERNET->MAC_MII_ADDR & MAC_MIIA_GB;
		if (sts == 0) {
			mst = 0;
			*data = LPC_ETHERNET->MAC_MII_DATA;
		} else {
			mst--;
			msDelay(1);
		}
	}

	if (sts != 0)
		sts = ERR_TIMEOUT;

	return sts;
}

/** \brief  Queues a pbuf into a free RX descriptor
 *
 *  \param[in] lpc_netifdata Pointer to the driver data structure
 *  \param[in] p             Pointer to pbuf to queue
 */
static void lpc_rxqueue_pbuf(struct lpc_enetdata *lpc_netifdata,
	struct pbuf *p)
{
	u32_t idx = lpc_netifdata->rx_next_idx;

	/* Save location of pbuf so we know what to pass to LWIP later */
	lpc_netifdata->rxpbufs[idx] = p;

	/* Buffer size and address for pbuf */
	lpc_netifdata->prdesc[idx].CTRL = (u32_t) RDES_ENH_BS1(p->len) |
		RDES_ENH_RCH;
	if (idx == (LPC_NUM_BUFF_RXDESCS - 1))
		lpc_netifdata->prdesc[idx].CTRL |= RDES_ENH_RER;
	lpc_netifdata->prdesc[idx].B1ADD = (u32_t) p->payload;

	/* Give descriptor to MAC/DMA */
	lpc_netifdata->prdesc[idx].STATUS = RDES_OWN;

	/* Update free count */
	lpc_netifdata->rx_free_descs--;

	LWIP_DEBUGF(UDP_LPC_EMAC | LWIP_DBG_TRACE,
		("lpc_rxqueue_pbuf: Queueing packet %p at index %d, free %d\n",
		p, idx, lpc_netifdata->rx_free_descs));

	/* Update index for next pbuf */
	idx++;
	if (idx >= LPC_NUM_BUFF_RXDESCS)
		idx = 0;
	lpc_netifdata->rx_next_idx = idx;
}

/** \brief  Attempt to allocate and requeue a new pbuf for RX
 *
 *  \param[in]  netif Pointer to the netif structure
 *  \returns    The number of new descriptors queued
 */
s32_t lpc_rx_queue(struct netif *netif)
{
	struct lpc_enetdata *lpc_netifdata = netif->state;
	struct pbuf *p;
	s32_t queued = 0;

	/* Attempt to requeue as many packets as possible */
	while (lpc_netifdata->rx_free_descs > 0) {
		/* Allocate a pbuf from the pool. We need to allocate at the
		   maximum size as we don't know the size of the yet to be
		   received packet. */
		p = pbuf_alloc(PBUF_RAW, (u16_t) EMAC_ETH_MAX_FLEN, PBUF_RAM);
		if (p == NULL) {
			LWIP_DEBUGF(UDP_LPC_EMAC | LWIP_DBG_TRACE,
				("lpc_rx_queue: could not allocate RX pbuf index %d, "
				"free %d)\n", lpc_netifdata->rx_next_idx,
				lpc_netifdata->rx_free_descs));
			return queued;
		}

		/* pbufs allocated from the RAM pool should be non-chained (although
		   the hardware will allow chaining) */
		LWIP_ASSERT("lpc_rx_queue: pbuf is not contiguous (chained)",
			pbuf_clen(p) <= 1);

		/* Queue packet */
		lpc_rxqueue_pbuf(lpc_netifdata, p);

		/* Update queued count */
		queued++;
	}

	return queued;
}

/** \brief  Sets up the RX descriptor ring buffers
 *
 *  This function sets up the descriptor list used for receive packets.
 *
 *  \param[in]  lpc_netifdata Pointer to driver data structure
 * \returns                   Always returns ERR_OK
 */
static err_t lpc_rx_setup(struct lpc_enetdata *lpc_netifdata)
{
	s32_t idx;

	/* Set to start of list */
	lpc_netifdata->rx_get_idx = 0;
	lpc_netifdata->rx_next_idx = 0;
	lpc_netifdata->rx_free_descs = LPC_NUM_BUFF_RXDESCS;

	/* Clear initial RX descriptor list */
	memset(lpc_netifdata->prdesc, 0, sizeof(lpc_netifdata->prdesc));

	/* Setup buffer chaining before allocating pbufs for descriptors
	   just in case memory runs out. */
	for (idx = 0; idx < LPC_NUM_BUFF_RXDESCS; idx++) {
		lpc_netifdata->prdesc[idx].CTRL = RDES_ENH_RCH;
		lpc_netifdata->prdesc[idx].B2ADD = (u32_t)
			&lpc_netifdata->prdesc[idx + 1];
	}
	lpc_netifdata->prdesc[LPC_NUM_BUFF_RXDESCS - 1].CTRL =
		RDES_ENH_RCH | RDES_ENH_RER;
	lpc_netifdata->prdesc[LPC_NUM_BUFF_RXDESCS - 1].B2ADD =
		(u32_t) &lpc_netifdata->prdesc[0];
	LPC_ETHERNET->DMA_REC_DES_ADDR = (u32_t) lpc_netifdata->prdesc;

	/* Setup up RX pbuf queue, but post a warning if not enough were
	   queued for all descriptors. */ 
	if (lpc_rx_queue(lpc_netifdata->netif) != LPC_NUM_BUFF_RXDESCS)
		LWIP_DEBUGF(UDP_LPC_EMAC | LWIP_DBG_TRACE,
			("lpc_rx_setup: Warning, not enough memory for RX pbufs\n"));

	return ERR_OK;
}

/** \brief  Gets data from queue and forwards to LWIP
 *
 *  \param[in] netif the lwip network interface structure for this lpc_enetif
 *  \return a pbuf filled with the received packet (including MAC header) or
 *         NULL on memory error
 */
static struct pbuf *lpc_low_level_input(struct netif *netif)
{
	struct lpc_enetdata *lpc_netifdata = netif->state;
	u32_t status, ridx;
	int rxerr = 0;
	struct pbuf *p;

#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_netifdata->TXLockMutex);
#endif
#endif

	/* If there are no used descriptors, then this call was
	   not for a received packet, try to setup some descriptors now */
	if (lpc_netifdata->rx_free_descs == LPC_NUM_BUFF_RXDESCS) {
		lpc_rx_queue(netif);
#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
		sys_mutex_unlock(&lpc_netifdata->TXLockMutex);
#endif
#endif
		return NULL;
	}

	/* Get index for next descriptor with data */
	ridx = lpc_netifdata->rx_get_idx;

	/* Return if descriptor is still owned by DMA */
	if (lpc_netifdata->prdesc[ridx].STATUS & RDES_OWN) {
#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
		sys_mutex_unlock(&lpc_netifdata->TXLockMutex);
#endif
#endif
		return NULL;
	}

	/* Get address of pbuf for this descriptor */
	p = lpc_netifdata->rxpbufs[ridx];

	/* Get receive packet status */
	status = lpc_netifdata->prdesc[ridx].STATUS;

	/* Check packet for errors */
	if (status & RDES_ES) {
		LINK_STATS_INC(link.drop);

		/* Error conditions that cause a packet drop */
		if (status & (
#if LPC_EMAC_RMII == 0
			RDES_CE | RDES_RE | RDES_RWT | RDES_LC | RDES_OE | RDES_SAF |
				RDES_AFM
#else
			RDES_CE | RDES_DE | RDES_RE | RDES_RWT | RDES_LC | RDES_OE |
			RDES_SAF | RDES_AFM
#endif																		  
		)) {
			LINK_STATS_INC(link.err);
			rxerr = 1;
		} else 

		/* Length error check needs qualification */
		if ((status & (RDES_LE | RDES_FT)) == RDES_LE) {
			LINK_STATS_INC(link.lenerr);
			rxerr = 1;
		} else

		/* CRC error check needs qualification */
		if ((status & (RDES_CE | RDES_LS)) == (RDES_CE | RDES_LS)) {
			LINK_STATS_INC(link.chkerr);
			rxerr = 1;
		}

		/* Descriptor error check needs qualification */
		if ((status & (RDES_DE | RDES_LS)) == (RDES_DE | RDES_LS)) {
			LINK_STATS_INC(link.err);
			rxerr = 1;
		} else

		/* Dribble bit error only applies in half duplex mode */
		if ((status & RDES_DE) &&
			(!(LPC_ETHERNET->MAC_CONFIG & MAC_CFG_DM))) {
			LINK_STATS_INC(link.err);
			rxerr = 1;
		}
	}

	/* Increment free descriptor count and next get index */
	lpc_netifdata->rx_free_descs++;
	ridx++;
	if (ridx >= LPC_NUM_BUFF_RXDESCS)
		ridx = 0;
	lpc_netifdata->rx_get_idx = ridx;

	/* If an error occurred, just re-queue the pbuf */
	if (rxerr) {
		lpc_rxqueue_pbuf(lpc_netifdata, p);
		p = NULL;

		LWIP_DEBUGF(UDP_LPC_EMAC | LWIP_DBG_TRACE,
			("lpc_low_level_input: RX error condition status 0x%08x\n",
			status));
	} else {
		/* Attempt to queue a new pbuf for the descriptor */
		lpc_rx_queue(netif);

		/* Get length of received packet */
		p->len = p->tot_len = (u16_t) RDES_FLMSK(status);

		LINK_STATS_INC(link.recv);

		LWIP_DEBUGF(UDP_LPC_EMAC | LWIP_DBG_TRACE,
			("lpc_low_level_input: Packet received, %d bytes, "
			"status 0x%08x\n", p->len, status));
	}

	/* (Re)start receive polling */
	LPC_ETHERNET->DMA_REC_POLL_DEMAND = 1;

#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_unlock(&lpc_netifdata->TXLockMutex);
#endif
#endif

	return p;  
}

/** \brief  Attempt to read a packet from the EMAC interface.
 *
 *  \param[in] netif the lwip network interface structure for this lpc_enetif
 */
void lpc_enetif_input(struct netif *netif)
{
	struct eth_hdr *ethhdr;
	struct pbuf *p;

	/* move received packet into a new pbuf */
	p = lpc_low_level_input(netif);
	if (p == NULL)
		return;

	/* points to packet payload, which starts with an Ethernet header */
	ethhdr = p->payload;

	switch (htons(ethhdr->type)) {
		case ETHTYPE_IP:
		case ETHTYPE_ARP:
#if PPPOE_SUPPORT
		case ETHTYPE_PPPOEDISC:
		case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
			/* full packet send to tcpip_thread to process */
			if (netif->input(p, netif) != ERR_OK) {
				LWIP_DEBUGF(NETIF_DEBUG,
					("lpc_enetif_input: IP input error\n"));
				/* Free buffer */
				pbuf_free(p);
			}
			break;

		default:
			/* Return buffer */
			pbuf_free(p);
			break;
	}
}

/** \brief  Sets up the TX descriptor ring buffers.
 *
 *  This function sets up the descriptor list used for transmit packets.
 *
 *  \param[in]   lpc_netifdata Pointer to driver data structure
 */
static err_t lpc_tx_setup(struct lpc_enetdata *lpc_netifdata)
{
	s32_t idx;

	/* Clear TX descriptors, will be queued with pbufs as needed */
	memset(&lpc_netifdata->ptdesc[0], 0, sizeof(lpc_netifdata->ptdesc));
	lpc_netifdata->tx_free_descs = LPC_NUM_BUFF_TXDESCS;
	lpc_netifdata->tx_fill_idx = 0;
	lpc_netifdata->tx_reclaim_idx = 0;

	/* Link/wrap descriptors */
	for (idx = 0; idx < LPC_NUM_BUFF_TXDESCS; idx++) {
		lpc_netifdata->ptdesc[idx].CTRLSTAT = TDES_ENH_TCH | TDES_ENH_CIC(3);
		lpc_netifdata->ptdesc[idx].B2ADD =
			(u32_t) &lpc_netifdata->ptdesc[idx + 1];
	}
	lpc_netifdata->ptdesc[LPC_NUM_BUFF_TXDESCS - 1].CTRLSTAT =
		TDES_ENH_TCH | TDES_ENH_TER | TDES_ENH_CIC(3);
	lpc_netifdata->ptdesc[LPC_NUM_BUFF_TXDESCS - 1].B2ADD =
			(u32_t) &lpc_netifdata->ptdesc[0];

	/* Setup pointer to TX descriptor table */
	LPC_ETHERNET->DMA_TRANS_DES_ADDR = (u32_t) lpc_netifdata->ptdesc;

	return ERR_OK;
}

/** \brief  Call for freeing TX buffers that are complete
 *
 *  \param[in] netif the lwip network interface structure for this lpc_enetif
 */
void lpc_tx_reclaim(struct netif *netif)
{
	struct lpc_enetdata *lpc_netifdata = netif->state;
	s32_t ridx;
	u32_t status;

#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_netifdata->TXLockMutex);
#endif

	/* If a descriptor is available and is no longer owned by the
	   hardware, it can be reclaimed */
	ridx = lpc_netifdata->tx_reclaim_idx;
	while ((lpc_netifdata->tx_free_descs < LPC_NUM_BUFF_TXDESCS) &&
		(!(lpc_netifdata->ptdesc[ridx].CTRLSTAT & TDES_OWN))) {
		/* Peek at the status of the descriptor to determine if the
		   packet is good and any status information. */
		status = lpc_netifdata->ptdesc[ridx].CTRLSTAT;

		LWIP_DEBUGF(UDP_LPC_EMAC | LWIP_DBG_TRACE,
			("lpc_tx_reclaim: Reclaiming sent packet %p, index %d\n",
			lpc_netifdata->txpbufs[ridx], ridx));

		/* Check TX error conditions */
		if (status & TDES_ES) {
			LWIP_DEBUGF(UDP_LPC_EMAC | LWIP_DBG_TRACE,
				("lpc_tx_reclaim: TX error condition status 0x%x\n", status));
			LINK_STATS_INC(link.err);

#if LINK_STATS == 1
			/* Error conditions that cause a packet drop */
			if (status & (TDES_UF | TDES_ED | TDES_EC | TDES_LC))
				LINK_STATS_INC(link.drop);
#endif
	  	}

		/* Reset control for this descriptor */
		if (ridx == (LPC_NUM_BUFF_TXDESCS - 1))
			lpc_netifdata->ptdesc[ridx].CTRLSTAT = TDES_ENH_TCH |
				TDES_ENH_TER;
		else
			lpc_netifdata->ptdesc[ridx].CTRLSTAT = TDES_ENH_TCH;

		/* Free the pbuf associate with this descriptor */
		if (lpc_netifdata->txpbufs[ridx])
			pbuf_free(lpc_netifdata->txpbufs[ridx]);

		/* Reclaim this descriptor */
		lpc_netifdata->tx_free_descs++;
#if NO_SYS == 0
		xSemaphoreGive(lpc_netifdata->xTXDCountSem);
#endif
		ridx++;
		if (ridx >= LPC_NUM_BUFF_TXDESCS)
			ridx = 0;
	}

	lpc_netifdata->tx_reclaim_idx = ridx;

#if NO_SYS == 0
	/* Restore access */
	sys_mutex_unlock(&lpc_netifdata->TXLockMutex);
#endif
}

/** \brief  Polls if an available TX descriptor is ready. Can be used to
 *          determine if the low level transmit function will block.
 *
 *  \param[in] netif the lwip network interface structure for this lpc_enetif
 *  \return 0 if no descriptors are read, or >0
 */
s32_t lpc_tx_ready(struct netif *netif)
{
	return ((struct lpc_enetdata *) netif->state)->tx_free_descs;
}

/** \brief  Low level output of a packet. Never call this from an
 *          interrupt context, as it may block until TX descriptors
 *          become available.
 *
 *  \param[in] netif the lwip network interface structure for this lpc_enetif
 *  \param[in] sendp the MAC packet to send (e.g. IP packet including MAC addresses and type)
 *  \return ERR_OK if the packet could be sent or
 *         an err_t value if the packet couldn't be sent
 */
static err_t lpc_low_level_output(struct netif *netif, struct pbuf *sendp)
{
	struct lpc_enetdata *lpc_netifdata = netif->state;
	u32_t idx, fidx, dn, fdn;
	struct pbuf *p = sendp;

#if LPC_CHECK_SLOWMEM == 1
	struct pbuf *q, *wp;
	u8_t *dst;
	int pcopy = 0;

	/* Check packet address to determine if it's in slow memory and
	   relocate if necessary */
 	for(q = p; ((q != NULL) && (pcopy == 0)); q = q->next) {
		fidx = 0;
		for (idx = 0; idx < sizeof(slmem);
			idx += sizeof(struct lpc_slowmem_array_t)) {
			if ((q->payload >= (void *) slmem[fidx].start) &&
				(q->payload <= (void *) slmem[fidx].end)) {
				/* Needs copy */
				pcopy = 1;
			}
		}
	}

	if (pcopy) {
		/* Create a new pbuf with the total pbuf size */
		wp = pbuf_alloc(PBUF_RAW, (u16_t) EMAC_ETH_MAX_FLEN, PBUF_RAM);
		if (!wp) {
			/* Exit with error */
			return ERR_MEM;
		}

		/* Copy pbuf */
		dst = (u8_t *) wp->payload;
		wp->tot_len = 0;
 		for(q = p; q != NULL; q = q->next) {
			MEMCPY(dst, (u8_t *) q->payload, q->len);
			dst += q->len;
			wp->tot_len += q->len;
		}
		wp->len = wp->tot_len;

		/* LWIP will free original pbuf on exit of function */

		p = sendp = wp;
	}
#endif

	/* Zero-copy TX buffers may be fragmented across mutliple payload
	   chains. Determine the number of descriptors needed for the
	   transfer. The pbuf chaining can be a mess! */
	dn = (u32_t) pbuf_clen(p);

	/* Wait until enough descriptors are available for the transfer. */
	/* THIS WILL BLOCK UNTIL THERE ARE ENOUGH DESCRIPTORS AVAILABLE */
	while (dn > lpc_tx_ready(netif))
#if NO_SYS == 0
		xSemaphoreTake(lpc_netifdata->xTXDCountSem, 0);
#else
		msDelay(1);
#endif

	/* Get the next free descriptor index */
	fidx = idx = lpc_netifdata->tx_fill_idx;

#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_netifdata->TXLockMutex);
#endif

	/* Fill in the next free descriptor(s) */
	while (dn > 0) {
		dn--;

		/* Setup packet address and length */
		lpc_netifdata->ptdesc[idx].B1ADD = (u32_t) p->payload;
		lpc_netifdata->ptdesc[idx].BSIZE = (u32_t) TDES_ENH_BS1(p->len);

		/* Save pointer to pbuf so we can reclain the memory for
		   the pbuf after the buffer has been sent. Only the first
		   pbuf in a chain is saved since the full chain doesn't
		   need to be freed. */
		/* For first packet only, first flag */
		lpc_netifdata->tx_free_descs--;
		if (idx == fidx) {
			lpc_netifdata->ptdesc[idx].CTRLSTAT |= TDES_ENH_FS;

#if LPC_CHECK_SLOWMEM == 1
			/* If this is a copied pbuf, then avoid getting the extra reference
			   or the TX reclaim will be off by 1 */
			if (!pcopy)
				pbuf_ref(p);
#else
			/* Increment reference count on this packet so LWIP doesn't
			   attempt to free it on return from this call */
				pbuf_ref(p);
#endif
		} else
			lpc_netifdata->ptdesc[idx].CTRLSTAT |= TDES_OWN;

		/* Save address of pbuf, but make sure it's associated with the
		   first chained pbuf so it gets freed once all pbuf chains are
		   transferred. */
		if (!dn)
			lpc_netifdata->txpbufs[idx] = sendp;
		else
			lpc_netifdata->txpbufs[idx] = NULL;

		/* For last packet only, interrupt and last flag */
		if (dn == 0)
			lpc_netifdata->ptdesc[idx].CTRLSTAT |= TDES_ENH_LS |
				TDES_ENH_IC;

		/* FIXME: For now, only IP header checksumming */
		lpc_netifdata->ptdesc[idx].CTRLSTAT |= TDES_ENH_CIC(3);

		LWIP_DEBUGF(UDP_LPC_EMAC | LWIP_DBG_TRACE,
			("lpc_low_level_output: pbuf packet %p sent, chain %d,"
			" size %d, index %d, free %d\n", p, dn, p->len, idx,
			lpc_netifdata->tx_free_descs));

		/* Update next available descriptor */
		idx++;
		if (idx >= LPC_NUM_BUFF_TXDESCS)
			idx = 0;

		/* Next packet fragment */
		p = p->next;
	}

	lpc_netifdata->tx_fill_idx = idx;

	LINK_STATS_INC(link.xmit);

	/* Give first descriptor to DMA to start transfer */
	lpc_netifdata->ptdesc[fidx].CTRLSTAT |= TDES_OWN;

	/* Tell DMA to poll descriptors to start transfer */
	LPC_ETHERNET->DMA_TRANS_POLL_DEMAND = 1;

#if NO_SYS == 0
	/* Restore access */
	sys_mutex_unlock(&lpc_netifdata->TXLockMutex);
#endif

	return ERR_OK;
}

/** \brief  LPC EMAC interrupt handler.
 *
 *  This function handles the transmit, receive, and error interrupt of
 *  the LPC118xx/43xx. This is meant to be used when NO_SYS=0.
 */
void ETH_IRQHandler(void)
{
#if NO_SYS == 1
	/* Interrupts are not used without an RTOS */
    NVIC_DisableIRQ(ETHERNET_IRQn);
#else
	signed portBASE_TYPE xRecTaskWoken = pdFALSE, XTXTaskWoken = pdFALSE;
	uint32_t ints;

	/* Get pending interrupts */
	ints = LPC_ETHERNET->DMA_STAT;

	/* RX group interrupt(s) */
	if (ints & (DMA_ST_RI | DMA_ST_OVF | DMA_ST_RU)) {
		/* Give semaphore to wakeup RX receive task. Note the FreeRTOS
		   method is used instead of the LWIP arch method. */
        xSemaphoreGiveFromISR(lpc_enetdata.RxSem, &xRecTaskWoken);
	}

	/* TX group interrupt(s) */
	if (ints & (DMA_ST_TI | DMA_ST_UNF | DMA_ST_TU)) {
		/* Give semaphore to wakeup TX cleanup task. Note the FreeRTOS
		   method is used instead of the LWIP arch method. */
        xSemaphoreGiveFromISR(lpc_enetdata.TxCleanSem, &XTXTaskWoken);
	}

	/* Clear pending interrupts */
	LPC_ETHERNET->DMA_STAT = ints;

	/* Context switch needed? */
	portEND_SWITCHING_ISR( xRecTaskWoken || XTXTaskWoken );
#endif
}

#if NO_SYS == 0
/** \brief  Packet reception task
 *
 * This task is called when a packet is received. It will
 * pass the packet to the LWIP core.
 *
 *  \param[in] pvParameters Pointer to driver data
 */
static portTASK_FUNCTION( vPacketReceiveTask, pvParameters )
{
	struct lpc_enetdata *lpc_netifdata = pvParameters;

	while (1) {
		/* Wait for receive task to wakeup */
		sys_arch_sem_wait(&lpc_netifdata->RxSem, 0);

		/* Process receive packets */
		while (!(lpc_netifdata->prdesc[lpc_netifdata->rx_get_idx].STATUS
			& RDES_OWN))
			lpc_enetif_input(lpc_netifdata->netif);
	}
}

/** \brief  Transmit cleanup task
 *
 * This task is called when a transmit interrupt occurs and
 * reclaims the pbuf and descriptor used for the packet once
 * the packet has been transferred.
 *
 *  \param[in] pvParameters Pointer to driver data
 */
static portTASK_FUNCTION( vTransmitCleanupTask, pvParameters )
{
	struct lpc_enetdata *lpc_netifdata = pvParameters;

	while (1) {
		/* Wait for transmit cleanup task to wakeup */
		sys_arch_sem_wait(&lpc_netifdata->TxCleanSem, 0);

		/* Free TX pbufs and descriptors that are done */
		lpc_tx_reclaim(lpc_netifdata->netif);
	}
}
#endif

/** \brief  Low level init of the MAC and PHY.
 *
 *  \param[in]       netif  Pointer to LWIP netif structure
 *  \return          ERR_OK or error code
 */
static err_t low_level_init(struct netif *netif)
{
	struct lpc_enetdata *lpc_netifdata = netif->state;
	err_t err;
	s32_t timeout;

	/* Enable MAC clocking from same source as CPU */
	CGU_EntityConnect(CGU_CLKSRC_PLL1, CGU_PERIPHERAL_ETHERNET);

	/* Slightly different clocking for RMII and MII modes */
	CGU_EntityConnect(CGU_CLKSRC_ENET_TX_CLK, CGU_BASE_PHY_TX);
#if LPC_EMAC_RMII == 1
	/* RMII mode gets PHY RX clock from ENET_REF_CLK (same pin as
	   ENET_TX_CLK on the chip) */
	CGU_EntityConnect(CGU_CLKSRC_ENET_TX_CLK, CGU_BASE_PHY_RX);
#else
	/* MII mode gets PHY RX clock from ENET_RX_CLK */
	CGU_EntityConnect(CGU_CLKSRC_ENET_RX_CLK, CGU_BASE_PHY_RX);
#endif

	/* Reset ethernet via RGU. This should be 1 clock, but we wait
	   anyways. If the while loop really stalls, something else
	   is wrong. */
	LPC_RGU->RESET_CTRL0 = (1 << 22);
	timeout = 10;
	while (!(LPC_RGU->RESET_ACTIVE_STATUS0 & (1 << 22))) {
		msDelay(1);
		timeout--;
		if (timeout == 0)
			return ERR_TIMEOUT;
	}

	/* Reset MAC Subsystem internal registers and logic */
	LPC_ETHERNET->DMA_BUS_MODE |= DMA_BM_SWR;
//	timeout = 3;
//	while (LPC_ETHERNET->DMA_BUS_MODE & DMA_BM_SWR) {
//		msDelay(1);
//		timeout--;
//		if (timeout == 0)
//			return ERR_TIMEOUT;
//	}
	LPC_ETHERNET->DMA_BUS_MODE = DMA_BM_ATDS | DMA_BM_PBL(1) | DMA_BM_RPBL(1);

	/* Save MAC address */
	LPC_ETHERNET->MAC_ADDR0_LOW = ((u32_t) netif->hwaddr[3] << 24) |
		((u32_t) netif->hwaddr[2] << 16) | ((u32_t) netif->hwaddr[1] << 8) |
		(u32_t) netif->hwaddr[0];
	LPC_ETHERNET->MAC_ADDR0_HIGH = ((u32_t) netif->hwaddr[5] << 8) |
		(u32_t) netif->hwaddr[4];

	/* Initial MAC configuration for checksum offload, full duplex,
	   100Mbps, disable receive own in half duplex, inter-frame gap
	   of 64-bits */
	LPC_ETHERNET->MAC_CONFIG = MAC_CFG_BL(0) | MAC_CFG_IPC | MAC_CFG_DM |
		MAC_CFG_DO | MAC_CFG_FES | MAC_CFG_PS | MAC_CFG_IFG(3);

	/* Setup filter */
#if IP_SOF_BROADCAST_RECV
	LPC_ETHERNET->MAC_FRAME_FILTER = MAC_FF_PR | MAC_FF_RA;
#else
	LPC_ETHERNET->MAC_FRAME_FILTER = 0; /* Only matching MAC address */
#endif

	/* Initialize the PHY */
	err = lpc_phy_init(netif, LPC_EMAC_RMII);
	if (err != ERR_OK)
 		return err;

	/* Setup transmit and receive descriptors */
	if (lpc_tx_setup(lpc_netifdata) != ERR_OK)
		return ERR_BUF;
	if (lpc_rx_setup(lpc_netifdata) != ERR_OK)
		return ERR_BUF;

	/* Flush transmit FIFO */
	LPC_ETHERNET->DMA_OP_MODE = DMA_OM_FTF;

	/* Setup DMA to flush receive FIFOs at 32 bytes, service TX FIFOs at
	   64 bytes */
	LPC_ETHERNET->DMA_OP_MODE |= DMA_OM_RTC(1) | DMA_OM_TTC(0);

	/* Clear all MAC interrupts */
	LPC_ETHERNET->DMA_STAT = DMA_ST_ALL;

	/* Enable MAC interrupts */
	LPC_ETHERNET->DMA_INT_EN =
#if NO_SYS == 1
		0;
#else
		DMA_IE_TIE | DMA_IE_OVE | DMA_IE_UNE | DMA_IE_RIE | DMA_IE_NIE |
			DMA_IE_AIE | DMA_IE_TUE | DMA_IE_RUE;
#endif

	/* Enable receive and transmit DMA processes */
	LPC_ETHERNET->DMA_OP_MODE |= DMA_OM_ST | DMA_OM_SR;

	/* Enable packet reception */
	LPC_ETHERNET->MAC_CONFIG |= MAC_CFG_RE | MAC_CFG_TE; 

	/* Start receive polling */
	LPC_ETHERNET->DMA_REC_POLL_DEMAND = 1;

	return ERR_OK;
}

/**
 * This function provides a method for the PHY to setup the EMAC
 * for the PHY negotiated duplex mode.
 *
 * \param[in] full_duplex 0 = half duplex, 1 = full duplex
 */
void lpc_emac_set_duplex(int full_duplex)
{
	if (full_duplex)
		LPC_ETHERNET->MAC_CONFIG |= MAC_CFG_DM;
	else
		LPC_ETHERNET->MAC_CONFIG &= ~MAC_CFG_DM;
}

/**
 * This function provides a method for the PHY to setup the EMAC
 * for the PHY negotiated bit rate.
 *
 * \param[in] mbs_100     0 = 10mbs mode, 1 = 100mbs mode
 */
void lpc_emac_set_speed(int mbs_100)
{
	if (mbs_100)
		LPC_ETHERNET->MAC_CONFIG |= MAC_CFG_FES;
	else
		LPC_ETHERNET->MAC_CONFIG &= ~MAC_CFG_FES;
}

/**
 * This function is the ethernet packet send function. It calls
 * etharp_output after checking link status.
 *
 * \param[in] netif the lwip network interface structure for this lpc_enetif
 * \param[in] q Pointer to pbug to send
 * \param[in] ipaddr IP address 
 * \return ERR_OK or error code
 */
err_t lpc_etharp_output(struct netif *netif, struct pbuf *q,
	ip_addr_t *ipaddr)
{
	/* Only send packet is link is up */
	if (netif->flags & NETIF_FLAG_LINK_UP)
		return etharp_output(netif, q, ipaddr);

	return ERR_CONN;
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * \param[in] netif the lwip network interface structure for this lpc_enetif
 * \return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
void boardGetMACaddr(uint8_t *macaddr); // FIXME ethernet
err_t lpc_enetif_init(struct netif *netif)
{
	err_t err;

	LWIP_ASSERT("netif != NULL", (netif != NULL));
    
	lpc_enetdata.netif = netif;

	/* set MAC hardware address */
	boardGetMACaddr(netif->hwaddr);
	netif->hwaddr_len = ETHARP_HWADDR_LEN;

 	/* maximum transfer unit */
	netif->mtu = 1500;

	/* device capabilities */
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_UP |
		NETIF_FLAG_ETHERNET;

	/* Initialize the hardware */
	netif->state = &lpc_enetdata;
	err = low_level_init(netif);
	if (err != ERR_OK)
		return err;

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "lwiplpc";
#endif /* LWIP_NETIF_HOSTNAME */

	netif->name[0] = 'e';
	netif->name[1] = 'n';

	netif->output = lpc_etharp_output;
	netif->linkoutput = lpc_low_level_output;

	/* For FreeRTOS, start tasks */
#if NO_SYS == 0
	lpc_enetdata.xTXDCountSem = xSemaphoreCreateCounting(LPC_NUM_BUFF_TXDESCS,
		LPC_NUM_BUFF_TXDESCS);
	LWIP_ASSERT("xTXDCountSem creation error",
		(lpc_enetdata.xTXDCountSem != NULL));

	err = sys_mutex_new(&lpc_enetdata.TXLockMutex);
	LWIP_ASSERT("TXLockMutex creation error", (err == ERR_OK));

	/* Packet receive task */
	err = sys_sem_new(&lpc_enetdata.RxSem, 0);
	LWIP_ASSERT("RxSem creation error", (err == ERR_OK));
	sys_thread_new("receive_thread", vPacketReceiveTask, netif->state,
		DEFAULT_THREAD_STACKSIZE, tskRECPKT_PRIORITY);

	/* Transmit cleanup task */
	err = sys_sem_new(&lpc_enetdata.TxCleanSem, 0);
	LWIP_ASSERT("TxCleanSem creation error", (err == ERR_OK));
	sys_thread_new("txclean_thread", vTransmitCleanupTask, netif->state,
		DEFAULT_THREAD_STACKSIZE, tskTXCLEAN_PRIORITY);
#endif

	return ERR_OK;
}

/**
 * @}
 */

/* --------------------------------- End Of File ------------------------------ */
