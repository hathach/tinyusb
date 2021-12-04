/*
 * exception.c
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
#include <stdio.h>
#include <arm32.h>
#include <string.h>

static void show_regs(struct arm_regs_t * regs)
{
	int i;

	printf("pc : [<%08lx>] lr : [<%08lx>] cpsr: %08lx\r\n", regs->pc, regs->lr, regs->cpsr);
	printf("sp : %08lx\r\n", regs->sp);
	for(i = 12; i >= 0; i--)
	{
        printf("r%-2d: %08lx ", i, regs->r[i]);
	 	if(i % 2 == 0)
	 		printf("\r\n");
	 }
    printf("\r\n");
}

void arm32_do_undefined_instruction(struct arm_regs_t * regs)
{
	//gdbserver_handle_exception(regs);
}

void arm32_do_software_interrupt(struct arm_regs_t * regs)
{
	show_regs(regs);
	regs->pc += 4;
}

void arm32_do_prefetch_abort(struct arm_regs_t * regs)
{
	show_regs(regs);
	regs->pc += 4;
}

void arm32_do_data_abort(struct arm_regs_t * regs)
{
	show_regs(regs);
	regs->pc += 4;
}

_Noreturn void __fatal_error(const char *msg) {
    printf("%s\n", msg);
    while (1);
}

#ifndef NDEBUG
_Noreturn void __assert_func(const char *file, int line, const char *func, const char *expr) {
    //printf("Assertion '%s' failed, at file %s:%d\n", expr, file, line);
    __fatal_error("Assertion failed");
}
#endif