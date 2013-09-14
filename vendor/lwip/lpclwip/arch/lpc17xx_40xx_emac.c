/*
 * @brief LPC17xx/40xx LWIP EMAC driver
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

#include "lpc_17xx40xx_emac_config.h"
#include "lpc17xx_40xx_emac.h"

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

/** @ingroup NET_LWIP_LPC17XX40XX_EMAC_DRIVER
 * @{
 */

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

#if NO_SYS == 0
/**
 * @brief Driver transmit and receive thread priorities
 * Thread priorities for receive thread and TX cleanup thread. Alter
 * to prioritize receive or transmit bandwidth. In a heavily loaded
 * system or with LWIP_DEBUG enabled, the priorities might be better
 * the same. */
#define tskRECPKT_PRIORITY   (DEFAULT_THREAD_PRIO + 4)
#define tskTXCLEAN_PRIORITY  (DEFAULT_THREAD_PRIO + 5)
// #define tskTXCLEAN_PRIORITY  (TCPIP_THREAD_PRIO - 1) // FIXME
// #define tskRECPKT_PRIORITY   (TCPIP_THREAD_PRIO - 1) // FIXME
#endif

/** @brief Debug output formatter lock define
 * When using FreeRTOS and with LWIP_DEBUG enabled, enabling this
 * define will allow RX debug messages to not interleave with the
 * TX messages (so they are actually readable). Not enabling this
 * define when the system is under load will cause the output to
 * be unreadable. There is a small tradeoff in performance for this
 * so use it only for debug. */
// #define LOCK_RX_THREAD

#if NO_SYS == 0
/** @brief Receive group interrupts
 */
#define RXINTGROUP (ENET_INT_RXOVERRUN | ENET_INT_RXERROR | ENET_INT_RXDONE)

/** @brief Transmit group interrupts
 */
#define TXINTGROUP (ENET_INT_TXUNDERRUN | ENET_INT_TXERROR | ENET_INT_TXDONE)
#else
#define RXINTGROUP 0
#define TXINTGROUP 0
#endif

/* LPC EMAC driver data structure */
typedef struct {
	/* prxs must be 8 byte aligned! */
	ENET_RXSTAT_T prxs[LPC_NUM_BUFF_RXDESCS];	/**< Pointer to RX statuses */
	ENET_RXDESC_T prxd[LPC_NUM_BUFF_RXDESCS];	/**< Pointer to RX descriptor list */
	ENET_TXSTAT_T ptxs[LPC_NUM_BUFF_TXDESCS];	/**< Pointer to TX statuses */
	ENET_TXDESC_T ptxd[LPC_NUM_BUFF_TXDESCS];	/**< Pointer to TX descriptor list */
	struct netif *pnetif;						/**< Reference back to LWIP parent netif */

	struct pbuf *rxb[LPC_NUM_BUFF_RXDESCS];		/**< RX pbuf pointer list, zero-copy mode */

	u32_t rx_fill_desc_index;					/**< RX descriptor next available index */
	volatile u32_t rx_free_descs;				/**< Count of free RX descriptors */
	struct pbuf *txb[LPC_NUM_BUFF_TXDESCS];		/**< TX pbuf pointer list, zero-copy mode */

	u32_t lpc_last_tx_idx;						/**< TX last descriptor index, zero-copy mode */
#if NO_SYS == 0
	sys_sem_t rx_sem;							/**< RX receive thread wakeup semaphore */
	sys_sem_t tx_clean_sem;						/**< TX cleanup thread wakeup semaphore */
	sys_mutex_t tx_lock_mutex;					/**< TX critical section mutex */
	sys_mutex_t rx_lock_mutex;					/**< RX critical section mutex */
	xSemaphoreHandle xtx_count_sem;				/**< TX free buffer counting semaphore */
#endif
} lpc_enetdata_t;

/** \brief  LPC EMAC driver work data
 */
