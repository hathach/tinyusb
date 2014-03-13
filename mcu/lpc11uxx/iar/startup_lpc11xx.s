/**************************************************
 *
 * Part one of the system initialization code, contains low-level
 * initialization, plain thumb variant.
 *
 * Copyright 2009 IAR Systems. All rights reserved.
 *
 * $Revision: 47021 $
 *
 **************************************************/

;
; The modules in this file are included in the libraries, and may be replaced
; by any user-defined modules that define the PUBLIC symbol _program_start or
; a user defined start symbol.
; To override the cstartup defined in the library, simply add your modified
; version to the workbench project.
;
; The vector table is normally located at address 0.
; When debugging in RAM, it can be located in RAM, aligned to at least 2^6.
; The name "__vector_table" has special meaning for C-SPY:
; it is where the SP start value is found, and the NVIC vector
; table register (VTOR) is initialized to this address if != 0.
;
; Cortex-M version
;

				MODULE  ?cstartup

				;; Forward declaration of sections.
				SECTION CSTACK:DATA:NOROOT(3)

				SECTION .intvec:CODE:NOROOT(2)

				EXTERN  __iar_program_start

				PUBLIC  __vector_table
				PUBLIC  __vector_table_0x1c
				DATA
__vector_table
				DCD     sfe(CSTACK)                 ; Top of Stack
				DCD     __iar_program_start         ; Reset Handler
				DCD     NMI_Handler                 ; NMI Handler
				DCD     HardFault_Handler           ; Hard Fault Handler
				DCD     MemManage_Handler           ; MPU Fault Handler
				DCD     BusFault_Handler            ; Bus Fault Handler
				DCD     UsageFault_Handler          ; Usage Fault Handler
__vector_table_0x1c
				DCD     0                           ; Reserved
				DCD     0                           ; Reserved
				DCD     0                           ; Reserved
				DCD     0                           ; Reserved
				DCD     SVC_Handler                 ; SVCall Handler
				DCD     DebugMon_Handler            ; Debug Monitor Handler
				DCD     0                           ; Reserved
				DCD     PendSV_Handler              ; PendSV Ha dler
				DCD     SysTick_Handler             ; SysTick Handler
				; External Interrupts
				DCD     FLEX_INT0_IRQHandler        ; All GPIO pin can be routed to FLEX_INTx
				DCD     FLEX_INT1_IRQHandler          
				DCD     FLEX_INT2_IRQHandler                       
				DCD     FLEX_INT3_IRQHandler                         
				DCD     FLEX_INT4_IRQHandler                        
				DCD     FLEX_INT5_IRQHandler
				DCD     FLEX_INT6_IRQHandler
				DCD     FLEX_INT7_IRQHandler                       
				DCD     GINT0_IRQHandler                         
				DCD     GINT1_IRQHandler            ; PIO0 (0:7)              
				DCD     Reserved_IRQHandler         ; Reserved
				DCD     Reserved_IRQHandler
				DCD     Reserved_IRQHandler       
				DCD     Reserved_IRQHandler                       
				DCD     SSP1_IRQHandler             ; SSP1               
				DCD     I2C_IRQHandler              ; I2C
				DCD     TIMER16_0_IRQHandler        ; 16-bit Timer0
				DCD     TIMER16_1_IRQHandler        ; 16-bit Timer1
				DCD     TIMER32_0_IRQHandler        ; 32-bit Timer0
				DCD     TIMER32_1_IRQHandler        ; 32-bit Timer1
				DCD     SSP0_IRQHandler             ; SSP0
				DCD     UART_IRQHandler             ; UART
				DCD     USB_IRQHandler              ; USB IRQ
				DCD     USB_FIQHandler              ; USB FIQ
				DCD     ADC_IRQHandler              ; A/D Converter
				DCD     WDT_IRQHandler              ; Watchdog timer
				DCD     BOD_IRQHandler              ; Brown Out Detect
				DCD     FMC_IRQHandler              ; IP2111 Flash Memory Controller
				DCD     Reserved_IRQHandler         ; Reserved
				DCD     Reserved_IRQHandler         ; Reserved
				DCD     USBWakeup_IRQHandler        ; USB wake up
				DCD     Reserved_IRQHandler         ; Reserved


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Default interrupt handlers.
;;
        THUMB
        SECTION .text:CODE:REORDER:NOROOT(1)
        PUBWEAK NMI_Handler
        PUBWEAK HardFault_Handler
        PUBWEAK MemManage_Handler
        PUBWEAK BusFault_Handler
        PUBWEAK UsageFault_Handler
        PUBWEAK SVC_Handler
        PUBWEAK DebugMon_Handler
        PUBWEAK PendSV_Handler
        PUBWEAK SysTick_Handler
        PUBWEAK FLEX_INT0_IRQHandler
        PUBWEAK FLEX_INT1_IRQHandler
        PUBWEAK FLEX_INT2_IRQHandler
        PUBWEAK FLEX_INT3_IRQHandler
        PUBWEAK FLEX_INT4_IRQHandler
        PUBWEAK FLEX_INT5_IRQHandler
        PUBWEAK FLEX_INT6_IRQHandler
        PUBWEAK FLEX_INT7_IRQHandler
        PUBWEAK GINT0_IRQHandler
        PUBWEAK GINT1_IRQHandler
        PUBWEAK SSP1_IRQHandler
        PUBWEAK I2C_IRQHandler
        PUBWEAK TIMER16_0_IRQHandler
        PUBWEAK TIMER16_1_IRQHandler
        PUBWEAK TIMER32_0_IRQHandler
        PUBWEAK TIMER32_1_IRQHandler
        PUBWEAK SSP0_IRQHandler
        PUBWEAK UART_IRQHandler
        PUBWEAK USB_IRQHandler
        PUBWEAK USB_FIQHandler
        PUBWEAK ADC_IRQHandler
        PUBWEAK WDT_IRQHandler
        PUBWEAK BOD_IRQHandler
        PUBWEAK FMC_IRQHandler
        PUBWEAK USBWakeup_IRQHandler
        PUBWEAK Reserved_IRQHandler

