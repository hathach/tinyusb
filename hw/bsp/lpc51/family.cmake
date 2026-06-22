include_guard()

set(MCUX_DIR ${TOP}/hw/mcu/nxp/mcuxsdk-core)
set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-devices-lpc)
set(CMSIS_DIR ${TOP}/lib/CMSIS_6)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m0plus CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC51 CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/gcc/${MCU_VARIANT}_flash.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})

if (NOT DEFINED STARTUP_FILE_GNU)
  set(STARTUP_FILE_GNU ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT}.S)
endif ()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

if (NOT DEFINED LD_FILE_IAR)
  set(LD_FILE_IAR ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/iar/${MCU_VARIANT}_flash.icf)
endif ()

if (NOT DEFINED STARTUP_FILE_IAR)
  set(STARTUP_FILE_IAR ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/iar/startup_${MCU_VARIANT}.s)
endif ()

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    # driver
    ${MCUX_DIR}/drivers/lpc_gpio/fsl_gpio.c
    ${MCUX_DIR}/drivers/flexcomm/fsl_flexcomm.c
    ${MCUX_DIR}/drivers/flexcomm/usart/fsl_usart.c
    # mcu
    ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/system_${MCU_VARIANT}.c
    ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/drivers/fsl_clock.c
    ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/drivers/fsl_power.c
    ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/drivers/fsl_reset.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${TOP}/lib/sct_neopixel
    # driver
    ${MCUX_DIR}/drivers/common
    ${MCUX_DIR}/drivers/common
    ${MCUX_DIR}/drivers/flexcomm
    ${MCUX_DIR}/drivers/flexcomm/usart
    ${MCUX_DIR}/drivers/lpc_iocon
    ${MCUX_DIR}/drivers/lpc_gpio
    # mcu
    ${SDK_DIR}/LPC51U68/${MCU_VARIANT}
    ${SDK_DIR}/LPC51U68/${MCU_VARIANT}/drivers
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${SDK_DIR}/LPC51U68/periph
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    CFG_TUSB_MEM_ALIGN=TU_ATTR_ALIGNED\(64\)
    __STARTUP_CLEAR_BSS
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_LPC51)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c
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
      -nostartfiles
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")
  endif ()
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
  #family_flash_nxplink(${TARGET})
  #family_flash_pyocd(${TARGET})
endfunction()
