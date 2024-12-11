if( FREERTOS_PORT STREQUAL "GCC_RISC_V_GENERIC" )
    include( GCC/RISC-V/chip_extensions.cmake )
endif()

if( FREERTOS_PORT STREQUAL "IAR_RISC_V_GENERIC" )
    include( IAR/RISC-V/chip_extensions.cmake )
endif()

# FreeRTOS internal cmake file. Do not use it in user top-level project

if (FREERTOS_PORT STREQUAL "A_CUSTOM_PORT")
    message(STATUS "Using a custom FREERTOS_PORT.")
    return()
endif()

# FreeRTOS internal cmake file. Do not use it in user top-level project

add_library(freertos_kernel_port OBJECT
    # TEMPLATE Port
    $<$<STREQUAL:${FREERTOS_PORT},TEMPLATE>:
        template/port.c>

        # 16-Bit DOS ports for BCC
    $<$<STREQUAL:${FREERTOS_PORT},BCC_16BIT_DOS_FLSH186>:
        BCC/16BitDOS/common/portcomn.c
        BCC/16BitDOS/Flsh186/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},BCC_16BIT_DOS_PC>:
        BCC/16BitDOS/common/portcomn.c
        BCC/16BitDOS/PC/port.c>

    # ARMv7-M port for Texas Instruments Code Composer Studio
    $<$<STREQUAL:${FREERTOS_PORT},CCS_ARM_CM3>:
        CCS/ARM_CM3/port.c
        CCS/ARM_CM3/portasm.asm>

    # ARMv7E-M port for Texas Instruments Code Composer Studio
    $<$<STREQUAL:${FREERTOS_PORT},CCS_ARM_CM4F>:
        CCS/ARM_CM4F/port.c
        CCS/ARM_CM4F/portasm.asm>

    # ARMv7-R port for Texas Instruments Code Composer Studio
    $<$<STREQUAL:${FREERTOS_PORT},CCS_ARM_CR4>:
        CCS/ARM_Cortex-R4/port.c
        CCS/ARM_Cortex-R4/portASM.asm>

    # Texas Instruments MSP430 port for Texas Instruments Code Composer Studio
    $<$<STREQUAL:${FREERTOS_PORT},CCS_MSP430X>:
        CCS/MSP430X/port.c
        CCS/MSP430X/portext.asm>

    # NXP (formerly Motorola, Freescale) Cold Fire and 68HCS12 ports for Code Warrior
    $<$<STREQUAL:${FREERTOS_PORT},CODEWARRIOR_COLDFIRE_V1>:
        CodeWarrior/ColdFire_V1/port.c
        CodeWarrior/ColdFire_V1/portasm.S>

    $<$<STREQUAL:${FREERTOS_PORT},CODEWARRIOR_COLDFIRE_V2>:
        CodeWarrior/ColdFire_V2/port.c
        CodeWarrior/ColdFire_V2/portasm.S>

    $<$<STREQUAL:${FREERTOS_PORT},CODEWARRIOR_HCS12>:
        CodeWarrior/HCS12/port.c>

    # ARMv7-A port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CA9>:
        GCC/ARM_CA9/port.c
        GCC/ARM_CA9/portASM.S>

    # ARMv8-A ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_AARCH64>:
        GCC/ARM_AARCH64/port.c
        GCC/ARM_AARCH64/portASM.S>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_AARCH64_SRE>:
        GCC/ARM_AARCH64_SRE/port.c
        GCC/ARM_AARCH64_SRE/portASM.S>

    # ARMv6-M port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM0>:
        GCC/ARM_CM0/port.c
        GCC/ARM_CM0/portasm.c
        GCC/ARM_CM0/mpu_wrappers_v2_asm.c>

    # ARMv6-M / Cortex-M0 Raspberry PI RP2040 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RP2040>:
        ThirdParty/GCC/RP2040/port.c>

    # ARMv7-M ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM3>:
        GCC/ARM_CM3/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM3_MPU>:
        GCC/ARM_CM3_MPU/port.c
        GCC/ARM_CM3_MPU/mpu_wrappers_v2_asm.c>

    # ARMv7E-M ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM4_MPU>:
        GCC/ARM_CM4_MPU/port.c
        GCC/ARM_CM4_MPU/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM4F>:
        GCC/ARM_CM4F/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM7>:
        GCC/ARM_CM7/r0p1/port.c>

    # ARMv8-M ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM23_NONSECURE>:
        GCC/ARM_CM23/non_secure/port.c
        GCC/ARM_CM23/non_secure/portasm.c
        GCC/ARM_CM23/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM23_SECURE>:
        GCC/ARM_CM23/secure/secure_context_port.c
        GCC/ARM_CM23/secure/secure_context.c
        GCC/ARM_CM23/secure/secure_heap.c
        GCC/ARM_CM23/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM23_NTZ_NONSECURE>:
        GCC/ARM_CM23_NTZ/non_secure/port.c
        GCC/ARM_CM23_NTZ/non_secure/portasm.c
        GCC/ARM_CM23_NTZ/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM33_NONSECURE>:
        GCC/ARM_CM33/non_secure/port.c
        GCC/ARM_CM33/non_secure/portasm.c
        GCC/ARM_CM33/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM33_SECURE>:
        GCC/ARM_CM33/secure/secure_context_port.c
        GCC/ARM_CM33/secure/secure_context.c
        GCC/ARM_CM33/secure/secure_heap.c
        GCC/ARM_CM33/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM33_NTZ_NONSECURE>:
        GCC/ARM_CM33_NTZ/non_secure/port.c
        GCC/ARM_CM33_NTZ/non_secure/portasm.c
        GCC/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM33_TFM>:
        GCC/ARM_CM33_NTZ/non_secure/port.c
        GCC/ARM_CM33_NTZ/non_secure/portasm.c
        GCC/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.c
        ThirdParty/GCC/ARM_TFM/os_wrapper_freertos.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM35P_NONSECURE>:
        GCC/ARM_CM35P/non_secure/port.c
        GCC/ARM_CM35P/non_secure/portasm.c
        GCC/ARM_CM35P/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM35P_SECURE>:
        GCC/ARM_CM35P/secure/secure_context_port.c
        GCC/ARM_CM35P/secure/secure_context.c
        GCC/ARM_CM35P/secure/secure_heap.c
        GCC/ARM_CM35P/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM35P_NTZ_NONSECURE>:
        GCC/ARM_CM35P_NTZ/non_secure/port.c
        GCC/ARM_CM35P_NTZ/non_secure/portasm.c
        GCC/ARM_CM35P_NTZ/non_secure/mpu_wrappers_v2_asm.c>

    # ARMv8.1-M ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM55_NONSECURE>:
        GCC/ARM_CM55/non_secure/port.c
        GCC/ARM_CM55/non_secure/portasm.c
        GCC/ARM_CM55/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM55_SECURE>:
        GCC/ARM_CM55/secure/secure_context_port.c
        GCC/ARM_CM55/secure/secure_context.c
        GCC/ARM_CM55/secure/secure_heap.c
        GCC/ARM_CM55/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM55_NTZ_NONSECURE>:
        GCC/ARM_CM55_NTZ/non_secure/port.c
        GCC/ARM_CM55_NTZ/non_secure/portasm.c
        GCC/ARM_CM55_NTZ/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM55_TFM>:
        GCC/ARM_CM55_NTZ/non_secure/port.c
        GCC/ARM_CM55_NTZ/non_secure/portasm.c
        GCC/ARM_CM55_NTZ/non_secure/mpu_wrappers_v2_asm.c
        ThirdParty/GCC/ARM_TFM/os_wrapper_freertos.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM85_NONSECURE>:
        GCC/ARM_CM85/non_secure/port.c
        GCC/ARM_CM85/non_secure/portasm.c
        GCC/ARM_CM85/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM85_SECURE>:
        GCC/ARM_CM85/secure/secure_context_port.c
        GCC/ARM_CM85/secure/secure_context.c
        GCC/ARM_CM85/secure/secure_heap.c
        GCC/ARM_CM85/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM85_NTZ_NONSECURE>:
        GCC/ARM_CM85_NTZ/non_secure/port.c
        GCC/ARM_CM85_NTZ/non_secure/portasm.c
        GCC/ARM_CM85_NTZ/non_secure/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM85_TFM>:
        GCC/ARM_CM85_NTZ/non_secure/port.c
        GCC/ARM_CM85_NTZ/non_secure/portasm.c
        GCC/ARM_CM85_NTZ/non_secure/mpu_wrappers_v2_asm.c
        ThirdParty/GCC/ARM_TFM/os_wrapper_freertos.c>

    # ARMv7-R ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CR5>:
        GCC/ARM_CR5/port.c
        GCC/ARM_CR5/portASM.S>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CRX_MPU>:
        GCC/ARM_CRx_MPU/port.c
        GCC/ARM_CRx_MPU/portASM.S
        GCC/ARM_CRx_MPU/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CRX_NOGIC>:
        GCC/ARM_CRx_No_GIC/port.c
        GCC/ARM_CRx_No_GIC/portASM.S>

    # ARMv4T ARM7TDMI ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM7_AT91FR40008>:
        GCC/ARM7_AT91FR40008/port.c
        GCC/ARM7_AT91FR40008/portISR.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM7_AT91SAM7S>:
        GCC/ARM7_AT91SAM7S/lib_AT91SAM7X256.c
        GCC/ARM7_AT91SAM7S/port.c
        GCC/ARM7_AT91SAM7S/portISR.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM7_LPC2000>:
        GCC/ARM7_LPC2000/port.c
        GCC/ARM7_LPC2000/portISR.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM7_LPC23XX>:
        GCC/ARM7_LPC23xx/port.c
        GCC/ARM7_LPC23xx/portISR.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_STR75X>:
        GCC/STR75x/port.c
        GCC/STR75x/portISR.c>

    # Microchip (formerly Ateml) AVR8 ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ATMEGA323>:
        GCC/ATMega323/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ATMEGA>:
        ThirdParty/GCC/ATmega/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_AVRDX>:
        ThirdParty/Partner-Supported-Ports/GCC/AVR_AVRDx/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_AVR_MEGA0>:
        ThirdParty/Partner-Supported-Ports/GCC/AVR_Mega0/port.c>

    # Microchip (formerly Ateml) AVR32 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_AVR32_UC3>:
        GCC/AVR32_UC3/exception.S
        GCC/AVR32_UC3/port.c>

    # NXP (formerly Motorola, Freescale) Cold Fire and 68HCS12 ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_COLDFIRE_V2>:
        GCC/ColdFire_V2/port.c
        GCC/ColdFire_V2/portasm.S>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_HCS12>:
        GCC/HCS12/port.c>

    # Cortus APS3 soft core port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_CORTUS_APS3>:
        GCC/CORTUS_APS3/port.c>

    # Renesas (formerly Hitach) H8S port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_H8S2329>:
        GCC/H8S2329/port.c>

    # x86 / IA32 flat memory model port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_IA32_FLAT>:
        GCC/IA32_flat/port.c
        GCC/IA32_flat/portASM.S>

    # Xilinx MicroBlaze soft core ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_MICROBLAZE>:
        GCC/MicroBlaze/port.c
        GCC/MicroBlaze/portasm.s>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_MICROBLAZE_V8>:
        GCC/MicroBlazeV8/port.c
        GCC/MicroBlazeV8/port_exceptions.c
        GCC/MicroBlazeV8/portasm.S>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_MICROBLAZE_V9>:
        GCC/MicroBlazeV9/port.c
        GCC/MicroBlazeV9/port_exceptions.c
        GCC/MicroBlazeV9/portasm.S>

    # Xilinx PCC4XX soft core ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_PPC405_XILINX>:
        GCC/PPC405_Xilinx/port.c
        GCC/PPC405_Xilinx/portasm.S>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_PPC440_XILINX>:
        GCC/PPC440_Xilinx/port.c
        GCC/PPC440_Xilinx/portasm.S>

     # Texas Instruments MSP430 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_MSP430F449>:
        GCC/MSP430F449/port.c>

    # Intel (formerly Altera) NIOS II soft core port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_NIOSII>:
        GCC/NiosII/port.c
        GCC/NiosII/port_asm.S>

    # RISC-V architecture ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RISC_V>:
        GCC/RISC-V/port.c
        GCC/RISC-V/portASM.S>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_RISC_V_PULPINO_VEGA_RV32M1RM>:
        GCC/RISC-V/port.c
        GCC/RISC-V/portASM.S>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_RISC_V_GENERIC>:
        GCC/RISC-V/port.c
        GCC/RISC-V/portASM.S>

    # Renesas RL78 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RL78>:
        GCC/RL78/port.c
        GCC/RL78/portasm.S>

    # Renesas RX architecture ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX100>:
        GCC/RX100/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX200>:
        GCC/RX200/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX600>:
        GCC/RX600/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX600_V2>:
        GCC/RX600v2/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX700_V3_DPFPU>:
        GCC/RX700v3_DPFPU/port.c>

    # Infineon TriCore 1782 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_TRICORE_1782>:
        GCC/TriCore_1782/port.c
        GCC/TriCore_1782/porttrap.c>

    # Synopsys ARC architecture ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARC_EM_HS>:
        ThirdParty/GCC/ARC_EM_HS/arc_freertos_exceptions.c
        ThirdParty/GCC/ARC_EM_HS/arc_support.s
        ThirdParty/GCC/ARC_EM_HS/freertos_tls.c
        ThirdParty/GCC/ARC_EM_HS/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARC_V1>:
        ThirdParty/GCC/ARC_v1/arc_freertos_exceptions.c
        ThirdParty/GCC/ARC_v1/arc_support.s
        ThirdParty/GCC/ARC_v1/port.c>

    # Posix Simulator port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_POSIX>:
        ThirdParty/GCC/Posix/port.c
        ThirdParty/GCC/Posix/utils/wait_for_event.c>

    # Xtensa LX / Espressif ESP32 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_XTENSA_ESP32>:
        ThirdParty/GCC/Xtensa_ESP32/FreeRTOS-openocd.c
        ThirdParty/GCC/Xtensa_ESP32/port.c
        ThirdParty/GCC/Xtensa_ESP32/portasm.S
        ThirdParty/GCC/Xtensa_ESP32/xtensa_context.S
        ThirdParty/GCC/Xtensa_ESP32/xtensa_init.c
        ThirdParty/GCC/Xtensa_ESP32/xtensa_intr_asm.S
        ThirdParty/GCC/Xtensa_ESP32/xtensa_intr.c
        ThirdParty/GCC/Xtensa_ESP32/xtensa_loadstore_handler.S
        ThirdParty/GCC/Xtensa_ESP32/xtensa_overlay_os_hook.c
        ThirdParty/GCC/Xtensa_ESP32/xtensa_vector_defaults.S
        ThirdParty/GCC/Xtensa_ESP32/xtensa_vectors.S>

    # Renesas (formerly NEC) 78K port for IAR EW78K
    $<$<STREQUAL:${FREERTOS_PORT},IAR_78K0K>:
        IAR/78K0R/port.c
        IAR/78K0R/portasm.s26>

    # ARMv7-A ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CA5_NOGIC>:
        IAR/ARM_CA5_No_GIC/port.c
        IAR/ARM_CA5_No_GIC/portASM.s>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CA9>:
        IAR/ARM_CA9/port.c
        IAR/ARM_CA9/portASM.s>

    # ARMv6-M port for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM0>:
        IAR/ARM_CM0/port.c
        IAR/ARM_CM0/portasm.s>

    # ARMv7-M port for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM3>:
        IAR/ARM_CM3/port.c
        IAR/ARM_CM3/portasm.s>

    # ARMv7E-M ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM4F>:
        IAR/ARM_CM4F/port.c
        IAR/ARM_CM4F/portasm.s>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM4F_MPU>:
        IAR/ARM_CM4F_MPU/port.c
        IAR/ARM_CM4F_MPU/portasm.s
        IAR/ARM_CM4F_MPU/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM7>:
        IAR/ARM_CM7/r0p1/port.c
        IAR/ARM_CM7/r0p1/portasm.s>

    # ARMv8-M Ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM23_NONSECURE>:
        IAR/ARM_CM23/non_secure/port.c
        IAR/ARM_CM23/non_secure/portasm.s
        IAR/ARM_CM23/non_secure/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM23_SECURE>:
        IAR/ARM_CM23/secure/secure_context_port_asm.s
        IAR/ARM_CM23/secure/secure_context.c
        IAR/ARM_CM23/secure/secure_heap.c
        IAR/ARM_CM23/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM23_NTZ_NONSECURE>:
        IAR/ARM_CM23_NTZ/non_secure/port.c
        IAR/ARM_CM23_NTZ/non_secure/portasm.s
        IAR/ARM_CM23_NTZ/non_secure/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM33_NONSECURE>:
        IAR/ARM_CM33/non_secure/port.c
        IAR/ARM_CM33/non_secure/portasm.s
        IAR/ARM_CM33/non_secure/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM33_SECURE>:
        IAR/ARM_CM33/secure/secure_context_port_asm.s
        IAR/ARM_CM33/secure/secure_context.c
        IAR/ARM_CM33/secure/secure_heap.c
        IAR/ARM_CM33/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM33_NTZ_NONSECURE>:
        IAR/ARM_CM33_NTZ/non_secure/port.c
        IAR/ARM_CM33_NTZ/non_secure/portasm.s
        IAR/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM33_TFM>:
        IAR/ARM_CM33_NTZ/non_secure/port.c
        IAR/ARM_CM33_NTZ/non_secure/portasm.s
        IAR/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.S
        ThirdParty/GCC/ARM_TFM/os_wrapper_freertos.c>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM35P_NONSECURE>:
        IAR/ARM_CM35P/non_secure/port.c
        IAR/ARM_CM35P/non_secure/portasm.s
        IAR/ARM_CM35P/non_secure/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM35P_SECURE>:
        IAR/ARM_CM35P/secure/secure_context_port_asm.s
        IAR/ARM_CM35P/secure/secure_context.c
        IAR/ARM_CM35P/secure/secure_heap.c
        IAR/ARM_CM35P/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM35P_NTZ_NONSECURE>:
        IAR/ARM_CM35P_NTZ/non_secure/port.c
        IAR/ARM_CM35P_NTZ/non_secure/portasm.s
        IAR/ARM_CM35P_NTZ/non_secure/mpu_wrappers_v2_asm.S>

    # ARMv8.1-M ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM55_NONSECURE>:
        IAR/ARM_CM55/non_secure/port.c
        IAR/ARM_CM55/non_secure/portasm.s
        IAR/ARM_CM55/non_secure/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM55_SECURE>:
        IAR/ARM_CM55/secure/secure_context_port_asm.s
        IAR/ARM_CM55/secure/secure_context.c
        IAR/ARM_CM55/secure/secure_heap.c
        IAR/ARM_CM55/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM55_NTZ_NONSECURE>:
        IAR/ARM_CM55_NTZ/non_secure/port.c
        IAR/ARM_CM55_NTZ/non_secure/portasm.s
        IAR/ARM_CM55_NTZ/non_secure/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM55_TFM>:
        IAR/ARM_CM55_NTZ/non_secure/port.c
        IAR/ARM_CM55_NTZ/non_secure/portasm.s
        IAR/ARM_CM55_NTZ/non_secure/mpu_wrappers_v2_asm.S
        ThirdParty/GCC/ARM_TFM/os_wrapper_freertos.c>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM85_NONSECURE>:
        IAR/ARM_CM85/non_secure/port.c
        IAR/ARM_CM85/non_secure/portasm.s
        IAR/ARM_CM85/non_secure/mpu_wrappers_v2_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM85_SECURE>:
        IAR/ARM_CM85/secure/secure_context_port_asm.s
        IAR/ARM_CM85/secure/secure_context.c
        IAR/ARM_CM85/secure/secure_heap.c
        IAR/ARM_CM85/secure/secure_init.c>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM85_NTZ_NONSECURE>:
        IAR/ARM_CM85_NTZ/non_secure/port.c
        IAR/ARM_CM85_NTZ/non_secure/portasm.s
        IAR/ARM_CM85_NTZ/non_secure/mpu_wrappers_v2_asm.S>

        $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM85_TFM>:
        IAR/ARM_CM85_NTZ/non_secure/port.c
        IAR/ARM_CM85_NTZ/non_secure/portasm.s
        IAR/ARM_CM85_NTZ/non_secure/mpu_wrappers_v2_asm.S
        ThirdParty/GCC/ARM_TFM/os_wrapper_freertos.c>

    # ARMv7-R Ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CRX_NOGIC>:
        IAR/ARM_CRx_No_GIC/port.c
        IAR/ARM_CRx_No_GIC/portASM.s>

    # Microchip (formerly Atmel) AVR8 ports for IAR EWAVR
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ATMEGA323>:
        IAR/ATMega323/port.c
        IAR/ATMega323/portmacro.s90>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_AVR_AVRDX>:
        IAR/AVR_AVRDx/port.c
        IAR/AVR_AVRDx/portmacro.s90>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_AVR_MEGA0>:
        IAR/AVR_Mega0/port.c
        IAR/AVR_Mega0/portmacro.s90>

    # Microchip (formerly Atmel) AVR32 port for IAR Embedded Workbench for AVR32
    $<$<STREQUAL:${FREERTOS_PORT},IAR_AVR32_UC3>:
        IAR/AVR32_UC3/exception.s82
        IAR/AVR32_UC3/port.c
        IAR/AVR32_UC3/read.c
        IAR/AVR32_UC3/write.c>

    # Texas Instruments MSP430 ports for IAR Embedded Workbench for MSP430
    $<$<STREQUAL:${FREERTOS_PORT},IAR_MSP430>:
        IAR/MSP430/port.c
        IAR/MSP430/portext.s43>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_MSP430X>:
        IAR/MSP430X/port.c
        IAR/MSP430X/portext.s43>

    # RISC-V architecture port for IAR Embedded Workbench for RISC-V
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RISC_V>:
        IAR/RISC-V/port.c
        IAR/RISC-V/portASM.s>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_RISC_V_GENERIC>:
        IAR/RISC-V/port.c
        IAR/RISC-V/portASM.s>

    # Renesas RL78 port for IAR EWRL78
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RL78>:
        IAR/RL78/port.c
        IAR/RL78/portasm.s87>

    # Renesas RX architecture ports for IAR EWRX
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RX100>:
        IAR/RX100/port.c
        IAR/RX100/port_asm.s>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_RX600>:
        IAR/RX600/port.c
        IAR/RX600/port_asm.s>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_RX700_V3_DPFPU>:
        IAR/RX700v3_DPFPU/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_RX_V2>:
        IAR/RXv2/port.c
        IAR/RXv2/port_asm.s>

    # Renesas (formerly NEC) V850ES port for IAR EWV850
    $<$<STREQUAL:${FREERTOS_PORT},IAR_V850ES_FX3>:
        IAR/V850ES/port.c
        IAR/V850ES/portasm_Fx3.s85>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_V850ES_HX3>:
        IAR/V850ES/port.c
        IAR/V850ES/portasm_Hx2.s85>

    # ARMv4T ARM7TDMI ports for IAR Embedded Workbench for ARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_STR71X>:
        IAR/STR71x/port.c
        IAR/STR71x/portasm.s79>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_STR75X>:
        IAR/STR75x/port.c
        IAR/STR75x/portasm.s79>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_LPC2000>:
        IAR/LPC2000/port.c
        IAR/LPC2000/portasm.s79>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ATMEL_SAM7S64>:
        IAR/AtmelSAM7S64/port.c
        IAR/AtmelSAM7S64/portasm.s79>

    # ARMv5TE ARM926 ports for IAR Embedded Workbench for ARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_STR91X>:
        IAR/STR91x/port.c
        IAR/STR91x/portasm.s79>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ATMEL_SAM9XE>:
        IAR/AtmelSAM9XE/port.c
        IAR/AtmelSAM9XE/portasm.s79>

    # ARM Cortex-M4F port for the MikroElektronika MikroC compiler
    $<$<STREQUAL:${FREERTOS_PORT},MIKROC_ARM_CM4F>:
        MikroC/ARM_CM4F/port.c>

    # Microchip PIC18 8-bit MCU port for MPLAB XC8
    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC18F>:
        MPLAB/PIC18F/port.c>

    # Microchip PIC24 16-bit MCU port for MPLAB XC16
    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC24>:
        MPLAB/PIC24_dsPIC/port.c
        MPLAB/PIC24_dsPIC/portasm_PIC24.S> # TODO: What to do with portasm_dsPIC.S ?

    # Microchip MIPS 32-Bit MCU ports for MPLAB XC32
    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC32MEC14XX>:
        MPLAB/PIC32MEC14xx/port.c
        MPLAB/PIC32MEC14xx/port_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC32MX>:
        MPLAB/PIC32MX/port.c
        MPLAB/PIC32MX/port_asm.S>

    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC32MZ>:
        MPLAB/PIC32MZ/port.c
        MPLAB/PIC32MZ/port_asm.S>

    # Windows Simulator for Microsoft Visual C Compiler and MinGW GCC
    $<$<STREQUAL:${FREERTOS_PORT},MSVC_MINGW>:
        MSVC-MingW/port.c>

    # 16 bit DOS ports for Open Watcom
    $<$<STREQUAL:${FREERTOS_PORT},OWATCOM_16BIT_DOS_FLSH186>:
        oWatcom/16BitDOS/common/portcomn.c
        oWatcom/16BitDOS/Flsh186/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},OWATCOM_16BIT_DOS_PC>:
        oWatcom/16BitDOS/common/portcomn.c
        oWatcom/16BitDOS/PC/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},PARADIGM_TERN_EE_LARGE>:
        Paradigm/Tern_EE/large_untested/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},PARADIGM_TERN_EE_SMALL>:
        Paradigm/Tern_EE/small/port.c>

    # Renesas RX mcu ports for Renesas CC-RX
    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX100>:
        Renesas/RX100/port.c
        Renesas/RX100/port_asm.src>

    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX200>:
        Renesas/RX200/port.c
        Renesas/RX200/port_asm.src>

    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX600>:
        Renesas/RX600/port.c
        Renesas/RX600/port_asm.src>

    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX600_V2>:
        Renesas/RX600v2/port.c
        Renesas/RX600v2/port_asm.src>

    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX700_V3_DPFPU>:
        Renesas/RX700v3_DPFPU/port.c
        Renesas/RX700v3_DPFPU/port_asm.src>

    # Renesas (formerly  Hitach) SHA2 SuperH port for the Renesas SH C Compiler
    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_SH2A_FPU>:
        Renesas/SH2A_FPU/port.c
        Renesas/SH2A_FPU/portasm.src>

    # Texas Instruments MSP430 port for Rowley CrossWorks
    $<$<STREQUAL:${FREERTOS_PORT},ROWLEY_MSP430F449>:
        Rowley/MSP430F449/port.c
        Rowley/MSP430F449/portext.asm>

    # ARMv7-A Cortex-A9 port for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CA9>:
        RVDS/ARM_CA9/port.c
        RVDS/ARM_CA9/portASM.s>

    # ARMv6-M port for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM0>:
        RVDS/ARM_CM0/port.c>

    # ARMv7-M port for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM3>:
        RVDS/ARM_CM3/port.c>

    # ARMv7E-M ports for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM4_MPU>:
        RVDS/ARM_CM4_MPU/port.c
        RVDS/ARM_CM4_MPU/mpu_wrappers_v2_asm.c>

    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM4F>:
        RVDS/ARM_CM4F/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM7>:
        RVDS/ARM_CM7/r0p1/port.c>

    # ARMv4T / ARM7TDMI LPC21XX port for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM7_LPC21XX>:
        RVDS/ARM7_LPC21xx/port.c
        RVDS/ARM7_LPC21xx/portASM.s>

    # Cygnal c8051 port for SDCC (Small Device C Compiler)
    $<$<STREQUAL:${FREERTOS_PORT},SDCC_CYGNAL>:
        SDCC/Cygnal/port.c>

    # Infineon (formerly Fujitsu, Spansion, Cypress) MB9x ports for Softune C Compiler
    $<$<STREQUAL:${FREERTOS_PORT},SOFTUNE_MB91460>:
        Softune/MB91460/__STD_LIB_sbrk.c
        Softune/MB91460/port.c>

    $<$<STREQUAL:${FREERTOS_PORT},SOFTUNE_MB96340>:
        Softune/MB96340/__STD_LIB_sbrk.c
        Softune/MB96340/port.c>

    # ARMv7E-M (Cortex-M4F) port for TASKING VX-toolset for ARM
    $<$<STREQUAL:${FREERTOS_PORT},TASKING_ARM_CM4F>:
        Tasking/ARM_CM4F/port.c
        Tasking/ARM_CM4F/port_asm.asm>

    # Port for C-SKY T-HEAD CK802
    $<$<STREQUAL:${FREERTOS_PORT},CDK_THEAD_CK802>:
        ThirdParty/CDK/T-HEAD_CK802/port.c
        ThirdParty/CDK/T-HEAD_CK802/portasm.S>

    # Tensilica Xtensa port for XCC
    $<$<STREQUAL:${FREERTOS_PORT},XCC_XTENSA>:
        ThirdParty/XCC/Xtensa/port.c
        ThirdParty/XCC/Xtensa/portasm.S
        ThirdParty/XCC/Xtensa/portclib.c
        ThirdParty/XCC/Xtensa/xtensa_context.S
        ThirdParty/XCC/Xtensa/xtensa_init.c
        ThirdParty/XCC/Xtensa/xtensa_intr_asm.S
        ThirdParty/XCC/Xtensa/xtensa_intr.c
        ThirdParty/XCC/Xtensa/xtensa_overlay_os_hook.c
        ThirdParty/XCC/Xtensa/xtensa_vectors.S>

    # Microchip PIC18 port for WIZ-C
    $<$<STREQUAL:${FREERTOS_PORT},WIZC_PIC18>:
        WizC/PIC18/port.c
        WizC/PIC18/Drivers/Tick/isrTick.c
        WizC/PIC18/Drivers/Tick/Tick.c>
)

