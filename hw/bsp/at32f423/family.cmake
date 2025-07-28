include_guard()

set(AT_FAMILY at32f423)
set(AT_SDK_LIB ${TOP}/hw/mcu/artery/${AT_FAMILY}/libraries)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS AT32F423 CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  # Startup & Linker script
  set(STARTUP_FILE_GNU ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_${AT_FAMILY}.s)
  set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
  set(STARTUP_FILE_IAR ${AT_SDK_LIB}/cmsis/cm4/device_support/startup/iar/startup_${AT_FAMILY}.s)

  set(LD_FILE_Clang ${LD_FILE_GNU})

  add_library(${BOARD_TARGET} STATIC
    ${AT_SDK_LIB}/cmsis/cm4/device_support/system_${AT_FAMILY}.c
    ${AT_SDK_LIB}/drivers/src/${AT_FAMILY}_gpio.c
    ${AT_SDK_LIB}/drivers/src/${AT_FAMILY}_misc.c
    ${AT_SDK_LIB}/drivers/src/${AT_FAMILY}_usart.c
    ${AT_SDK_LIB}/drivers/src/${AT_FAMILY}_acc.c
    ${AT_SDK_LIB}/drivers/src/${AT_FAMILY}_crm.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${AT_SDK_LIB}/cmsis/cm4/core_support
    ${AT_SDK_LIB}/cmsis/cm4/device_support
    ${AT_SDK_LIB}/drivers/inc
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
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${AT_FAMILY}_clock.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${AT_FAMILY}_int.c
    )
  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_AT32F423)
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
