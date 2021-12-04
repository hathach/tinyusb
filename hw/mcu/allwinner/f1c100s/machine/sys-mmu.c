/*
 * sys-mmu.c
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
#include <io.h>
#include <sizes.h>
#include <arm32.h>

static void map_l1_section(uint32_t * ttb, virtual_addr_t virt, physical_addr_t phys, physical_size_t size, int type)
{
	physical_size_t i;

	virt >>= 20;
	phys >>= 20;
	size >>= 20;
	type &= 0x3;

	for(i = size; i > 0; i--, virt++, phys++)
		ttb[virt] = (phys << 20) | (0x3 << 10) | (0x0 << 5) | (type << 2) | (0x2 << 0);
}

void sys_mmu_init(void)
{
	uint32_t * ttb = (uint32_t *)(0x80000000 + SZ_1M * 31);

	map_l1_section(ttb, 0x00000000, 0x00000000, SZ_2G, 0);
	map_l1_section(ttb, 0x80000000, 0x80000000, SZ_2G, 0);
	map_l1_section(ttb, 0x80000000, 0x80000000, SZ_1M * 32, 3);

	arm32_ttb_set((uint32_t)(ttb));
	arm32_tlb_invalidate();
	arm32_domain_set(0x3);
	arm32_mmu_enable();
	arm32_icache_enable();
	arm32_dcache_enable();
}