if( FREERTOS_PORT MATCHES "GCC_ARM_CM(3|4)_MPU" OR
    FREERTOS_PORT STREQUAL "IAR_ARM_CM4F_MPU" OR
    FREERTOS_PORT STREQUAL "RVDS_ARM_CM4_MPU" OR
    FREERTOS_PORT MATCHES "GCC_ARM_CM(23|33|55|85)_NTZ_NONSECURE" OR
    FREERTOS_PORT MATCHES "GCC_ARM_CM(23|33|55|85)_NONSECURE" OR
    FREERTOS_PORT MATCHES "GCC_ARM_CM(33|55|85)_TFM" OR
    FREERTOS_PORT MATCHES "IAR_ARM_CM(23|33|55|85)_NTZ_NONSECURE" OR
    FREERTOS_PORT MATCHES "IAR_ARM_CM(23|33|55|85)_NONSECURE" OR
    FREERTOS_PORT MATCHES "IAR_ARM_CM(33|55|85)_TFM"
)
    target_sources(freertos_kernel_port PRIVATE
        Common/mpu_wrappers.c
        Common/mpu_wrappers_v2.c
    )
endif()

if (DEFINED FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG )

    if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
        message(FATAL_ERROR "ARMv8.1-M PACBTI support in the kernel is not yet enabled for GNU toolchain due to known issues.")
    endif()

    if(FREERTOS_PORT MATCHES ".*ARM_CM85")
        if(FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG STREQUAL "ARM_V_8_1_M_PACBTI_CONFIG_STANDARD")
            target_compile_options(freertos_kernel_port PUBLIC $<$<STREQUAL:${CMAKE_C_COMPILER_ID},ARMClang>:-mbranch-protection=standard>)
            target_compile_options(freertos_kernel_port PUBLIC $<$<STREQUAL:${CMAKE_C_COMPILER_ID},IAR>:$<$<COMPILE_LANGUAGE:C,CXX>:--branch_protection=bti+pac-ret>>)
            target_compile_definitions(freertos_config
                INTERFACE
                    configENABLE_PAC=1
                    configENABLE_BTI=1
            )
        elseif(FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG STREQUAL "ARM_V_8_1_M_PACBTI_CONFIG_PACRET_LEAF_BTI")
            if(${CMAKE_C_COMPILER_ID} STREQUAL "ARMClang")
                target_compile_options(freertos_kernel_port
                    PUBLIC
                        -mbranch-protection=bti+pac-ret+leaf
                )
                target_compile_definitions(freertos_config
                    INTERFACE
                        configENABLE_PAC=1
                        configENABLE_BTI=1
                )
            elseif(${CMAKE_C_COMPILER_ID} STREQUAL "IAR")
                message(FATAL_ERROR "ARM_V_8_1_M_PACBTI_CONFIG_PACRET_LEAF_BTI PACBTI option is not supported on IAR Compiler.")
            endif()
        elseif(FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG STREQUAL "ARM_V_8_1_M_PACBTI_CONFIG_PACRET")
            target_compile_options(freertos_kernel_port PUBLIC $<$<STREQUAL:${CMAKE_C_COMPILER_ID},ARMClang>:-mbranch-protection=pac-ret>)
            target_compile_options(freertos_kernel_port PUBLIC $<$<STREQUAL:${CMAKE_C_COMPILER_ID},IAR>:$<$<COMPILE_LANGUAGE:C,CXX>:--branch_protection=pac-ret>>)
            target_compile_definitions(freertos_config
                INTERFACE
                    configENABLE_PAC=1
            )
        elseif(FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG STREQUAL "ARM_V_8_1_M_PACBTI_CONFIG_PACRET_LEAF")
            if(${CMAKE_C_COMPILER_ID} STREQUAL "ARMClang")
                target_compile_options(freertos_kernel_port
                    PUBLIC
                        -mbranch-protection=pac-ret+leaf
                )
                target_compile_definitions(freertos_config
                    INTERFACE
                        configENABLE_PAC=1
                )
            elseif(${CMAKE_C_COMPILER_ID} STREQUAL "IAR")
                message(FATAL_ERROR "ARM_V_8_1_M_PACBTI_CONFIG_PACRET_LEAF PACBTI option is not supported on IAR Compiler.")
            endif()
        elseif(FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG STREQUAL "ARM_V_8_1_M_PACBTI_CONFIG_BTI")
            target_compile_options(freertos_kernel_port PUBLIC $<$<STREQUAL:${CMAKE_C_COMPILER_ID},ARMClang>:-mbranch-protection=bti>)
            target_compile_options(freertos_kernel_port PUBLIC $<$<STREQUAL:${CMAKE_C_COMPILER_ID},IAR>:$<$<COMPILE_LANGUAGE:C,CXX>:--branch_protection=bti>>)
            target_compile_definitions(freertos_config
                INTERFACE
                    configENABLE_BTI=1
            )
        elseif(FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG STREQUAL "ARM_V_8_1_M_PACBTI_CONFIG_NONE")
            if(${CMAKE_C_COMPILER_ID} STREQUAL "ARMClang")
                target_compile_options(freertos_kernel_port
                    PUBLIC
                        -mbranch-protection=none
                )
            endif()
            target_compile_definitions(freertos_config
                INTERFACE
                    configENABLE_PAC=0
                    configENABLE_BTI=0
            )
        else()
            message(FATAL_ERROR "Invalid FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG configuration, the supported configurations are
            ARM_V_8_1_M_PACBTI_CONFIG_STANDARD,
            ARM_V_8_1_M_PACBTI_CONFIG_PACRET_LEAF_BTI,
            ARM_V_8_1_M_PACBTI_CONFIG_PACRET,
            ARM_V_8_1_M_PACBTI_CONFIG_PACRET_LEAF,
            ARM_V_8_1_M_PACBTI_CONFIG_BTI,
            ARM_V_8_1_M_PACBTI_CONFIG_NONE
            ")
        endif()
        if(NOT FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG STREQUAL "ARM_V_8_1_M_PACBTI_CONFIG_NONE")
            # The reason why `--library_security=pacbti-m` link option is defined for both `freertos_kernel_port`, and
            # `freertos_kernel` targets even though `freertos_kernel_port` gets linked to `freertos_kernel` is that the
            # `freertos_kernel_port` is an object library where its linker options don't propagate to the targets that
            # link against it.
            target_link_options(freertos_kernel_port
                PUBLIC
                    --library_security=pacbti-m
            )
            target_link_options(freertos_kernel
                PUBLIC
                    --library_security=pacbti-m
            )
        endif()
    else()
        message(FATAL_ERROR "FREERTOS_ARM_V_8_1_M_PACBTI_CONFIG option is currently only supported on ARM Cortex-M85 FreeRTOS port.")
    endif()
endif()

add_library(freertos_kernel_port_headers INTERFACE)

target_include_directories(freertos_kernel_port_headers INTERFACE
    # TEMPLATE Port
    $<$<STREQUAL:${FREERTOS_PORT},TEMPLATE>:${CMAKE_CURRENT_LIST_DIR}/template>

        # 16-Bit DOS ports for BCC
    $<$<STREQUAL:${FREERTOS_PORT},BCC_16BIT_DOS_FLSH186>:
        ${CMAKE_CURRENT_LIST_DIR}/BCC/16BitDOS/common
        ${CMAKE_CURRENT_LIST_DIR}/BCC/16BitDOS/Flsh186>

    $<$<STREQUAL:${FREERTOS_PORT},BCC_16BIT_DOS_PC>:
        ${CMAKE_CURRENT_LIST_DIR}/BCC/16BitDOS/common
        ${CMAKE_CURRENT_LIST_DIR}/BCC/16BitDOS/PC>

    # ARMv7-M port for Texas Instruments Code Composer Studio
    $<$<STREQUAL:${FREERTOS_PORT},CCS_ARM_CM3>:${CMAKE_CURRENT_LIST_DIR}/CCS/ARM_CM3>

    # ARMv7E-M port for Texas Instruments Code Composer Studio
    $<$<STREQUAL:${FREERTOS_PORT},CCS_ARM_CM4F>:${CMAKE_CURRENT_LIST_DIR}/CCS/ARM_CM4F>

    # ARMv7-R port for Texas Instruments Code Composer Studio
    $<$<STREQUAL:${FREERTOS_PORT},CCS_ARM_CR4>:${CMAKE_CURRENT_LIST_DIR}/CCS/ARM_Cortex-R4>

    # Texas Instruments MSP430 port for Texas Instruments Code Composer Studio
    $<$<STREQUAL:${FREERTOS_PORT},CCS_MSP430X>:${CMAKE_CURRENT_LIST_DIR}/CCS/MSP430X>

    # NXP (formerly Motorola, Freescale) Cold Fire and 68HCS12 ports for Code Warrior
    $<$<STREQUAL:${FREERTOS_PORT},CODEWARRIOR_COLDFIRE_V1>:${CMAKE_CURRENT_LIST_DIR}/CodeWarrior/ColdFire_V1>
    $<$<STREQUAL:${FREERTOS_PORT},CODEWARRIOR_COLDFIRE_V2>:${CMAKE_CURRENT_LIST_DIR}/CodeWarrior/ColdFire_V2>
    $<$<STREQUAL:${FREERTOS_PORT},CODEWARRIOR_HCS12>:${CMAKE_CURRENT_LIST_DIR}/CodeWarrior/HCS12>

    # ARMv7-A port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CA9>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CA9>

    # ARMv8-A ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_AARCH64>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_AARCH64>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_AARCH64_SRE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_AARCH64_SRE>

    # ARMv6-M port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM0>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM0>

    # ARMv6-M / Cortex-M0 Raspberry PI RP2040 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RP2040>:${CMAKE_CURRENT_LIST_DIR}/ThirdParty/GCC/RP2040/include>

    # ARMv7-M ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM3>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM3>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM3_MPU>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM3_MPU>

    # ARMv7E-M ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM4_MPU>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM4_MPU>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM4F>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM4F>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM7>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM7/r0p1>

    # ARMv8-M ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM23_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM23/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM23_SECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM23/secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM23_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM23_NTZ/non_secure>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM33_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM33/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM33_SECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM33/secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM33_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM33_NTZ/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM33_TFM>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM33_NTZ/non_secure>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM35P_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM35P/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM35P_SECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM35P/secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM35P_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM35P_NTZ/non_secure>

    # ARMv8.1-M ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM55_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM55/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM55_SECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM55/secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM55_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM55_NTZ/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM55_TFM>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM55_NTZ/non_secure>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM85_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM85/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM85_SECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM85/secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM85_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM85_NTZ/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CM85_TFM>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CM85_NTZ/non_secure>

    # ARMv7-R ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CR5>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CR5>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CRX_MPU>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CRx_MPU>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM_CRX_NOGIC>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM_CRx_No_GIC>

    # ARMv4T ARM7TDMI ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM7_AT91FR40008>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM7_AT91FR40008>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM7_AT91SAM7S>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM7_AT91SAM7S>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM7_LPC2000>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM7_LPC2000>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARM7_LPC23XX>:${CMAKE_CURRENT_LIST_DIR}/GCC/ARM7_LPC23xx>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_STR75X>:${CMAKE_CURRENT_LIST_DIR}/GCC/STR75x>

    # Microchip (formerly Ateml) AVR8 ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ATMEGA323>:${CMAKE_CURRENT_LIST_DIR}/GCC/ATMega323>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ATMEGA>:${CMAKE_CURRENT_LIST_DIR}/ThirdParty/GCC/ATmega>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_AVRDX>:${CMAKE_CURRENT_LIST_DIR}/ThirdParty/Partner-Supported-Ports/GCC/AVR_AVRDx>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_AVR_MEGA0>:${CMAKE_CURRENT_LIST_DIR}/ThirdParty/Partner-Supported-Ports/GCC/AVR_Mega0>

    # Microchip (formerly Ateml) AVR32 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_AVR32_UC3>:${CMAKE_CURRENT_LIST_DIR}/GCC/AVR32_UC3>

    # NXP (formerly Motorola, Freescale) Cold Fire and 68HCS12 ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_COLDFIRE_V2>:${CMAKE_CURRENT_LIST_DIR}/GCC/ColdFire_V2>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_HCS12>:${CMAKE_CURRENT_LIST_DIR}/GCC/HCS12>

    # Cortus APS3 soft core port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_CORTUS_APS3>:${CMAKE_CURRENT_LIST_DIR}/GCC/CORTUS_APS3>

    # Renesas (formerly Hitach) H8S port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_H8S2329>:${CMAKE_CURRENT_LIST_DIR}/GCC/H8S2329>

    # x86 / IA32 flat memory model port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_IA32_FLAT>:${CMAKE_CURRENT_LIST_DIR}/GCC/IA32_flat>

    # Intel (formerly Altera) NIOS II soft core port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_NIOSII>:${CMAKE_CURRENT_LIST_DIR}/GCC/NiosII>

    # Texas Instruments MSP430 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_MSP430F449>:${CMAKE_CURRENT_LIST_DIR}/GCC/MSP430F449>

    # Xilinx MicroBlaze soft core ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_MICROBLAZE>:${CMAKE_CURRENT_LIST_DIR}/GCC/MicroBlaze>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_MICROBLAZE_V8>:${CMAKE_CURRENT_LIST_DIR}/GCC/MicroBlazeV8>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_MICROBLAZE_V9>:${CMAKE_CURRENT_LIST_DIR}/GCC/MicroBlazeV9>

    # Xilinx PCC4XX soft core ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_PPC405_XILINX>:${CMAKE_CURRENT_LIST_DIR}/GCC/PPC405_Xilinx>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_PPC440_XILINX>:${CMAKE_CURRENT_LIST_DIR}/GCC/PPC440_Xilinx>

    # RISC-V architecture ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RISC_V>:
        ${CMAKE_CURRENT_LIST_DIR}/GCC/RISC-V
        ${CMAKE_CURRENT_LIST_DIR}/GCC/RISC-V/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_RISC_V_PULPINO_VEGA_RV32M1RM>:
        ${CMAKE_CURRENT_LIST_DIR}/GCC/RISC-V
        ${CMAKE_CURRENT_LIST_DIR}/GCC/RISC-V/chip_specific_extensions/Pulpino_Vega_RV32M1RM>

    $<$<STREQUAL:${FREERTOS_PORT},GCC_RISC_V_GENERIC>:
        ${CMAKE_CURRENT_LIST_DIR}/GCC/RISC-V
        ${CMAKE_CURRENT_LIST_DIR}/GCC/RISC-V/chip_specific_extensions/${FREERTOS_RISCV_EXTENSION}>

    # Renesas RL78 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RL78>:${CMAKE_CURRENT_LIST_DIR}/GCC/RL78>

    # Renesas RX architecture ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX100>:${CMAKE_CURRENT_LIST_DIR}/GCC/RX100>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX200>:${CMAKE_CURRENT_LIST_DIR}/GCC/RX200>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX600>:${CMAKE_CURRENT_LIST_DIR}/GCC/RX600>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX600_V2>:${CMAKE_CURRENT_LIST_DIR}/GCC/RX600v2>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_RX700_V3_DPFPU>:${CMAKE_CURRENT_LIST_DIR}/GCC/RX700v3_DPFPU>

    # Infineon TriCore 1782 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_TRICORE_1782>:${CMAKE_CURRENT_LIST_DIR}/GCC/TriCore_1782>

    # Synopsys ARC architecture ports for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARC_EM_HS>:${CMAKE_CURRENT_LIST_DIR}/ThirdParty/GCC/ARC_EM_HS>
    $<$<STREQUAL:${FREERTOS_PORT},GCC_ARC_V1>:${CMAKE_CURRENT_LIST_DIR}/ThirdParty/GCC/ARC_v1>

    # Posix Simulator port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_POSIX>:
        ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/GCC/Posix
        ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/GCC/Posix/utils>

    # Xtensa LX / Espressif ESP32 port for GCC
    $<$<STREQUAL:${FREERTOS_PORT},GCC_XTENSA_ESP32>:
        ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/GCC/Xtensa_ESP32
        ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/GCC/Xtensa_ESP32/include>

    # Renesas (formerly NEC) 78K port for IAR EW78K
    $<$<STREQUAL:${FREERTOS_PORT},IAR_78K0K>:${CMAKE_CURRENT_LIST_DIR}/IAR/78K0R>

    # ARMv7-A ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CA5_NOGIC>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CA5_No_GIC>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CA9>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CA9>

    # ARMv6-M port for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM0>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM0>

    # ARMv7-M port for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM3>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM3>

    # ARMv7E-M ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM4F>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM4F>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM4F_MPU>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM4F_MPU>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM7>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM7/r0p1>

    # ARMv8-M Ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM23_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM23/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM23_SECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM23/secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM23_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM23_NTZ/non_secure>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM33_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM33/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM33_SECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM33/secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM33_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM33_NTZ/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM33_TFM>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM33_NTZ/non_secure>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM35P_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM35P/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM35P_SECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM35P/secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM35P_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM35P_NTZ/non_secure>

    # ARMv8.1-M ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM55_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM55/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM55_SECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM55/secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM55_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM55_NTZ/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM55_TFM>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM55_NTZ/non_secure>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM85_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM85/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM85_SECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM85/secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM85_NTZ_NONSECURE>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM85_NTZ/non_secure>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CM85_TFM>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CM85_NTZ/non_secure>

    # ARMv7-R Ports for IAR EWARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ARM_CRX_NOGIC>:${CMAKE_CURRENT_LIST_DIR}/IAR/ARM_CRx_No_GIC>

    # ARMv4T ARM7TDMI ports for IAR Embedded Workbench for ARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_STR71X>:${CMAKE_CURRENT_LIST_DIR}/IAR/STR71x>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_STR75X>:${CMAKE_CURRENT_LIST_DIR}/IAR/STR75x>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_LPC2000>:${CMAKE_CURRENT_LIST_DIR}/IAR/LPC2000>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ATMEL_SAM7S64>:${CMAKE_CURRENT_LIST_DIR}/IAR/AtmelSAM7S64>

    # ARMv5TE ARM926 ports for IAR Embedded Workbench for ARM
    $<$<STREQUAL:${FREERTOS_PORT},IAR_STR91X>:${CMAKE_CURRENT_LIST_DIR}/IAR/STR91x>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ATMEL_SAM9XE>:${CMAKE_CURRENT_LIST_DIR}/IAR/AtmelSAM9XE>

    # Microchip (formerly Atmel) AVR8 ports for IAR EWAVR
    $<$<STREQUAL:${FREERTOS_PORT},IAR_ATMEGA323>:${CMAKE_CURRENT_LIST_DIR}/IAR/ATMega323>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_AVR_AVRDX>:${CMAKE_CURRENT_LIST_DIR}/IAR/AVR_AVRDx>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_AVR_MEGA0>:${CMAKE_CURRENT_LIST_DIR}/IAR/AVR_Mega0>

    # Microchip (formerly Atmel) AVR32 port for IAR Embedded Workbench for AVR32
    $<$<STREQUAL:${FREERTOS_PORT},IAR_AVR32_UC3>:${CMAKE_CURRENT_LIST_DIR}/IAR/AVR32_UC3>

    # Texas Instruments MSP430 ports for IAR Embedded Workbench for MSP430
    $<$<STREQUAL:${FREERTOS_PORT},IAR_MSP430>:${CMAKE_CURRENT_LIST_DIR}/IAR/MSP430>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_MSP430X>:${CMAKE_CURRENT_LIST_DIR}/IAR/MSP430X>

    # RISC-V architecture port for IAR Embedded Workbench for RISC-V
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RISC_V>:
        ${CMAKE_CURRENT_LIST_DIR}/IAR/RISC-V
        ${CMAKE_CURRENT_LIST_DIR}/IAR/RISC-V/chip_specific_extensions/RV32I_CLINT_no_extensions>

    $<$<STREQUAL:${FREERTOS_PORT},IAR_RISC_V_GENERIC>:
        ${CMAKE_CURRENT_LIST_DIR}/IAR/RISC-V
        ${CMAKE_CURRENT_LIST_DIR}/IAR/RISC-V/chip_specific_extensions/${FREERTOS_RISCV_EXTENSION}>

    # Renesas RL78 port for IAR EWRL78
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RL78>:${CMAKE_CURRENT_LIST_DIR}/IAR/RL78>

    # Renesas RX architecture ports for IAR EWRX
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RX100>:${CMAKE_CURRENT_LIST_DIR}/IAR/RX100>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RX600>:${CMAKE_CURRENT_LIST_DIR}/IAR/RX600>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RX700_V3_DPFPU>:${CMAKE_CURRENT_LIST_DIR}/IAR/RX700v3_DPFPU>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_RX_V2>:${CMAKE_CURRENT_LIST_DIR}/IAR/RXv2>

    # Renesas (formerly NEC) V850ES port for IAR EWV850
    $<$<STREQUAL:${FREERTOS_PORT},IAR_V850ES_FX3>:${CMAKE_CURRENT_LIST_DIR}/IAR/V850ES>
    $<$<STREQUAL:${FREERTOS_PORT},IAR_V850ES_HX3>:${CMAKE_CURRENT_LIST_DIR}/IAR/V850ES>

    # ARM Cortex-M4F port for the MikroElektronika MikroC compiler
    $<$<STREQUAL:${FREERTOS_PORT},MIKROC_ARM_CM4F>:${CMAKE_CURRENT_LIST_DIR}/MikroC/ARM_CM4F>

    # Microchip PIC18 8-bit MCU port for MPLAB XC8
    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC18F>:${CMAKE_CURRENT_LIST_DIR}/MPLAB/PIC18F>

    # Microchip PIC24 16-bit MCU port for MPLAB XC16
    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC24>:${CMAKE_CURRENT_LIST_DIR}/MPLAB/PIC24_dsPIC>

    # Microchip MIPS 32-Bit MCU ports for MPLAB XC32
    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC32MEC14XX>:${CMAKE_CURRENT_LIST_DIR}/MPLAB/PIC32MEC14xx>
    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC32MX>:${CMAKE_CURRENT_LIST_DIR}/MPLAB/PIC32MX>
    $<$<STREQUAL:${FREERTOS_PORT},MPLAB_PIC32MZ>:${CMAKE_CURRENT_LIST_DIR}/MPLAB/PIC32MZ>

    # Windows Simulator for Microsoft Visual C Compiler and MinGW GCC
    $<$<STREQUAL:${FREERTOS_PORT},MSVC_MINGW>:${CMAKE_CURRENT_LIST_DIR}/MSVC-MingW>

    # 16 bit DOS ports for Open Watcom
    $<$<STREQUAL:${FREERTOS_PORT},OWATCOM_16BIT_DOS_FLSH186>:
        ${CMAKE_CURRENT_LIST_DIR}/oWatcom/16BitDOS/common
        ${CMAKE_CURRENT_LIST_DIR}/oWatcom/16BitDOS/Flsh186>
    $<$<STREQUAL:${FREERTOS_PORT},OWATCOM_16BIT_DOS_PC>:
        ${CMAKE_CURRENT_LIST_DIR}/oWatcom/16BitDOS/common
        ${CMAKE_CURRENT_LIST_DIR}/oWatcom/16BitDOS/PC>

    $<$<STREQUAL:${FREERTOS_PORT},PARADIGM_TERN_EE_LARGE>:${CMAKE_CURRENT_LIST_DIR}/Paradigm/Tern_EE/large_untested>
    $<$<STREQUAL:${FREERTOS_PORT},PARADIGM_TERN_EE_SMALL>:${CMAKE_CURRENT_LIST_DIR}/Paradigm/Tern_EE/small>

    # Renesas RX mcu ports for Renesas CC-RX
    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX100>:${CMAKE_CURRENT_LIST_DIR}/Renesas/RX100>
    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX200>:${CMAKE_CURRENT_LIST_DIR}/Renesas/RX200>
    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX600>:${CMAKE_CURRENT_LIST_DIR}/Renesas/RX600>
    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX600_V2>:${CMAKE_CURRENT_LIST_DIR}/Renesas/RX600v2>
    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_RX700_V3_DPFPU>:${CMAKE_CURRENT_LIST_DIR}/Renesas/RX700v3_DPFPU>

    # Renesas (formerly  Hitach) SHA2 SuperH port for the Renesas SH C Compiler
    $<$<STREQUAL:${FREERTOS_PORT},RENESAS_SH2A_FPU>:${CMAKE_CURRENT_LIST_DIR}/Renesas/SH2A_FPU>

    # Texas Instruments MSP430 port for Rowley CrossWorks
    $<$<STREQUAL:${FREERTOS_PORT},ROWLEY_MSP430F449>:${CMAKE_CURRENT_LIST_DIR}/Rowley/MSP430F449>

    # ARMv7-A Cortex-A9 port for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CA9>:${CMAKE_CURRENT_LIST_DIR}/RVDS/ARM_CA9>

    # ARMv6-M port for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM0>:${CMAKE_CURRENT_LIST_DIR}/RVDS/ARM_CM0>

    # ARMv7-M port for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM3>:${CMAKE_CURRENT_LIST_DIR}/RVDS/ARM_CM3>

    # ARMv7E-M ports for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM4_MPU>:${CMAKE_CURRENT_LIST_DIR}/RVDS/ARM_CM4_MPU>
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM4F>:${CMAKE_CURRENT_LIST_DIR}/RVDS/ARM_CM4F>
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM_CM7>:${CMAKE_CURRENT_LIST_DIR}/RVDS/ARM_CM7/r0p1>

    # ARMv4T / ARM7TDMI LPC21XX port for ARM RVDS / armcc
    $<$<STREQUAL:${FREERTOS_PORT},RVDS_ARM7_LPC21XX>:${CMAKE_CURRENT_LIST_DIR}/RVDS/ARM7_LPC21xx>

    # Cygnal c8051 port for SDCC (Small Device C Compiler)
    $<$<STREQUAL:${FREERTOS_PORT},SDCC_CYGNAL>:${CMAKE_CURRENT_LIST_DIR}/SDCC/Cygnal>

    # Infineon (formerly Fujitsu, Spansion, Cypress) MB9x ports for Softune C Compiler
    $<$<STREQUAL:${FREERTOS_PORT},SOFTUNE_MB91460>:${CMAKE_CURRENT_LIST_DIR}/Softune/MB91460>
    $<$<STREQUAL:${FREERTOS_PORT},SOFTUNE_MB96340>:${CMAKE_CURRENT_LIST_DIR}/Softune/MB96340>

    # ARMv7E-M (Cortex-M4F) port for TASKING VX-toolset for ARM
    $<$<STREQUAL:${FREERTOS_PORT},TASKING_ARM_CM4F>:${CMAKE_CURRENT_LIST_DIR}/Tasking/ARM_CM4F>

    # Port for C-SKY T-HEAD CK802
    $<$<STREQUAL:${FREERTOS_PORT},CDK_THEAD_CK802>:${CMAKE_CURRENT_LIST_DIR}/ThirdParty/CDK/T-HEAD_CK802>

    # Tensilica Xtensa port for XCC
    $<$<STREQUAL:${FREERTOS_PORT},XCC_XTENSA>:${CMAKE_CURRENT_LIST_DIR}/ThirdParty/XCC/Xtensa>

    # Microchip PIC18 port for WIZ-C
    $<$<STREQUAL:${FREERTOS_PORT},WIZC_PIC18>:${CMAKE_CURRENT_LIST_DIR}/WizC/PIC18>
)

if(FREERTOS_PORT STREQUAL GCC_POSIX)
    find_package(Threads REQUIRED)
endif()

target_link_libraries(freertos_kernel_port
    PUBLIC
        $<$<STREQUAL:${FREERTOS_PORT},GCC_RP2040>:pico_base_headers>
        $<$<STREQUAL:${FREERTOS_PORT},GCC_XTENSA_ESP32>:idf::esp32>
        freertos_kernel_port_headers
    PRIVATE
        freertos_kernel_include
        $<$<STREQUAL:${FREERTOS_PORT},GCC_POSIX>:Threads::Threads>
        "$<$<STREQUAL:${FREERTOS_PORT},GCC_RP2040>:hardware_clocks;hardware_exception>"
        $<$<STREQUAL:${FREERTOS_PORT},MSVC_MINGW>:winmm> # Windows library which implements timers
)
