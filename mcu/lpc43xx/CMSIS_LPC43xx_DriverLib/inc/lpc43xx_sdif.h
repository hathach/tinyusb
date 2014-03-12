/**********************************************************************
* $Id$		lpc43xx_sdif.h		2012-Aug-15
*//**
* @file		lpc43xx_sdif.h
* @brief	Contains all macro definitions and function prototypes
* 			support for SDIO firmware library on LPC43xx
* @version	1.0
* @date		15. Aug. 2012
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
* Permission to use, copy, modify, and distribute this software and its
* documentation is hereby granted, under NXP Semiconductors'
* relevant copyright in the software, without fee, provided that it
* is used in conjunction with NXP Semiconductors microcontrollers.  This
* copyright, permission, and disclaimer notice must appear in all copies of
* this code.
**********************************************************************/

/* Peripheral group ----------------------------------------------------------- */
/** @defgroup SDIF	SDIF (SD Card Interface)
 * @ingroup LPC4300CMSIS_FwLib_Drivers
 * @{
 */
#ifndef LPC43XX_SDIF_H
#define LPC43XX_SDIF_H

#include "LPC43xx.h"
#include "lpc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Private Macros ------------------------------------------------------------- */
/** @defgroup SDIF_Private_Macros SDIF Private Macros
 * @{
 */

/** \brief  SDIO chained DMA descriptor
 */
typedef struct {
  volatile uint32_t des0;                       /*!< Control and status */
  volatile uint32_t des1;                       /*!< Buffer size(s) */
  volatile uint32_t des2;                       /*!< Buffer address pointer 1 */
  volatile uint32_t des3;                       /*!< Buffer address pointer 2 */
} LPC_SDMMC_DMA_Type;

/** \brief  SDIO DMA descriptor control (des0) register defines
 */
#define MCI_DMADES0_OWN         (1UL << 31)     /*!< DMA owns descriptor bit */
#define MCI_DMADES0_CES         (1 << 30)       /*!< Card Error Summary bit */
#define MCI_DMADES0_ER          (1 << 5)        /*!< End of descriptopr ring bit */
#define MCI_DMADES0_CH          (1 << 4)        /*!< Second address chained bit */
#define MCI_DMADES0_FS          (1 << 3)        /*!< First descriptor bit */
#define MCI_DMADES0_LD          (1 << 2)        /*!< Last descriptor bit */
#define MCI_DMADES0_DIC         (1 << 1)        /*!< Disable interrupt on completion bit */

/** \brief  SDIO DMA descriptor size (des1) register defines
 */
#define MCI_DMADES1_BS1(x)      (x)             /*!< Size of buffer 1 */
#define MCI_DMADES1_BS2(x)      ((x) << 13)     /*!< Size of buffer 2 */
#define MCI_DMADES1_MAXTR       4096            /*!< Max transfer size per buffer */


/** \brief  SDIO control register defines
 */
#define MCI_CTRL_USE_INT_DMAC   (1 << 25)       /*!< Use internal DMA */
#define MCI_CTRL_CARDV_MASK     (0x7 << 16)     /*!< SD_VOLT[2:0} pins output state mask */
#define MCI_CTRL_CEATA_INT_EN   (1 << 11)       /*!< Enable CE-ATA interrupts */
#define MCI_CTRL_SEND_AS_CCSD   (1 << 10)       /*!< Send auto-stop */
#define MCI_CTRL_SEND_CCSD      (1 << 9)        /*!< Send CCSD */
#define MCI_CTRL_ABRT_READ_DATA (1 << 8)        /*!< Abort read data */
#define MCI_CTRL_SEND_IRQ_RESP  (1 << 7)        /*!< Send auto-IRQ response */
#define MCI_CTRL_READ_WAIT      (1 << 6)        /*!< Assert read-wait for SDIO */
#define MCI_CTRL_INT_ENABLE     (1 << 4)        /*!< Global interrupt enable */
#define MCI_CTRL_DMA_RESET      (1 << 2)        /*!< Reset internal DMA */
#define MCI_CTRL_FIFO_RESET     (1 << 1)        /*!< Reset data FIFO pointers */
#define MCI_CTRL_RESET          (1 << 0)        /*!< Reset controller */

/** \brief SDIO Power Enable register defines
 */
#define MCI_POWER_ENABLE        0x1             /*!< Enable slot power signal (SD_POW) */

/** \brief SDIO Clock divider register defines
 */
#define MCI_CLOCK_DIVIDER(dn, d2) ((d2) << ((dn) * 8)) /*!< Set cklock divider */

/** \brief SDIO Clock source register defines
 */
#define MCI_CLKSRC_CLKDIV0      0
#define MCI_CLKSRC_CLKDIV1      1
#define MCI_CLKSRC_CLKDIV2      2
#define MCI_CLKSRC_CLKDIV3      3
#define MCI_CLK_SOURCE(clksrc)  (clksrc)        /*!< Set cklock divider source */

