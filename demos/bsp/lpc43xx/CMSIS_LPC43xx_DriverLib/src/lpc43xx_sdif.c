/**********************************************************************
* $Id$		lpc43xx_sdif.c		2012-Aug-15
*//**
* @file		lpc43xx_sdif.c
* @brief	LPC43xx SD interface driver
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
/** @addtogroup SDIF
 * @{
 */

/* Includes ------------------------------------------------------------------- */
#include "LPC43xx.h"                    /* LPC43xx definitions                */
#include "system_LPC43xx.h"
#include "lpc_sdmmc.h"
#include "lpc43xx_sdif.h"
#include "lpc43xx_cgu.h"

/* If this source file built with example, the lpc43xx FW library configuration
 * file in each example directory ("lpc43xx_libcfg.h") must be included,
 * otherwise the default FW library configuration file must be included instead
 */
#ifdef __BUILD_WITH_EXAMPLE__
#include "lpc43xx_libcfg.h"
#else
#include "lpc43xx_libcfg_default.h"
#endif /* __BUILD_WITH_EXAMPLE__ */

#ifdef _SDIF

/* Local data structure for the SDIF driver */
struct _sdif_device {
    MCI_IRQ_CB_FUNC_T irq_cb;
    LPC_SDMMC_DMA_Type mci_dma_dd[1 + (0x10000 / MCI_DMADES1_MAXTR)];
    uint32_t sdio_clk_rate;
    uint32_t sdif_slot_clk_rate;
    int32_t clock_enabled;
};
static struct _sdif_device sdif_dev;

/*********************************************************************//**
* @brief 		Enables the SDIO controller clock
* @param[in]	None
* @return 		None
**********************************************************************/
static void sdif_enable_clock(void)
{
    if (!sdif_dev.clock_enabled)
    {
        /* Enable SD MMC clock */
        CGU_ConfigPWR(CGU_PERIPHERAL_SDIO, ENABLE);
        sdif_dev.clock_enabled = 1;
    }
}

/*********************************************************************//**
* @brief 		Disables the SDIO controller clock
* @param[in]	None
* @return 		None
**********************************************************************/
static void sdif_disable_clock(void)
{
    if (!sdif_dev.clock_enabled)
    {
        /* Disable SD MMC clock */
        CGU_ConfigPWR(CGU_PERIPHERAL_SDIO, (FunctionalState)FALSE);
        sdif_dev.clock_enabled = 0;
    }
}

/* Public Functions ----------------------------------------------------------- */
/** @defgroup SDIF_Public_Functions
 * @ingroup SDIF
 * @{
 */

/*********************************************************************//**
* @brief 		Setup DMA descriptors
* @param[in]	addr Address of buffer (source or destination)
* @param[in]	size size of buffer in bytes (64K max)
* @return 		None
**********************************************************************/
void sdif_dma_setup(uint32_t addr, uint32_t size)
{
	int i = 0;
	uint32_t ctrl, maxs;

	/* Reset DMA */
	LPC_SDMMC->CTRL |= MCI_CTRL_DMA_RESET | MCI_CTRL_FIFO_RESET;
 	while (LPC_SDMMC->CTRL & MCI_CTRL_DMA_RESET);

	/* Build a descriptor list using the chained DMA method */
	while (size > 0)
    {
		/* Limit size of the transfer to maximum buffer size */
		maxs = size;
		if (maxs > MCI_DMADES1_MAXTR)
			maxs = MCI_DMADES1_MAXTR;
		size -= maxs;

		/* Set buffer size */
		sdif_dev.mci_dma_dd[i].des1 = MCI_DMADES1_BS1(maxs);

		/* Setup buffer address (chained) */
		sdif_dev.mci_dma_dd[i].des2 = addr + (i * MCI_DMADES1_MAXTR);

		/* Setup basic control */
		ctrl = MCI_DMADES0_OWN | MCI_DMADES0_CH;
		if (i == 0)
			ctrl |= MCI_DMADES0_FS; /* First DMA buffer */

		/* No more data? Then this is the last descriptor */
		if (!size)
			ctrl |= MCI_DMADES0_LD;
		else
			ctrl |= MCI_DMADES0_DIC;

		/* Another descriptor is needed */
		sdif_dev.mci_dma_dd[i].des3 = (uint32_t) &sdif_dev.mci_dma_dd[i + 1];
		sdif_dev.mci_dma_dd[i].des0 = ctrl;

		i++;
	}

	/* Set DMA derscriptor base address */
	LPC_SDMMC->DBADDR = (uint32_t) &sdif_dev.mci_dma_dd[0];
}

