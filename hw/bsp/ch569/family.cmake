include_guard()

set(SDK_DIR ${TOP}/hw/mcu/wch/ch569/EVT/EXAM/SRC)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU rv32imac-ilp32 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/riscv_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS CH569 CACHE INTERNAL "")
set(OPENOCD_OPTION "-f ${CMAKE_CURRENT_LIST_DIR}/wch-riscv.cfg")

# Controller selection: super = USB3.0 SuperSpeed (USBSS), high = USB2.0 HighSpeed (USBHS)
if (NOT DEFINED SPEED)
  set(SPEED super)
endif ()

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/linker/ch569.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED STARTUP_FILE_GNU)
  set(STARTUP_FILE_GNU ${SDK_DIR}/Startup/startup_CH56x.S)
endif ()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/Peripheral/src/CH56x_clk.c
    ${SDK_DIR}/Peripheral/src/CH56x_gpio.c
    ${SDK_DIR}/Peripheral/src/CH56x_uart.c
    ${SDK_DIR}/Peripheral/src/CH56x_sys.c
    ${SDK_DIR}/RVMSIS/core_riscv.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/Peripheral/inc
    ${SDK_DIR}/RVMSIS
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )
  if (NOT DEFINED FREQ_SYS)
    set(FREQ_SYS 120000000)
  endif ()
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    FREQ_SYS=${FREQ_SYS}
    )

  if (SPEED STREQUAL super)
    # USB3 SuperSpeed with runtime USB2 high-speed fallback (uses TMR0 as training timeout)
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      CFG_TUD_WCH_USBIP_USB30=1
      CFG_TUD_WCH_USB30_FALLBACK=1
      )
  else ()
    target_compile_definitions(${BOARD_TARGET} PUBLIC CFG_TUD_WCH_USBIP_USBHS=1)
  endif ()

  # USB DMA can only reach RAMX with 16-byte alignment: place TinyUSB transfer buffers in the
  # .dmadata (RAMX) section so the dcd can use them zero-copy
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    [=[CFG_TUSB_MEM_SECTION=__attribute__((section(".dmadata")))]=]
    [=[CFG_TUSB_MEM_ALIGN=__attribute__((aligned(16)))]=]
    )

  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${BOARD_TARGET} PUBLIC
      -flto
      -msmall-data-limit=16
      -mno-save-restore
      -fmessage-length=0
      -fsigned-char
      -Wno-error=strict-prototypes
      -Wno-comment
      )
  endif ()
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_CH569)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/debug_uart.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/wch/dcd_ch56x_usb30.c
    ${TOP}/src/portable/wch/dcd_ch56x_usbhs.c
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
    message(FATAL_ERROR "Clang is not supported for CH56x")
  endif ()

  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_openocd_wch(${TARGET})
endfunction()