/** \brief SDIO Clock Enable register defines
 */
#define MCI_CLKEN_LOW_PWR       (1 << 16)       /*!< Enable clock idle for slot */
#define MCI_CLKEN_ENABLE        (1 << 0)        /*!< Enable slot clock */

/** \brief SDIO time-out register defines
 */
#define MCI_TMOUT_DATA(clks)    ((clks) << 8)   /*!< Data timeout clocks */
#define MCI_TMOUT_DATA_MSK      0xFFFFFF00
#define MCI_TMOUT_RESP(clks)    ((clks) & 0xFF) /*!< Response timeout clocks */
#define MCI_TMOUT_RESP_MSK      0xFF

/** \brief SDIO card-type register defines
 */
#define MCI_CTYPE_8BIT          (1 << 16)       /*!< Enable 4-bit mode */
#define MCI_CTYPE_4BIT          (1 << 0)        /*!< Enable 8-bit mode */

/** \brief SDIO Interrupt status & mask register defines
 */
#define MCI_INT_SDIO            (1 << 16)       /*!< SDIO interrupt */
#define MCI_INT_EBE             (1 << 15)       /*!< End-bit error */
#define MCI_INT_ACD             (1 << 14)       /*!< Auto command done */
#define MCI_INT_SBE             (1 << 13)       /*!< Start bit error */
#define MCI_INT_HLE             (1 << 12)       /*!< Hardware locked error */
#define MCI_INT_FRUN            (1 << 11)       /*!< FIFO overrun/underrun error */
#define MCI_INT_HTO             (1 << 10)       /*!< Host data starvation error */
#define MCI_INT_DTO             (1 << 9)        /*!< Data timeout error */
#define MCI_INT_RTO             (1 << 8)        /*!< Response timeout error */
#define MCI_INT_DCRC            (1 << 7)        /*!< Data CRC error */
#define MCI_INT_RCRC            (1 << 6)        /*!< Response CRC error */
#define MCI_INT_RXDR            (1 << 5)        /*!< RX data ready */
#define MCI_INT_TXDR            (1 << 4)        /*!< TX data needed */
#define MCI_INT_DATA_OVER       (1 << 3)        /*!< Data transfer over */
#define MCI_INT_CMD_DONE        (1 << 2)        /*!< Command done */
#define MCI_INT_RESP_ERR        (1 << 1)        /*!< Command response error */
#define MCI_INT_CD              (1 << 0)        /*!< Card detect */

/** \brief SDIO Command register defines
 */
#define MCI_CMD_START           (1UL << 31)     /*!< Start command */
#define MCI_CMD_VOLT_SWITCH     (1 << 28)       /*!< Voltage switch bit */
#define MCI_CMD_BOOT_MODE       (1 << 27)       /*!< Boot mode */
#define MCI_CMD_DISABLE_BOOT    (1 << 26)       /*!< Disable boot */
#define MCI_CMD_EXPECT_BOOT_ACK (1 << 25)       /*!< Expect boot ack */
#define MCI_CMD_ENABLE_BOOT     (1 << 24)       /*!< Enable boot */
#define MCI_CMD_CCS_EXP         (1 << 23)       /*!< CCS expected */
#define MCI_CMD_CEATA_RD        (1 << 22)       /*!< CE-ATA read in progress */
#define MCI_CMD_UPD_CLK         (1 << 21)       /*!< Update clock register only */
#define MCI_CMD_INIT            (1 << 15)       /*!< Send init sequence */
#define MCI_CMD_STOP            (1 << 14)       /*!< Stop/abort command */
#define MCI_CMD_PRV_DAT_WAIT    (1 << 13)       /*!< Wait before send */
#define MCI_CMD_SEND_STOP       (1 << 12)       /*!< Send auto-stop */
#define MCI_CMD_STRM_MODE       (1 << 11)       /*!< Stream transfer mode */
#define MCI_CMD_DAT_WR          (1 << 10)       /*!< Read(0)/Write(1) selection */
#define MCI_CMD_DAT_EXP         (1 << 9)        /*!< Data expected */
#define MCI_CMD_RESP_CRC        (1 << 8)        /*!< Check response CRC */
#define MCI_CMD_RESP_LONG       (1 << 7)        /*!< Response length */
#define MCI_CMD_RESP_EXP        (1 << 6)        /*!< Response expected */
#define MCI_CMD_INDX(n)         ((n) & 0x1F)

/** \brief SDIO status register definess
 */
#define MCI_STS_GET_FCNT(x)     (((x) >> 17) & 0x1FF)

/** \brief SDIO FIFO threshold defines
 */
