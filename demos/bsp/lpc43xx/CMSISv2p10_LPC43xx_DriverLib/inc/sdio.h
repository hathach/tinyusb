/****************************************************************************************************//**
 * @file     sdio.h
 *
 * @status   EXPERIMENTAL
 *
 * @brief    Header file for NXP LPC18xx/43xx SDIO driver
 *
 * @version  V1.0
 * @date     02. November 2011
*
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
* Permission to use, copy, modify, and distribute this software and its
* documentation is hereby granted, under NXP Semiconductors’
* relevant copyright in the software, without fee, provided that it
* is used in conjunction with NXP Semiconductors microcontrollers.  This
* copyright, permission, and disclaimer notice must appear in all copies of
* this code.
 *
 *******************************************************************************************************/

#ifndef __SDIO_H
#define __SDIO_H

/** \defgroup LPCSDMMC_Definitions LPC18xx_43xx SDIO definitions
  This file defines common definitions and values used for SDMMC:
    - Registers, bitfields, and structures
    - Commands and statuses
    - States
  @{
*/

/** \brief  SDIO chained DMA descriptor
 */
typedef struct {
  volatile uint32_t des0;				/*!< Control and status */
  volatile uint32_t des1;				/*!< Buffer size(s) */
  volatile uint32_t des2;				/*!< Buffer address pointer 1 */
  volatile uint32_t des3;				/*!< Buffer address pointer 2 */
} LPC_SDMMC_DMA_Type;

/** \brief  SDIO DMA descriptor control (des0) register defines
 */
#define MCI_DMADES0_OWN			(1UL<<31)	/*!< DMA owns descriptor bit */
#define MCI_DMADES0_CES			(1<<30)	/*!< Card Error Summary bit */
#define MCI_DMADES0_ER			(1<<5)	/*!< End of descriptopr ring bit */
#define MCI_DMADES0_CH			(1<<4)	/*!< Second address chained bit */
#define MCI_DMADES0_FS			(1<<3)	/*!< First descriptor bit */
#define MCI_DMADES0_LD			(1<<2)	/*!< Last descriptor bit */
#define MCI_DMADES0_DIC			(1<<1)	/*!< Disable interrupt on completion bit */

/** \brief  SDIO DMA descriptor size (des1) register defines
 */
#define MCI_DMADES1_BS1(x)		(x)		/*!< Size of buffer 1 */
#define MCI_DMADES1_BS2(x)		((x) << 13)	/*!< Size of buffer 2 */
#define MCI_DMADES1_MAXTR		4096	/*!< Max transfer size per buffer */

/** \brief  SD/SDIO/MMC control register defines
 */
#define MCI_CTRL_USE_INT_DMAC   (1<<25)		/*!< Use internal DMA */
#define MCI_CTRL_ENABLE_OD_PUP  (1<<24)		/*!< Enable external open-drain pullup */
#define MCI_CTRL_CARDV_B_MASK   0xF00000	/*!< Card regulator-B voltage setting; */
#define MCI_CTRL_CARDV_A_MASK   0xF0000		/*!< Card regulator-A voltage setting; */
#define MCI_CTRL_CEATA_INT_EN   (1<<11)		/*!< Enable CE-ATA interrupts */
#define MCI_CTRL_SEND_AS_CCSD   (1<<10)		/*!< Send auto-stop */
#define MCI_CTRL_SEND_CCSD      (1<<9)		/*!< Send CCSD */
#define MCI_CTRL_ABRT_READ_DATA (1<<8)		/*!< Abort read data */
#define MCI_CTRL_SEND_IRQ_RESP  (1<<7)		/*!< Send auto-IRQ response */
#define MCI_CTRL_READ_WAIT      (1<<6)		/*!< Assert read-wait for SDIO */
#define MCI_CTRL_DMA_ENABLE     (1<<5)		/*!< Enable DMA transfer mode */
#define MCI_CTRL_INT_ENABLE     (1<<4)		/*!< Global interrupt enable */
#define MCI_CTRL_DMA_RESET      (1<<2)		/*!< Reset internal DMA */
#define MCI_CTRL_FIFO_RESET     (1<<1)		/*!< Reset data FIFO pointers */
#define MCI_CTRL_RESET          (1<<0)		/*!< Reset controller */

/** \brief Power Enable register defines
 */
#define MCI_POWER_ENABLE(slot)     (1<<(slot))	/*!< Enable slot power signal */

/** \brief Clock divider register defines
 */
#define MCI_CLOCK_DIVIDER(divnum, divby2) ((divby2)<<((divnum) * 8))	/*!< Set slot cklock divider */

/** \brief Clock source register defines
 */
#define MCI_CLKSRC_CLKDIV0     0
#define MCI_CLKSRC_CLKDIV1     1
#define MCI_CLKSRC_CLKDIV2     2
#define MCI_CLKSRC_CLKDIV3     3
#define MCI_CLK_SOURCE(slot, clksrc)   ((clksrc)<<((slot) * 2)) 	/*!< Set slot cklock divider source */

/** \brief Clock Enable register defines
 */
