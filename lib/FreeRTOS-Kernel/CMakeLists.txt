cmake_minimum_required(VERSION 3.15)

# User is responsible to one mandatory option:
#   FREERTOS_PORT, if not specified and native port detected, uses the native compile.
#
# User is responsible for one library target:
#   freertos_config ,typically an INTERFACE library
#
# DEPRECATED: FREERTOS_CONFIG_FILE_DIRECTORY - but still supported if no freertos_config defined for now.
#             May be removed at some point in the future.
#
# User can choose which heap implementation to use (either the implementations
# included with FreeRTOS [1..5] or a custom implementation) by providing the
# option FREERTOS_HEAP. When dynamic allocation is used, the user must specify a
# heap implementation. If the option is not set, the cmake will use no heap
# implementation (e.g. when only static allocation is used).

# `freertos_config` target defines the path to FreeRTOSConfig.h and optionally other freertos based config files
if(NOT TARGET freertos_config )
    if (NOT DEFINED FREERTOS_CONFIG_FILE_DIRECTORY )

        message(FATAL_ERROR " freertos_config target not specified.  Please specify a cmake target that defines the include directory for FreeRTOSConfig.h:\n"
            "  add_library(freertos_config INTERFACE)\n"
            "  target_include_directories(freertos_config SYSTEM\n"
            "    INTERFACE\n"
            "      include) # The config file directory\n"
            "  target_compile_definitions(freertos_config\n"
            "    PUBLIC\n"
            "    projCOVERAGE_TEST=0)\n")
    else()
        message(WARNING " Using deprecated 'FREERTOS_CONFIG_FILE_DIRECTORY' - please update your project CMakeLists.txt file:\n"
            "  add_library(freertos_config INTERFACE)\n"
            "  target_include_directories(freertos_config SYSTEM\n"
            "    INTERFACE\n"
            "      include) # The config file directory\n"
            "  target_compile_definitions(freertos_config\n"
            "    PUBLIC\n"
            "    projCOVERAGE_TEST=0)\n")
    endif()
endif()