ALIGNED(8) lpc_enetdata_t lpc_enetdata;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Queues a pbuf into the RX descriptor list */
STATIC void lpc_rxqueue_pbuf(lpc_enetdata_t *lpc_enetif, struct pbuf *p)
{
	u32_t idx;

	/* Get next free descriptor index */
	idx = lpc_enetif->rx_fill_desc_index;

	/* Setup descriptor and clear statuses */
	lpc_enetif->prxd[idx].Control = ENET_RCTRL_INT | ((u32_t) ENET_RCTRL_SIZE(p->len));
	lpc_enetif->prxd[idx].Packet = (u32_t) p->payload;
	lpc_enetif->prxs[idx].StatusInfo = 0xFFFFFFFF;
	lpc_enetif->prxs[idx].StatusHashCRC = 0xFFFFFFFF;

	/* Save pbuf pointer for push to network layer later */
	lpc_enetif->rxb[idx] = p;

	/* Wrap at end of descriptor list */
	idx++;
	if (idx >= LPC_NUM_BUFF_RXDESCS) {
		idx = 0;
	}

	/* Queue descriptor(s) */
	lpc_enetif->rx_free_descs -= 1;
	lpc_enetif->rx_fill_desc_index = idx;

	LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
				("lpc_rxqueue_pbuf: pbuf packet queued: %p (free desc=%d)\n", p,
				 lpc_enetif->rx_free_descs));
}

/* Sets up the RX descriptor ring buffers. */
STATIC err_t lpc_rx_setup(lpc_enetdata_t *lpc_enetif)
{
	/* Setup pointers to RX structures */
	Chip_ENET_InitRxDescriptors(LPC_ETHERNET, lpc_enetif->prxd, lpc_enetif->prxs, LPC_NUM_BUFF_RXDESCS);

	lpc_enetif->rx_free_descs = LPC_NUM_BUFF_RXDESCS;
	lpc_enetif->rx_fill_desc_index = 0;

	/* Build RX buffer and descriptors */
	lpc_rx_queue(lpc_enetif->pnetif);

	return ERR_OK;
}

