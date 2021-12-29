/*
 * sys-copyself.c
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

extern unsigned char __image_start;
extern unsigned char __image_end;
extern void return_to_fel(void);
extern void sys_mmu_init(void);
extern void sys_uart_putc(char c);
extern void sys_spi_flash_init(void);
extern void sys_spi_flash_exit(void);
extern void sys_spi_flash_read(int addr, void * buf, int count);

enum {
	BOOT_DEVICE_FEL	= 0,
	BOOT_DEVICE_SPI	= 1,
	BOOT_DEVICE_MMC	= 2,
};

static int get_boot_device(void)
{
	uint32_t * t = (void *)0x00000058;

	if(t[0] == 0x1)
		return BOOT_DEVICE_FEL;
	return BOOT_DEVICE_SPI;
}

void sys_copyself(void)
{
	int d = get_boot_device();
	void * mem;
	uint32_t size;

	if(d == BOOT_DEVICE_FEL)
	{
		sys_uart_putc('B');
		sys_uart_putc('o');
		sys_uart_putc('o');
		sys_uart_putc('t');
		sys_uart_putc(' ');
		sys_uart_putc('t');
		sys_uart_putc('o');
		sys_uart_putc(' ');
		sys_uart_putc('F');
		sys_uart_putc('E');
		sys_uart_putc('L');
		sys_uart_putc(' ');
		sys_uart_putc('m');
		sys_uart_putc('o');
		sys_uart_putc('d');
		sys_uart_putc('e');
		sys_uart_putc('\r');
		sys_uart_putc('\n');
		return_to_fel();
	}
	else if(d == BOOT_DEVICE_SPI)
	{
		sys_uart_putc('B');
		sys_uart_putc('o');
		sys_uart_putc('o');
		sys_uart_putc('t');
		sys_uart_putc(' ');
		sys_uart_putc('t');
		sys_uart_putc('o');
		sys_uart_putc(' ');
		sys_uart_putc('S');
		sys_uart_putc('P');
		sys_uart_putc('I');
		sys_uart_putc(' ');
		sys_uart_putc('m');
		sys_uart_putc('o');
		sys_uart_putc('d');
		sys_uart_putc('e');
		sys_uart_putc('\r');
		sys_uart_putc('\n');
		mem = (void *)&__image_start;
		size = &__image_end - &__image_start;
		sys_mmu_init();

		sys_spi_flash_init();
		sys_spi_flash_read(0, mem, size);
		sys_spi_flash_exit();
	}
	else if(d == BOOT_DEVICE_MMC)
	{
		mem = (void *)&__image_start;
		size = (&__image_end - &__image_start + 512) >> 9;
	}
}
