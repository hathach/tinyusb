include_guard()

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

string(REPLACE "mm32f" "MM32F" MCU_VARIANT_UPPER ${MCU_VARIANT})
set(SDK_DIR ${TOP}/hw/mcu/mindmotion/mm32sdk/${MCU_VARIANT_UPPER})
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m3 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS MM32F327X CACHE INTERNAL "")


#------------------------------------
# Startup & Linker script
#------------------------------------
set(STARTUP_FILE_GNU ${SDK_DIR}/Source/GCC_StartAsm/startup_${MCU_VARIANT}_gcc.s)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
set(STARTUP_FILE_IAR ${SDK_DIR}/Source/IAR_StartAsm/startup_${MCU_VARIANT}_iar.s)
set(LD_FILE_Clang ${LD_FILE_GNU})
# set(LD_FILE_IAR )

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/Source/system_${MCU_VARIANT}.c
    ${SDK_DIR}/HAL_Lib/Src/hal_gpio.c
    ${SDK_DIR}/HAL_Lib/Src/hal_rcc.c
    ${SDK_DIR}/HAL_Lib/Src/hal_uart.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMSIS_5}/CMSIS/Core/Include
    ${SDK_DIR}/Include
    ${SDK_DIR}/HAL_Lib/Inc
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_MM32F327X)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/mindmotion/mm32/dcd_mm32f327x_otg.c
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
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
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
endfunction()
