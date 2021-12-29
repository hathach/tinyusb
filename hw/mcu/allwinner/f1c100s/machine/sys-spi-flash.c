/*
 * sys-spi-flash.c
 *
 * Copyright(c) 2007-2018 Jianjun Jiang <8192542@qq.com>
 * Official site: http://xboot.org
 * Mobile phone: +86-18665388956
 * QQ: 8192542
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdint.h>
#include <types.h>
#include <string.h>
#include <io.h>

enum {
	SPI_GCR	= 0x04,
	SPI_TCR	= 0x08,
	SPI_IER	= 0x10,
	SPI_ISR	= 0x14,
	SPI_FCR	= 0x18,
	SPI_FSR	= 0x1c,
	SPI_WCR	= 0x20,
	SPI_CCR	= 0x24,
	SPI_MBC	= 0x30,
	SPI_MTC	= 0x34,
	SPI_BCC	= 0x38,
	SPI_TXD	= 0x200,
	SPI_RXD	= 0x300,
};

void sys_spi_flash_init(void)
{
	virtual_addr_t addr;
	uint32_t val;

	/* Config GPIOC0, GPIOC1, GPIOC2 and GPIOC3 */
	addr = 0x01c20848 + 0x00;
	val = read32(addr);
	val &= ~(0xf << ((0 & 0x7) << 2));
	val |= ((0x2 & 0x7) << ((0 & 0x7) << 2));
	write32(addr, val);

	val = read32(addr);
	val &= ~(0xf << ((1 & 0x7) << 2));
	val |= ((0x2 & 0x7) << ((1 & 0x7) << 2));
	write32(addr, val);

	val = read32(addr);
	val &= ~(0xf << ((2 & 0x7) << 2));
	val |= ((0x2 & 0x7) << ((2 & 0x7) << 2));
	write32(addr, val);

	val = read32(addr);
	val &= ~(0xf << ((3 & 0x7) << 2));
	val |= ((0x2 & 0x7) << ((3 & 0x7) << 2));
	write32(addr, val);

	/* Deassert spi0 reset */
	addr = 0x01c202c0;
	val = read32(addr);
	val |= (1 << 20);
	write32(addr, val);

	/* Open the spi0 bus gate */
	addr = 0x01c20000 + 0x60;
	val = read32(addr);
	val |= (1 << 20);
	write32(addr, val);

	/* Set spi clock rate control register, divided by 4 */
	addr = 0x01c05000;
	write32(addr + SPI_CCR, 0x00001001);

	/* Enable spi0 and do a soft reset */
	addr = 0x01c05000;
	val = read32(addr + SPI_GCR);
	val |= (1 << 31) | (1 << 7) | (1 << 1) | (1 << 0);
	write32(addr + SPI_GCR, val);
	while(read32(addr + SPI_GCR) & (1 << 31));

	val = read32(addr + SPI_TCR);
	val &= ~(0x3 << 0);
	val |= (1 << 6) | (1 << 2);
	write32(addr + SPI_TCR, val);

	val = read32(addr + SPI_FCR);
	val |= (1 << 31) | (1 << 15);
	write32(addr + SPI_FCR, val);
}

void sys_spi_flash_exit(void)
{
	virtual_addr_t addr = 0x01c05000;
	uint32_t val;

	/* Disable the spi0 controller */
	val = read32(addr + SPI_GCR);
	val &= ~((1 << 1) | (1 << 0));
	write32(addr + SPI_GCR, val);
}

static void sys_spi_select(void)
{
	virtual_addr_t addr = 0x01c05000;
	uint32_t val;

	val = read32(addr + SPI_TCR);
	val &= ~((0x3 << 4) | (0x1 << 7));
	val |= ((0 & 0x3) << 4) | (0x0 << 7);
	write32(addr + SPI_TCR, val);
}

static void sys_spi_deselect(void)
{
	virtual_addr_t addr = 0x01c05000;
	uint32_t val;

	val = read32(addr + SPI_TCR);
	val &= ~((0x3 << 4) | (0x1 << 7));
	val |= ((0 & 0x3) << 4) | (0x1 << 7);
	write32(addr + SPI_TCR, val);
}

static void sys_spi_write_txbuf(uint8_t * buf, int len)
{
	virtual_addr_t addr = 0x01c05000;
	int i;

	if(!buf)
		len = 0;

	write32(addr + SPI_MTC, len & 0xffffff);
	write32(addr + SPI_BCC, len & 0xffffff);
	for(i = 0; i < len; ++i)
		write8(addr + SPI_TXD, *buf++);
}

static int sys_spi_transfer(void * txbuf, void * rxbuf, int len)
{
	virtual_addr_t addr = 0x01c05000;
	int count = len;
	uint8_t * tx = txbuf;
	uint8_t * rx = rxbuf;
	uint8_t val;
	unsigned int n, i;

	while(count > 0)
	{
		n = (count <= 64) ? count : 64;
		write32(addr + SPI_MBC, n);
		sys_spi_write_txbuf(tx, n);
		write32(addr + SPI_TCR, read32(addr + SPI_TCR) | (1 << 31));

		while((read32(addr + SPI_FSR) & 0xff) < n);
		for(i = 0; i < n; i++)
		{
			val = read8(addr + SPI_RXD);
			if(rx)
				*rx++ = val;
		}

		if(tx)
			tx += n;
		count -= n;
	}
	return len;
}

static int sys_spi_write_then_read(void * txbuf, int txlen, void * rxbuf, int rxlen)
{
	if(sys_spi_transfer(txbuf, NULL, txlen) != txlen)
		return -1;
	if(sys_spi_transfer(NULL, rxbuf, rxlen) != rxlen)
		return -1;
	return 0;
}

void sys_spi_flash_read(int addr, void * buf, int count)
{
	uint8_t tx[4];

	tx[0] = 0x03;
	tx[1] = (uint8_t)(addr >> 16);
	tx[2] = (uint8_t)(addr >> 8);
	tx[3] = (uint8_t)(addr >> 0);
	sys_spi_select();
	sys_spi_write_then_read(tx, 4, buf, count);
	sys_spi_deselect();
}