#define MCI_FIFOTH_TX_WM(x)     ((x) & 0xFFF)
#define MCI_FIFOTH_RX_WM(x)     (((x) & 0xFFF) << 16)
#define MCI_FIFOTH_DMA_MTS_1    (0UL << 28)
#define MCI_FIFOTH_DMA_MTS_4    (1UL << 28)
#define MCI_FIFOTH_DMA_MTS_8    (2UL << 28)
#define MCI_FIFOTH_DMA_MTS_16   (3UL << 28)
#define MCI_FIFOTH_DMA_MTS_32   (4UL << 28)
#define MCI_FIFOTH_DMA_MTS_64   (5UL << 28)
#define MCI_FIFOTH_DMA_MTS_128  (6UL << 28)
#define MCI_FIFOTH_DMA_MTS_256  (7UL << 28)

/** \brief Bus mode register defines
 */
#define MCI_BMOD_PBL1           (0 << 8)        /*!< Burst length = 1 */
#define MCI_BMOD_PBL4           (1 << 8)        /*!< Burst length = 4 */
#define MCI_BMOD_PBL8           (2 << 8)        /*!< Burst length = 8 */
#define MCI_BMOD_PBL16          (3 << 8)        /*!< Burst length = 16 */
#define MCI_BMOD_PBL32          (4 << 8)        /*!< Burst length = 32 */
#define MCI_BMOD_PBL64          (5 << 8)        /*!< Burst length = 64 */
#define MCI_BMOD_PBL128         (6 << 8)        /*!< Burst length = 128 */
#define MCI_BMOD_PBL256         (7 << 8)        /*!< Burst length = 256 */
#define MCI_BMOD_DE             (1 << 7)        /*!< Enable internal DMAC */
#define MCI_BMOD_DSL(len)       ((len) << 2)    /*!< Descriptor skip length */
#define MCI_BMOD_FB             (1 << 1)        /*!< Fixed bursts */
#define MCI_BMOD_SWR            (1 << 0)        /*!< Software reset of internal registers */

/** \brief Commonly used definitions
 */
#define SD_FIFO_SZ              32              /*!< Size of SDIO FIFOs (32-bit wide) */

/***********************************************************************
 * MCI device structure and it defines
 **********************************************************************/

/* Function prototype for SD interface IRQ callback */
typedef uint32_t (*MCI_IRQ_CB_FUNC_T)(uint32_t);

/* Function prototype for SD detect and write protect status check */
typedef int32_t (*PSCHECK_FUNC_T) (void);

/* Function prototype for SD slot power enable or slot reset */
typedef void (*PS_POWER_FUNC_T) (int32_t enable);

/* Card specific setup data */
struct _mci_card_struct
{
    uint32_t response[4];                       /*!< Most recent response */
    uint32_t cid[4];                            /*!< CID of acquired card  */
    uint32_t csd[4];                            /*!< CSD of acquired card */
    uint32_t ext_csd[512 / 4];
    uint32_t card_type;
    uint32_t rca;                               /*!< Relative address assigned to card */
    uint32_t speed;
    uint32_t block_len;
    uint32_t device_size;
    uint32_t blocknr;
    PSCHECK_FUNC_T sdck_det;
    PSCHECK_FUNC_T sdck_wp;
    PS_POWER_FUNC_T sd_setpow;
    PS_POWER_FUNC_T sd_setrst;
};

/**
 * @}
 */

/* Public Functions ----------------------------------------------------------- */
/** @defgroup SDIO_Public_Functions SDIO Public Functions
 * @{
 */

/* Setup DMA descriptors */
void sdif_dma_setup(uint32_t addr, uint32_t size);

 /* Send a command on the SD bus */
int32_t sdif_send_cmd(uint32_t cmd, uint32_t arg);

/* Read the response from the last command */
void sdif_get_response(uint32_t *resp);

/* Sets the SD bus clock speed */
void sdif_set_clock(uint32_t speed);

/* Detect if an SD card is inserted
   (uses SD_CD pin, returns 0 on card detect) */
int32_t sdif_card_ndetect(void);

/* Detect if write protect is enabled
   (uses SD_WP pin, returns 1 if card is write protected) */
int32_t sdif_card_wp_on(void);

/* Enable or disable slot power, !0 = enable slot power
   (Uses SD_POW pin, set to high or low based on enable parameter state) */
void sdif_power_onoff(int32_t enable);

/* Reset card in slot, must manually de-assert reset after assertion
   (Uses SD_RST pin, set per reset parameter state) */
void sdif_reset(int32_t reset);

/* Set block size for transfer */
void sdif_set_blksize(uint32_t bytes);

/* Enter or exit low power mode. */
void sdif_set_lowpower_mode(int32_t lpmode);

/* Initialize the SD controller */
void sdif_init(uint32_t sdio_clock, MCI_IRQ_CB_FUNC_T irq_callback);

/* Close the SD controller */
void sdif_deinit(void);

/**
 * @}
 */
 
#ifdef __cplusplus
}
#endif

#endif /* end LPC43XX_SDIF_H */
/**
 * @}
 */

/* --------------------------------- End Of File ------------------------------ */