# FreeRTOS port option
if(NOT FREERTOS_PORT)
    message(WARNING " FREERTOS_PORT is not set. Please specify it from top-level CMake file (example):\n"
        "  set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING \"\")\n"
        " or from CMake command line option:\n"
        "  -DFREERTOS_PORT=GCC_ARM_CM4F\n"
        " \n"
        " Available port options:\n"
        " A_CUSTOM_PORT                    - Compiler: User Defined  Target: User Defined\n"
        " BCC_16BIT_DOS_FLSH186            - Compiler: BCC           Target: 16 bit DOS Flsh186\n"
        " BCC_16BIT_DOS_PC                 - Compiler: BCC           Target: 16 bit DOS PC\n"
        " CCS_ARM_CM3                      - Compiler: CCS           Target: ARM Cortex-M3\n"
        " CCS_ARM_CM4F                     - Compiler: CCS           Target: ARM Cortex-M4 with FPU\n"
        " CCS_ARM_CR4                      - Compiler: CCS           Target: ARM Cortex-R4\n"
        " CCS_MSP430X                      - Compiler: CCS           Target: MSP430X\n"
        " CODEWARRIOR_COLDFIRE_V1          - Compiler: CoreWarrior   Target: ColdFire V1\n"
        " CODEWARRIOR_COLDFIRE_V2          - Compiler: CoreWarrior   Target: ColdFire V2\n"
        " CODEWARRIOR_HCS12                - Compiler: CoreWarrior   Target: HCS12\n"
        " GCC_ARM_CA9                      - Compiler: GCC           Target: ARM Cortex-A9\n"
        " GCC_ARM_AARCH64                  - Compiler: GCC           Target: ARM v8-A\n"
        " GCC_ARM_AARCH64_SRE              - Compiler: GCC           Target: ARM v8-A SRE\n"
        " GCC_ARM_CM0                      - Compiler: GCC           Target: ARM Cortex-M0\n"
        " GCC_ARM_CM3                      - Compiler: GCC           Target: ARM Cortex-M3\n"
        " GCC_ARM_CM3_MPU                  - Compiler: GCC           Target: ARM Cortex-M3 with MPU\n"
        " GCC_ARM_CM4_MPU                  - Compiler: GCC           Target: ARM Cortex-M4 with MPU\n"
        " GCC_ARM_CM4F                     - Compiler: GCC           Target: ARM Cortex-M4 with FPU\n"
        " GCC_ARM_CM7                      - Compiler: GCC           Target: ARM Cortex-M7\n"
        " GCC_ARM_CM23_NONSECURE           - Compiler: GCC           Target: ARM Cortex-M23 non-secure\n"
        " GCC_ARM_CM23_SECURE              - Compiler: GCC           Target: ARM Cortex-M23 secure\n"
        " GCC_ARM_CM23_NTZ_NONSECURE       - Compiler: GCC           Target: ARM Cortex-M23 non-trustzone non-secure\n"
        " GCC_ARM_CM33_NONSECURE           - Compiler: GCC           Target: ARM Cortex-M33 non-secure\n"
        " GCC_ARM_CM33_SECURE              - Compiler: GCC           Target: ARM Cortex-M33 secure\n"
        " GCC_ARM_CM33_NTZ_NONSECURE       - Compiler: GCC           Target: ARM Cortex-M33 non-trustzone non-secure\n"
        " GCC_ARM_CM33_TFM                 - Compiler: GCC           Target: ARM Cortex-M33 non-secure for TF-M\n"
        " GCC_ARM_CM35P_NONSECURE          - Compiler: GCC           Target: ARM Cortex-M35P non-secure\n"
        " GCC_ARM_CM35P_SECURE             - Compiler: GCC           Target: ARM Cortex-M35P secure\n"
        " GCC_ARM_CM35P_NTZ_NONSECURE      - Compiler: GCC           Target: ARM Cortex-M35P non-trustzone non-secure\n"
        " GCC_ARM_CM55_NONSECURE           - Compiler: GCC           Target: ARM Cortex-M55 non-secure\n"
        " GCC_ARM_CM55_SECURE              - Compiler: GCC           Target: ARM Cortex-M55 secure\n"
        " GCC_ARM_CM55_NTZ_NONSECURE       - Compiler: GCC           Target: ARM Cortex-M55 non-trustzone non-secure\n"
        " GCC_ARM_CM55_TFM                 - Compiler: GCC           Target: ARM Cortex-M55 non-secure for TF-M\n"
        " GCC_ARM_CM85_NONSECURE           - Compiler: GCC           Target: ARM Cortex-M85 non-secure\n"
        " GCC_ARM_CM85_SECURE              - Compiler: GCC           Target: ARM Cortex-M85 secure\n"
        " GCC_ARM_CM85_NTZ_NONSECURE       - Compiler: GCC           Target: ARM Cortex-M85 non-trustzone non-secure\n"
        " GCC_ARM_CM85_TFM                 - Compiler: GCC           Target: ARM Cortex-M85 non-secure for TF-M\n"
        " GCC_ARM_CR5                      - Compiler: GCC           Target: ARM Cortex-R5\n"
        " GCC_ARM_CRX_MPU                  - Compiler: GCC           Target: ARM Cortex-Rx with MPU\n"
        " GCC_ARM_CRX_NOGIC                - Compiler: GCC           Target: ARM Cortex-Rx no GIC\n"
        " GCC_ARM7_AT91FR40008             - Compiler: GCC           Target: ARM7 Atmel AT91R40008\n"
        " GCC_ARM7_AT91SAM7S               - Compiler: GCC           Target: ARM7 Atmel AT91SAM7S\n"
        " GCC_ARM7_LPC2000                 - Compiler: GCC           Target: ARM7 LPC2000\n"
        " GCC_ARM7_LPC23XX                 - Compiler: GCC           Target: ARM7 LPC23xx\n"
        " GCC_ATMEGA323                    - Compiler: GCC           Target: ATMega323\n"
        " GCC_AVR32_UC3                    - Compiler: GCC           Target: AVR32 UC3\n"
        " GCC_COLDFIRE_V2                  - Compiler: GCC           Target: ColdFire V2\n"
        " GCC_CORTUS_APS3                  - Compiler: GCC           Target: CORTUS APS3\n"
        " GCC_H8S2329                      - Compiler: GCC           Target: H8S2329\n"
        " GCC_HCS12                        - Compiler: GCC           Target: HCS12\n"
        " GCC_IA32_FLAT                    - Compiler: GCC           Target: IA32 flat\n"
        " GCC_MICROBLAZE                   - Compiler: GCC           Target: MicroBlaze\n"
        " GCC_MICROBLAZE_V8                - Compiler: GCC           Target: MicroBlaze V8\n"
        " GCC_MICROBLAZE_V9                - Compiler: GCC           Target: MicroBlaze V9\n"
        " GCC_MSP430F449                   - Compiler: GCC           Target: MSP430F449\n"
        " GCC_NIOSII                       - Compiler: GCC           Target: NiosII\n"
        " GCC_PPC405_XILINX                - Compiler: GCC           Target: Xilinx PPC405\n"
        " GCC_PPC440_XILINX                - Compiler: GCC           Target: Xilinx PPC440\n"
        " GCC_RISC_V                       - Compiler: GCC           Target: RISC-V\n"
        " GCC_RISC_V_PULPINO_VEGA_RV32M1RM - Compiler: GCC           Target: RISC-V Pulpino Vega RV32M1RM\n"
        " GCC_RISC_V_GENERIC               - Compiler: GCC           Target: RISC-V with FREERTOS_RISCV_EXTENSION\n"
        " GCC_RL78                         - Compiler: GCC           Target: Renesas RL78\n"
        " GCC_RX100                        - Compiler: GCC           Target: Renesas RX100\n"
        " GCC_RX200                        - Compiler: GCC           Target: Renesas RX200\n"
        " GCC_RX600                        - Compiler: GCC           Target: Renesas RX600\n"
        " GCC_RX600_V2                     - Compiler: GCC           Target: Renesas RX600 v2\n"
        " GCC_RX700_V3_DPFPU               - Compiler: GCC           Target: Renesas RX700 v3 with DPFPU\n"
        " GCC_STR75X                       - Compiler: GCC           Target: STR75x\n"
        " GCC_TRICORE_1782                 - Compiler: GCC           Target: TriCore 1782\n"
        " GCC_ARC_EM_HS                    - Compiler: GCC           Target: DesignWare ARC EM HS\n"
        " GCC_ARC_V1                       - Compiler: GCC           Target: DesignWare ARC v1\n"
        " GCC_ATMEGA                       - Compiler: GCC           Target: ATmega\n"
        " GCC_POSIX                        - Compiler: GCC           Target: Posix\n"
        " GCC_RP2040                       - Compiler: GCC           Target: RP2040 ARM Cortex-M0+\n"
        " GCC_XTENSA_ESP32                 - Compiler: GCC           Target: Xtensa ESP32\n"
        " GCC_AVRDX                        - Compiler: GCC           Target: AVRDx\n"
        " GCC_AVR_MEGA0                    - Compiler: GCC           Target: AVR Mega0\n"
        " IAR_78K0K                        - Compiler: IAR           Target: Renesas 78K0K\n"
        " IAR_ARM_CA5_NOGIC                - Compiler: IAR           Target: ARM Cortex-A5 no GIC\n"
        " IAR_ARM_CA9                      - Compiler: IAR           Target: ARM Cortex-A9\n"
        " IAR_ARM_CM0                      - Compiler: IAR           Target: ARM Cortex-M0\n"
        " IAR_ARM_CM3                      - Compiler: IAR           Target: ARM Cortex-M3\n"
        " IAR_ARM_CM4F                     - Compiler: IAR           Target: ARM Cortex-M4 with FPU\n"
        " IAR_ARM_CM4F_MPU                 - Compiler: IAR           Target: ARM Cortex-M4 with FPU and MPU\n"
        " IAR_ARM_CM7                      - Compiler: IAR           Target: ARM Cortex-M7\n"
        " IAR_ARM_CM23_NONSECURE           - Compiler: IAR           Target: ARM Cortex-M23 non-secure\n"
        " IAR_ARM_CM23_SECURE              - Compiler: IAR           Target: ARM Cortex-M23 secure\n"
        " IAR_ARM_CM23_NTZ_NONSECURE       - Compiler: IAR           Target: ARM Cortex-M23 non-trustzone non-secure\n"
        " IAR_ARM_CM33_NONSECURE           - Compiler: IAR           Target: ARM Cortex-M33 non-secure\n"
        " IAR_ARM_CM33_SECURE              - Compiler: IAR           Target: ARM Cortex-M33 secure\n"
        " IAR_ARM_CM33_NTZ_NONSECURE       - Compiler: IAR           Target: ARM Cortex-M33 non-trustzone non-secure\n"
        " IAR_ARM_CM33_TFM                 - Compiler: IAR           Target: ARM Cortex-M33 non-secure for TF-M\n"
        " IAR_ARM_CM35P_NONSECURE          - Compiler: IAR           Target: ARM Cortex-M35P non-secure\n"
        " IAR_ARM_CM35P_SECURE             - Compiler: IAR           Target: ARM Cortex-M35P secure\n"
        " IAR_ARM_CM35P_NTZ_NONSECURE      - Compiler: IAR           Target: ARM Cortex-M35P non-trustzone non-secure\n"
        " IAR_ARM_CM55_NONSECURE           - Compiler: IAR           Target: ARM Cortex-M55 non-secure\n"
        " IAR_ARM_CM55_SECURE              - Compiler: IAR           Target: ARM Cortex-M55 secure\n"
        " IAR_ARM_CM55_NTZ_NONSECURE       - Compiler: IAR           Target: ARM Cortex-M55 non-trustzone non-secure\n"
        " IAR_ARM_CM55_TFM                 - Compiler: IAR           Target: ARM Cortex-M55 non-secure for TF-M\n"
        " IAR_ARM_CM85_NONSECURE           - Compiler: IAR           Target: ARM Cortex-M85 non-secure\n"
        " IAR_ARM_CM85_SECURE              - Compiler: IAR           Target: ARM Cortex-M85 secure\n"
        " IAR_ARM_CM85_NTZ_NONSECURE       - Compiler: IAR           Target: ARM Cortex-M85 non-trustzone non-secure\n"
        " IAR_ARM_CM85_TFM                 - Compiler: IAR           Target: ARM Cortex-M85 non-secure for TF-M\n"
        " IAR_ARM_CRX_NOGIC                - Compiler: IAR           Target: ARM Cortex-Rx no GIC\n"
        " IAR_ATMEGA323                    - Compiler: IAR           Target: ATMega323\n"
        " IAR_ATMEL_SAM7S64                - Compiler: IAR           Target: Atmel SAM7S64\n"
        " IAR_ATMEL_SAM9XE                 - Compiler: IAR           Target: Atmel SAM9XE\n"
        " IAR_AVR_AVRDX                    - Compiler: IAR           Target: AVRDx\n"
        " IAR_AVR_MEGA0                    - Compiler: IAR           Target: AVR Mega0\n"
        " IAR_AVR32_UC3                    - Compiler: IAR           Target: AVR32 UC3\n"
        " IAR_LPC2000                      - Compiler: IAR           Target: LPC2000\n"
        " IAR_MSP430                       - Compiler: IAR           Target: MSP430\n"
        " IAR_MSP430X                      - Compiler: IAR           Target: MSP430X\n"
        " IAR_RISC_V                       - Compiler: IAR           Target: RISC-V\n"
        " IAR_RISC_V_GENERIC               - Compiler: IAR           Target: RISC-V with FREERTOS_RISCV_EXTENSION\n"
        " IAR_RL78                         - Compiler: IAR           Target: Renesas RL78\n"
        " IAR_RX100                        - Compiler: IAR           Target: Renesas RX100\n"
        " IAR_RX600                        - Compiler: IAR           Target: Renesas RX600\n"
        " IAR_RX700_V3_DPFPU               - Compiler: IAR           Target: Renesas RX700 v3 with DPFPU\n"
        " IAR_RX_V2                        - Compiler: IAR           Target: Renesas RX v2\n"
        " IAR_STR71X                       - Compiler: IAR           Target: STR71x\n"
        " IAR_STR75X                       - Compiler: IAR           Target: STR75x\n"
        " IAR_STR91X                       - Compiler: IAR           Target: STR91x\n"
        " IAR_V850ES_FX3                   - Compiler: IAR           Target: Renesas V850ES/Fx3\n"
        " IAR_V850ES_HX3                   - Compiler: IAR           Target: Renesas V850ES/Hx3\n"
        " MIKROC_ARM_CM4F                  - Compiler: MikroC        Target: ARM Cortex-M4 with FPU\n"
        " MPLAB_PIC18F                     - Compiler: MPLAB         Target: PIC18F\n"
        " MPLAB_PIC24                      - Compiler: MPLAB         Target: PIC24\n"
        " MPLAB_PIC32MEC14XX               - Compiler: MPLAB         Target: PIC32MEC14xx\n"
        " MPLAB_PIC32MX                    - Compiler: MPLAB         Target: PIC32MX\n"
        " MPLAB_PIC32MZ                    - Compiler: MPLAB         Target: PIC32MZ\n"
        " MSVC_MINGW                       - Compiler: MSVC or MinGW Target: x86\n"
        " OWATCOM_16BIT_DOS_FLSH186        - Compiler: Open Watcom   Target: 16 bit DOS Flsh186\n"
        " OWATCOM_16BIT_DOS_PC             - Compiler: Open Watcom   Target: 16 bit DOS PC\n"
        " PARADIGM_TERN_EE_LARGE           - Compiler: Paradigm      Target: Tern EE large\n"
        " PARADIGM_TERN_EE_SMALL           - Compiler: Paradigm      Target: Tern EE small\n"
        " RENESAS_RX100                    - Compiler: Renesas       Target: RX100\n"
        " RENESAS_RX200                    - Compiler: Renesas       Target: RX200\n"
        " RENESAS_RX600                    - Compiler: Renesas       Target: RX600\n"
        " RENESAS_RX600_V2                 - Compiler: Renesas       Target: RX600 v2\n"
        " RENESAS_RX700_V3_DPFPU           - Compiler: Renesas       Target: RX700 v3 with DPFPU\n"
        " RENESAS_SH2A_FPU                 - Compiler: Renesas       Target: SH2A with FPU\n"
        " ROWLEY_MSP430F449                - Compiler: Rowley        Target: MSP430F449\n"
        " RVDS_ARM_CA9                     - Compiler: RVDS          Target: ARM Cortex-A9\n"
        " RVDS_ARM_CM0                     - Compiler: RVDS          Target: ARM Cortex-M0\n"
        " RVDS_ARM_CM3                     - Compiler: RVDS          Target: ARM Cortex-M3\n"
        " RVDS_ARM_CM4_MPU                 - Compiler: RVDS          Target: ARM Cortex-M4 with MPU\n"
        " RVDS_ARM_CM4F                    - Compiler: RVDS          Target: ARM Cortex-M4 with FPU\n"
        " RVDS_ARM_CM7                     - Compiler: RVDS          Target: ARM Cortex-M7\n"
        " RVDS_ARM7_LPC21XX                - Compiler: RVDS          Target: ARM7 LPC21xx\n"
        " SDCC_CYGNAL                      - Compiler: SDCC          Target: Cygnal\n"
        " SOFTUNE_MB91460                  - Compiler: Softune       Target: MB91460\n"
        " SOFTUNE_MB96340                  - Compiler: Softune       Target: MB96340\n"
        " TASKING_ARM_CM4F                 - Compiler: Tasking       Target: ARM Cortex-M4 with FPU\n"
        " TEMPLATE                         - Compiler: HOST          Target: None\n"
        " CDK_THEAD_CK802                  - Compiler: CDK           Target: T-head CK802\n"
        " XCC_XTENSA                       - Compiler: XCC           Target: Xtensa\n"
        " WIZC_PIC18                       - Compiler: WizC          Target: PIC18")
    # Native FREERTOS_PORT for Linux and Windows MINGW builds
    if(UNIX)
        message(STATUS " Auto-Detected Unix, setting FREERTOS_PORT=GCC_POSIX")
        set(FREERTOS_PORT GCC_POSIX CACHE STRING "FreeRTOS port name")
    elseif(MINGW)
        message(STATUS " Auto-Detected MINGW, setting FREERTOS_PORT=MSVC_MINGW")
        set(FREERTOS_PORT MSVC_MINGW CACHE STRING "FreeRTOS port name")
    endif()
