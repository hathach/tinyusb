/**
  ******************************************************************************
  * @file      startup_mm32m3ux_u_gcc.s
  * @author    AE Team
  * @brief     MM32 devices vector table for GCC toolchain.
  *            This module performs:
  *                - Set the initial SP
  *                - Set the initial PC == Reset_Handler,
  *                - Set the vector table entries with the exceptions ISR address
  *                - Branches to main in the C library (which eventually
  *                  calls main()).
  *            After Reset the Cortex-M3 processor is in Thread mode,
  *            priority is Privileged, and the Stack is set to Main.
  ******************************************************************************
  */

                .syntax     unified
                .cpu        cortex-m3
                .fpu        softvfp
                .thumb

                .global     g_pfnVectors
                .global     Default_Handler

/* start address for the initialization values of the .data section.
defined in linker script */
                .word       _sidata
/* start address for the .data section. defined in linker script */
                .word       _sdata
/* end address for the .data section. defined in linker script */
                .word       _edata
/* start address for the .bss section. defined in linker script */
                .word       _sbss
/* end address for the .bss section. defined in linker script */
                .word       _ebss

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called. 
 * @param  None
 * @retval : None
*/

                .section    .text.Reset_Handler
                .weak       Reset_Handler
                .type       Reset_Handler, %function
Reset_Handler:

/* Copy the data segment initializers from flash to SRAM */  
                movs        r1, #0
                b           LoopCopyDataInit

CopyDataInit:
                ldr         r3, =_sidata
                ldr         r3, [r3, r1]
                str         r3, [r0, r1]
                adds        r1, r1, #4
    
LoopCopyDataInit:
                ldr         r0, =_sdata
                ldr         r3, =_edata
                adds        r2, r0, r1
                cmp         r2, r3
                bcc         CopyDataInit
                ldr         r2, =_sbss
                b           LoopFillZerobss
/* Zero fill the bss segment. */  
FillZerobss:
                movs        r3, #0
                str         r3, [r2], #4
    
LoopFillZerobss:
                ldr         r3, = _ebss
                cmp         r2, r3
                bcc         FillZerobss

/* Call the clock system intitialization function.*/
                bl          SystemInit
/* Call static constructors */
                bl          __libc_init_array
/* Call the application's entry point.*/
                bl          main
                bx          lr    
                .size       Reset_Handler, .-Reset_Handler

/**
 * @brief  This is the code that gets called when the processor receives an 
 *         unexpected interrupt.  This simply enters an infinite loop, preserving
 *         the system state for examination by a debugger.
 * @param  None     
 * @retval None       
*/
                .section    .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
                b           Infinite_Loop
                .size       Default_Handler, .-Default_Handler
