include_guard()

set(SDK_DIR ${TOP}/hw/mcu/wch/ch32v4x7/EVT/EXAM/SRC)
# system_ch32v4x7.c is a per-project file in the EVT: use the GPIO demo's (SYSCLK 400 MHz, HCLK 200 MHz
# from the 25 MHz HSE)
set(SDK_SYSTEM_FILE ${TOP}/hw/mcu/wch/ch32v4x7/EVT/EXAM/GPIO/GPIO_Toggle/User/system_ch32v4x7.c)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU rv32imac-ilp32 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/riscv_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS CH32V407 CACHE INTERNAL "")

# rhport 0 = USBHS1 (PA11/PA12), rhport 1 = USBHS2 (PB6/PB7)
if (NOT DEFINED RHPORT_DEVICE)
  set(RHPORT_DEVICE 0)
endif ()

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${SDK_DIR}/Ld/Link.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED STARTUP_FILE_GNU)
  set(STARTUP_FILE_GNU ${SDK_DIR}/Startup/startup_ch32v4x7.S)
endif ()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/Core/core_riscv.c
    ${SDK_DIR}/Peripheral/src/ch32v4x7_gpio.c
    ${SDK_DIR}/Peripheral/src/ch32v4x7_rcc.c
    ${SDK_DIR}/Peripheral/src/ch32v4x7_usart.c
    ${SDK_SYSTEM_FILE}
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/Core
    ${SDK_DIR}/Peripheral/inc
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    BOARD_TUD_RHPORT=${RHPORT_DEVICE}
    )

  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${BOARD_TARGET} PUBLIC
      -msmall-data-limit=8
      -mno-save-restore
      -fmessage-length=0
      -fsigned-char
      -Wno-error=strict-prototypes
      )
  endif ()
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_CH32V407)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/wch/usbhs_v2/dcd_wch_usbhs_v2.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${TARGET} PUBLIC
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
    message(FATAL_ERROR "Clang is not supported for CH32V407")
  endif ()

  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_wlink_rs(${TARGET})
endfunction()
