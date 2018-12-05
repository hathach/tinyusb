/*
 * @brief LPC17xx/40xx Ethernet driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2014
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

#ifndef __ENET_17XX_40XX_H_
#define __ENET_17XX_40XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup ENET_17XX_40XX CHIP: LPC17xx/40xx Ethernet driver (2)
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

/**
 * @brief Ethernet MAC register block structure
 */
typedef struct {
	__IO uint32_t MAC1;			/*!< MAC Configuration register 1 */
	__IO uint32_t MAC2;			/*!< MAC Configuration register 2 */
	__IO uint32_t IPGT;			/*!< Back-to-Back Inter-Packet-Gap register */
	__IO uint32_t IPGR;			/*!< Non Back-to-Back Inter-Packet-Gap register */
	__IO uint32_t CLRT;			/*!< Collision window / Retry register */
	__IO uint32_t MAXF;			/*!< Maximum Frame register */
	__IO uint32_t SUPP;			/*!< PHY Support register */
	__IO uint32_t TEST;			/*!< Test register */
	__IO uint32_t MCFG;			/*!< MII Mgmt Configuration register */
	__IO uint32_t MCMD;			/*!< MII Mgmt Command register */
	__IO uint32_t MADR;			/*!< MII Mgmt Address register */
	__O  uint32_t MWTD;			/*!< MII Mgmt Write Data register */
	__I  uint32_t MRDD;			/*!< MII Mgmt Read Data register */
	__I  uint32_t MIND;			/*!< MII Mgmt Indicators register */
	uint32_t RESERVED0[2];
	__IO uint32_t SA[3];		/*!< Station Address registers */
} ENET_MAC_T;

/**
 * @brief Ethernet Transfer register Block Structure
 */
typedef struct {
	__IO uint32_t DESCRIPTOR;		/*!< Descriptor base address register */
	__IO uint32_t STATUS;			/*!< Status base address register */
	__IO uint32_t DESCRIPTORNUMBER;	/*!< Number of descriptors register */
	__IO uint32_t PRODUCEINDEX;		/*!< Produce index register */
	__IO uint32_t CONSUMEINDEX;		/*!< Consume index register */
} ENET_TRANSFER_INFO_T;

/**
 * @brief Ethernet Control register block structure
 */
typedef struct {
	__IO uint32_t COMMAND;				/*!< Command register */
	__I  uint32_t STATUS;				/*!< Status register */
	ENET_TRANSFER_INFO_T RX;	/*!< Receive block registers */
	ENET_TRANSFER_INFO_T TX;	/*!< Transmit block registers */
	uint32_t RESERVED0[10];
	__I  uint32_t TSV0;					/*!< Transmit status vector 0 register */
	__I  uint32_t TSV1;					/*!< Transmit status vector 1 register */
	__I  uint32_t RSV;					/*!< Receive status vector register */
	uint32_t RESERVED1[3];
	__IO uint32_t FLOWCONTROLCOUNTER;	/*!< Flow control counter register */
	__I  uint32_t FLOWCONTROLSTATUS;	/*!< Flow control status register */
} ENET_CONTROL_T;

/**
 * @brief Ethernet Receive Filter register block structure
 */
typedef struct {
	__IO uint32_t CONTROL;			/*!< Receive filter control register */
	__I  uint32_t WOLSTATUS;		/*!< Receive filter WoL status register */
	__O  uint32_t WOLCLEAR;			/*!< Receive filter WoL clear register */
	uint32_t RESERVED;
	__IO uint32_t HashFilterL;		/*!< Hash filter table LSBs register */
	__IO uint32_t HashFilterH;		/*!< Hash filter table MSBs register */
} ENET_RXFILTER_T;

/**
 * @brief Ethernet Module Control register block structure
 */
typedef struct {
	__I  uint32_t INTSTATUS;		/*!< Interrupt status register */
	__IO uint32_t INTENABLE;		/*!< Interrupt enable register */
	__O  uint32_t INTCLEAR;			/*!< Interrupt clear register */
	__O  uint32_t INTSET;			/*!< Interrupt set register */
	uint32_t RESERVED;
	__IO uint32_t POWERDOWN;		/*!< Power-down register */
} ENET_MODULE_CTRL_T;

/**
 * @brief Ethernet register block structure
 */
typedef struct {
	ENET_MAC_T  MAC;				/*!< MAC registers */
	uint32_t RESERVED1[45];
	ENET_CONTROL_T CONTROL;		/*!< Control registers */
	uint32_t RESERVED4[34];
	ENET_RXFILTER_T RXFILTER;		/*!< RxFilter registers */
	uint32_t RESERVED6[882];
	ENET_MODULE_CTRL_T MODULE_CONTROL;	/*!< Module Control registers */
} LPC_ENET_T;

/*
 * @brief MAC Configuration Register 1  bit definitions
 */
