include_guard()

set(AT32_FAMILY at32f403a_407)
set(AT32_SDK_LIB ${TOP}/hw/mcu/artery/${AT32_FAMILY}/libraries)

string(TOUPPER ${AT32_FAMILY} AT32_FAMILY_UPPER)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS ${AT32_FAMILY_UPPER} CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
set(STARTUP_FILE_GNU ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/gcc/startup_${AT32_FAMILY}.s)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
set(STARTUP_FILE_IAR ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/iar/startup_${AT32_FAMILY}.s)
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/gcc/linker/${MCU_LINKER_NAME}_FLASH.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
set(LD_FILE_IAR ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/iar/linker/${MCU_LINKER_NAME}.icf)

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${AT32_SDK_LIB}/cmsis/cm4/device_support/system_${AT32_FAMILY}.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_gpio.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_misc.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_usart.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_acc.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_crm.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${AT32_SDK_LIB}/cmsis/cm4/core_support
    ${AT32_SDK_LIB}/cmsis/cm4/device_support
    ${AT32_SDK_LIB}/drivers/inc
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_${AT32_FAMILY_UPPER})

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${AT32_FAMILY}_clock.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${AT32_FAMILY}_int.c
    ${TOP}/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
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
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