/* Allocates a pbuf and returns the data from the incoming packet */
STATIC struct pbuf *lpc_low_level_input(struct netif *netif) {
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *p = NULL;
	u32_t idx, length;

#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_enetif->rx_lock_mutex);
#endif
#endif

	/* Monitor RX overrun status. This should never happen unless
	   (possibly) the internal bus is behing held up by something.
	   Unless your system is running at a very low clock speed or
	   there are possibilities that the internal buses may be held
	   up for a long time, this can probably safely be removed. */
	if (Chip_ENET_GetIntStatus(LPC_ETHERNET) & ENET_INT_RXOVERRUN) {
		LINK_STATS_INC(link.err);
		LINK_STATS_INC(link.drop);

		/* Temporarily disable RX */
		Chip_ENET_RXDisable(LPC_ETHERNET);

		/* Reset the RX side */
		Chip_ENET_ResetRXLogic(LPC_ETHERNET);
		Chip_ENET_ClearIntStatus(LPC_ETHERNET, ENET_INT_RXOVERRUN);

		/* De-allocate all queued RX pbufs */
		for (idx = 0; idx < LPC_NUM_BUFF_RXDESCS; idx++) {
			if (lpc_enetif->rxb[idx] != NULL) {
				pbuf_free(lpc_enetif->rxb[idx]);
				lpc_enetif->rxb[idx] = NULL;
			}
		}

		/* Start RX side again */
		lpc_rx_setup(lpc_enetif);

		/* Re-enable RX */
		Chip_ENET_RXEnable(LPC_ETHERNET);

#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
		sys_mutex_unlock(&lpc_enetif->rx_lock_mutex);
#endif
#endif

		return NULL;
	}

	/* Determine if a frame has been received */
	length = 0;
	idx = Chip_ENET_GetRXConsumeIndex(LPC_ETHERNET);
	if (!Chip_ENET_IsRxEmpty(LPC_ETHERNET)) {
		/* Handle errors */
		if (lpc_enetif->prxs[idx].StatusInfo & (ENET_RINFO_CRC_ERR |
												ENET_RINFO_SYM_ERR | ENET_RINFO_ALIGN_ERR | ENET_RINFO_LEN_ERR)) {
#if LINK_STATS
			if (lpc_enetif->prxs[idx].StatusInfo & (ENET_RINFO_CRC_ERR |
													ENET_RINFO_SYM_ERR | ENET_RINFO_ALIGN_ERR)) {
				LINK_STATS_INC(link.chkerr);
			}
			if (lpc_enetif->prxs[idx].StatusInfo & ENET_RINFO_LEN_ERR) {
				LINK_STATS_INC(link.lenerr);
			}
#endif

			/* Drop the frame */
			LINK_STATS_INC(link.drop);

			/* Re-queue the pbuf for receive */
			lpc_enetif->rx_free_descs++;
			p = lpc_enetif->rxb[idx];
			lpc_enetif->rxb[idx] = NULL;
			lpc_rxqueue_pbuf(lpc_enetif, p);

			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_low_level_input: Packet dropped with errors (0x%x)\n",
						 lpc_enetif->prxs[idx].StatusInfo));

			p = NULL;
		}
		else {
			/* A packet is waiting, get length */
			length = ENET_RINFO_SIZE(lpc_enetif->prxs[idx].StatusInfo) - 4;	/* Remove FCS */

			/* Zero-copy */
			p = lpc_enetif->rxb[idx];
			p->len = (u16_t) length;

			/* Free pbuf from desriptor */
			lpc_enetif->rxb[idx] = NULL;
			lpc_enetif->rx_free_descs++;

			/* Queue new buffer(s) */
			if (lpc_rx_queue(lpc_enetif->pnetif) == 0) {

				/* Re-queue the pbuf for receive */
				lpc_rxqueue_pbuf(lpc_enetif, p);

				/* Drop the frame */
				LINK_STATS_INC(link.drop);

				LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
							("lpc_low_level_input: Packet dropped since it could not allocate Rx Buffer\n"));

				p = NULL;
			}
			else {

				LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
							("lpc_low_level_input: Packet received: %p, size %d (index=%d)\n",
							 p, length, idx));

				/* Save size */
				p->tot_len = (u16_t) length;
				LINK_STATS_INC(link.recv);
			}
		}

		/* Update Consume index */
		Chip_ENET_IncRXConsumeIndex(LPC_ETHERNET);
	}

#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
	sys_mutex_unlock(&lpc_enetif->rx_lock_mutex);
#endif
#endif

	return p;
}

/* Determine if the passed address is usable for the ethernet DMA controller */
STATIC s32_t lpc_packet_addr_notsafe(void *addr) {
#if defined(CHIP_LPC175X_6X)
	/* Check for legal address ranges */
	if ((((u32_t) addr >= 0x10000000) && ((u32_t) addr < 0x10008000)) /* 32kB local SRAM */
		|| (((u32_t) addr >= 0x1FFF0000) && ((u32_t) addr < 0x1FFF2000)) /* 8kB ROM */
		|| (((u32_t) addr >= 0x2007C000) && ((u32_t) addr < 0x20084000)) /* 32kB AHB SRAM */
		) {
		return 0;
	}
	return 1;
#else
	/* Check for legal address ranges */
	if ((((u32_t) addr >= 0x20000000) && ((u32_t) addr < 0x20008000)) ||
		(((u32_t) addr >= 0x80000000) && ((u32_t) addr < 0xF0000000))) {
		return 0;
	}
	return 1;
#endif	
}

/* Sets up the TX descriptor ring buffers */
STATIC err_t lpc_tx_setup(lpc_enetdata_t *lpc_enetif)
{
	s32_t idx;

	/* Build TX descriptors for local buffers */
	for (idx = 0; idx < LPC_NUM_BUFF_TXDESCS; idx++) {
		lpc_enetif->ptxd[idx].Control = 0;
		lpc_enetif->ptxs[idx].StatusInfo = 0xFFFFFFFF;
	}

	/* Setup pointers to TX structures */
	Chip_ENET_InitTxDescriptors(LPC_ETHERNET, lpc_enetif->ptxd, lpc_enetif->ptxs, LPC_NUM_BUFF_TXDESCS);

	lpc_enetif->lpc_last_tx_idx = 0;

	return ERR_OK;
}

