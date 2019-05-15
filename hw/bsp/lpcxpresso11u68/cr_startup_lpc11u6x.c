//*****************************************************************************
// LPC11U6x Microcontroller Startup code for use with LPCXpresso IDE
//
// Version : 140113
//*****************************************************************************
//
// Copyright(C) NXP Semiconductors, 2014
// All rights reserved.
//
// Software that is described herein is for illustrative purposes only
// which provides customers with programming information regarding the
// LPC products.  This software is supplied "AS IS" without any warranties of
// any kind, and NXP Semiconductors and its licensor disclaim any and
// all warranties, express or implied, including all implied warranties of
// merchantability, fitness for a particular purpose and non-infringement of
// intellectual property rights.  NXP Semiconductors assumes no responsibility
// or liability for the use of the software, conveys no license or rights under any
// patent, copyright, mask work right, or any other intellectual property rights in
// or to any products. NXP Semiconductors reserves the right to make changes
// in the software without notification. NXP Semiconductors also makes no
// representation or warranty that such application will be suitable for the
// specified use without further testing or modification.
//
// Permission to use, copy, modify, and distribute this software and its
// documentation is hereby granted, under NXP Semiconductors' and its
// licensor's relevant copyrights in the software, without fee, provided that it
// is used in conjunction with NXP Semiconductors microcontrollers.  This
// copyright, permission, and disclaimer notice must appear in all copies of
// this code.
//*****************************************************************************

#if defined (__cplusplus)
#ifdef __REDLIB__
#error Redlib does not support C++
#else
//*****************************************************************************
//
// The entry point for the C++ library startup
//
//*****************************************************************************
extern "C" {
    extern void __libc_init_array(void);
}
#endif
#endif

#define WEAK __attribute__ ((weak))
#define ALIAS(f) __attribute__ ((weak, alias (#f)))

