include_guard()

set(SDK_DIR ${TOP}/hw/mcu/wch/ch58x)
set(SDK_SRC_DIR ${SDK_DIR}/EVT/EXAM/SRC)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU rv32imac-ilp32 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/riscv_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS CH582 CACHE INTERNAL "")
set(OPENOCD_OPTION "-f ${CMAKE_CURRENT_LIST_DIR}/wch-riscv.cfg")

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/linker/ch582.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED STARTUP_FILE_GNU)
  set(STARTUP_FILE_GNU ${SDK_SRC_DIR}/Startup/startup_CH583.S)
endif ()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_SRC_DIR}/StdPeriphDriver/CH58x_gpio.c
    ${SDK_SRC_DIR}/StdPeriphDriver/CH58x_clk.c
    ${SDK_SRC_DIR}/StdPeriphDriver/CH58x_uart1.c
    ${SDK_SRC_DIR}/StdPeriphDriver/CH58x_sys.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ch58x_it.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/system_ch58x.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_SRC_DIR}/RVMSIS
    ${SDK_SRC_DIR}/StdPeriphDriver/inc
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    CFG_TUD_WCH_USBIP_USBFS=1
    FREQ_SYS=60000000
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
  family_add_tinyusb(${TARGET} OPT_MCU_CH582)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/debug_uart.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/wch/dcd_ch58x_usbfs.c
    ${TOP}/src/portable/wch/hcd_ch58x_usbfs.c
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
      -Wl,--defsym=__FLASH_SIZE=${LD_FLASH_SIZE}
      -Wl,--defsym=__RAM_SIZE=${LD_RAM_SIZE}
      "LINKER:--script=${LD_FILE_GNU}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Clang is not supported for CH58x")
  endif ()

  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_openocd_wch(${TARGET})
  family_flash_wlink_rs(${TARGET})
endfunction()
