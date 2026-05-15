include_guard()

set(CH32_FAMILY ch32h417)
set(SDK_DIR ${TOP}/hw/mcu/wch/${CH32_FAMILY})
set(SDK_SRC_DIR ${SDK_DIR}/EVT/EXAM/SRC)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up

# In actual fact, it is IMACBF
set(CMAKE_SYSTEM_CPU rv32imac-ilp32 CACHE INTERNAL "System Processor" FORCE)
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/riscv_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS CH32H417 CACHE INTERNAL "")
set(OPENOCD_OPTION "-f ${CMAKE_CURRENT_LIST_DIR}/wch-dual-core.cfg -c noload")

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${SDK_SRC_DIR}/Ld/V3F/Link_v3f.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED STARTUP_FILE_GNU)
  set(STARTUP_FILE_GNU ${SDK_SRC_DIR}/Startup/startup_${CH32_FAMILY}_v3f.S)
endif ()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_SRC_DIR}/Core/core_riscv.c
    ${SDK_SRC_DIR}/Peripheral/src/${CH32_FAMILY}_flash.c
    ${SDK_SRC_DIR}/Peripheral/src/${CH32_FAMILY}_gpio.c
    ${SDK_SRC_DIR}/Peripheral/src/${CH32_FAMILY}_rcc.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/system_${CH32_FAMILY}.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_SRC_DIR}/Core
    ${SDK_SRC_DIR}/Peripheral/inc
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    )

  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${BOARD_TARGET} PUBLIC
      -msmall-data-limit=8
      -mno-save-restore
      -fmessage-length=0
      -fsigned-char
      )
  endif ()
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_CH32H417)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${TARGET} PUBLIC
    ${SDK_SRC_DIR}/Core
    ${SDK_SRC_DIR}/Peripheral/inc
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      "LINKER:--script=${LD_FILE_GNU}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Clang is not supported")
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
  family_flash_openocd_wch(${TARGET})
endfunction()