/******************************************************************************
*
* The minimal vector table for a Cortex M3. Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/
                .section    .isr_vector,"a",%progbits
                .type       g_pfnVectors, %object
                .size       g_pfnVectors, .-g_pfnVectors


g_pfnVectors:                                                                   
                .word       _estack                                             /*        Top of Stack                         */
                .word       Reset_Handler                                       /*        Reset Handler                        */
                .word       NMI_Handler                                         /*-14     NMI Handler                          */
                .word       HardFault_Handler                                   /*-13     Hard Fault Handler                   */
                .word       MemManage_Handler                                   /*-12     MPU Fault Handler                    */
                .word       BusFault_Handler                                    /*-11     Bus Fault Handler                    */
                .word       UsageFault_Handler                                  /*-10     Usage Fault Handler                  */
                .word       0                                                   /*-9      Reserved                             */
                .word       0                                                   /*-8      Reserved                             */
                .word       0                                                   /*-7      Reserved                             */
                .word       0                                                   /*-6      Reserved                             */
                .word       SVC_Handler                                         /*-5      SVCall Handler                       */
                .word       DebugMon_Handler                                    /*-4      Debug Monitor Handler                */
                .word       0                                                   /*-3      Reserved                             */
                .word       PendSV_Handler                                      /*-2      PendSV Handler                       */
                .word       SysTick_Handler                                     /*-1      SysTick Handler                      */
                                                                                
                /* External Interrupts */
                .word       WWDG_IRQHandler                                     /*0       Window Watchdog                      */
                .word       PVD_IRQHandler                                      /*1       PVD through EXTI Line detect         */
                .word       TAMPER_IRQHandler                                   /*2       Tamper                               */
                .word       RTC_IRQHandler                                      /*3       RTC                                  */
                .word       FLASH_IRQHandler                                    /*4       Flash                                */
                .word       RCC_CRS_IRQHandler                                  /*5       RCC                                  */
                .word       EXTI0_IRQHandler                                    /*6       EXTI Line 0                          */
                .word       EXTI1_IRQHandler                                    /*7       EXTI Line 1                          */
                .word       EXTI2_IRQHandler                                    /*8       EXTI Line 2                          */
                .word       EXTI3_IRQHandler                                    /*9       EXTI Line 3                          */
                .word       EXTI4_IRQHandler                                    /*10      EXTI Line 4                          */
                .word       DMA1_Channel1_IRQHandler                            /*11      DMA1 Channel 1                       */
                .word       DMA1_Channel2_IRQHandler                            /*12      DMA1 Channel 2                       */
                .word       DMA1_Channel3_IRQHandler                            /*13      DMA1 Channel 3                       */
                .word       DMA1_Channel4_IRQHandler                            /*14      DMA1 Channel 4                       */
                .word       DMA1_Channel5_IRQHandler                            /*15      DMA1 Channel 5                       */
                .word       DMA1_Channel6_IRQHandler                            /*16      DMA1 Channel 6                       */
                .word       DMA1_Channel7_IRQHandler                            /*17      DMA1 Channel 7                       */
                .word       ADC1_2_IRQHandler                                   /*18      ADC1 and ADC2                        */
                .word       FlashCache_IRQHandler                               /*19      FlashCache outage                    */
                .word       0                                                   /*20      Reserved                             */
                .word       CAN1_RX_IRQHandler                                  /*21      CAN1_RX                              */
                .word       0                                                   /*22      Reserved                             */
                .word       EXTI9_5_IRQHandler                                  /*23      EXTI Line 9..5                       */
                .word       TIM1_BRK_IRQHandler                                 /*24      TIM1 Break                           */
                .word       TIM1_UP_IRQHandler                                  /*25      TIM1 Update                          */
                .word       TIM1_TRG_COM_IRQHandler                             /*26      TIM1 Trigger and Commutation         */
                .word       TIM1_CC_IRQHandler                                  /*27      TIM1 Capture Compare                 */
                .word       TIM2_IRQHandler                                     /*28      TIM2                                 */
                .word       TIM3_IRQHandler                                     /*29      TIM3                                 */
                .word       TIM4_IRQHandler                                     /*30      TIM4                                 */
                .word       I2C1_IRQHandler                                     /*31      I2C1 Event                           */
                .word       0                                                   /*32      Reserved                             */
                .word       I2C2_IRQHandler                                     /*33      I2C2 Event                           */
                .word       0                                                   /*34      Reserved                             */
                .word       SPI1_IRQHandler                                     /*35      SPI1                                 */
                .word       SPI2_IRQHandler                                     /*36      SPI2                                 */
                .word       UART1_IRQHandler                                    /*37      UART1                                */
                .word       UART2_IRQHandler                                    /*38      UART2                                */
                .word       UART3_IRQHandler                                    /*39      UART3                                */
                .word       EXTI15_10_IRQHandler                                /*40      EXTI Line 15..10                     */
                .word       RTCAlarm_IRQHandler                                 /*41      RTC Alarm through EXTI Line 17       */
                .word       OTG_FS_WKUP_IRQHandler                              /*42      USB OTG FS Wakeup through EXTI line  */
                .word       TIM8_BRK_IRQHandler                                 /*43      TIM8 Break                           */
                .word       TIM8_UP_IRQHandler                                  /*44      TIM8 Update                          */
                .word       TIM8_TRG_COM_IRQHandler                             /*45      TIM8 Trigger and Commutation         */
                .word       TIM8_CC_IRQHandler                                  /*46      TIM8 Capture Compare                 */
                .word       ADC3_IRQHandler                                     /*47      ADC3                                 */
                .word       0                                                   /*48      Reserved                             */
                .word       SDIO_IRQHandler                                     /*49      SDIO                                 */
                .word       TIM5_IRQHandler                                     /*50      TIM5                                 */
                .word       SPI3_IRQHandler                                     /*51      SPI3                                 */
                .word       UART4_IRQHandler                                    /*52      UART4                                */
                .word       UART5_IRQHandler                                    /*53      UART5                                */
                .word       TIM6_IRQHandler                                     /*54      TIM6                                 */
                .word       TIM7_IRQHandler                                     /*55      TIM7                                 */
                .word       DMA2_Channel1_IRQHandler                            /*56      DMA2 Channel 1                       */
                .word       DMA2_Channel2_IRQHandler                            /*57      DMA2 Channel 2                       */
                .word       DMA2_Channel3_IRQHandler                            /*58      DMA2 Channel 3                       */
                .word       DMA2_Channel4_IRQHandler                            /*59      DMA2 Channel 4                       */
                .word       DMA2_Channel5_IRQHandler                            /*60      DMA2 Channel 5                       */
                .word       ETH_IRQHandler                                      /*61      Ethernet                             */
                .word       0                                                   /*62      Reserved                             */
                .word       0                                                   /*63      Reserved                             */
                .word       COMP1_2_IRQHandler                                  /*64      COMP1,COMP2                          */
                .word       0                                                   /*65      Reserved                             */
                .word       0                                                   /*66      Reserved                             */
                .word       OTG_FS_IRQHandler                                   /*67      USB OTG_FullSpeed                    */
                .word       0                                                   /*68      Reserved                             */
                .word       0                                                   /*69      Reserved                             */
                .word       0                                                   /*70      Reserved                             */
                .word       UART6_IRQHandler                                    /*71      UART6                                */
                .word       0                                                   /*72      Reserved                             */
                .word       0                                                   /*73      Reserved                             */
                .word       0                                                   /*74      Reserved                             */
                .word       0                                                   /*75      Reserved                             */
                .word       0                                                   /*76      Reserved                             */
                .word       0                                                   /*77      Reserved                             */
                .word       0                                                   /*78      Reserved                             */
                .word       0                                                   /*79      Reserved                             */
                .word       0                                                   /*80      Reserved                             */
                .word       0                                                   /*81      Reserved                             */
                .word       UART7_IRQHandler                                    /*82      UART7                                */
                .word       UART8_IRQHandler                                    /*83      UART8                                */