elseif((FREERTOS_PORT STREQUAL "A_CUSTOM_PORT") AND (NOT TARGET freertos_kernel_port) )
    message(FATAL_ERROR " FREERTOS_PORT is set to A_CUSTOM_PORT. Please specify the custom port target with all necessary files. For example:\n"
    " Assuming a directory of:\n"
    "  FreeRTOSCustomPort/\n"
    "    CMakeLists.txt\n"
    "    port.c\n"
    "    portmacro.h\n"
    " Where FreeRTOSCustomPort/CMakeLists.txt is a modified version of:\n"
    "   add_library(freertos_kernel_port OBJECT)\n"
    "   target_sources(freertos_kernel_port\n"
    "     PRIVATE\n"
    "       port.c\n"
    "       portmacro.h)\n"
    "   add_library(freertos_kernel_port_headers INTERFACE)\n"
    "     target_include_directories(freertos_kernel_port_headers INTERFACE \n"
    "      .)\n"
    "   target_link_libraries(freertos_kernel_port\n"
    "     PRIVATE\n"
    "       freertos_kernel_port_headers\n"
    "       freertos_kernel_include)")
endif()

add_library(freertos_kernel STATIC)

########################################################################
add_subdirectory(include)
add_subdirectory(portable)

target_sources(freertos_kernel PRIVATE
    croutine.c
    event_groups.c
    list.c
    queue.c
    stream_buffer.c
    tasks.c
    timers.c
)

if (DEFINED FREERTOS_HEAP )
    # User specified a heap implementation add heap implementation to freertos_kernel.
    target_sources(freertos_kernel PRIVATE
        # If FREERTOS_HEAP is digit between 1 .. 5 - it is heap number, otherwise - it is path to custom heap source file
        $<IF:$<BOOL:$<FILTER:${FREERTOS_HEAP},EXCLUDE,^[1-5]$>>,${FREERTOS_HEAP},portable/MemMang/heap_${FREERTOS_HEAP}.c>
    )
endif()


target_link_libraries(freertos_kernel
    PUBLIC
        freertos_kernel_include
        freertos_kernel_port_headers
    PRIVATE
        freertos_kernel_port

)

########################################################################