#define MCI_CLKEN_LOW_PWR(slot) (1<<((slot) + 16))	/*!< Enable clock idle for slot */
#define MCI_CLKEN_ENABLE(slot)  (1<<(slot))	/*!< Enable slot clock */

/** \brief time-out register defines
 */
#define MCI_TMOUT_DATA(clks)   ((clks)<<8)		/*!< Data timeout clocks */
#define MCI_TMOUT_DATA_MSK     0xFFFFFF00
#define MCI_TMOUT_RESP(clks)   ((clks) & 0xFF)	/*!< Response timeout clocks */
#define MCI_TMOUT_RESP_MSK     0xFF

/** \brief card-type register defines
 */
#define MCI_CTYPE_8BIT(slot)   (1<<((slot) + 16))	/*!< Enable 4-bit mode */
#define MCI_CTYPE_4BIT(slot)   (1<<(slot))		/*!< Enable 8-bit mode */

/** \brief Bus mode register defines
 */
#define MCI_BMOD_PBL1          (0<<8)			/*!< Burst length = 1 */
#define MCI_BMOD_PBL4          (1<<8)			/*!< Burst length = 4 */
#define MCI_BMOD_PBL8          (2<<8)			/*!< Burst length = 8 */
#define MCI_BMOD_PBL16         (3<<8)			/*!< Burst length = 16 */
#define MCI_BMOD_PBL32         (4<<8)			/*!< Burst length = 32 */
#define MCI_BMOD_PBL64         (5<<8)			/*!< Burst length = 64 */
#define MCI_BMOD_PBL128        (6<<8)			/*!< Burst length = 128 */
#define MCI_BMOD_PBL256        (7<<8)			/*!< Burst length = 256 */
#define MCI_BMOD_DE	           (1<<7)			/*!< Enable internal DMAC */
#define MCI_BMOD_DSL(len)      ((len)<<2)		/*!< Descriptor skip length */
#define MCI_BMOD_FB            (1<<1)			/*!< Fixed bursts */
#define MCI_BMOD_SWR           (1<<0)			/*!< Software reset of internal registers */

/** \brief Interrupt status & mask register defines
 */
#define MCI_INT_SDIO(slot)     (1<<(slot))		/*!< Slot specific interrupt enable */
#define MCI_INT_EBE            (1<<15)			/*!< End-bit error */
#define MCI_INT_ACD            (1<<14)			/*!< Auto command done */
#define MCI_INT_SBE            (1<<13)			/*!< Start bit error */
#define MCI_INT_HLE            (1<<12)			/*!< Hardware locked error */
#define MCI_INT_FRUN           (1<<11)			/*!< FIFO overrun/underrun error */
#define MCI_INT_HTO            (1<<10)			/*!< Host data starvation error */
#define MCI_INT_DTO            (1<<9)			/*!< Data timeout error */
#define MCI_INT_RTO            (1<<8)			/*!< Response timeout error */
#define MCI_INT_DCRC           (1<<7)			/*!< Data CRC error */
#define MCI_INT_RCRC           (1<<6)			/*!< Response CRC error */
#define MCI_INT_RXDR           (1<<5)			/*!< RX data ready */
#define MCI_INT_TXDR           (1<<4)			/*!< TX data needed */
#define MCI_INT_DATA_OVER      (1<<3)			/*!< Data transfer over */
#define MCI_INT_CMD_DONE       (1<<2)			/*!< Command done */
#define MCI_INT_RESP_ERR       (1<<1)			/*!< Command response error */
#define MCI_INT_CD             (1<<0)			/*!< Card detect */
#define MCI_INT_ERROR          0xbfc2

/** \brief Command register defines
 */
#define MCI_CMD_START         (1UL<<31)			/*!< Start command */
#define MCI_CMD_VOLT_SWITCH   (1<<28)			/*!< Voltage switch bit */
#define MCI_CMD_BOOT_MODE     (1<<27)			/*!< Boot mode */
#define MCI_CMD_DISABLE_BOOT  (1<<26)			/*!< Disable boot */
#define MCI_CMD_EXPECT_BOOT_ACK (1<<25)			/*!< Expect boot ack */
#define MCI_CMD_ENABLE_BOOT   (1<<24)			/*!< Enable boot */
#define MCI_CMD_CCS_EXP       (1<<23)			/*!< CCS expected */
#define MCI_CMD_CEATA_RD      (1<<22)			/*!< CE-ATA read in progress */
#define MCI_CMD_UPD_CLK       (1<<21)			/*!< Update clock register only */
#define MCI_CMD_CARDNUM       0x1F0000
#define MCI_CMD_INIT          (1<<15)			/*!< Send init sequence */
#define MCI_CMD_STOP          (1<<14)			/*!< Stop/abort command */
#define MCI_CMD_PRV_DAT_WAIT  (1<<13)			/*!< Wait before send */
#define MCI_CMD_SEND_STOP     (1<<12)			/*!< Send auto-stop */
#define MCI_CMD_STRM_MODE     (1<<11)			/*!< Stream transfer mode */
#define MCI_CMD_DAT_WR        (1<<10)			/*!< Read(0)/Write(1) selection */
#define MCI_CMD_DAT_EXP       (1<<9)			/*!< Data expected */
#define MCI_CMD_RESP_CRC      (1<<8)			/*!< Check response CRC */
#define MCI_CMD_RESP_LONG     (1<<7)			/*!< Response length */
#define MCI_CMD_RESP_EXP      (1<<6)			/*!< Response expected */
#define MCI_CMD_INDX(n)       ((n) & 0x1F)