/*********************************************************************//**
* @brief 		Function to send command to Card interface unit (CIU)
* @param[in]	cmd Command with all flags set
* @param[in]	arg Argument for the command
* @return 		TRUE on times-out, otherwise FALSE
**********************************************************************/
int32_t sdif_send_cmd(uint32_t cmd, uint32_t arg)
{
    volatile int32_t tmo = 50;
    volatile int delay;

    /* set command arg reg*/
    LPC_SDMMC->CMDARG = arg;
    LPC_SDMMC->CMD = MCI_CMD_START | cmd;

    /* poll untill command is accepted by the CIU */
    while (--tmo && (LPC_SDMMC->CMD & MCI_CMD_START))
    {
        if (tmo & 1)
            delay = 50;
        else
            delay = 18000;

        while (--delay > 1);
    }

    return (tmo < 1) ? 1 : 0;
}

/*********************************************************************//**
* @brief 		Function to retrieve command response
* @param[in]	pdev Pointer to card info structure
* @return 		None
**********************************************************************/
void sdif_get_response(uint32_t *resp)
{
  /* on this chip response is not a fifo so read all 4 regs */
  resp[0] = LPC_SDMMC->RESP0;
  resp[1] = LPC_SDMMC->RESP1;
  resp[2] = LPC_SDMMC->RESP2;
  resp[3] = LPC_SDMMC->RESP3;
}

/*********************************************************************//**
* @brief 		Function to set speed of the clock going to card
* @param[in]	speed Desired clock speed to the card
* @return 		None
**********************************************************************/
void sdif_set_clock(uint32_t speed)
{
    /* compute SD/MMC clock dividers */
    uint32_t div;

    /* Exit if the clock is already set at the passed speed */
    if (sdif_dev.sdif_slot_clk_rate == speed)
        return;

    div = ((sdif_dev.sdio_clk_rate / speed) + 2) >> 1;
    sdif_dev.sdif_slot_clk_rate = speed;

    if ((div == LPC_SDMMC->CLKDIV) && LPC_SDMMC->CLKENA)
        return; /* Closest speed is already set */

    /* disable clock */
    LPC_SDMMC->CLKENA = 0;

    /* User divider 0 */
    LPC_SDMMC->CLKSRC = MCI_CLKSRC_CLKDIV0;

    /* inform CIU */
    sdif_send_cmd(MCI_CMD_UPD_CLK | MCI_CMD_PRV_DAT_WAIT, 0);

    /* set divider 0 to desired value */
    LPC_SDMMC->CLKDIV = MCI_CLOCK_DIVIDER(0, div);

    /* inform CIU */
    sdif_send_cmd(MCI_CMD_UPD_CLK | MCI_CMD_PRV_DAT_WAIT, 0);

    /* enable clock */
    LPC_SDMMC->CLKENA = MCI_CLKEN_ENABLE;

    /* inform CIU */
    sdif_send_cmd(MCI_CMD_UPD_CLK | MCI_CMD_PRV_DAT_WAIT, 0);
}

/*********************************************************************//**
* @brief 		Detect if an SD card is inserted
* @param[in]	None
* @return 		Returns 0 if a card is detected, otherwise 1
**********************************************************************/
int32_t sdif_card_ndetect(void)
{
	/* No card = high state in regsiter */
	if (LPC_SDMMC->CDETECT & 1)
		return 0;

	return 1;
}

/*********************************************************************//**
* @brief 		Detect if write protect is enabled
* @param[in]	None
* @return 		Returns 1 if card is write protected, otherwise 0
**********************************************************************/
int32_t sdif_card_wp_on(void)
{
	if (LPC_SDMMC->WRTPRT & 1)
		return 1;

	return 0;
}

/*********************************************************************//**
* @brief 		Enable or disable slot power
* @param[in]	enable !0 to enable, or 0 to disable
* @return 		None
**********************************************************************/
void sdif_power_onoff(int32_t enable)
{
	if (enable)
		LPC_SDMMC->PWREN = 1;
	else
		LPC_SDMMC->PWREN = 0;
}

