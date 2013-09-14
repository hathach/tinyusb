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

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "netif/etharp.h"
#include "netif/ppp_oe.h"

#include "lpc_18xx43xx_emac_config.h"
#include "lpc18xx_43xx_emac.h"

#include "chip.h"
#include "board.h"
#include "lpc_phy.h"

#include <string.h>

extern void msDelay(uint32_t ms);

#if LPC_NUM_BUFF_TXDESCS < 2
#error LPC_NUM_BUFF_TXDESCS must be at least 2
#endif

#if LPC_NUM_BUFF_RXDESCS < 3
#error LPC_NUM_BUFF_RXDESCS must be at least 3
#endif

#ifndef LPC_CHECK_SLOWMEM
#error LPC_CHECK_SLOWMEM must be 0 or 1
#endif

/** @ingroup NET_LWIP_LPC18XX43XX_EMAC_DRIVER
 * @{
 */

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

#if NO_SYS == 0
/**
 * @brief	Driver transmit and receive thread priorities
 * Thread priorities for receive thread and TX cleanup thread. Alter
 * to prioritize receive or transmit bandwidth. In a heavily loaded
 * system or with LWIP_DEBUG enabled, the priorities might be better
 * the same. */
#define tskTXCLEAN_PRIORITY  (TCPIP_THREAD_PRIO - 1)
#define tskRECPKT_PRIORITY   (TCPIP_THREAD_PRIO - 1)
#endif

/** @brief	Debug output formatter lock define
 * When using FreeRTOS and with LWIP_DEBUG enabled, enabling this
 * define will allow RX debug messages to not interleave with the
 * TX messages (so they are actually readable). Not enabling this
 * define when the system is under load will cause the output to
 * be unreadable. There is a small tradeoff in performance for this
 * so use it only for debug. */
// #define LOCK_RX_THREAD

/* LPC EMAC driver data structure */
struct lpc_enetdata {
	struct netif *netif;		/**< Reference back to LWIP parent netif */

	IP_ENET_001_ENHTXDESC_T ptdesc[LPC_NUM_BUFF_TXDESCS];	/**< TX descriptor list */
	IP_ENET_001_ENHRXDESC_T prdesc[LPC_NUM_BUFF_RXDESCS];	/**< RX descriptor list */
	struct pbuf *txpbufs[LPC_NUM_BUFF_TXDESCS];	/**< Saved pbuf pointers, for free after TX */

	volatile u32_t tx_free_descs;	/**< Number of free TX descriptors */
	u32_t tx_fill_idx;	/**< Current free TX descriptor index */
	u32_t tx_reclaim_idx;	/**< Next incoming TX packet descriptor index */
	struct pbuf *rxpbufs[LPC_NUM_BUFF_RXDESCS];	/**< Saved pbuf pointers for RX */

	volatile u32_t rx_free_descs;	/**< Number of free RX descriptors */
	volatile u32_t rx_get_idx;	/**< Index to next RX descriptor that id to be received */
	u32_t rx_next_idx;	/**< Index to next RX descriptor that needs a pbuf */
#if NO_SYS == 0
	sys_sem_t RxSem;/**< RX receive thread wakeup semaphore */
	sys_sem_t TxCleanSem;	/**< TX cleanup thread wakeup semaphore */
	sys_mutex_t TXLockMutex;/**< TX critical section mutex */
	xSemaphoreHandle xTXDCountSem;	/**< TX free buffer counting semaphore */
#endif
};

/* LPC EMAC driver work data */
static struct lpc_enetdata lpc_enetdata;

static uint32_t intMask;

#if LPC_CHECK_SLOWMEM == 1
struct lpc_slowmem_array_t {
	u32_t start;
	u32_t end;
};

const static struct lpc_slowmem_array_t slmem[] = LPC_SLOWMEM_ARRAY;
#endif

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Queues a pbuf into a free RX descriptor */
static void lpc_rxqueue_pbuf(struct lpc_enetdata *lpc_netifdata,
							 struct pbuf *p)
{
	u32_t idx = lpc_netifdata->rx_next_idx;

	/* Save location of pbuf so we know what to pass to LWIP later */
	lpc_netifdata->rxpbufs[idx] = p;

	/* Buffer size and address for pbuf */
	lpc_netifdata->prdesc[idx].CTRL = (u32_t) RDES_ENH_BS1(p->len) |
									  RDES_ENH_RCH;
	if (idx == (LPC_NUM_BUFF_RXDESCS - 1)) {
		lpc_netifdata->prdesc[idx].CTRL |= RDES_ENH_RER;
	}
	lpc_netifdata->prdesc[idx].B1ADD = (u32_t) p->payload;

