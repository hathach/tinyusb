#ifndef __ARM32_H__
#define __ARM32_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct arm_regs_t {
	uint32_t r[13];
	uint32_t sp;
	uint32_t lr;
	uint32_t pc;
	uint32_t cpsr;
};

static inline uint32_t arm32_read_p15_c1(void)
{
	uint32_t value;

	__asm__ __volatile__(
		"mrc p15, 0, %0, c1, c0, 0"
		: "=r" (value)
		:
		: "memory");

	return value;
}

static inline void arm32_write_p15_c1(uint32_t value)
{
	__asm__ __volatile__(
		"mcr p15, 0, %0, c1, c0, 0"
		:
		: "r" (value)
		: "memory");
	arm32_read_p15_c1();
}

static inline void arm32_interrupt_enable(void)
{
	uint32_t tmp;

	__asm__ __volatile__(
		"mrs %0, cpsr\n"
		"bic %0, %0, #(1<<7)\n"
		"msr cpsr_cxsf, %0"
		: "=r" (tmp)
		:
		: "memory");
}

static inline void arm32_interrupt_disable(void)
{
	uint32_t tmp;

	__asm__ __volatile__(
		"mrs %0, cpsr\n"
		"orr %0, %0, #(1<<7)\n"
		"msr cpsr_cxsf, %0"
		: "=r" (tmp)
		:
		: "memory");
}

static inline void arm32_mmu_enable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value | (1 << 0));
}

static inline void arm32_mmu_disable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value & ~(1 << 0));
}

static inline void arm32_dcache_enable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value | (1 << 2));
}

static inline void arm32_dcache_disable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value & ~(1 << 2));
}

static inline void arm32_icache_enable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value | (1 << 12));
}

static inline void arm32_icache_disable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value & ~(1 << 12));
}

static inline uint32_t arm32_smp_processor_id(void)
{
	uint32_t tmp;

	__asm__ __volatile__(
		"mrc p15,0,%0,c0,c0,5\n"
		"and %0,%0,#0x3\n"
		: "=r" (tmp)
		:
		: "memory");
	return tmp;
}

static inline void arm32_ttb_set(uint32_t base)
{
	__asm__ __volatile__(
		"mcr p15, 0, %0, c2, c0, 0"
		:
		: "r" (base)
		: "memory");
}

static inline void arm32_domain_set(uint32_t domain)
{
	__asm__ __volatile__(
		"mcr p15, 0, %0, c3, c0, 0"
		:
		: "r" (domain)
		: "memory");
}

static inline void arm32_tlb_invalidate(void)
{
	__asm__ __volatile__(
		"mov r0, #0\n"
		"mcr p15, 0, r0, c7, c10, 4\n"
		"mcr p15, 0, r0, c8, c6, 0\n"
		"mcr p15, 0, r0, c8, c5, 0\n"
		:
		:
		: "r0");
}

#ifdef __cplusplus
}
#endif

#endif /* __ARM32_H__ */
