include_guard()

set(MCUX_CORE ${TOP}/hw/mcu/nxp/mcuxsdk-core)
set(MCUX_DEVICES ${TOP}/hw/mcu/nxp/mcux-devices-kinetis)
set(CMSIS_DIR ${TOP}/lib/CMSIS_6)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m0plus CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS KINETIS_K32L CACHE INTERNAL "")


#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${MCUX_DEVICES}/K32L/${MCU_VARIANT}/gcc/${MCU_CORE}_flash.ld)
endif ()
if (NOT DEFINED STARTUP_FILE_GNU)
  set(STARTUP_FILE_GNU ${MCUX_DEVICES}/K32L/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT}.S)
endif ()

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    # driver
    ${MCUX_CORE}/drivers/gpio/fsl_gpio.c
    ${MCUX_CORE}/drivers/common/fsl_common_arm.c
    ${MCUX_CORE}/drivers/lpuart/fsl_lpuart.c
    # mcu
    ${MCUX_DEVICES}/K32L/${MCU_VARIANT}/system_${MCU_VARIANT}.c
    ${MCUX_DEVICES}/K32L/${MCU_VARIANT}/drivers/fsl_clock.c
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __STARTUP_CLEAR_BSS
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${MCUX_CORE}/drivers/common
    ${MCUX_CORE}/drivers/gpio
    ${MCUX_CORE}/drivers/lpuart
    ${MCUX_CORE}/drivers/port
    ${MCUX_CORE}/drivers/smc
    ${MCUX_DEVICES}/K32L/${MCU_VARIANT}
    ${MCUX_DEVICES}/K32L/${MCU_VARIANT}/drivers
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_KINETIS_K32L)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/nxp/khci/dcd_khci.c
    ${TOP}/src/portable/nxp/khci/hcd_khci.c
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
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_flash_jlink(${TARGET})
  family_add_bin_hex(${TARGET})
  family_flash_teensy(${TARGET})
endfunction()