/*******************************************************************************
*
* Provide weak aliases for each Exception handler to the Default_Handler. 
* As they are weak aliases, any function with the same name will override 
* this definition.
* 
*******************************************************************************/
                .weak       NMI_Handler
                .thumb_set  NMI_Handler                 ,Default_Handler

                .weak       HardFault_Handler
                .thumb_set  HardFault_Handler           ,Default_Handler

                .weak       MemManage_Handler
                .thumb_set  MemManage_Handler           ,Default_Handler

                .weak       BusFault_Handler
                .thumb_set  BusFault_Handler            ,Default_Handler

                .weak       UsageFault_Handler
                .thumb_set  UsageFault_Handler          ,Default_Handler

                .weak       SVC_Handler
                .thumb_set  SVC_Handler                 ,Default_Handler

                .weak       DebugMon_Handler
                .thumb_set  DebugMon_Handler            ,Default_Handler

                .weak       PendSV_Handler
                .thumb_set  PendSV_Handler              ,Default_Handler

                .weak       SysTick_Handler
                .thumb_set  SysTick_Handler             ,Default_Handler              


                .weak       WWDG_IRQHandler             
                .thumb_set  WWDG_IRQHandler             ,Default_Handler
                                                        
                .weak       PVD_IRQHandler              
                .thumb_set  PVD_IRQHandler              ,Default_Handler
                                                        
                .weak       TAMPER_IRQHandler           
                .thumb_set  TAMPER_IRQHandler           ,Default_Handler
                                                        
                .weak       RTC_IRQHandler              
                .thumb_set  RTC_IRQHandler              ,Default_Handler
                                                        
                .weak       FLASH_IRQHandler            
                .thumb_set  FLASH_IRQHandler            ,Default_Handler
                                                        
                .weak       RCC_CRS_IRQHandler          
                .thumb_set  RCC_CRS_IRQHandler          ,Default_Handler
                                                        
                .weak       EXTI0_IRQHandler            
                .thumb_set  EXTI0_IRQHandler            ,Default_Handler
                                                        
                .weak       EXTI1_IRQHandler            
                .thumb_set  EXTI1_IRQHandler            ,Default_Handler
                                                        
                .weak       EXTI2_IRQHandler            
                .thumb_set  EXTI2_IRQHandler            ,Default_Handler
                                                        
                .weak       EXTI3_IRQHandler            
                .thumb_set  EXTI3_IRQHandler            ,Default_Handler
                                                        
                .weak       EXTI4_IRQHandler            
                .thumb_set  EXTI4_IRQHandler            ,Default_Handler
                                                        
                .weak       DMA1_Channel1_IRQHandler    
                .thumb_set  DMA1_Channel1_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA1_Channel2_IRQHandler    
                .thumb_set  DMA1_Channel2_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA1_Channel3_IRQHandler    
                .thumb_set  DMA1_Channel3_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA1_Channel4_IRQHandler    
                .thumb_set  DMA1_Channel4_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA1_Channel5_IRQHandler    
                .thumb_set  DMA1_Channel5_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA1_Channel6_IRQHandler    
                .thumb_set  DMA1_Channel6_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA1_Channel7_IRQHandler    
                .thumb_set  DMA1_Channel7_IRQHandler    ,Default_Handler
                                                        
                .weak       ADC1_2_IRQHandler           
                .thumb_set  ADC1_2_IRQHandler           ,Default_Handler
                                                        
                .weak       FlashCache_IRQHandler       
                .thumb_set  FlashCache_IRQHandler       ,Default_Handler
                                                        
                .weak       CAN1_RX_IRQHandler          
                .thumb_set  CAN1_RX_IRQHandler          ,Default_Handler
                                                        
                .weak       EXTI9_5_IRQHandler          
                .thumb_set  EXTI9_5_IRQHandler          ,Default_Handler
                                                        
                .weak       TIM1_BRK_IRQHandler         
                .thumb_set  TIM1_BRK_IRQHandler         ,Default_Handler
                                                        
                .weak       TIM1_UP_IRQHandler          
                .thumb_set  TIM1_UP_IRQHandler          ,Default_Handler
                                                        
                .weak       TIM1_TRG_COM_IRQHandler     
                .thumb_set  TIM1_TRG_COM_IRQHandler     ,Default_Handler
                                                        
                .weak       TIM1_CC_IRQHandler          
                .thumb_set  TIM1_CC_IRQHandler          ,Default_Handler
                                                        
                .weak       TIM2_IRQHandler             
                .thumb_set  TIM2_IRQHandler             ,Default_Handler
                                                        
                .weak       TIM3_IRQHandler             
                .thumb_set  TIM3_IRQHandler             ,Default_Handler
                                                        
                .weak       TIM4_IRQHandler             
                .thumb_set  TIM4_IRQHandler             ,Default_Handler
                                                        
                .weak       I2C1_IRQHandler             
                .thumb_set  I2C1_IRQHandler             ,Default_Handler
                                                        
                .weak       I2C2_IRQHandler             
                .thumb_set  I2C2_IRQHandler             ,Default_Handler
                                                        
                .weak       SPI1_IRQHandler             
                .thumb_set  SPI1_IRQHandler             ,Default_Handler
                                                        
                .weak       SPI2_IRQHandler             
                .thumb_set  SPI2_IRQHandler             ,Default_Handler
                                                        
                .weak       UART1_IRQHandler            
                .thumb_set  UART1_IRQHandler            ,Default_Handler
                                                        
                .weak       UART2_IRQHandler            
                .thumb_set  UART2_IRQHandler            ,Default_Handler
                                                        
                .weak       UART3_IRQHandler            
                .thumb_set  UART3_IRQHandler            ,Default_Handler
                                                        
                .weak       EXTI15_10_IRQHandler        
                .thumb_set  EXTI15_10_IRQHandler        ,Default_Handler
                                                        
                .weak       RTCAlarm_IRQHandler         
                .thumb_set  RTCAlarm_IRQHandler         ,Default_Handler
                                                        
                .weak       OTG_FS_WKUP_IRQHandler      
                .thumb_set  OTG_FS_WKUP_IRQHandler      ,Default_Handler
                                                        
                .weak       TIM8_BRK_IRQHandler         
                .thumb_set  TIM8_BRK_IRQHandler         ,Default_Handler
                                                        
                .weak       TIM8_UP_IRQHandler          
                .thumb_set  TIM8_UP_IRQHandler          ,Default_Handler
                                                        
                .weak       TIM8_TRG_COM_IRQHandler     
                .thumb_set  TIM8_TRG_COM_IRQHandler     ,Default_Handler
                                                        
                .weak       TIM8_CC_IRQHandler          
                .thumb_set  TIM8_CC_IRQHandler          ,Default_Handler
                                                        
                .weak       ADC3_IRQHandler             
                .thumb_set  ADC3_IRQHandler             ,Default_Handler
                                                        
                .weak       SDIO_IRQHandler             
                .thumb_set  SDIO_IRQHandler             ,Default_Handler
                                                        
                .weak       TIM5_IRQHandler             
                .thumb_set  TIM5_IRQHandler             ,Default_Handler
                                                        
                .weak       SPI3_IRQHandler             
                .thumb_set  SPI3_IRQHandler             ,Default_Handler
                                                        
                .weak       UART4_IRQHandler            
                .thumb_set  UART4_IRQHandler            ,Default_Handler
                                                        
                .weak       UART5_IRQHandler            
                .thumb_set  UART5_IRQHandler            ,Default_Handler
                                                        
                .weak       TIM6_IRQHandler             
                .thumb_set  TIM6_IRQHandler             ,Default_Handler
                                                        
                .weak       TIM7_IRQHandler             
                .thumb_set  TIM7_IRQHandler             ,Default_Handler
                                                        
                .weak       DMA2_Channel1_IRQHandler    
                .thumb_set  DMA2_Channel1_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA2_Channel2_IRQHandler    
                .thumb_set  DMA2_Channel2_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA2_Channel3_IRQHandler    
                .thumb_set  DMA2_Channel3_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA2_Channel4_IRQHandler    
                .thumb_set  DMA2_Channel4_IRQHandler    ,Default_Handler
                                                        
                .weak       DMA2_Channel5_IRQHandler    
                .thumb_set  DMA2_Channel5_IRQHandler    ,Default_Handler
                                                        
                .weak       ETH_IRQHandler              
                .thumb_set  ETH_IRQHandler              ,Default_Handler
                                                        
                .weak       COMP1_2_IRQHandler          
                .thumb_set  COMP1_2_IRQHandler          ,Default_Handler
                                                        
                .weak       OTG_FS_IRQHandler           
                .thumb_set  OTG_FS_IRQHandler           ,Default_Handler
                                                        
                .weak       UART6_IRQHandler            
                .thumb_set  UART6_IRQHandler            ,Default_Handler
                                                        
                .weak       UART7_IRQHandler            
                .thumb_set  UART7_IRQHandler            ,Default_Handler
                                                        
                .weak       UART8_IRQHandler            
                .thumb_set  UART8_IRQHandler            ,Default_Handler




/****END OF FILE****/