/* Free TX buffers that are complete */
STATIC void lpc_tx_reclaim_st(lpc_enetdata_t *lpc_enetif, u32_t cidx)
{
#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_enetif->tx_lock_mutex);
#endif

	while (cidx != lpc_enetif->lpc_last_tx_idx) {
		if (lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx] != NULL) {
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_tx_reclaim_st: Freeing packet %p (index %d)\n",
						 lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx],
						 lpc_enetif->lpc_last_tx_idx));
			pbuf_free(lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx]);
			lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx] = NULL;
		}

#if NO_SYS == 0
		xSemaphoreGive(lpc_enetif->xtx_count_sem);
#endif
		lpc_enetif->lpc_last_tx_idx++;
		if (lpc_enetif->lpc_last_tx_idx >= LPC_NUM_BUFF_TXDESCS) {
			lpc_enetif->lpc_last_tx_idx = 0;
		}
	}

#if NO_SYS == 0
	/* Restore access */
	sys_mutex_unlock(&lpc_enetif->tx_lock_mutex);
#endif
}

/* Low level output of a packet. Never call this from an interrupt context,
 * as it may block until TX descriptors become available. */
STATIC err_t lpc_low_level_output(struct netif *netif, struct pbuf *p)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *q;

#if LPC_TX_PBUF_BOUNCE_EN == 1
	u8_t *dst;
	struct pbuf *np;
#endif
	u32_t idx;
	u32_t dn, notdmasafe = 0;

	/* Zero-copy TX buffers may be fragmented across mutliple payload
	   chains. Determine the number of descriptors needed for the
	   transfer. The pbuf chaining can be a mess! */
	dn = (u32_t) pbuf_clen(p);

	/* Test to make sure packet addresses are DMA safe. A DMA safe
	   address is once that uses external memory or periphheral RAM.
	   IRAM and FLASH are not safe! */
	for (q = p; q != NULL; q = q->next) {
		notdmasafe += lpc_packet_addr_notsafe(q->payload);
	}

#if LPC_TX_PBUF_BOUNCE_EN == 1
	/* If the pbuf is not DMA safe, a new bounce buffer (pbuf) will be
	   created that will be used instead. This requires an copy from the
	   non-safe DMA region to the new pbuf */
	if (notdmasafe) {
		/* Allocate a pbuf in DMA memory */
		np = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
		if (np == NULL) {
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_low_level_output: could not allocate TX pbuf\n"));
			return ERR_MEM;
		}

		/* This buffer better be contiguous! */
		LWIP_ASSERT("lpc_low_level_output: New transmit pbuf is chained",
					(pbuf_clen(np) == 1));

		/* Copy to DMA safe pbuf */
		dst = (u8_t *) np->payload;
		for (q = p; q != NULL; q = q->next) {
			/* Copy the buffer to the descriptor's buffer */
			MEMCPY(dst, (u8_t *) q->payload, q->len);
			dst += q->len;
		}
		np->len = p->tot_len;

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
					("lpc_low_level_output: Switched to DMA safe buffer, old=%p, new=%p\n",
					 q, np));

		/* use the new buffer for descrptor queueing. The original pbuf will
		   be de-allocated outsuide this driver. */
		p = np;
		dn = 1;
	}
#else
	if (notdmasafe) {
		LWIP_ASSERT("lpc_low_level_output: Not a DMA safe pbuf",
					(notdmasafe == 0));
	}
#endif

	/* Wait until enough descriptors are available for the transfer. */
	/* THIS WILL BLOCK UNTIL THERE ARE ENOUGH DESCRIPTORS AVAILABLE */
	while (dn > lpc_tx_ready(netif)) {
#if NO_SYS == 0
		xSemaphoreTake(lpc_enetif->xtx_count_sem, 0);
#else
		msDelay(1);
#endif
	}

	/* Get free TX buffer index */
	idx = Chip_ENET_GetTXProduceIndex(LPC_ETHERNET);

