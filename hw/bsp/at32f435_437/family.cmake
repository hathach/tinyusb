include_guard()

set(AT32_FAMILY at32f435_437)
set(AT32_SDK_LIB ${TOP}/hw/mcu/artery/${AT32_FAMILY}/libraries)

string(TOUPPER ${AT32_FAMILY} AT32_FAMILY_UPPER)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS ${AT32_FAMILY_UPPER} CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  # Startup & Linker script
  set(STARTUP_FILE_GNU ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/gcc/startup_${AT32_FAMILY}.s)
  set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
  set(STARTUP_FILE_IAR ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/iar/startup_${AT32_FAMILY}.s)

  if (NOT DEFINED LD_FILE_GNU)
    set(LD_FILE_GNU ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/gcc/linker/${MCU_LINKER_NAME}_FLASH.ld)
  endif ()
  set(LD_FILE_Clang ${LD_FILE_GNU})
  set(LD_FILE_IAR ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/iar/linker/${MCU_LINKER_NAME}.icf)

  add_library(${BOARD_TARGET} STATIC
    ${AT32_SDK_LIB}/cmsis/cm4/device_support/system_${AT32_FAMILY}.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_gpio.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_misc.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_usart.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_acc.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_crm.c
    ${AT32_SDK_LIB}/drivers/src/${AT32_FAMILY}_exint.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${AT32_SDK_LIB}/cmsis/cm4/core_support
    ${AT32_SDK_LIB}/cmsis/cm4/device_support
    ${AT32_SDK_LIB}/drivers/inc
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    BOARD_TUD_RHPORT=1
    BOARD_TUH_RHPORT=0
    BOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
    BOARD_TUH_MAX_SPEED=OPT_MODE_FULL_SPEED
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
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${AT32_FAMILY}_clock.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${AT32_FAMILY}_int.c
    )
  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_${AT32_FAMILY_UPPER})
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
