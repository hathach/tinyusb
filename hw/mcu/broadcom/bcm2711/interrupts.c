#include <stdbool.h>
#include <stdint.h>

void disable_async_interrupts(void) {
    
}

void enable_async_interrupts(void) {
    
}

void Default_Handler(void) {
    while (true) {}
}

__attribute__((weak)) void handle_irq(void) {
    unsigned int irq = *((volatile uint32_t*) 0x3F00B204);
    switch (irq) {
        // case (SYSTEM_TIMER_IRQ_1):
        //     handle_timer_irq();
        //     break;
        default:
            // printf("Unknown pending irq: %x\r\n", irq);
            Default_Handler();
    }
}