#define ENET_MAC1_MASK          0xcf1f		/*!< MAC1 register mask */
#define ENET_MAC1_RXENABLE      0x00000001	/*!< Receive Enable */
#define ENET_MAC1_PARF          0x00000002	/*!< Pass All Receive Frames */
#define ENET_MAC1_RXFLOWCTRL    0x00000004	/*!< RX Flow Control */
#define ENET_MAC1_TXFLOWCTRL    0x00000008	/*!< TX Flow Control */
#define ENET_MAC1_LOOPBACK      0x00000010	/*!< Loop Back Mode */
#define ENET_MAC1_RESETTX       0x00000100	/*!< Reset TX Logic */
#define ENET_MAC1_RESETMCSTX    0x00000200	/*!< Reset MAC TX Control Sublayer */
#define ENET_MAC1_RESETRX       0x00000400	/*!< Reset RX Logic */
#define ENET_MAC1_RESETMCSRX    0x00000800	/*!< Reset MAC RX Control Sublayer */
#define ENET_MAC1_SIMRESET      0x00004000	/*!< Simulation Reset */
#define ENET_MAC1_SOFTRESET     0x00008000	/*!< Soft Reset MAC */

/*
 * @brief MAC Configuration Register 2 bit definitions
 */
#define ENET_MAC2_MASK          0x73ff		/*!< MAC2 register mask */
#define ENET_MAC2_FULLDUPLEX    0x00000001	/*!< Full-Duplex Mode */
#define ENET_MAC2_FLC           0x00000002	/*!< Frame Length Checking */
#define ENET_MAC2_HFEN          0x00000004	/*!< Huge Frame Enable */
#define ENET_MAC2_DELAYEDCRC    0x00000008	/*!< Delayed CRC Mode */
#define ENET_MAC2_CRCEN         0x00000010	/*!< Append CRC to every Frame */
#define ENET_MAC2_PADCRCEN      0x00000020	/*!< Pad all Short Frames */
#define ENET_MAC2_VLANPADEN     0x00000040	/*!< VLAN Pad Enable */
#define ENET_MAC2_AUTODETPADEN  0x00000080	/*!< Auto Detect Pad Enable */
#define ENET_MAC2_PPENF         0x00000100	/*!< Pure Preamble Enforcement */
#define ENET_MAC2_LPENF         0x00000200	/*!< Long Preamble Enforcement */
#define ENET_MAC2_NOBACKOFF     0x00001000	/*!< No Backoff Algorithm */
#define ENET_MAC2_BP_NOBACKOFF  0x00002000	/*!< Backoff Presurre / No Backoff */
#define ENET_MAC2_EXCESSDEFER   0x00004000	/*!< Excess Defer */

/*
 * @brief Back-to-Back Inter-Packet-Gap Register bit definitions
 */
/** Programmable field representing the nibble time offset of the minimum possible period
 * between the end of any transmitted packet to the beginning of the next */
#define ENET_IPGT_BTOBINTEGAP(n) ((n) & 0x7F)

/** Recommended value for Full Duplex of Programmable field representing the nibble time
 * offset of the minimum possible period between the end of any transmitted packet to the
 * beginning of the next */
#define ENET_IPGT_FULLDUPLEX (ENET_IPGT_BTOBINTEGAP(0x15))

/** Recommended value for Half Duplex of Programmable field representing the nibble time
 * offset of the minimum possible period between the end of any transmitted packet to the
 * beginning of the next */
#define ENET_IPGT_HALFDUPLEX (ENET_IPGT_BTOBINTEGAP(0x12))

/*
 * @brief Non Back-to-Back Inter-Packet-Gap Register bit definitions
 */

/** Programmable field representing the Non-Back-to-Back Inter-Packet-Gap */
#define ENET_IPGR_NBTOBINTEGAP2(n) ((n) & 0x7F)

/** Recommended value for Programmable field representing the Non-Back-to-Back Inter-Packet-Gap Part 1 */
#define ENET_IPGR_P2_DEF (ENET_IPGR_NBTOBINTEGAP2(0x12))

/** Programmable field representing the optional carrierSense window referenced in
 * IEEE 802.3/4.2.3.2.1 'Carrier Deference' */
#define ENET_IPGR_NBTOBINTEGAP1(n) (((n) & 0x7F) << 8)

/** Recommended value for Programmable field representing the Non-Back-to-Back Inter-Packet-Gap Part 2 */
#define ENET_IPGR_P1_DEF ENET_IPGR_NBTOBINTEGAP1(0x0C)

/*
 * @brief Collision Window/Retry Register bit definitions
 */
/** Programmable field specifying the number of retransmission attempts following a collision before
 * aborting the packet due to excessive collisions */
#define ENET_CLRT_RETRANSMAX(n) ((n) & 0x0F)

/** Programmable field representing the slot time or collision window during which collisions occur
 * in properly configured networks */
#define ENET_CLRT_COLLWIN(n) (((n) & 0x3F) << 8)

/** Default value for Collision Window / Retry register */
#define ENET_CLRT_DEF ((ENET_CLRT_RETRANSMAX(0x0F)) | (ENET_CLRT_COLLWIN(0x37)))

/*
 * @brief Maximum Frame Register bit definitions
 */
/** Represents a maximum receive frame of 1536 octets */
#define ENET_MAXF_MAXFLEN(n) ((n) & 0xFFFF)
#define ENET_MAXF_MAXFLEN_DEF (0x600)