#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_enetif->tx_lock_mutex);
#endif

	/* Prevent LWIP from de-allocating this pbuf. The driver will
	   free it once it's been transmitted. */
	if (!notdmasafe) {
		pbuf_ref(p);
	}

	/* Setup transfers */
	q = p;
	while (dn > 0) {
		dn--;

		/* Only save pointer to free on last descriptor */
		if (dn == 0) {
			/* Save size of packet and signal it's ready */
			lpc_enetif->ptxd[idx].Control = ENET_TCTRL_SIZE(q->len) | ENET_TCTRL_INT |
											ENET_TCTRL_LAST;
			lpc_enetif->txb[idx] = p;
		}
		else {
			/* Save size of packet, descriptor is not last */
			lpc_enetif->ptxd[idx].Control = ENET_TCTRL_SIZE(q->len) | ENET_TCTRL_INT;
			lpc_enetif->txb[idx] = NULL;
		}

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
					("lpc_low_level_output: pbuf packet(%p) sent, chain#=%d,"
					 " size = %d (index=%d)\n", q->payload, dn, q->len, idx));

		lpc_enetif->ptxd[idx].Packet = (u32_t) q->payload;

		q = q->next;

		idx = Chip_ENET_IncTXProduceIndex(LPC_ETHERNET);
	}

	LINK_STATS_INC(link.xmit);

#if NO_SYS == 0
	/* Restore access */
	sys_mutex_unlock(&lpc_enetif->tx_lock_mutex);
#endif

	return ERR_OK;
}

#if NO_SYS == 0
/* Packet reception task for FreeRTOS */
STATIC portTASK_FUNCTION(vPacketReceiveTask, pvParameters)
{
	lpc_enetdata_t *lpc_enetif = pvParameters;

	while (1) {
		/* Wait for receive task to wakeup */
		sys_arch_sem_wait(&lpc_enetif->rx_sem, 0);

		/* Process packets until all empty */
		while (!Chip_ENET_IsRxEmpty(LPC_ETHERNET)) {
			lpc_enetif_input(lpc_enetif->pnetif);
		}
	}
}

/* Transmit cleanup task for FreeRTOS */
STATIC portTASK_FUNCTION(vTransmitCleanupTask, pvParameters)
{
	lpc_enetdata_t *lpc_enetif = pvParameters;
	s32_t idx;

	while (1) {
		/* Wait for transmit cleanup task to wakeup */
		sys_arch_sem_wait(&lpc_enetif->tx_clean_sem, 0);

		/* Error handling for TX underruns. This should never happen unless
		   something is holding the bus or the clocks are going too slow. It
		   can probably be safely removed. */
		if (Chip_ENET_GetIntStatus(LPC_ETHERNET) & ENET_INT_TXUNDERRUN) {
			LINK_STATS_INC(link.err);
			LINK_STATS_INC(link.drop);

#if NO_SYS == 0
			/* Get exclusive access */
			sys_mutex_lock(&lpc_enetif->tx_lock_mutex);
#endif
			/* Reset the TX side */
			Chip_ENET_ResetTXLogic(LPC_ETHERNET);
			Chip_ENET_ClearIntStatus(LPC_ETHERNET, ENET_INT_TXUNDERRUN);

			/* De-allocate all queued TX pbufs */
			for (idx = 0; idx < LPC_NUM_BUFF_TXDESCS; idx++) {
				if (lpc_enetif->txb[idx] != NULL) {
					pbuf_free(lpc_enetif->txb[idx]);
					lpc_enetif->txb[idx] = NULL;
				}
			}

#if NO_SYS == 0
			/* Restore access */
			sys_mutex_unlock(&lpc_enetif->tx_lock_mutex);
#endif
			/* Start TX side again */
			lpc_tx_setup(lpc_enetif);
		}
		else {
			/* Free TX buffers that are done sending */
			lpc_tx_reclaim(lpc_enetdata.pnetif);
		}
	}
}

