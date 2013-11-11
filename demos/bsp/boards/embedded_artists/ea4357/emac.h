#ifndef __EMAC_H
#define __EMAC_H


/* Configuration */

/* Interface Selection */
#define MII				0		// =0 RMII  -  =1 MII

/* MAC Configuration */
#define MYMAC_1         0x1EU            /* our ethernet (MAC) address        */
#define MYMAC_2         0x30U            /* (MUST be unique in LAN!)          */
#define MYMAC_3         0x6cU
#define MYMAC_4         0xa2U
#define MYMAC_5         0x45U
#define MYMAC_6         0x5eU


#define ETH_FRAG_SIZE		1536		
#define NUM_RX_DESC			3
#define NUM_TX_DESC			3

/* End of Configuration   */


/* EMAC Descriptors and Buffers located in 16K SRAM */
/*  Rx Descriptors   */
#define RX_DESC_BASE		0x20008000
#define RX_STAT_BASE		RX_DESC_BASE
#define RX_CTRL_BASE		(RX_STAT_BASE + 4)
#define RX_BUFADDR_BASE		(RX_CTRL_BASE + 4)
#define RX_NEXTDESC_BASE	(RX_BUFADDR_BASE + 4)
#define RX_BUF_BASE			(RX_DESC_BASE + NUM_RX_DESC*16)	 

#define RX_DESC_STAT(i)		(*(unsigned int *)(RX_STAT_BASE + 16*i))	 
#define RX_DESC_CTRL(i)		(*(unsigned int *)(RX_CTRL_BASE + 16*i))
#define RX_BUFADDR(i)		(*(unsigned int *)(RX_BUFADDR_BASE + 16*i))
#define RX_NEXTDESC(i)		(*(unsigned int *)(RX_NEXTDESC_BASE + 16*i))
#define RX_BUF(i)			(RX_BUF_BASE + ETH_FRAG_SIZE*i)

/*  Tx Descriptors   */
#define TX_DESC_BASE		RX_BUF_BASE + (ETH_FRAG_SIZE * NUM_RX_DESC)
#define TX_STAT_BASE		TX_DESC_BASE
#define TX_CTRL_BASE		(TX_STAT_BASE + 4)
#define TX_BUFADDR_BASE		(TX_CTRL_BASE + 4)
#define TX_NEXTDESC_BASE	(TX_BUFADDR_BASE + 4)
#define TX_BUF_BASE			(TX_DESC_BASE + NUM_TX_DESC*16)

#define TX_DESC_STAT(i)		(*(unsigned int *)(TX_STAT_BASE + 16*i))
#define TX_DESC_CTRL(i)		(*(unsigned int *)(TX_CTRL_BASE + 16*i))
#define TX_BUFADDR(i)		(*(unsigned int *)(TX_BUFADDR_BASE + 16*i))
#define TX_NEXTDESC(i)		(*(unsigned int *)(TX_NEXTDESC_BASE + 16*i))
#define TX_BUF(i)			(TX_BUF_BASE + ETH_FRAG_SIZE*i)		

/*  Descriptors Fields bits       */
#define OWN_BIT				(1U<<31)	/*  Own bit in RDES0 & TDES0              */
#define RX_END_RING			(1<<15)		/*  Receive End of Ring bit in RDES1      */
#define RX_NXTDESC_FLAG		(1<<14)		/*  Second Address Chained bit in RDES1   */
#define TX_LAST_SEGM		(1<<29)		/*  Last Segment bit in TDES0             */
#define TX_FIRST_SEGM		(1<<28)		/*  First Segment bit in TDES0            */
#define TX_END_RING			(1<<21)		/*  Transmit End of Ring bit in TDES0     */
#define TX_NXTDESC_FLAG		(1<<20)		/*  Second Address Chained bit in TDES0   */





/* EMAC Control and Status bits   */
#define MAC_RX_ENABLE	 (1<<2)			/*  Receiver Enable in MAC_CONFIG reg      */
#define MAC_TX_ENABLE	 (1<<3)			/*  Transmitter Enable in MAC_CONFIG reg   */
#define MAC_PADCRC_STRIP (1<<7)			/*  Automatic Pad-CRC Stripping in MAC_CONFIG reg   */
#define MAC_DUPMODE		 (1<<11)		/*  Duplex Mode in  MAC_CONFIG reg         */
#define MAC_100MPS		 (1<<14)		/*  Speed is 100Mbps in MAC_CONFIG reg     */
#define MAC_PROMISCUOUS  (1U<<0)		/*  Promiscuous Mode bit in MAC_FRAME_FILTER reg    */
#define MAC_DIS_BROAD    (1U<<5)		/*  Disable Broadcast Frames bit in	MAC_FRAME_FILTER reg    */
#define MAC_RECEIVEALL   (1U<<31)       /*  Receive All bit in MAC_FRAME_FILTER reg    */
#define DMA_SOFT_RESET	  0x01          /*  Software Reset bit in DMA_BUS_MODE reg */
#define DMA_SS_RECEIVE   (1<<1)         /*  Start/Stop Receive bit in DMA_OP_MODE reg  */
#define DMA_SS_TRANSMIT  (1<<13)        /*  Start/Stop Transmission bit in DMA_OP_MODE reg  */
#define DMA_INT_TRANSMIT (1<<0)         /*  Transmit Interrupt Enable bit in DMA_INT_EN reg */
#define DMA_INT_OVERFLOW (1<<4)         /*  Overflow Interrupt Enable bit in DMA_INT_EN reg */ 
#define DMA_INT_UNDERFLW (1<<5)         /*  Underflow Interrupt Enable bit in DMA_INT_EN reg */
#define DMA_INT_RECEIVE  (1<<6)         /*  Receive Interrupt Enable bit in DMA_INT_EN reg */
#define DMA_INT_ABN_SUM  (1<<15)        /*  Abnormal Interrupt Summary Enable bit in DMA_INT_EN reg */
#define DMA_INT_NOR_SUM  (1<<16)        /*  Normal Interrupt Summary Enable bit in DMA_INT_EN reg */