/* PHY Support Register */
#define ENET_SUPP_100Mbps_SPEED 0x00000100		/*!< Reduced MII Logic Current Speed */

/*
 * @brief Test Register bit definitions
 */
#define ENET_TEST_SCPQ          0x00000001		/*!< Shortcut Pause Quanta */
#define ENET_TEST_TESTPAUSE     0x00000002		/*!< Test Pause */
#define ENET_TEST_TESTBP        0x00000004		/*!< Test Back Pressure */

/*
 * @brief MII Management Configuration Register bit definitions
 */
#define ENET_MCFG_SCANINC       0x00000001		/*!< Scan Increment PHY Address */
#define ENET_MCFG_SUPPPREAMBLE  0x00000002		/*!< Suppress Preamble */
#define ENET_MCFG_CLOCKSEL(n)   (((n) & 0x0F) << 2)	/*!< Clock Select Field */
#define ENET_MCFG_RES_MII       0x00008000		/*!< Reset MII Management Hardware */
#define ENET_MCFG_RESETMIIMGMT  2500000UL		/*!< MII Clock max */

/*
 * @brief MII Management Command Register bit definitions
 */
#define ENET_MCMD_READ          0x00000001		/*!< MII Read */
#define ENET_MCMD_SCAN          0x00000002		/*!< MII Scan continuously */
#define ENET_MII_WR_TOUT        0x00050000		/*!< MII Write timeout count */
#define ENET_MII_RD_TOUT        0x00050000		/*!< MII Read timeout count */

/*
 * @brief MII Management Address Register bit definitions
 */
#define ENET_MADR_REGADDR(n)    ((n) & 0x1F)		/*!< MII Register Address field */
#define ENET_MADR_PHYADDR(n)    (((n) & 0x1F) << 8)	/*!< PHY Address Field */

/*
 * @brief MII Management Write Data Register bit definitions
 */
#define ENET_MWTD_DATA(n)       ((n) & 0xFFFF)		/*!< Data field for MMI Management Write Data register */

/**
 * @brief MII Management Read Data Register bit definitions
 */
#define ENET_MRDD_DATA(n)       ((n) & 0xFFFF)		/*!< Data field for MMI Management Read Data register */

/*
 * @brief MII Management Indicators Register bit definitions
 */
#define ENET_MIND_BUSY          0x00000001		/*!< MII is Busy */
#define ENET_MIND_SCANNING      0x00000002		/*!< MII Scanning in Progress */
#define ENET_MIND_NOTVALID      0x00000004		/*!< MII Read Data not valid */
#define ENET_MIND_MIILINKFAIL   0x00000008		/*!< MII Link Failed */

/*
 * @brief Command Register bit definitions
 */
#define ENET_COMMAND_RXENABLE           0x00000001		/*!< Enable Receive */
#define ENET_COMMAND_TXENABLE           0x00000002		/*!< Enable Transmit */
#define ENET_COMMAND_REGRESET           0x00000008		/*!< Reset Host Registers */
#define ENET_COMMAND_TXRESET            0x00000010		/*!< Reset Transmit Datapath */
#define ENET_COMMAND_RXRESET            0x00000020		/*!< Reset Receive Datapath */
#define ENET_COMMAND_PASSRUNTFRAME      0x00000040		/*!< Pass Runt Frames */
#define ENET_COMMAND_PASSRXFILTER       0x00000080		/*!< Pass RX Filter */
#define ENET_COMMAND_TXFLOWCONTROL      0x00000100		/*!< TX Flow Control */
#define ENET_COMMAND_RMII               0x00000200		/*!< Reduced MII Interface */
#define ENET_COMMAND_FULLDUPLEX         0x00000400		/*!< Full Duplex */

/*
 * @brief Status Register bit definitions
 */
#define ENET_STATUS_RXSTATUS        0x00000001		/*!< Receive Channel Active Status */
#define ENET_STATUS_TXSTATUS        0x00000002		/*!< Transmit Channel Active Status */

/*
 * @brief Transmit Status Vector 0 Register bit definitions
 */
#define ENET_TSV0_CRCERR        0x00000001	/*!< CRC error */
#define ENET_TSV0_LCE           0x00000002	/*!< Length Check Error */
#define ENET_TSV0_LOR           0x00000004	/*!< Length Out of Range */
#define ENET_TSV0_DONE          0x00000008	/*!< Tramsmission Completed  */
#define ENET_TSV0_MULTICAST     0x00000010	/*!< Multicast Destination */
#define ENET_TSV0_BROADCAST     0x00000020	/*!< Broadcast Destination */
#define ENET_TSV0_PACKETDEFER   0x00000040	/*!< Packet Deferred */
#define ENET_TSV0_EXDF          0x00000080	/*!< Excessive Packet Deferral */
#define ENET_TSV0_EXCOL         0x00000100	/*!< Excessive Collision */
#define ENET_TSV0_LCOL          0x00000200	/*!< Late Collision Occured  */
#define ENET_TSV0_GIANT         0x00000400	/*!< Giant Frame */
#define ENET_TSV0_UNDERRUN      0x00000800	/*!< Buffer Underrun */
#define ENET_TSV0_TOTALBYTES    0x0FFFF000	/*!< Total Bytes Transferred  */
#define ENET_TSV0_CONTROLFRAME  0x10000000	/*!< Control Frame */
#define ENET_TSV0_PAUSE         0x20000000	/*!< Pause Frame */
#define ENET_TSV0_BACKPRESSURE  0x40000000	/*!< Backpressure Method Applied */
#define ENET_TSV0_VLAN          0x80000000	/*!< VLAN Frame */