#endif

/* Low level init of the MAC and PHY */
STATIC err_t low_level_init(struct netif *netif)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	err_t err = ERR_OK;

	Chip_ENET_Init(LPC_ETHERNET);

	/* Initialize the PHY */
	Chip_ENET_SetupMII(LPC_ETHERNET, Chip_ENET_FindMIIDiv(LPC_ETHERNET, 2500000), LPC_PHYDEF_PHYADDR);
#if defined(USE_RMII)
	if (lpc_phy_init(true, msDelay) != SUCCESS) {
		return ERROR;
	}
#else
	if (lpc_phy_init(false, msDelay) != SUCCESS) {
		return ERROR;
	}
#endif

	/* Save station address */
	Chip_ENET_SetADDR(LPC_ETHERNET, netif->hwaddr);

	/* Setup transmit and receive descriptors */
	if (lpc_tx_setup(lpc_enetif) != ERR_OK) {
		return ERR_BUF;
	}
	if (lpc_rx_setup(lpc_enetif) != ERR_OK) {
		return ERR_BUF;
	}

	/* Enable packet reception */
#if IP_SOF_BROADCAST_RECV
	Chip_ENET_EnableRXFilter(LPC_ETHERNET, ENET_RXFILTERCTRL_APE | ENET_RXFILTERCTRL_ABE);
#else
	Chip_ENET_EnableRXFilter(ENET_RXFILTERCTRL_APE);
#endif

	/* Clear and enable rx/tx interrupts */
	Chip_ENET_EnableInt(LPC_ETHERNET, RXINTGROUP | TXINTGROUP);

	/* Enable RX and TX */
	Chip_ENET_TXEnable(LPC_ETHERNET);
	Chip_ENET_RXEnable(LPC_ETHERNET);

	return err;
}

/* This function is the ethernet packet send function. It calls
 * etharp_output after checking link status. */
STATIC err_t lpc_etharp_output(struct netif *netif, struct pbuf *q,
							   ip_addr_t *ipaddr)
{
	/* Only send packet is link is up */
	if (netif->flags & NETIF_FLAG_LINK_UP) {
		return etharp_output(netif, q, ipaddr);
	}

	return ERR_CONN;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/
/* Attempt to allocate and requeue a new pbuf for RX */
s32_t lpc_rx_queue(struct netif *netif)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *p;

	s32_t queued = 0;

	/* Attempt to requeue as many packets as possible */
	while (lpc_enetif->rx_free_descs > 0) {
		/* Allocate a pbuf from the pool. We need to allocate at the
		   maximum size as we don't know the size of the yet to be
		   received packet. */
		p = pbuf_alloc(PBUF_RAW, (u16_t) ENET_ETH_MAX_FLEN, PBUF_RAM);
		if (p == NULL) {
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_rx_queue: could not allocate RX pbuf (free desc=%d)\n",
						 lpc_enetif->rx_free_descs));
			return queued;
		}

		/* pbufs allocated from the RAM pool should be non-chained. */
		LWIP_ASSERT("lpc_rx_queue: pbuf is not contiguous (chained)",
					pbuf_clen(p) <= 1);

		/* Queue packet */
		lpc_rxqueue_pbuf(lpc_enetif, p);

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
			LWIP_DEBUGF(NETIF_DEBUG, ("lpc_enetif_input: IP input error\n"));
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
	lpc_tx_reclaim_st((lpc_enetdata_t *) netif->state,
					  Chip_ENET_GetTXConsumeIndex(LPC_ETHERNET));
}

/* Polls if an available TX descriptor is ready */
s32_t lpc_tx_ready(struct netif *netif)
{
	u32_t pidx, cidx;

	cidx = Chip_ENET_GetTXConsumeIndex(LPC_ETHERNET);
	pidx = Chip_ENET_GetTXProduceIndex(LPC_ETHERNET);

	return Chip_ENET_GetFreeDescNum(LPC_ETHERNET, pidx, cidx, LPC_NUM_BUFF_TXDESCS);
}

