#pragma once

void disable_async_interrupts(void);
void enable_async_interrupts(void);

#define GIC_BASE 0x4c0040000
#define NUM_CPUS 4
#define NUM_SPIS 192