/* MII Management Command Register */
#define GMII_READ           (0<<1)		/* GMII Read PHY                     */
#define GMII_WRITE          (1<<1)      /* GMII Write PHY                    */
#define GMII_BUSY           0x00000001  /* GMII is Busy / Start Read/Write   */
#define MII_WR_TOUT         0x00050000  /* MII Write timeout count           */
#define MII_RD_TOUT         0x00050000  /* MII Read timeout count            */

/* MII Management Address Register */
#define MADR_PHY_ADR        0x00001F00  /* PHY Address Mask                  */

/* DP83848C PHY Registers */
#define PHY_REG_BMCR        0x00        /* Basic Mode Control Register       */
#define PHY_REG_BMSR        0x01        /* Basic Mode Status Register        */
#define PHY_REG_IDR1        0x02        /* PHY Identifier 1                  */
#define PHY_REG_IDR2        0x03        /* PHY Identifier 2                  */
#define PHY_REG_ANAR        0x04        /* Auto-Negotiation Advertisement    */
#define PHY_REG_ANLPAR      0x05        /* Auto-Neg. Link Partner Abitily    */
#define PHY_REG_ANER        0x06        /* Auto-Neg. Expansion Register      */
#define PHY_REG_ANNPTR      0x07        /* Auto-Neg. Next Page TX            */

/* PHY Extended Registers */
#define PHY_REG_STS         0x10        /* Status Register                   */
#define PHY_REG_MICR        0x11        /* MII Interrupt Control Register    */
#define PHY_REG_MISR        0x12        /* MII Interrupt Status Register     */
#define PHY_REG_FCSCR       0x14        /* False Carrier Sense Counter       */
#define PHY_REG_RECR        0x15        /* Receive Error Counter             */
#define PHY_REG_PCSR        0x16        /* PCS Sublayer Config. and Status   */
#define PHY_REG_RBR         0x17        /* RMII and Bypass Register          */
#define PHY_REG_LEDCR       0x18        /* LED Direct Control Register       */
#define PHY_REG_PHYCR       0x19        /* PHY Control Register              */
#define PHY_REG_10BTSCR     0x1A        /* 10Base-T Status/Control Register  */
#define PHY_REG_CDCTRL1     0x1B        /* CD Test Control and BIST Extens.  */
#define PHY_REG_EDCR        0x1D        /* Energy Detect Control Register    */

/* PHY Control and Status bits  */
#define PHY_FULLD_100M      0x2100      /* Full Duplex 100Mbit               */
#define PHY_HALFD_100M      0x2000      /* Half Duplex 100Mbit               */
#define PHY_FULLD_10M       0x0100      /* Full Duplex 10Mbit                */
#define PHY_HALFD_10M       0x0000      /* Half Duplex 10MBit                */
#define PHY_AUTO_NEG        0x1000      /* Select Auto Negotiation           */
#define PHY_AUTO_NEG_DONE   0x0020		/* AutoNegotiation Complete in BMSR PHY reg  */
#define PHY_BMCR_RESET		0x8000		/* Reset bit at BMCR PHY reg         */
#define LINK_VALID_STS		0x0001		/* Link Valid Status at REG_STS PHY reg	 */
#define FULL_DUP_STS		0x0004		/* Full Duplex Status at REG_STS PHY reg */
#define SPEED_10M_STS		0x0002		/* 10Mbps Status at REG_STS PHY reg */

#define DP83848C_DEF_ADR    0x01        /* Default PHY device address        */
#define DP83848C_ID         0x20005C90  /* PHY Identifier (without Rev. info */

#define LAN8720_ID          0x0007C0F0  /* PHY Identifier                    */
#define PHY_REG_SCSR		0x1F		/* PHY Special Control/Status Register */

/*  Misc    */
#define ETHERNET_RST		22			/* 	Reset Output for EMAC at RGU     */
#define RMII_SELECT			0x04		/*  Select RMII in EMACCFG           */


/*  Prototypes               */
void           Init_EMAC(void);
unsigned short ReadFrameBE_EMAC(void);
void           CopyToFrame_EMAC(void *Source, unsigned int Size);
void           CopyFromFrame_EMAC(void *Dest, unsigned short Size);
void           DummyReadFrame_EMAC(unsigned short Size);
unsigned short StartReadFrame(void);
void           EndReadFrame(void);
unsigned int   CheckFrameReceived(void);
void           RequestSend(unsigned short FrameSize);
unsigned int   Rdy4Tx(void);


#endif