/*********************************************************************//**
* @brief 		Reset card in slot
* @param[in]	reset Sets SD_RST to passed state
* @return 		None
**********************************************************************/
void sdif_reset(int32_t reset)
{
	if (reset)
		LPC_SDMMC->RST_N = 1;
	else
		LPC_SDMMC->RST_N = 0;
}

/*********************************************************************//**
* @brief 		Set block size for transfer
* @param[in]	bytes Lock size in bytes
* @return 		None
**********************************************************************/
void sdif_set_blksize(uint32_t bytes)
{
      LPC_SDMMC->BLKSIZ = bytes;
}

/*********************************************************************//**
* @brief 		Enter or exit low power mode (disables clocking)
* @param[in]	lpmode !0 to enable low power mode, 0 = exit
* @return 		None
**********************************************************************/
void sdif_set_lowpower_mode(int32_t lpmode)
{
	/* Once in low power mode, no SDIF functions should ever be
	   called, as it can hang the chip. Always exit low power mode
	   prior to resuming SDIF functions */
    if (lpmode)
        sdif_disable_clock();
    else
        sdif_enable_clock();
}

/*********************************************************************//**
* @brief 		Initializes the MCI card controller
* @param[in]	waitfunc Pointer to wait function to be used during for poll command status
* @param[in]	irq_callback Pointer to IRQ callback function
* @return 		None
**********************************************************************/
void sdif_init(uint32_t sdio_clock, MCI_IRQ_CB_FUNC_T irq_callback)
{
    volatile uint32_t i;

    sdif_dev.sdio_clk_rate = sdio_clock;
    sdif_dev.irq_cb = irq_callback;

    /* enable SD/MMC clock */
    sdif_enable_clock();

    /* Software reset */
    LPC_SDMMC->BMOD = MCI_BMOD_SWR;

    /* reset all blocks */
    LPC_SDMMC->CTRL = MCI_CTRL_RESET | MCI_CTRL_FIFO_RESET |
        MCI_CTRL_DMA_RESET;
    while (LPC_SDMMC->CTRL &
        (MCI_CTRL_RESET | MCI_CTRL_FIFO_RESET | MCI_CTRL_DMA_RESET));

    /* Internal DMA setup for control register */
    LPC_SDMMC->CTRL = MCI_CTRL_USE_INT_DMAC | MCI_CTRL_INT_ENABLE;
    LPC_SDMMC->INTMASK = 0;

    /* Clear the interrupts for the host controller */
    LPC_SDMMC->RINTSTS = 0xFFFFFFFF;

    /* Put in max timeout */
    LPC_SDMMC->TMOUT = 0xFFFFFFFF;

    /* FIFO threshold settings for DMA, DMA burst of 4,
       FIFO watermark at 16 */
    LPC_SDMMC->FIFOTH = MCI_FIFOTH_DMA_MTS_4 |
        MCI_FIFOTH_RX_WM((SD_FIFO_SZ / 2) - 1) |
        MCI_FIFOTH_TX_WM(SD_FIFO_SZ / 2);

    /* Enable internal DMA, burst size of 4, fixed burst */
    LPC_SDMMC->BMOD = MCI_BMOD_DE | MCI_BMOD_PBL4 | MCI_BMOD_DSL(4);

    /* disable clock to CIU (needs latch) */
    LPC_SDMMC->CLKENA = 0;
    LPC_SDMMC->CLKSRC = 0;
}

/*********************************************************************//**
* @brief 		Close the MCI
* @param[in]	None
* @return 		None
**********************************************************************/
void sdif_deinit(void)
{
    /* clear mmc structure*/
    sdif_disable_clock();
}

/*********************************************************************//**
* @brief 		SDIO controller interrupt handler
* @param[in]	None
* @return 		None
**********************************************************************/
void SDIO_IRQHandler(void)
{
    /* All SD based register handling is done in the callback
	   function. The SDIO interrupt is not enabled as part of this
	   driver and needs to be enabled/disabled in the callbacks or
	   application as needed. This is to allow flexibility with IRQ
	   handling for applicaitons and RTOSes. */
    sdif_dev.irq_cb(LPC_SDMMC->RINTSTS);
}

/**
 * @}
 */

#endif /* _SDIF */

/**
 * @}
 */