/*
 * @brief Transmit Status Vector 0 Register bit definitions
 */
#define ENET_TSV1_TBC       0x0000FFFF	/*!< Transmit Byte Count */
#define ENET_TSV1_TCC       0x000F0000	/*!< Transmit Collision Count */

/*
 * @brief Receive Status Vector Register bit definitions
 */
#define ENET_RSV_RBC            0x0000FFFF	/*!< Receive Byte Count */
#define ENET_RSV_PPI            0x00010000	/*!< Packet Previously Ignored */
#define ENET_RSV_RXDVSEEN       0x00020000	/*!< RXDV Event Previously Seen */
#define ENET_RSV_CESEEN         0x00040000	/*!< Carrier Event Previously Seen */
#define ENET_RSV_RCV            0x00080000	/*!< Receive Code Violation */
#define ENET_RSV_CRCERR         0x00100000	/*!< CRC Error */
#define ENET_RSV_LCERR          0x00200000	/*!< Length Check Error */
#define ENET_RSV_LOR            0x00400000	/*!< Length Out of Range */
#define ENET_RSV_ROK            0x00800000	/*!< Frame Received OK */
#define ENET_RSV_MULTICAST      0x01000000	/*!< Multicast Frame */
#define ENET_RSV_BROADCAST      0x02000000	/*!< Broadcast Frame */
#define ENET_RSV_DRIBBLENIBBLE  0x04000000	/*!< Dribble Nibble */
#define ENET_RSV_CONTROLFRAME   0x08000000	/*!< Control Frame */
#define ENET_RSV_PAUSE          0x10000000	/*!< Pause Frame */
#define ENET_RSV_UO             0x20000000	/*!< Unsupported Opcode */
#define ENET_RSV_VLAN           0x40000000	/*!< VLAN Frame */

/*
 * @brief Flow Control Counter Register bit definitions
 */
#define ENET_FLOWCONTROLCOUNTER_MC(n)   ((n) & 0xFFFF)			/*!< Mirror Counter */
#define ENET_FLOWCONTROLCOUNTER_PT(n)   (((n) & 0xFFFF) << 16)	/*!< Pause Timer */

/*
 * @brief Flow Control Status Register bit definitions
 */
#define ENET_FLOWCONTROLSTATUS_MCC(n) ((n) & 0xFFFF)	/*!< Mirror Counter Current            */

/*
 * @brief Receive Filter Control Register bit definitions
 */
#define ENET_RXFILTERCTRL_AUE       0x00000001	/*!< Accept Unicast Frames Enable */
#define ENET_RXFILTERCTRL_ABE       0x00000002	/*!< Accept Broadcast Frames Enable */
#define ENET_RXFILTERCTRL_AME       0x00000004	/*!< Accept Multicast Frames Enable */
#define ENET_RXFILTERCTRL_AUHE      0x00000008	/*!< Accept Unicast Hash Filter Frames */
#define ENET_RXFILTERCTRL_AMHE      0x00000010	/*!< Accept Multicast Hash Filter Fram */
#define ENET_RXFILTERCTRL_APE       0x00000020	/*!< Accept Perfect Match Enable */
#define ENET_RXFILTERCTRL_MPEW      0x00001000	/*!< Magic Packet Filter WoL Enable */
#define ENET_RXFILTERCTRL_RFEW      0x00002000	/*!< Perfect Filter WoL Enable */

/*
 * @brief Receive Filter WoL Status/Clear Register bit definitions
 */
#define ENET_RXFILTERWOLSTATUS_AUW          0x00000001	/*!< Unicast Frame caused WoL */
#define ENET_RXFILTERWOLSTATUS_ABW          0x00000002	/*!< Broadcast Frame caused WoL */
#define ENET_RXFILTERWOLSTATUS_AMW          0x00000004	/*!< Multicast Frame caused WoL */
#define ENET_RXFILTERWOLSTATUS_AUHW         0x00000008	/*!< Unicast Hash Filter Frame WoL */
#define ENET_RXFILTERWOLSTATUS_AMHW         0x00000010	/*!< Multicast Hash Filter Frame WoL */
#define ENET_RXFILTERWOLSTATUS_APW          0x00000020	/*!< Perfect Filter WoL */
#define ENET_RXFILTERWOLSTATUS_RFW          0x00000080	/*!< RX Filter caused WoL */
#define ENET_RXFILTERWOLSTATUS_MPW          0x00000100	/*!< Magic Packet Filter caused WoL */
#define ENET_RXFILTERWOLSTATUS_BITMASK      0x01BF		/*!< Receive Filter WoL Status/Clear bitmasl value */