//*****************************************************************************
#if defined (__cplusplus)
extern "C" {
#endif

//*****************************************************************************
#if defined (__USE_CMSIS) || defined (__USE_LPCOPEN)
// Declaration of external SystemInit function
extern void SystemInit(void);
#endif

// Patch the AEABI integer divide functions to use MCU's romdivide library
#ifdef __USE_ROMDIVIDE
// Location in memory that holds the address of the ROM Driver table
#define PTR_ROM_DRIVER_TABLE ((unsigned int *)(0x1FFF1FF8))
// Variables to store addresses of idiv and udiv functions within MCU ROM
unsigned int *pDivRom_idiv;
unsigned int *pDivRom_uidiv;
#endif

//*****************************************************************************
//
// Forward declaration of the default handlers. These are aliased.
// When the application defines a handler (with the same name), this will 
// automatically take precedence over these weak definitions
//
//*****************************************************************************
     void ResetISR(void);
WEAK void NMI_Handler(void);
WEAK void HardFault_Handler(void);
WEAK void SVC_Handler(void);
WEAK void PendSV_Handler(void);
WEAK void SysTick_Handler(void);
WEAK void IntDefaultHandler(void);
//*****************************************************************************
//
// Forward declaration of the specific IRQ handlers. These are aliased
// to the IntDefaultHandler, which is a 'forever' loop. When the application
// defines a handler (with the same name), this will automatically take
// precedence over these weak definitions
//
//*****************************************************************************
void PIN_INT0_IRQHandler (void) ALIAS(IntDefaultHandler);
void PIN_INT1_IRQHandler (void) ALIAS(IntDefaultHandler);
void PIN_INT2_IRQHandler (void) ALIAS(IntDefaultHandler);
void PIN_INT3_IRQHandler (void) ALIAS(IntDefaultHandler);
void PIN_INT4_IRQHandler (void) ALIAS(IntDefaultHandler);
void PIN_INT5_IRQHandler (void) ALIAS(IntDefaultHandler);
void PIN_INT6_IRQHandler (void) ALIAS(IntDefaultHandler);
void PIN_INT7_IRQHandler (void) ALIAS(IntDefaultHandler);
void GINT0_IRQHandler (void) ALIAS(IntDefaultHandler);
void GINT1_IRQHandler (void) ALIAS(IntDefaultHandler);
void I2C1_IRQHandler (void) ALIAS(IntDefaultHandler);
void USART1_4_IRQHandler (void) ALIAS(IntDefaultHandler);
void USART2_3_IRQHandler (void) ALIAS(IntDefaultHandler);
void SCT0_1_IRQHandler (void) ALIAS(IntDefaultHandler);
void SSP1_IRQHandler (void) ALIAS(IntDefaultHandler);
void I2C0_IRQHandler (void) ALIAS(IntDefaultHandler);
void TIMER16_0_IRQHandler (void) ALIAS(IntDefaultHandler);
void TIMER16_1_IRQHandler (void) ALIAS(IntDefaultHandler);
void TIMER32_0_IRQHandler (void) ALIAS(IntDefaultHandler);
void TIMER32_1_IRQHandler (void) ALIAS(IntDefaultHandler);
void SSP0_IRQHandler (void) ALIAS(IntDefaultHandler);
void USART0_IRQHandler (void) ALIAS(IntDefaultHandler);
void USB_IRQHandler (void) ALIAS(IntDefaultHandler);
void USB_FIQHandler (void) ALIAS(IntDefaultHandler);
void ADCA_IRQHandler (void) ALIAS(IntDefaultHandler);
void RTC_IRQHandler (void) ALIAS(IntDefaultHandler);
void BOD_WDT_IRQHandler (void) ALIAS(IntDefaultHandler);
void FMC_IRQHandler (void) ALIAS(IntDefaultHandler);
void DMA_IRQHandler (void) ALIAS(IntDefaultHandler);
void ADCB_IRQHandler (void) ALIAS(IntDefaultHandler);
void USBWakeup_IRQHandler (void) ALIAS(IntDefaultHandler);

//*****************************************************************************
// The entry point for the application.
// __main() is the entry point for redlib based applications
// main() is the entry point for newlib based applications
//*****************************************************************************
#if defined (__REDLIB__)
extern void __main(void);
#endif
extern int main(void);
//*****************************************************************************
//
// External declaration for the pointer to the stack top from the Linker Script
//
//*****************************************************************************
extern void _vStackTop(void);

//*****************************************************************************
#if defined (__cplusplus)
} // extern "C"
#endif
//*****************************************************************************
//
// The vector table.  Note that the proper constructs must be placed on this to
// ensure that it ends up at physical address 0x0000.0000.
//
//*****************************************************************************
extern void (* const g_pfnVectors[])(void);
__attribute__ ((section(".isr_vector"))) __attribute__ ((used))
void (* const g_pfnVectors[])(void) = {
    &_vStackTop,                     // The initial stack pointer
    ResetISR,                        // The reset handler
    NMI_Handler,                     // The NMI handler
    HardFault_Handler,               // The hard fault handler
    0,                               // Reserved
    0,                               // Reserved
    0,                               // Reserved
    0,                               // Reserved
    0,                               // Reserved
    0,                               // Reserved
    0,                               // Reserved
    SVC_Handler,                     // SVCall handler
    0,                               // Reserved
    0,                               // Reserved
    PendSV_Handler,                  // The PendSV handler
    SysTick_Handler,                 // The SysTick handler

    // LPC11U6x specific handlers
    PIN_INT0_IRQHandler,             //  0 - GPIO pin interrupt 0
    PIN_INT1_IRQHandler,             //  1 - GPIO pin interrupt 1
    PIN_INT2_IRQHandler,             //  2 - GPIO pin interrupt 2
    PIN_INT3_IRQHandler,             //  3 - GPIO pin interrupt 3
    PIN_INT4_IRQHandler,             //  4 - GPIO pin interrupt 4
    PIN_INT5_IRQHandler,             //  5 - GPIO pin interrupt 5
    PIN_INT6_IRQHandler,             //  6 - GPIO pin interrupt 6
    PIN_INT7_IRQHandler,             //  7 - GPIO pin interrupt 7
    GINT0_IRQHandler,                //  8 - GPIO GROUP0 interrupt
    GINT1_IRQHandler,                //  9 - GPIO GROUP1 interrupt
    I2C1_IRQHandler,                 // 10 - I2C1
    USART1_4_IRQHandler,             // 11 - combined USART1 & 4 interrupt
    USART2_3_IRQHandler,             // 12 - combined USART2 & 3 interrupt
    SCT0_1_IRQHandler,               // 13 - combined SCT0 and 1 interrupt
    SSP1_IRQHandler,                 // 14 - SPI/SSP1 Interrupt
    I2C0_IRQHandler,                 // 15 - I2C0
    TIMER16_0_IRQHandler,            // 16 - CT16B0 (16-bit Timer 0)
    TIMER16_1_IRQHandler,            // 17 - CT16B1 (16-bit Timer 1)
    TIMER32_0_IRQHandler,            // 18 - CT32B0 (32-bit Timer 0)
    TIMER32_1_IRQHandler,            // 19 - CT32B1 (32-bit Timer 1)
    SSP0_IRQHandler,                 // 20 - SPI/SSP0 Interrupt
    USART0_IRQHandler,               // 21 - USART0
    USB_IRQHandler,                  // 22 - USB IRQ
    USB_FIQHandler,                  // 23 - USB FIQ
    ADCA_IRQHandler,                 // 24 - ADC A(A/D Converter)
    RTC_IRQHandler,                  // 25 - Real Time CLock interrpt
    BOD_WDT_IRQHandler,              // 25 - Combined Brownout/Watchdog interrupt
    FMC_IRQHandler,                  // 27 - IP2111 Flash Memory Controller
    DMA_IRQHandler,                  // 28 - DMA interrupt
    ADCB_IRQHandler,                 // 24 - ADC B (A/D Converter)
    USBWakeup_IRQHandler,            // 30 - USB wake-up interrupt
    0,                               // 31 - Reserved
};

