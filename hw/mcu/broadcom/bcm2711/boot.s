.section ".text.boot"  // Make sure the linker puts this at the start of the kernel image

.global _start  // Execution starts here

_start:
    // Check processor ID is zero (executing on main core), else hang
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbz     x1, 2f
    // We're not on the main core, so hang in an infinite wait loop
1:  wfe
    b       1b
2:  // We're on the main core!

    // Set stack to start below our code
    ldr     x1, =_start
    mov     sp, x1

    adr    x0, vectors        // load VBAR_EL1 with virtual
    // msr    vbar_el3, x0        // vector table address
    msr    vbar_el1, x0        // vector table address
    msr    vbar_el2, x0        // vector table address
    isb

    // Clean the BSS section
    ldr     x1, =__bss_start     // Start address
    ldr     w2, =__bss_size      // Size of the section
3:  cbz     w2, 4f               // Quit loop if zero
    str     xzr, [x1], #8
    sub     w2, w2, #1
    cbnz    w2, 3b               // Loop if non-zero

    // Jump to our main() routine in C (make sure it doesn't return)
4:  bl      main
    // In case it does return, halt the master core too
    b       1b

.macro  ventry  label
.align  7
b   \label
.endm

.macro handle_invalid_entry type
irq_entry
mov x0, #\type
mrs x1, esr_el1
mrs x2, elr_el1
mrs x3, esr_el2
mrs x4, elr_el2
b   err_hang
.endm

.macro  irq_entry
sub sp, sp, #272
stp x0, x1, [sp, #16 * 0]
stp x2, x3, [sp, #16 * 1]
stp x4, x5, [sp, #16 * 2]
stp x6, x7, [sp, #16 * 3]
stp x8, x9, [sp, #16 * 4]
stp x10, x11, [sp, #16 * 5]
stp x12, x13, [sp, #16 * 6]
stp x14, x15, [sp, #16 * 7]
stp x16, x17, [sp, #16 * 8]
stp x18, x19, [sp, #16 * 9]
stp x20, x21, [sp, #16 * 10]
stp x22, x23, [sp, #16 * 11]
stp x24, x25, [sp, #16 * 12]
stp x26, x27, [sp, #16 * 13]
stp x28, x29, [sp, #16 * 14]
str x30, [sp, #16 * 15] 
.endm

.macro  irq_exit
ldp x0, x1, [sp, #16 * 0]
ldp x2, x3, [sp, #16 * 1]
ldp x4, x5, [sp, #16 * 2]
ldp x6, x7, [sp, #16 * 3]
ldp x8, x9, [sp, #16 * 4]
ldp x10, x11, [sp, #16 * 5]
ldp x12, x13, [sp, #16 * 6]
ldp x14, x15, [sp, #16 * 7]
ldp x16, x17, [sp, #16 * 8]
ldp x18, x19, [sp, #16 * 9]
ldp x20, x21, [sp, #16 * 10]
ldp x22, x23, [sp, #16 * 11]
ldp x24, x25, [sp, #16 * 12]
ldp x26, x27, [sp, #16 * 13]
ldp x28, x29, [sp, #16 * 14]
ldr x30, [sp, #16 * 15] 
add sp, sp, #272       
eret
.endm


/*
 * Exception vectors.
 */
.align  11
.globl vectors 
vectors:
    ventry  sync_invalid_el1t           // Synchronous EL1t
    ventry  irq_invalid_el1t            // IRQ EL1t
    ventry  fiq_invalid_el1t            // FIQ EL1t
    ventry  error_invalid_el1t          // Error EL1t

    ventry  sync_invalid_el1h           // Synchronous EL1h
    ventry  el1_irq                 // IRQ EL1h
    ventry  fiq_invalid_el1h            // FIQ EL1h
    ventry  error_invalid_el1h          // Error EL1h

    ventry  sync_invalid_el0_64         // Synchronous 64-bit EL0
    ventry  irq_invalid_el0_64          // IRQ 64-bit EL0
    ventry  fiq_invalid_el0_64          // FIQ 64-bit EL0
    ventry  error_invalid_el0_64            // Error 64-bit EL0

    ventry  sync_invalid_el0_32         // Synchronous 32-bit EL0
    ventry  irq_invalid_el0_32          // IRQ 32-bit EL0
    ventry  fiq_invalid_el0_32          // FIQ 32-bit EL0
    ventry  error_invalid_el0_32            // Error 32-bit EL0

sync_invalid_el1t:
    handle_invalid_entry  0

irq_invalid_el1t:
    handle_invalid_entry  1

fiq_invalid_el1t:
    handle_invalid_entry  2

error_invalid_el1t:
    handle_invalid_entry  3

sync_invalid_el1h:
    handle_invalid_entry  4

fiq_invalid_el1h:
    handle_invalid_entry  5

error_invalid_el1h:
    handle_invalid_entry  6

sync_invalid_el0_64:
    handle_invalid_entry  7

irq_invalid_el0_64:
    handle_invalid_entry  8

fiq_invalid_el0_64:
    handle_invalid_entry  9

error_invalid_el0_64:
    handle_invalid_entry  10

sync_invalid_el0_32:
    handle_invalid_entry  11

irq_invalid_el0_32:
    handle_invalid_entry  12

fiq_invalid_el0_32:
    handle_invalid_entry  13

error_invalid_el0_32:
    handle_invalid_entry  14

el1_irq:
    irq_entry 
    bl  handle_irq
    irq_exit 

.globl err_hang
err_hang: b err_hang
