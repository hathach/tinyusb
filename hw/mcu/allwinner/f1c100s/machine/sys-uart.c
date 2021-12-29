/*
 * sys-uart.c
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

#include <types.h>
#include <stdint.h>
#include <io.h>

void sys_uart_init(void)
{
	virtual_addr_t addr;
	uint32_t val;

	/* Config GPIOE1 and GPIOE0 to txd0 and rxd0 */
	addr = 0x01c20890 + 0x00;
	val = read32(addr);
	val &= ~(0xf << ((1 & 0x7) << 2));
	val |= ((0x5 & 0x7) << ((1 & 0x7) << 2));
	write32(addr, val);

	val = read32(addr);
	val &= ~(0xf << ((0 & 0x7) << 2));
	val |= ((0x5 & 0x7) << ((0 & 0x7) << 2));
	write32(addr, val);

	/* Open the clock gate for uart0 */
	addr = 0x01c20068;
	val = read32(addr);
	val |= 1 << 20;
	write32(addr, val);

	/* Deassert uart0 reset */
	addr = 0x01c202d0;
	val = read32(addr);
	val |= 1 << 20;
	write32(addr, val);

	/* Config uart0 to 115200-8-1-0 */
	addr = 0x01c25000;
	write32(addr + 0x04, 0x0);
	write32(addr + 0x08, 0xf7);
	write32(addr + 0x10, 0x0);
	val = read32(addr + 0x0c);
	val |= (1 << 7);
	write32(addr + 0x0c, val);
	write32(addr + 0x00, 0x36 & 0xff);
	write32(addr + 0x04, (0x36 >> 8) & 0xff);
	val = read32(addr + 0x0c);
	val &= ~(1 << 7);
	write32(addr + 0x0c, val);
	val = read32(addr + 0x0c);
	val &= ~0x1f;
	val |= (0x3 << 0) | (0 << 2) | (0x0 << 3);
	write32(addr + 0x0c, val);
}

void sys_uart_putc(char c)
{
	virtual_addr_t addr = 0x01c25000;

	while((read32(addr + 0x7c) & (0x1 << 1)) == 0);
	write32(addr + 0x00, c);
}
