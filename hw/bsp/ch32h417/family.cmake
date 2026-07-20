include_guard()

set(SDK_DIR ${TOP}/hw/mcu/wch/ch32h417/EVT/EXAM/SRC)
# system_ch32h417.c (clock setup) is a per-project file in the EVT; use the plain GPIO demo's V3F
# copy (SYSCLK 400 MHz, V3F core 100 MHz from the 25 MHz HSE - the vendor default all USB demos run)
set(SDK_SYSTEM_FILE ${TOP}/hw/mcu/wch/ch32h417/EVT/EXAM/GPIO/GPIO_Toggle/V3F/User/system_ch32h417.c)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU rv32imac-ilp32 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/riscv_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS CH32H417 CACHE INTERNAL "")
set(OPENOCD_OPTION "-f ${CMAKE_CURRENT_LIST_DIR}/wch-riscv.cfg")

# Controller selection: super = USB3.0 SuperSpeed (USBSS), high = USB2.0 HighSpeed (USBHS)
if (NOT DEFINED SPEED)
  set(SPEED super)
endif ()

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/linker/ch32h417_v3f.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED STARTUP_FILE_GNU)
  set(STARTUP_FILE_GNU ${SDK_DIR}/Startup/startup_ch32h417_v3f.S)
endif ()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/Core/core_riscv.c
    ${SDK_DIR}/Peripheral/src/ch32h417_dbgmcu.c
    ${SDK_DIR}/Peripheral/src/ch32h417_flash.c
    ${SDK_DIR}/Peripheral/src/ch32h417_gpio.c
    ${SDK_DIR}/Peripheral/src/ch32h417_rcc.c
    ${SDK_DIR}/Peripheral/src/ch32h417_iwdg.c
    ${SDK_DIR}/Peripheral/src/ch32h417_usart.c
    ${SDK_SYSTEM_FILE}
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/Core
    ${SDK_DIR}/Peripheral/inc
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )
  # TinyUSB runs entirely on the V3F core (core 0, the boot core): single image, single link,
  # standard flow. The V5F core (core 1) is left parked. Core_V3F selects the per-core paths in
  # the SDK headers.
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    Core_V3F=1
    )

  if (SPEED STREQUAL super)
    # USB3 SuperSpeed, SS-only by default: the USB2-HS fallback PHY takes PB8/PB9, which ARE the
    # chip's only SWD/SDI debug pins - a fallback image cannot be debugged or reflashed in-system.
    # Pass -DCFG_TUD_WCH_USB30_FALLBACK=1 to enable the runtime USB2 fallback (TIM12 training timeout).
    if (NOT DEFINED CFG_TUD_WCH_USB30_FALLBACK)
      set(CFG_TUD_WCH_USB30_FALLBACK 0)
    endif ()
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      CFG_TUD_WCH_USBIP_USB30=1
      CFG_TUD_WCH_USB30_FALLBACK=${CFG_TUD_WCH_USB30_FALLBACK}
      )
  else ()
    target_compile_definitions(${BOARD_TARGET} PUBLIC CFG_TUD_WCH_USBIP_USBHS=1)
  endif ()

  # BSP knobs of family.c, both settable with cmake -D<knob>=<value> (see family.c for details):
  #   CFG_BOARD_CH32H417_UART_LOADER  default 1: USART1 park + UART flash loader. Keep it on unless
  #     you can unplug the USB cable - the USB2 pins are the chip's only SWD/SDI pins, so this is
  #     the only in-system reflash path once a USB image runs.
  #   CFG_BOARD_CH32H417_CRASH_WDT    default 0: ~8 s IWDG kicked from SysTick and board_led_write.
  #     The HIL rig sets 1 (a crashed board then reboots into the park window); it also resets the
  #     chip during any core halt longer than the period, so normal builds leave it off.
  foreach (knob CFG_BOARD_CH32H417_UART_LOADER CFG_BOARD_CH32H417_CRASH_WDT)
    if (DEFINED ${knob})
      target_compile_definitions(${BOARD_TARGET} PUBLIC ${knob}=${${knob}})
    endif ()
  endforeach ()

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
  family_add_tinyusb(${TARGET} OPT_MCU_CH32H417)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/wch/dcd_ch32h417_usb30.c
    ${TOP}/src/portable/wch/dcd_ch32h417_usbhs.c
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
    message(FATAL_ERROR "Clang is not supported for CH32H417")
  endif ()

  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_openocd(${TARGET})
endfunction()