/**
 * @brief	EMAC interrupt handler
 * @return	Nothing
 * @note	This function handles the transmit, receive, and error interrupt of
 * the LPC17xx/40xx. This is meant to be used when NO_SYS=0.
 */
void ETH_IRQHandler(void)
{
#if NO_SYS == 1
	/* Interrupts are not used without an RTOS */
	NVIC_DisableIRQ(ETHERNET_IRQn);
#else
	signed portBASE_TYPE xRecTaskWoken = pdFALSE, XTXTaskWoken = pdFALSE;
	uint32_t ints;

	/* Interrupts are of 2 groups - transmit or receive. Based on the
	   interrupt, kick off the receive or transmit (cleanup) task */

	/* Get pending interrupts */
	ints = Chip_ENET_GetIntStatus(LPC_ETHERNET);

	if (ints & RXINTGROUP) {
		/* RX group interrupt(s) */
		/* Give semaphore to wakeup RX receive task. Note the FreeRTOS
		   method is used instead of the LWIP arch method. */
		xSemaphoreGiveFromISR(lpc_enetdata.rx_sem, &xRecTaskWoken);
	}

	if (ints & TXINTGROUP) {
		/* TX group interrupt(s) */
		/* Give semaphore to wakeup TX cleanup task. Note the FreeRTOS
		   method is used instead of the LWIP arch method. */
		xSemaphoreGiveFromISR(lpc_enetdata.tx_clean_sem, &XTXTaskWoken);
	}

	/* Clear pending interrupts */
	Chip_ENET_ClearIntStatus(LPC_ETHERNET, ints);

	/* Context switch needed? */
	portEND_SWITCHING_ISR(xRecTaskWoken || XTXTaskWoken);
#endif
}

/* Set up the MAC interface duplex */
void lpc_emac_set_duplex(int full_duplex)
{
	if (full_duplex) {
		Chip_ENET_SetFullDuplex(LPC_ETHERNET);
	}
	else {
		Chip_ENET_SetHalfDuplex(LPC_ETHERNET);
	}
}

/* Set up the MAC interface speed */
void lpc_emac_set_speed(int mbs_100)
{
	if (mbs_100) {
		Chip_ENET_Set100Mbps(LPC_ETHERNET);
	}
	else {
		Chip_ENET_Set10Mbps(LPC_ETHERNET);
	}
}

/* LWIP 17xx/40xx EMAC initialization function */
err_t lpc_enetif_init(struct netif *netif)
{
	err_t err;

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	lpc_enetdata.pnetif = netif;

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
	lpc_enetdata.xtx_count_sem = xSemaphoreCreateCounting(LPC_NUM_BUFF_TXDESCS,
														  LPC_NUM_BUFF_TXDESCS);
	LWIP_ASSERT("xtx_count_sem creation error",
				(lpc_enetdata.xtx_count_sem != NULL));

	err = sys_mutex_new(&lpc_enetdata.tx_lock_mutex);
	LWIP_ASSERT("tx_lock_mutex creation error", (err == ERR_OK));

	err = sys_mutex_new(&lpc_enetdata.rx_lock_mutex);
	LWIP_ASSERT("rx_lock_mutex creation error", (err == ERR_OK));

	/* Packet receive task */
	err = sys_sem_new(&lpc_enetdata.rx_sem, 0);
	LWIP_ASSERT("rx_sem creation error", (err == ERR_OK));
	sys_thread_new("receive_thread", vPacketReceiveTask, netif->state,
				   DEFAULT_THREAD_STACKSIZE, tskRECPKT_PRIORITY);

	/* Transmit cleanup task */
	err = sys_sem_new(&lpc_enetdata.tx_clean_sem, 0);
	LWIP_ASSERT("tx_clean_sem creation error", (err == ERR_OK));
	sys_thread_new("txclean_thread", vTransmitCleanupTask, netif->state,
				   DEFAULT_THREAD_STACKSIZE, tskTXCLEAN_PRIORITY);
#endif

	return ERR_OK;
}

/**
 * @}
 */
