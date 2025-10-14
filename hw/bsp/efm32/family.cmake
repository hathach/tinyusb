include_guard()

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# EFM32_FAMILY should be set by board.cmake (e.g. efm32gg12b)
string(TOUPPER ${EFM32_FAMILY} EFM32_FAMILY_UPPER)
set(SILABS_CMSIS ${TOP}/hw/mcu/silabs/cmsis-dfp-${EFM32_FAMILY}/Device/SiliconLabs/${EFM32_FAMILY_UPPER})
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS EFM32GG CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  set(LD_FILE_GNU ${SILABS_CMSIS}/Source/GCC/${EFM32_FAMILY}.ld)
  set(LD_FILE_Clang ${LD_FILE_GNU})

  if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
    message(FATAL_ERROR "LD_FILE_${CMAKE_C_COMPILER_ID} not defined")
  endif ()

  set(STARTUP_FILE_GNU ${SILABS_CMSIS}/Source/GCC/startup_${EFM32_FAMILY}.S)
  set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

  add_library(${BOARD_TARGET} STATIC
    ${SILABS_CMSIS}/Source/system_${EFM32_FAMILY}.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMSIS_5}/CMSIS/Core/Include
    ${SILABS_CMSIS}/Include
    )

  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __STARTUP_CLEAR_BSS
    __START=main
    ${EFM32_MCU}
    )

  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()
endfunction()


#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})

  # Board target
  add_board_target(board_${BOARD})

  #---------- Port Specific ----------
  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PUBLIC
    # BSP
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    )
  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_EFM32GG)
  target_sources(${TARGET} PUBLIC
    ${TOP}/src/portable/synopsys/dwc2/dcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/hcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/dwc2_common.c
    )
  target_link_libraries(${TARGET} PUBLIC board_${BOARD})

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