/*
 * @brief Interrupt Status/Enable/Clear/Set Register bit definitions
 */
#define ENET_INT_RXOVERRUN      0x00000001	/*!< Overrun Error in RX Queue */
#define ENET_INT_RXERROR        0x00000002	/*!< Receive Error */
#define ENET_INT_RXFINISHED     0x00000004	/*!< RX Finished Process Descriptors */
#define ENET_INT_RXDONE         0x00000008	/*!< Receive Done */
#define ENET_INT_TXUNDERRUN     0x00000010	/*!< Transmit Underrun */
#define ENET_INT_TXERROR        0x00000020	/*!< Transmit Error */
#define ENET_INT_TXFINISHED     0x00000040	/*!< TX Finished Process Descriptors */
#define ENET_INT_TXDONE         0x00000080	/*!< Transmit Done */
#define ENET_INT_SOFT           0x00001000	/*!< Software Triggered Interrupt */
#define ENET_INT_WAKEUP         0x00002000	/*!< Wakeup Event Interrupt */

/*
 * @brief Power Down Register bit definitions
 */
#define ENET_POWERDOWN_PD 0x80000000	/*!< Power Down MAC */

/**
 * @brief RX Descriptor structure
 */
typedef struct {
	uint32_t Packet;		/*!< Base address of the data buffer for storing receive data */
	uint32_t Control;		/*!< Control information */
} ENET_RXDESC_T;

/**
 * @brief RX Descriptor Control structure type definition
 */
#define ENET_RCTRL_SIZE(n)       (((n) - 1) & 0x7FF)	/*!< Buffer size field */
#define ENET_RCTRL_INT           0x80000000				/*!< Generate RxDone Interrupt */

/**
 * @brief RX Status structure
 */
typedef struct {
	uint32_t StatusInfo;		/*!< Receive status return flags.*/
	uint32_t StatusHashCRC;		/*!< The concatenation of the destination address hash CRC and the source
								   address hash CRC */
} ENET_RXSTAT_T;

/*
 * @brief RX Status Hash CRC Word definition
 */
#define ENET_RHASH_SA            0x000001FF				/*!< Hash CRC for Source Address */
#define ENET_RHASH_DA            0x001FF000				/*!< Hash CRC for Destination Address */

/* RX Status Information Word */
#define ENET_RINFO_SIZE(n)       (((n) & 0x7FF) + 1)	/*!< Data size in bytes */
#define ENET_RINFO_CTRL_FRAME    0x00040000				/*!< Control Frame */
#define ENET_RINFO_VLAN          0x00080000				/*!< VLAN Frame */
#define ENET_RINFO_FAIL_FILT     0x00100000				/*!< RX Filter Failed */
#define ENET_RINFO_MCAST         0x00200000				/*!< Multicast Frame */
#define ENET_RINFO_BCAST         0x00400000				/*!< Broadcast Frame */
#define ENET_RINFO_CRC_ERR       0x00800000				/*!< CRC Error in Frame */
#define ENET_RINFO_SYM_ERR       0x01000000				/*!< Symbol Error from PHY */
#define ENET_RINFO_LEN_ERR       0x02000000				/*!< Length Error */
#define ENET_RINFO_RANGE_ERR     0x04000000				/*!< Range Error (exceeded max. size) */
#define ENET_RINFO_ALIGN_ERR     0x08000000				/*!< Alignment Error */
#define ENET_RINFO_OVERRUN       0x10000000				/*!< Receive overrun */
#define ENET_RINFO_NO_DESCR      0x20000000				/*!< No new Descriptor available */
#define ENET_RINFO_LAST_FLAG     0x40000000				/*!< Last Fragment in Frame */
#define ENET_RINFO_ERR           0x80000000				/*!< Error Occured (OR of all errors) */
/** RX Error status mask */
#define ENET_RINFO_ERR_MASK     (ENET_RINFO_FAIL_FILT | ENET_RINFO_CRC_ERR   | ENET_RINFO_SYM_ERR |	\
								 ENET_RINFO_LEN_ERR   | ENET_RINFO_ALIGN_ERR | ENET_RINFO_OVERRUN)

/**
 * @brief TX Descriptor structure
 */
typedef struct {
	uint32_t Packet;	/*!< Base address of the data buffer containing transmit data */
	uint32_t Control;	/*!< Control information */
} ENET_TXDESC_T;

/*
 * @brief TX Descriptor Control structure type definition
 */
#define ENET_TCTRL_SIZE(n)       (((n) - 1) & 0x7FF)	/*!< Size of data buffer in bytes */
#define ENET_TCTRL_OVERRIDE      0x04000000				/*!< Override Default MAC Registers */
#define ENET_TCTRL_HUGE          0x08000000				/*!< Enable Huge Frame */
#define ENET_TCTRL_PAD           0x10000000				/*!< Pad short Frames to 64 bytes */
#define ENET_TCTRL_CRC           0x20000000				/*!< Append a hardware CRC to Frame */
#define ENET_TCTRL_LAST          0x40000000				/*!< Last Descriptor for TX Frame */
#define ENET_TCTRL_INT           0x80000000				/*!< Generate TxDone Interrupt */

