include_guard()

set(SDK_DIR ${TOP}/hw/mcu/hpmicro/hpm_sdk)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
if (NOT DEFINED CMAKE_SYSTEM_CPU)
  set(CMAKE_SYSTEM_CPU rv32imac-ilp32 CACHE INTERNAL "System Processor")
endif ()
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/riscv_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS HPMICRO CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${HPM_SOC}/toolchains/gcc/flash_xip.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})

set(STARTUP_FILE_GNU ${HPM_SOC}/toolchains/gcc/start.S)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/pinmux.c
    ${HPM_SOC}/boot/hpm_bootheader.c
    ${HPM_SOC}/toolchains/gcc/initfini.c
    ${HPM_SOC}/toolchains/reset.c
    ${HPM_SOC}/toolchains/trap.c
    ${HPM_SOC}/system.c
    ${HPM_SOC}/hpm_sysctl_drv.c
    ${HPM_SOC}/hpm_clock_drv.c
    ${HPM_SOC}/hpm_otp_drv.c
    ${SDK_DIR}/arch/riscv/l1c/hpm_l1c_drv.c
    ${SDK_DIR}/drivers/src/hpm_gpio_drv.c
    ${SDK_DIR}/drivers/src/hpm_uart_drv.c
    ${SDK_DIR}/drivers/src/hpm_usb_drv.c
    ${SDK_DIR}/drivers/src/hpm_pcfg_drv.c
    ${SDK_DIR}/drivers/src/hpm_pmp_drv.c
    ${SDK_DIR}/drivers/src/${HPM_PLLCTL_DRV_FILE}
    )

  target_compile_definitions(${BOARD_TARGET} PUBLIC
    FLASH_XIP
    [=[CFG_TUD_MEM_SECTION=__attribute__((section(".noncacheable.non_init")))]=]
    [=[CFG_TUH_MEM_SECTION=__attribute__((section(".noncacheable.non_init")))]=]
    )

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board
    ${HPM_SOC}
    ${HPM_IP_REGS}
    ${HPM_SOC}/boot
    ${HPM_SOC}/toolchains
    ${HPM_SOC}/toolchains/gcc
    ${SDK_DIR}/arch
    ${SDK_DIR}/arch/riscv/intc
    ${SDK_DIR}/arch/riscv/l1c
    ${SDK_DIR}/drivers/inc
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_HPM)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${SDK_DIR}/utils/hpm_sbrk.c
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${TOP}/src/portable/chipidea/ci_hs
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      -Wl,--defsym,_flash_size=${BOARD_FLASH_SIZE}
      -Wl,--defsym,_stack_size=${BOARD_STACK_SIZE}
      -Wl,--defsym,_heap_size=${BOARD_HEAP_SIZE}
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -Wl,--defsym,_flash_size=${BOARD_FLASH_SIZE}
      -Wl,--defsym,_stack_size=${BOARD_STACK_SIZE}
      -Wl,--defsym,_heap_size=${BOARD_HEAP_SIZE}
      )
  endif ()

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
      ${SDK_DIR}/utils/hpm_sbrk.c
      PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes -Wno-cast-align -Wno-discarded-qualifiers"
    )
  endif ()
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