	/* Give descriptor to MAC/DMA */
	lpc_netifdata->prdesc[idx].STATUS = RDES_OWN;

	/* Update free count */
	lpc_netifdata->rx_free_descs--;

	LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
				("lpc_rxqueue_pbuf: Queueing packet %p at index %d, free %d\n",
				 p, idx, lpc_netifdata->rx_free_descs));

	/* Update index for next pbuf */
	idx++;
	if (idx >= LPC_NUM_BUFF_RXDESCS) {
		idx = 0;
	}
	lpc_netifdata->rx_next_idx = idx;
}

/* This function sets up the descriptor list used for receive packets */
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
	if (lpc_rx_queue(lpc_netifdata->netif) != LPC_NUM_BUFF_RXDESCS) {
		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
					("lpc_rx_setup: Warning, not enough memory for RX pbufs\n"));
	}

	return ERR_OK;
}

/* Gets data from queue and forwards to LWIP */
static struct pbuf *lpc_low_level_input(struct netif *netif) {
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
		if (status & intMask) {
			LINK_STATS_INC(link.err);
			rxerr = 1;
		}
		else
		/* Length error check needs qualification */
		if ((status & (RDES_LE | RDES_FT)) == RDES_LE) {
			LINK_STATS_INC(link.lenerr);
			rxerr = 1;
		}
		else
		/* CRC error check needs qualification */
		if ((status & (RDES_CE | RDES_LS)) == (RDES_CE | RDES_LS)) {
			LINK_STATS_INC(link.chkerr);
			rxerr = 1;
		}

		/* Descriptor error check needs qualification */
		if ((status & (RDES_DE | RDES_LS)) == (RDES_DE | RDES_LS)) {
			LINK_STATS_INC(link.err);
			rxerr = 1;
		}
		else
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
	if (ridx >= LPC_NUM_BUFF_RXDESCS) {
		ridx = 0;
	}
	lpc_netifdata->rx_get_idx = ridx;