/**
 * @brief TX Status structure
 */
typedef struct {
	uint32_t StatusInfo;	/*!< Receive status return flags.*/
} ENET_TXSTAT_T;

/* TX Status Information Word */
#define ENET_TINFO_COL_CNT       0x01E00000		/*!< Collision Count */
#define ENET_TINFO_DEFER         0x02000000		/*!< Packet Deferred (not an error) */
#define ENET_TINFO_EXCESS_DEF    0x04000000		/*!< Excessive Deferral */
#define ENET_TINFO_EXCESS_COL    0x08000000		/*!< Excessive Collision */
#define ENET_TINFO_LATE_COL      0x10000000		/*!< Late Collision Occured */
#define ENET_TINFO_UNDERRUN      0x20000000		/*!< Transmit Underrun */
#define ENET_TINFO_NO_DESCR      0x40000000		/*!< No new Descriptor available */
#define ENET_TINFO_ERR           0x80000000		/*!< Error Occured (OR of all errors) */

/**
 * @brief Maximum size of an ethernet buffer
 */
#define ENET_ETH_MAX_FLEN (1536)

/**
 * @brief ENET Buffer status definition
 */
typedef enum {
	ENET_BUFF_EMPTY,			/* buffer is empty */
	ENET_BUFF_PARTIAL_FULL,		/* buffer contains some packets */
	ENET_BUFF_FULL,				/* buffer is full */
} ENET_BUFF_STATUS_T;

/**
 * @brief	Resets the ethernet interface
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 * @note	Resets the ethernet interface. This should be called prior to
 * Chip_ENET_Init with a small delay after this call.
 */
STATIC INLINE void Chip_ENET_Reset(LPC_ENET_T *pENET)
{
	/* This should be called prior to IP_ENET_Init. The MAC controller may
	   not be ready for a call to init right away so a small delay should
	   occur after this call. */
	pENET->MAC.MAC1 = ENET_MAC1_RESETTX | ENET_MAC1_RESETMCSTX | ENET_MAC1_RESETRX |
					  ENET_MAC1_RESETMCSRX | ENET_MAC1_SIMRESET | ENET_MAC1_SOFTRESET;
	pENET->CONTROL.COMMAND = ENET_COMMAND_REGRESET | ENET_COMMAND_TXRESET | ENET_COMMAND_RXRESET |
							 ENET_COMMAND_PASSRUNTFRAME;
}

/**
 * @brief	Sets the address of the interface
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	macAddr	: Pointer to the 6 bytes used for the MAC address
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_SetADDR(LPC_ENET_T *pENET, const uint8_t *macAddr)
{
	/* Save MAC address */
	pENET->MAC.SA[0] = ((uint32_t) macAddr[5] << 8) | ((uint32_t) macAddr[4]);
	pENET->MAC.SA[1] = ((uint32_t) macAddr[3] << 8) | ((uint32_t) macAddr[2]);
	pENET->MAC.SA[2] = ((uint32_t) macAddr[1] << 8) | ((uint32_t) macAddr[0]);
}

/**
 * @brief	Sets up the PHY link clock divider and PHY address
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	div		: Divider index, not a divider value, see user manual
 * @param	addr	: PHY address, used with MII read and write
 * @return	Nothing
 * @note	The MII clock divider rate is divided from the peripheral clock returned
 * from the Chip_Clock_GetSystemClockRate() function. Use Chip_ENET_FindMIIDiv()
 * with a desired clock rate to find the correct divider index value.
 */
void Chip_ENET_SetupMII(LPC_ENET_T *pENET, uint32_t div, uint8_t addr);

/**
 * @brief	Starts a PHY write via the MII
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	reg		: PHY register to write
 * @param	data	: Data to write to PHY register
 * @return	Nothing
 * @note	Start a PHY write operation. Does not block, requires calling
 * IP_ENET_IsMIIBusy to determine when write is complete.
 */
void Chip_ENET_StartMIIWrite(LPC_ENET_T *pENET, uint8_t reg, uint16_t data);

/**
 * @brief	Starts a PHY read via the MII
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	reg		: PHY register to read
 * @return	Nothing
 * @note	Start a PHY read operation. Does not block, requires calling
 * IP_ENET_IsMIIBusy to determine when read is complete and calling
 * IP_ENET_ReadMIIData to get the data.
 */
void Chip_ENET_StartMIIRead(LPC_ENET_T *pENET, uint8_t reg);

/**
 * @brief	Returns MII link (PHY) busy status
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Returns true if busy, otherwise false
 */
STATIC INLINE bool Chip_ENET_IsMIIBusy(LPC_ENET_T *pENET)
{
	return (pENET->MAC.MIND & ENET_MIND_BUSY) ? true : false;
}

/**
 * @brief	Returns the value read from the PHY
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Read value from PHY
 */
uint16_t Chip_ENET_ReadMIIData(LPC_ENET_T *pENET);

/**
 * @brief	Enables ethernet transmit
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_TXEnable(LPC_ENET_T *pENET)
{
	/* Descriptor list head pointers must be setup prior to enable */
	pENET->CONTROL.COMMAND |= ENET_COMMAND_TXENABLE;
}

