#include <stdbool.h>
#include <stdint.h>


#include "mmu.h"

// Each entry is a gig.
volatile uint64_t level_1_table[512] __attribute__((aligned(4096)));

// Third gig has peripherals
uint64_t level_2_0x0_c000_0000_to_0x1_0000_0000[512] __attribute__((aligned(4096)));

void setup_mmu_flat_map(void) {
    // Set the first gig to regular access.
    level_1_table[0] = 0x0000000000000000 | 
                       MM_DESCRIPTOR_MAIR_INDEX(MT_NORMAL_NC) |
                       MM_DESCRIPTOR_ACCESS_FLAG |
                       MM_DESCRIPTOR_BLOCK |
                       MM_DESCRIPTOR_VALID;
    level_1_table[3] = ((uint64_t) level_2_0x0_c000_0000_to_0x1_0000_0000) |
                       MM_DESCRIPTOR_TABLE |
                       MM_DESCRIPTOR_VALID;
    // Set peripherals to register access.
    for (uint64_t i = 480; i < 512; i++) {
        level_2_0x0_c000_0000_to_0x1_0000_0000[i] = (0x00000000c0000000 + (i << 21)) |
                                                    MM_DESCRIPTOR_EXECUTE_NEVER |
                                                    MM_DESCRIPTOR_MAIR_INDEX(MT_DEVICE_nGnRnE) |
                                                    MM_DESCRIPTOR_ACCESS_FLAG |
                                                    MM_DESCRIPTOR_BLOCK |
                                                    MM_DESCRIPTOR_VALID;
    }
    uint64_t mair = MAIR_VALUE;
    uint64_t tcr = TCR_VALUE;
    uint64_t ttbr0 = ((uint64_t) level_1_table) | MM_TTBR_CNP;
    uint64_t sctlr = 0;
    __asm__ volatile (
        // Set MAIR
        "MSR MAIR_EL2, %[mair]\n\t"
        // Set TTBR0
        "MSR TTBR0_EL2, %[ttbr0]\n\t"
        // Set TCR
        "MSR TCR_EL2, %[tcr]\n\t"
        // The ISB forces these changes to be seen before the MMU is enabled.
        "ISB\n\t"
        // Read System Control Register configuration data
        "MRS %[sctlr], SCTLR_EL2\n\t"
        // Write System Control Register configuration data
        "ORR %[sctlr], %[sctlr], #1\n\t"
        // Set [M] bit and enable the MMU.
        "MSR SCTLR_EL2, %[sctlr]\n\t"
        // The ISB forces these changes to be seen by the next instruction
        "ISB\n\t"
        // "AT S1EL2R %[ttbr0]"
        : /* No outputs. */
        : [mair] "r" (mair),
          [tcr] "r" (tcr),
          [ttbr0] "r" (ttbr0),
          [sctlr] "r" (sctlr)
    );
    //__asm__ ("brk #123");
    //while (true) {}
}