//*****************************************************************************
// Functions to carry out the initialization of RW and BSS data sections. These
// are written as separate functions rather than being inlined within the
// ResetISR() function in order to cope with MCUs with multiple banks of
// memory.
//*****************************************************************************
__attribute__ ((section(".after_vectors")))
void data_init(unsigned int romstart, unsigned int start, unsigned int len) {
    unsigned int *pulDest = (unsigned int*) start;
    unsigned int *pulSrc = (unsigned int*) romstart;
    unsigned int loop;
    for (loop = 0; loop < len; loop = loop + 4)
        *pulDest++ = *pulSrc++;
}

__attribute__ ((section(".after_vectors")))
void bss_init(unsigned int start, unsigned int len) {
    unsigned int *pulDest = (unsigned int*) start;
    unsigned int loop;
    for (loop = 0; loop < len; loop = loop + 4)
        *pulDest++ = 0;
}

//*****************************************************************************
// The following symbols are constructs generated by the linker, indicating
// the location of various points in the "Global Section Table". This table is
// created by the linker via the Code Red managed linker script mechanism. It
// contains the load address, execution address and length of each RW data
// section and the execution and length of each BSS (zero initialized) section.
//*****************************************************************************
extern unsigned int __data_section_table;
extern unsigned int __data_section_table_end;
extern unsigned int __bss_section_table;
extern unsigned int __bss_section_table_end;

//*****************************************************************************
// Reset entry point for your code.
// Sets up a simple runtime environment and initializes the C/C++
// library.
//*****************************************************************************
__attribute__ ((section(".after_vectors")))
void
ResetISR(void) {

    // Optionally enable RAM banks that may be off by default at reset
#if !defined (DONT_ENABLE_DISABLED_RAMBANKS)
    volatile unsigned int *SYSCON_SYSAHBCLKCTRL = (unsigned int *) 0x40048080;
    // Ensure that RAM1(26) and USBSRAM(27) bits in SYSAHBCLKCTRL are set
    *SYSCON_SYSAHBCLKCTRL |= (1 << 26) | (1 <<27);
#endif

    //
    // Copy the data sections from flash to SRAM.
    //
    unsigned int LoadAddr, ExeAddr, SectionLen;
    unsigned int *SectionTableAddr;

    // Load base address of Global Section Table
    SectionTableAddr = &__data_section_table;

    // Copy the data sections from flash to SRAM.
    while (SectionTableAddr < &__data_section_table_end) {
        LoadAddr = *SectionTableAddr++;
        ExeAddr = *SectionTableAddr++;
        SectionLen = *SectionTableAddr++;
        data_init(LoadAddr, ExeAddr, SectionLen);
    }
    // At this point, SectionTableAddr = &__bss_section_table;
    // Zero fill the bss segment
    while (SectionTableAddr < &__bss_section_table_end) {
        ExeAddr = *SectionTableAddr++;
        SectionLen = *SectionTableAddr++;
        bss_init(ExeAddr, SectionLen);
    }

    // Patch the AEABI integer divide functions to use MCU's romdivide library
#ifdef __USE_ROMDIVIDE
    // Get address of Integer division routines function table in ROM
    unsigned int *div_ptr = (unsigned int *)((unsigned int *)*(PTR_ROM_DRIVER_TABLE))[4];
    // Get addresses of integer divide routines in ROM
    // These address are then used by the code in aeabi_romdiv_patch.s
    pDivRom_idiv = (unsigned int *)div_ptr[0];
    pDivRom_uidiv = (unsigned int *)div_ptr[1];
#endif

#if defined (__USE_CMSIS) || defined (__USE_LPCOPEN)
    SystemInit();
#endif

#if defined (__cplusplus)
    //
    // Call C++ library initialisation
    //
    __libc_init_array();
#endif

#if defined (__REDLIB__)
    // Call the Redlib library, which in turn calls main()
    __main() ;
#else
    main();
#endif
    //
    // main() shouldn't return, but if it does, we'll just enter an infinite loop
    //
    while (1) {
        ;
    }
}

//*****************************************************************************
// Default exception handlers. Override the ones here by defining your own
// handler routines in your application code.
//*****************************************************************************
__attribute__ ((section(".after_vectors")))
void NMI_Handler(void)
{   while(1) { }
}
__attribute__ ((section(".after_vectors")))
void HardFault_Handler(void)
{   while(1) { }
}

__attribute__ ((section(".after_vectors")))
void SVC_Handler(void)
{   while(1) { }
}

__attribute__ ((section(".after_vectors")))
void PendSV_Handler(void)
{   while(1) { }
}

__attribute__ ((section(".after_vectors")))
void SysTick_Handler(void)
{   while(1) { }
}

//*****************************************************************************
//
// Processor ends up here if an unexpected interrupt occurs or a specific
// handler is not present in the application code.
//
//*****************************************************************************
__attribute__ ((section(".after_vectors")))
void IntDefaultHandler(void)
{   while(1) { }
}