/**
 * @brief Disables ethernet transmit
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_TXDisable(LPC_ENET_T *pENET)
{
	pENET->CONTROL.COMMAND &= ~ENET_COMMAND_TXENABLE;
}

/**
 * @brief	Enables ethernet packet reception
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_RXEnable(LPC_ENET_T *pENET)
{
	/* Descriptor list head pointers must be setup prior to enable */
	pENET->CONTROL.COMMAND |= ENET_COMMAND_RXENABLE;
	pENET->MAC.MAC1 |= ENET_MAC1_RXENABLE;
}

/**
 * @brief	Disables ethernet packet reception
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_RXDisable(LPC_ENET_T *pENET)
{
	pENET->CONTROL.COMMAND &= ~ENET_COMMAND_RXENABLE;
	pENET->MAC.MAC1 &= ~ENET_MAC1_RXENABLE;
}

/**
 * @brief	Reset Tx Logic
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_ResetTXLogic(LPC_ENET_T *pENET)
{
	pENET->MAC.MAC1 |= ENET_MAC1_RESETTX;
}

/**
 * @brief	Reset Rx Logic
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_ResetRXLogic(LPC_ENET_T *pENET)
{
	pENET->MAC.MAC1 |= ENET_MAC1_RESETRX;
}

/**
 * @brief	Enable Rx Filter
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	mask	: Filter mask (Or-ed bit values of ENET_RXFILTERCTRL_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_EnableRXFilter(LPC_ENET_T *pENET, uint32_t mask)
{
	pENET->CONTROL.COMMAND &= ~ENET_COMMAND_PASSRXFILTER;
	pENET->RXFILTER.CONTROL |=  mask;
}

/**
 * @brief	Disable Rx Filter
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	mask	: Filter mask (Or-ed bit values of ENET_RXFILTERCTRL_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_DisableRXFilter(LPC_ENET_T *pENET, uint32_t mask)
{
	pENET->RXFILTER.CONTROL &= ~mask;
}

/**
 * @brief	Sets full duplex operation for the interface
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
void Chip_ENET_SetFullDuplex(LPC_ENET_T *pENET);

/**
 * @brief	Sets half duplex operation for the interface
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
void Chip_ENET_SetHalfDuplex(LPC_ENET_T *pENET);

/**
 * @brief	Selects 100Mbps for the current speed
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_Set100Mbps(LPC_ENET_T *pENET)
{
	pENET->MAC.SUPP = ENET_SUPP_100Mbps_SPEED;
}

/**
 * @brief	Selects 10Mbps for the current speed
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_Set10Mbps(LPC_ENET_T *pENET)
{
	pENET->MAC.SUPP = 0;
}

/**
 * @brief	Configures the initial ethernet transmit descriptors
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	pDescs	: Pointer to TX descriptor list
 * @param	pStatus	: Pointer to TX status list
 * @param	descNum	: the number of desciptors
 * @return	Nothing
 */
void Chip_ENET_InitTxDescriptors(LPC_ENET_T *pENET, ENET_TXDESC_T *pDescs,
								 ENET_TXSTAT_T *pStatus,
								 uint32_t descNum);

/**
 * @brief	Configures the initial ethernet receive descriptors
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	pDescs	: Pointer to TX descriptor list
 * @param	pStatus	: Pointer to TX status list
 * @param	descNum	: the number of desciptors
 * @return	Nothing
 */
void Chip_ENET_InitRxDescriptors(LPC_ENET_T *pENET, ENET_RXDESC_T *pDescs,
								 ENET_RXSTAT_T *pStatus,
								 uint32_t descNum);

/**
 * @brief	Get the current Tx Produce Descriptor Index
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Tx Produce Index
 */
STATIC INLINE uint16_t Chip_ENET_GetTXProduceIndex(LPC_ENET_T *pENET)
{
	return pENET->CONTROL.TX.PRODUCEINDEX;
}

/**
 * @brief	Get the current Tx Consume Descriptor Index
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Tx Consume Index
 */
STATIC INLINE uint16_t Chip_ENET_GetTXConsumeIndex(LPC_ENET_T *pENET)
{
	return pENET->CONTROL.TX.CONSUMEINDEX;
}

/**
 * @brief	Get the current Rx Produce Descriptor Index
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Rx Produce Index
 */
STATIC INLINE uint16_t Chip_ENET_GetRXProduceIndex(LPC_ENET_T *pENET)
{
	return pENET->CONTROL.RX.PRODUCEINDEX;
}

/**
 * @brief	Get the current Rx Consume Descriptor Index
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Rx Consume Index
 */
STATIC INLINE uint16_t Chip_ENET_GetRXConsumeIndex(LPC_ENET_T *pENET)
{
	return pENET->CONTROL.RX.CONSUMEINDEX;
}

/**
 * @brief	Get the buffer status with the current Produce Index and Consume Index
 * @param	pENET			: The base of ENET peripheral on the chip
 * @param	produceIndex	: Produce Index
 * @param	consumeIndex	: Consume Index
 * @param	buffSize		: Buffer size
 * @return	Status (One of status value: ENET_BUFF_EMPTY/ENET_BUFF_FULL/ENET_BUFF_PARTIAL_FULL)
 */