	/* If an error occurred, just re-queue the pbuf */
	if (rxerr) {
		lpc_rxqueue_pbuf(lpc_netifdata, p);
		p = NULL;

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
					("lpc_low_level_input: RX error condition status 0x%08x\n",
					 status));
	}
	else {
		/* Attempt to queue a new pbuf for the descriptor */
		lpc_rx_queue(netif);

		/* Get length of received packet */
		p->len = p->tot_len = (u16_t) RDES_FLMSK(status);

		LINK_STATS_INC(link.recv);

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
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

/* This function sets up the descriptor list used for transmit packets */
static err_t lpc_tx_setup(struct lpc_enetdata *lpc_netifdata)
{
	s32_t idx;

	/* Clear TX descriptors, will be queued with pbufs as needed */
	memset((void *) &lpc_netifdata->ptdesc[0], 0, sizeof(lpc_netifdata->ptdesc));
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

/* Low level output of a packet. Never call this from an interrupt context,
   as it may block until TX descriptors become available */
static err_t lpc_low_level_output(struct netif *netif, struct pbuf *sendp)
{
	struct lpc_enetdata *lpc_netifdata = netif->state;
	u32_t idx, fidx, dn;
	struct pbuf *p = sendp;

#if LPC_CHECK_SLOWMEM == 1
	struct pbuf *q, *wp;

	u8_t *dst;
	int pcopy = 0;

	/* Check packet address to determine if it's in slow memory and
	   relocate if necessary */
	for (q = p; ((q != NULL) && (pcopy == 0)); q = q->next) {
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
		for (q = p; q != NULL; q = q->next) {
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
	{xSemaphoreTake(lpc_netifdata->xTXDCountSem, 0); }
#else
	{msDelay(1); }
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
			if (!pcopy) {
				pbuf_ref(p);
			}
#else
			/* Increment reference count on this packet so LWIP doesn't
			   attempt to free it on return from this call */
			pbuf_ref(p);
#endif
		}
		else {
			lpc_netifdata->ptdesc[idx].CTRLSTAT |= TDES_OWN;
		}

		/* Save address of pbuf, but make sure it's associated with the
		   first chained pbuf so it gets freed once all pbuf chains are
		   transferred. */
		if (!dn) {
			lpc_netifdata->txpbufs[idx] = sendp;
		}
		else {
			lpc_netifdata->txpbufs[idx] = NULL;
		}

		/* For last packet only, interrupt and last flag */
		if (dn == 0) {
			lpc_netifdata->ptdesc[idx].CTRLSTAT |= TDES_ENH_LS |
												   TDES_ENH_IC;
		}

		/* IP checksumming requires full buffering in IP */
		lpc_netifdata->ptdesc[idx].CTRLSTAT |= TDES_ENH_CIC(3);

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
					("lpc_low_level_output: pbuf packet %p sent, chain %d,"
					 " size %d, index %d, free %d\n", p, dn, p->len, idx,
					 lpc_netifdata->tx_free_descs));

		/* Update next available descriptor */
		idx++;
		if (idx >= LPC_NUM_BUFF_TXDESCS) {
			idx = 0;
		}

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

/* This function is the ethernet packet send function. It calls
   etharp_output after checking link status */
static err_t lpc_etharp_output(struct netif *netif, struct pbuf *q,
							   ip_addr_t *ipaddr)
{
	/* Only send packet is link is up */
	if (netif->flags & NETIF_FLAG_LINK_UP) {
		return etharp_output(netif, q, ipaddr);
	}

	return ERR_CONN;
}

#if NO_SYS == 0
/* Packet reception task
   This task is called when a packet is received. It will
   pass the packet to the LWIP core */
static portTASK_FUNCTION(vPacketReceiveTask, pvParameters) {
	struct lpc_enetdata *lpc_netifdata = pvParameters;

	while (1) {
		/* Wait for receive task to wakeup */
		sys_arch_sem_wait(&lpc_netifdata->RxSem, 0);

		/* Process receive packets */
		while (!(lpc_netifdata->prdesc[lpc_netifdata->rx_get_idx].STATUS
				 & RDES_OWN)) {
			lpc_enetif_input(lpc_netifdata->netif);
		}
	}
}

/* Transmit cleanup task
   This task is called when a transmit interrupt occurs and
   reclaims the pbuf and descriptor used for the packet once
   the packet has been transferred */
static portTASK_FUNCTION(vTransmitCleanupTask, pvParameters) {
	struct lpc_enetdata *lpc_netifdata = pvParameters;

	while (1) {
		/* Wait for transmit cleanup task to wakeup */
		sys_arch_sem_wait(&lpc_netifdata->TxCleanSem, 0);

		/* Free TX pbufs and descriptors that are done */
		lpc_tx_reclaim(lpc_netifdata->netif);
	}
}
#endif

/* Low level init of the MAC and PHY */
static err_t low_level_init(struct netif *netif)
{
	struct lpc_enetdata *lpc_netifdata = netif->state;

	/* Initialize via Chip ENET function */
	Chip_ENET_Init(LPC_ETHERNET);

	/* Save MAC address */
	Chip_ENET_SetADDR(LPC_ETHERNET, netif->hwaddr);

	/* Initial MAC configuration for checksum offload, full duplex,
	   100Mbps, disable receive own in half duplex, inter-frame gap
	   of 64-bits */
	LPC_ETHERNET->MAC_CONFIG = MAC_CFG_BL(0) | MAC_CFG_IPC | MAC_CFG_DM |
							   MAC_CFG_DO | MAC_CFG_FES | MAC_CFG_PS | MAC_CFG_IFG(3);

	/* Setup filter */
#if IP_SOF_BROADCAST_RECV
	LPC_ETHERNET->MAC_FRAME_FILTER = MAC_FF_PR | MAC_FF_RA;
#else
	LPC_ETHERNET->MAC_FRAME_FILTER = 0;	/* Only matching MAC address */
#endif

	/* Initialize the PHY */
#if defined(USE_RMII)
	if (lpc_phy_init(true, msDelay) != SUCCESS) {
		return ERROR;
	}

	intMask = RDES_CE | RDES_DE | RDES_RE | RDES_RWT | RDES_LC | RDES_OE |
			  RDES_SAF | RDES_AFM;
#else
	if (lpc_phy_init(false, msDelay) != SUCCESS) {
		return ERROR;
	}

	intMask = RDES_CE | RDES_RE | RDES_RWT | RDES_LC | RDES_OE | RDES_SAF |
			  RDES_AFM;
#endif

	/* Setup transmit and receive descriptors */
	if (lpc_tx_setup(lpc_netifdata) != ERR_OK) {
		return ERR_BUF;
	}
	if (lpc_rx_setup(lpc_netifdata) != ERR_OK) {
		return ERR_BUF;
	}

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

/*****************************************************************************
 * Public functions
 ****************************************************************************/
/* Attempt to allocate and requeue a new pbuf for RX */
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
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
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

/* Attempt to read a packet from the EMAC interface */
void lpc_enetif_input(struct netif *netif)
{
	struct eth_hdr *ethhdr;

	struct pbuf *p;

	/* move received packet into a new pbuf */
	p = lpc_low_level_input(netif);
	if (p == NULL) {
		return;
	}

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

/* Call for freeing TX buffers that are complete */
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

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
					("lpc_tx_reclaim: Reclaiming sent packet %p, index %d\n",
					 lpc_netifdata->txpbufs[ridx], ridx));

		/* Check TX error conditions */
		if (status & TDES_ES) {
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_tx_reclaim: TX error condition status 0x%x\n", status));
			LINK_STATS_INC(link.err);

#if LINK_STATS == 1
			/* Error conditions that cause a packet drop */
			if (status & (TDES_UF | TDES_ED | TDES_EC | TDES_LC)) {
				LINK_STATS_INC(link.drop);
			}
#endif
		}

		/* Reset control for this descriptor */
		if (ridx == (LPC_NUM_BUFF_TXDESCS - 1)) {
			lpc_netifdata->ptdesc[ridx].CTRLSTAT = TDES_ENH_TCH |
												   TDES_ENH_TER;
		}
		else {
			lpc_netifdata->ptdesc[ridx].CTRLSTAT = TDES_ENH_TCH;
		}

		/* Free the pbuf associate with this descriptor */
		if (lpc_netifdata->txpbufs[ridx]) {
			pbuf_free(lpc_netifdata->txpbufs[ridx]);
		}

		/* Reclaim this descriptor */
		lpc_netifdata->tx_free_descs++;
#if NO_SYS == 0
		xSemaphoreGive(lpc_netifdata->xTXDCountSem);
#endif
		ridx++;
		if (ridx >= LPC_NUM_BUFF_TXDESCS) {
			ridx = 0;
		}
	}

	lpc_netifdata->tx_reclaim_idx = ridx;

#if NO_SYS == 0
	/* Restore access */
	sys_mutex_unlock(&lpc_netifdata->TXLockMutex);
#endif
}

/* Polls if an available TX descriptor is ready */
s32_t lpc_tx_ready(struct netif *netif)
{
	return ((struct lpc_enetdata *) netif->state)->tx_free_descs;
}

/**
 * @brief	EMAC interrupt handler
 * @return	Nothing
 * @note	This function handles the transmit, receive, and error interrupt of
 * the LPC118xx/43xx. This is meant to be used when NO_SYS=0.
 */
void ETH_IRQHandler(void)
{
#if NO_SYS == 1
	/* Interrupts are not used without an RTOS */
	NVIC_DisableIRQ((IRQn_Type) ETHERNET_IRQn);
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
	portEND_SWITCHING_ISR(xRecTaskWoken || XTXTaskWoken);
#endif
}

/* Set up the MAC interface duplex */
void lpc_emac_set_duplex(int full_duplex)
{
	if (full_duplex) {
		LPC_ETHERNET->MAC_CONFIG |= MAC_CFG_DM;
	}
	else {
		LPC_ETHERNET->MAC_CONFIG &= ~MAC_CFG_DM;
	}
}

/* Set up the MAC interface speed */
void lpc_emac_set_speed(int mbs_100)
{
	if (mbs_100) {
		LPC_ETHERNET->MAC_CONFIG |= MAC_CFG_FES;
	}
	else {
		LPC_ETHERNET->MAC_CONFIG &= ~MAC_CFG_FES;
	}
}

/* LWIP 18xx/43xx EMAC initialization function */
err_t lpc_enetif_init(struct netif *netif)
{
	err_t err;
	extern void Board_ENET_GetMacADDR(u8_t *mcaddr);

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	lpc_enetdata.netif = netif;

	/* set MAC hardware address */
	Board_ENET_GetMacADDR(netif->hwaddr);
	netif->hwaddr_len = ETHARP_HWADDR_LEN;

	/* maximum transfer unit */
	netif->mtu = 1500;

	/* device capabilities */
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_UP |
				   NETIF_FLAG_ETHERNET;

	/* Initialize the hardware */
	netif->state = &lpc_enetdata;
	err = low_level_init(netif);
	if (err != ERR_OK) {
		return err;
	}

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