NMI_Handler
HardFault_Handler
MemManage_Handler
BusFault_Handler
UsageFault_Handler
SVC_Handler
DebugMon_Handler
PendSV_Handler
SysTick_Handler
FLEX_INT0_IRQHandler
FLEX_INT1_IRQHandler
FLEX_INT2_IRQHandler
FLEX_INT3_IRQHandler
FLEX_INT4_IRQHandler
FLEX_INT5_IRQHandler
FLEX_INT6_IRQHandler
FLEX_INT7_IRQHandler
GINT0_IRQHandler
GINT1_IRQHandler
SSP1_IRQHandler
I2C_IRQHandler
TIMER16_0_IRQHandler
TIMER16_1_IRQHandler
TIMER32_0_IRQHandler
TIMER32_1_IRQHandler
SSP0_IRQHandler
UART_IRQHandler
USB_IRQHandler
USB_FIQHandler
ADC_IRQHandler
WDT_IRQHandler
BOD_IRQHandler
FMC_IRQHandler
USBWakeup_IRQHandler
Reserved_IRQHandler
Default_Handler:
				B Default_Handler

        SECTION .crp:CODE:ROOT(2)
        DATA
/* Code Read Protection
NO_ISP  0x4E697370 -  Prevents sampling of pin PIO0_1 for entering ISP mode
CRP1    0x12345678 - Write to RAM command cannot access RAM below 0x10000300.
                   - Copy RAM to flash command can not write to Sector 0.
                   - Erase command can erase Sector 0 only when all sectors
                     are selected for erase.
                   - Compare command is disabled.
                   - Read Memory command is disabled.
CRP2    0x87654321 - Read Memory is disabled.
                   - Write to RAM is disabled.
                   - "Go" command is disabled.
                   - Copy RAM to flash is disabled.
                   - Compare is disabled.
CRP3    0x43218765 - Access to chip via the SWD pins is disabled. ISP entry
                     by pulling PIO0_1 LOW is disabled if a valid user code is
                     present in flash sector 0.
Caution: If CRP3 is selected, no future factory testing can be
performed on the device.
*/
	DCD	0xFFFFFFFF

        END
