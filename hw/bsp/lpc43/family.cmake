include_guard()

set(SDK_DIR ${TOP}/hw/mcu/nxp/lpcopen/lpc43xx/lpc_chip_43xx)
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC43XX CACHE INTERNAL "")


#------------------------------------
# Startup & Linker script
#------------------------------------
set(STARTUP_FILE_GNU ${SDK_DIR}/../gcc/cr_startup_lpc43xx.c)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
set(STARTUP_FILE_IAR ${SDK_DIR}/../iar/iar_startup_lpc18xx43xx.s)
set(LD_FILE_IAR ${SDK_DIR}/../iar/linker/lpc18xx_43xx_ldscript_iflash.icf)

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/src/chip_18xx_43xx.c
    ${SDK_DIR}/src/clock_18xx_43xx.c
    ${SDK_DIR}/src/fpu_init.c
    ${SDK_DIR}/src/gpio_18xx_43xx.c
    ${SDK_DIR}/src/iap_18xx_43xx.c
    ${SDK_DIR}/src/sysinit_18xx_43xx.c
    ${SDK_DIR}/src/uart_18xx_43xx.c
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __USE_LPCOPEN
    CORE_M4
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/inc
    ${SDK_DIR}/inc/config_43xx
    ${CMSIS_5}/CMSIS/Core/Include
    )

  update_board(${BOARD_TARGET})

  # warning by LPCOpen
  if (TOOLCHAIN STREQUAL "gcc" OR TOOLCHAIN STREQUAL "clang")
    set_target_properties(${BOARD_TARGET} PROPERTIES COMPILE_FLAGS -Wno-error=incompatible-pointer-types)
  endif ()
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_LPC43XX)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()

  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