ENET_BUFF_STATUS_T Chip_ENET_GetBufferStatus(LPC_ENET_T *pENET, uint16_t produceIndex,
											 uint16_t consumeIndex,
											 uint16_t buffSize);

/**
 * @brief	Get the number of descriptors filled
 * @param	pENET			: The base of ENET peripheral on the chip
 * @param	produceIndex	: Produce Index
 * @param	consumeIndex	: Consume Index
 * @param	buffSize		: Buffer size
 * @return	the number of descriptors
 */
uint32_t Chip_ENET_GetFillDescNum(LPC_ENET_T *pENET, uint16_t produceIndex, uint16_t consumeIndex, uint16_t buffSize);

/**
 * @brief	Get the number of free descriptors
 * @param	pENET			: The base of ENET peripheral on the chip
 * @param	produceIndex	: Produce Index
 * @param	consumeIndex	: Consume Index
 * @param	buffSize		: Buffer size
 * @return	the number of descriptors
 */
STATIC INLINE uint32_t Chip_ENET_GetFreeDescNum(LPC_ENET_T *pENET,
												uint16_t produceIndex,
												uint16_t consumeIndex,
												uint16_t buffSize)
{
	return buffSize - 1 - Chip_ENET_GetFillDescNum(pENET, produceIndex, consumeIndex, buffSize);
}

/**
 * @brief	Check if Tx buffer is full
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	true/false
 */
STATIC INLINE bool Chip_ENET_IsTxFull(LPC_ENET_T *pENET)
{
	return ((pENET->CONTROL.TX.CONSUMEINDEX == (pENET->CONTROL.TX.PRODUCEINDEX + 1)) ||
			((pENET->CONTROL.TX.CONSUMEINDEX == 0) &&
			 (pENET->CONTROL.TX.PRODUCEINDEX == pENET->CONTROL.TX.DESCRIPTORNUMBER))) ? true : false;
}

/**
 * @brief	Check if Rx buffer is empty
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	true/false
 */
STATIC INLINE bool Chip_ENET_IsRxEmpty(LPC_ENET_T *pENET)
{
	uint32_t tem = pENET->CONTROL.RX.PRODUCEINDEX;
	return (pENET->CONTROL.RX.CONSUMEINDEX != tem) ? false : true;
}

/**
 * @brief	Increase the current Tx Produce Descriptor Index
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	The new index value
 */
uint16_t Chip_ENET_IncTXProduceIndex(LPC_ENET_T *pENET);

/**
 * @brief	Increase the current Rx Consume Descriptor Index
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	The new index value
 */
uint16_t Chip_ENET_IncRXConsumeIndex(LPC_ENET_T *pENET);

/**
 * @brief	Enable ENET interrupts
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	mask	: Interrupt mask  (Or-ed bit values of ENET_INT_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_EnableInt(LPC_ENET_T *pENET, uint32_t mask)
{
	pENET->MODULE_CONTROL.INTENABLE |= mask;
}

/**
 * @brief	Disable ENET interrupts
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	mask	: Interrupt mask  (Or-ed bit values of ENET_INT_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_DisableInt(LPC_ENET_T *pENET, uint32_t mask)
{
	pENET->MODULE_CONTROL.INTENABLE &= ~mask;
}

/**
 * @brief	Get the interrupt status
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	interrupt status (Or-ed bit values of ENET_INT_*)
 */
STATIC INLINE uint32_t Chip_ENET_GetIntStatus(LPC_ENET_T *pENET)
{
	return pENET->MODULE_CONTROL.INTSTATUS;
}

/**
 * @brief	Clear the interrupt status
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	mask	: Interrupt mask  (Or-ed bit values of ENET_INT_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_ENET_ClearIntStatus(LPC_ENET_T *pENET, uint32_t mask)
{
	pENET->MODULE_CONTROL.INTCLEAR = mask;
}

/**
 * @brief	Initialize ethernet interface
 * @param	pENET	: The base of ENET peripheral on the chip
 * @param	useRMII	: true to setup interface for RMII, false for MII
 * @return	Nothing
 * @note	Performs basic initialization of the ethernet interface in a default
 * state. This is enough to place the interface in a usable state, but
 * may require more setup outside this function.
 */
void Chip_ENET_Init(LPC_ENET_T *pENET, bool useRMII);

/**
 * @brief	De-initialize the ethernet interface
 * @param	pENET	: The base of ENET peripheral on the chip
 * @return	Nothing
 */
void Chip_ENET_DeInit(LPC_ENET_T *pENET);

/**
 * @brief	Find the divider index for a desired MII clock rate
 * @param	pENET		: The base of ENET peripheral on the chip
 * @param	clockRate	: Clock rate to get divider index for
 * @return	MII divider index to get the closest clock rate for clockRate
 * @note	Use this function to get a divider index for the Chip_ENET_SetupMII()
 * function determined from the desired MII clock rate.
 */
uint32_t Chip_ENET_FindMIIDiv(LPC_ENET_T *pENET, uint32_t clockRate);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __ENET_17XX_40XX_H_ */
