include_guard()

set(MCU_DIR ${TOP}/hw/mcu/renesas/rx)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/rx_gcc.cmake)

set(FAMILY_MCUS ${MCU_FAMILY} CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
set(LD_FILE_Clang ${LD_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${MCU_DIR}/${MCU_VARIANT}/vects.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${MCU_DIR}/${MCU_VARIANT}
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    SSIZE_MAX=__INT_MAX__
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_${MCU_FAMILY})

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/renesas/rusb2/rusb2_common.c
    ${TOP}/src/portable/renesas/rusb2/dcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/hcd_rusb2.c
    ${MCU_DIR}/${MCU_VARIANT}/start.S
    ${BOARD_SOURCES}
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${TARGET} PUBLIC
      -Wno-error=redundant-decls
      )
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      )
  endif ()

  set_source_files_properties(${MCU_DIR}/${MCU_VARIANT}/start.S PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Suppress warnings for board-specific and family source files
  set_source_files_properties(
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${BOARD_SOURCES}
    PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
  family_flash_rfp(${TARGET})
endfunction()