/** \brief status register definess
 */
#define MCI_STS_GET_FCNT(x)	  (((x)>>17) & 0x1FF)

/** \brief card type defines
 */
#define CARD_TYPE_SD    (1 << 0)
#define CARD_TYPE_4BIT  (1 << 1)
#define CARD_TYPE_8BIT  (1 << 2)
#define CARD_TYPE_HC    (OCR_HC_CCS) /*!< high capacity card > 2GB */

/** \brief Commonly used definitions
 */

#define MMC_SECTOR_SIZE 		512
#define MCI_FIFO_SZ             32			/*!< Size of SDIO FIFO (32-bit wide) */

/** \brief Setup options for the SDIO driver
 */
#define US_TIMEOUT 1000000 /*!< give 1 atleast 1 sec for the card to respond */
#define MS_ACQUIRE_DELAY	(10) /*!< inter-command acquire oper condition delay in msec*/
#define INIT_OP_RETRIES   10  /*!< initial OP_COND retries */
#define SET_OP_RETRIES    200 /*!< set OP_COND retries */
#define SDIO_BUS_WIDTH	4	/*!< Max bus width supported */
#define SD_MMC_ENUM_CLOCK       400000		/*!< Typical enumeration clock rate */
#define MMC_MAX_CLOCK           20000000	/*!< Max MMC clock rate */
#define MMC_LOW_BUS_MAX_CLOCK   26000000	/*!< Type 0 MMC card max clock rate */
#define MMC_HIGH_BUS_MAX_CLOCK  52000000	/*!< Type 1 MMC card max clock rate */
#define SD_MAX_CLOCK            25000000	/*!< Max SD clock rate */
#define SYS_REG_SD_CARD_DELAY   0x1B		/*!< SD card delay (register) */
#define SYS_REG_MMC_CARD_DELAY  0x16		/*!< MMC card delay (register) */

/* The SDIO driver can be used in polled or IRQ based modes. In polling
   mode, the driver functions block until complete. In IRQ mode, the
   functions won't block and the status must be checked elsewhere. */
#define SDIO_USE_POLLING /* non-polling mode does not work yet */

/* If the following define is enabled, 'double buffer' type DMA descriptors
   will be used instead of chained descriptors. */
/* Note: Avoid using double buffer mode - is isn't working yet. */
//#define USE_DMADESC_DBUFF

/***********************************************************************
 * MCI device structure and it defines
 **********************************************************************/
typedef struct  _mci_card_struct MCI_CARD_INFO_T;
typedef uint32_t (*MCI_CMD_WAIT_FUNC_T)(MCI_CARD_INFO_T* , uint32_t);
typedef void (*MCI_IRQ_CB_FUNC_T)(MCI_CARD_INFO_T* , uint32_t);
struct  _mci_card_struct
{
  uint32_t response[4];		/*!< Most recent response */
  uint32_t cid[4];			/*!< CID of acquired card  */
  uint32_t csd[4];			/*!< CSD of acquired card */
  uint32_t ext_csd[MMC_SECTOR_SIZE/4];
  uint32_t card_type;
  uint32_t rca;			    /*!< Relative address assigned to card */
  uint32_t speed;
  uint32_t block_len;
  uint32_t device_size;
  uint32_t blocknr;
  MCI_CMD_WAIT_FUNC_T wait_func;
  MCI_IRQ_CB_FUNC_T irq_callback;
};

/** \brief MCI driver API functions
 */

/* Initialize the SDIO controller */
void sdio_init(MCI_CMD_WAIT_FUNC_T waitfunc,
			   MCI_IRQ_CB_FUNC_T irqfunc);

/* Detect if an SD card is inserted */
int sdio_card_detect(void);

/* Detect if write protect is enabled */
int sdio_card_wp_on(void);

/* Enable or disable slot power */
void sdio_power_onoff(int enable);
void sdio_power_on(void);
void sdio_power_off(void);
 
/* Function to enumerate the SD/MMC/SDHC/MMC+ cards */
int sdio_acquire(void);

/* Close the SDIO controller */
void sdio_deinit(void);

/* SDIO read function - reads data from a card */
int sdio_read_blocks(void *buffer,
                     int start_block,
                     int end_block);

/* SDIO write function - writes data to a card. After calling this
   function, do not use read or write until the card state has
   left the program state. */
int sdio_write_blocks(void *buffer,
                      int start_block,
                      int end_block);

/* Get card's current state (idle, transfer, program, etc.) */
int sdio_get_state(void);

int sdio_get_device_size(void);

extern uint32_t sdio_clk_rate;

#ifdef __cplusplus
}
#endif

#endif /* end __SDIO_H */
/*****************************************************************************
**                            End Of File
******************************************************************************/
