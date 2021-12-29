/*
 * start.S
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

/*
 * Exception vector table
 */
.text
	.arm

	.global	_start
_start:
	/* Boot head information for BROM */
	.long 0xea000016
	.byte 'e', 'G', 'O', 'N', '.', 'B', 'T', '0'
	.long 0, __bootloader_size
	.byte 'S', 'P', 'L', 2
	.long 0, 0
	.long 0, 0, 0, 0, 0, 0, 0, 0
	.long 0, 0, 0, 0, 0, 0, 0, 0	/* 0x40 - boot params, 0x58 - fel boot type, 0x5c - dram size */

_vector:
	b reset
	ldr pc, _undefined_instruction
	ldr pc, _software_interrupt
	ldr pc, _prefetch_abort
	ldr pc, _data_abort
	ldr pc, _not_used
	ldr pc, _irq
	ldr pc, _fiq

_undefined_instruction:
	.word undefined_instruction
_software_interrupt:
	.word software_interrupt
_prefetch_abort:
	.word prefetch_abort
_data_abort:
	.word data_abort
_not_used:
	.word not_used
_irq:
	.word irq
_fiq:
	.word fiq

/*
 * The actual reset code
 */
reset:
	/* Save boot params to 0x00000040 */
	ldr r0, =0x00000040
	str sp, [r0, #0]
	str lr, [r0, #4]
	mrs lr, cpsr
	str lr, [r0, #8]
	mrc p15, 0, lr, c1, c0, 0
	str lr, [r0, #12]
	mrc p15, 0, lr, c1, c0, 0
	str lr, [r0, #16]

	/* Check boot type just for fel */
	mov r0, #0x0
	ldr r1, [r0, #8]
	ldr r2, =0x4c45462e
	cmp r1, r2
	bne 1f
	ldr r1, =0x1
	str r1, [r0, #0x58]
1:	nop

	/* Enter svc mode and mask interrupts */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0xd3
	msr cpsr, r0

	/* Set vector to the low address */
	mrc p15, 0, r0, c1, c0, 0
	bic r0, #(1<<13)
	mcr p15, 0, r0, c1, c0, 0

	/* Copy vector to the correct address */
	adr r0, _vector
	mrc p15, 0, r2, c1, c0, 0
	ands r2, r2, #(1 << 13)
	ldreq r1, =0x00000000
	ldrne r1, =0xffff0000
	ldmia r0!, {r2-r8, r10}
	stmia r1!, {r2-r8, r10}
	ldmia r0!, {r2-r8, r10}
	stmia r1!, {r2-r8, r10}

	/* Initial system clock, ddr and uart */
	bl sys_clock_init
	bl sys_uart_init

	/* Check if we are already running from dram */
	adr r0, _start
	ldr r1, =_start
	cmp r0, r1
	beq _init_mmu

	/* Init dram if not running from dram */
	bl sys_dram_init

	/* Copy bootloader to faster dram (sram is slower) */
	ldr r0, =0x81f80000
	adr r1, _start
	mov r2, #0x4000
	bl memcpy
	ldr r0, =_copy_self
	ldr r1, =_start
	sub r0, r0, r1
	ldr r1, =0x81f80000
	add r0, r0, r1
	mov pc, r0
_init_mmu:
	bl sys_mmu_init
	b 1f
_copy_self:
	/* Copyself to link address */
	bl sys_copyself

1:	nop
	/* Initialize stacks */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r1, r0, #0x1b
	msr cpsr_cxsf, r1
	ldr sp, _stack_und_end

	bic r0, r0, #0x1f
	orr r1, r0, #0x17
	msr cpsr_cxsf, r1
	ldr sp, _stack_abt_end

	bic r0, r0, #0x1f
	orr r1, r0, #0x12
	msr cpsr_cxsf, r1
	ldr sp, _stack_irq_end

	bic r0, r0, #0x1f
	orr r1, r0, #0x11
	msr cpsr_cxsf, r1
	ldr sp, _stack_fiq_end

	bic r0, r0, #0x1f
	orr r1, r0, #0x13
	msr cpsr_cxsf, r1
	ldr sp, _stack_srv_end

	/* Copy data section */
	ldr r0, _data_start
	ldr r1, _data_shadow_start
	ldr r2, _data_shadow_end
	sub r2, r2, r1
	bl memcpy

	/* Clear bss section */
	ldr r0, _bss_start
	ldr r2, _bss_end
	sub r2, r2, r0
	mov r1, #0
	bl memset

	/* Call _main */
	ldr r1, =_main
	mov pc, r1
_main:
	bl main
	b _main

	.global return_to_fel
return_to_fel:
	mov r0, #0x4
	mov r1, #'e'
	strb r1, [r0, #0]
	mov r1, #'G'
	strb r1, [r0, #1]
	mov r1, #'O'
	strb r1, [r0, #2]
	mov r1, #'N'
	strb r1, [r0, #3]
	mov r1, #'.'
	strb r1, [r0, #4]
	mov r1, #'F'
	strb r1, [r0, #5]
	mov r1, #'E'
	strb r1, [r0, #6]
	mov r1, #'L'
	strb r1, [r0, #7]
	ldr r0, =0x00000040
	ldr sp, [r0, #0]
	ldr lr, [r0, #4]
	ldr r1, [r0, #16]
	mcr p15, 0, r1, c1, c0, 0
	ldr r1, [r0, #12]
	mcr p15, 0, r1, c1, c0, 0
	ldr r1, [r0, #8]
	msr cpsr, r1
	bx lr

/*
 * Exception handlers
 */
	.align 5
undefined_instruction:
	b .

	.align 5
software_interrupt:
	b .

	.align 5
prefetch_abort:
	b .

	.align 5
data_abort:
	b .

	.align 5
not_used:
	b .

	.align 5
irq:
	ldr sp, _stack_irq_end
	sub sp, sp, #72
	stmia sp, {r0 - r12}
	add r8, sp, #60
	stmdb r8, {sp, lr}^
	str lr, [r8, #0]
	mrs r6, spsr
	str r6, [r8, #4]
	str r0, [r8, #8]
	mov r0, sp
	bl arm32_do_irq
	ldmia sp, {r0 - lr}^
	mov r0, r0
	ldr lr, [sp, #60]
	add sp, sp, #72
	subs pc, lr, #4

	.align 5
fiq:
	ldr sp, _stack_irq_end
	sub sp, sp, #72
	stmia sp, {r0 - r12}
	add r8, sp, #60
	stmdb r8, {sp, lr}^
	str lr, [r8, #0]
	mrs r6, spsr
	str r6, [r8, #4]
	str r0, [r8, #8]
	mov r0, sp
	bl arm32_do_fiq
	ldmia sp, {r0 - lr}^
	mov r0, r0
	ldr lr, [sp, #60]
	add sp, sp, #72
	subs pc, lr, #4

/*
 * The location of section
 */
 	.align 4
_image_start:
	.long __image_start
_image_end:
	.long __image_end
_data_shadow_start:
	.long __data_shadow_start
_data_shadow_end:
	.long __data_shadow_end
_data_start:
	.long __data_start
_data_end:
	.long __data_end
_bss_start:
	.long __bss_start
_bss_end:
	.long __bss_end
_stack_und_end:
	.long __stack_und_end
_stack_abt_end:
	.long __stack_abt_end
_stack_irq_end:
	.long __stack_irq_end
_stack_fiq_end:
	.long __stack_fiq_end
_stack_srv_end:
	.long __stack_srv_end
