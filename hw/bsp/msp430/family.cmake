include_guard()

set(SDK_DIR ${TOP}/hw/mcu/ti/msp430/msp430-gcc-support-files/include)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU msp430 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/msp430_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS MSP430x5xx CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} INTERFACE)
  target_compile_definitions(${BOARD_TARGET} INTERFACE
    CFG_TUD_ENDPOINT0_SIZE=8
    CFG_EXAMPLE_VIDEO_READONLY
    CFG_EXAMPLE_MSC_READONLY
    )
  target_include_directories(${BOARD_TARGET} INTERFACE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${SDK_DIR}
    )

  update_board(${BOARD_TARGET})
endfunction()


#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_MSP430x5xx)

  #---------- Port Specific ----------
  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/ti/msp430x5xx/dcd_msp430x5xx.c
    )
  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -L${SDK_DIR}
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()
  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_msp430flasher(${TARGET})
endfunction()
