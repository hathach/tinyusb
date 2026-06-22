include_guard()

set(FT9XX_SDK ${TOP}/hw/mcu/bridgetek/ft9xx/ft90x-sdk/Source)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU ft32 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/ft32_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS FT90X CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${FT9XX_SDK}/src/sys.c
    ${FT9XX_SDK}/src/interrupt.c
    ${FT9XX_SDK}/src/delay.c
    ${FT9XX_SDK}/src/timers.c
    ${FT9XX_SDK}/src/uart_simple.c
    ${FT9XX_SDK}/src/gpio.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${FT9XX_SDK}/include
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    BOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
    )
  target_compile_options(${BOARD_TARGET} PUBLIC
    -fmessage-length=0
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_FT90X)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/bridgetek/ft9xx/dcd_ft9xx.c
    ${FT9XX_SDK}/src/bootstrap.c
    )
  set_source_files_properties(${FT9XX_SDK}/src/bootstrap.c PROPERTIES
    COMPILE_OPTIONS "-w")
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )
  target_compile_options(${TARGET} PUBLIC
    -Wno-error=shadow
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${TOP}/hw/mcu/bridgetek/ft9xx/scripts/ldscript.ld"
      "LINKER:--entry=_start"
      )
  endif ()

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_ft9xx(${TARGET})
endfunction()
