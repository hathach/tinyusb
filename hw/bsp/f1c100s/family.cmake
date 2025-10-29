include_guard()

set(SDK_DIR ${TOP}/hw/mcu/allwinner/f1c100s)

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU arm926ej-s CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS F1C100S CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${SDK_DIR}/f1c100s.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED STARTUP_FILE_GNU)
set(STARTUP_FILE_GNU ${SDK_DIR}/machine/start.S)
endif ()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/lib/malloc.c
    ${SDK_DIR}/lib/printf.c
    ${SDK_DIR}/lib/memcpy.S
    ${SDK_DIR}/lib/memset.S
    ${SDK_DIR}/machine/sys-uart.c
    ${SDK_DIR}/machine/exception.c
    ${SDK_DIR}/machine/sys-clock.c
    ${SDK_DIR}/machine/sys-copyself.c
    ${SDK_DIR}/machine/sys-dram.c
    ${SDK_DIR}/machine/sys-mmu.c
    ${SDK_DIR}/machine/sys-spi-flash.c
    ${SDK_DIR}/machine/f1c100s-intc.c
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __ARM32_ARCH__=5
    __ARM926EJS__
    )
  target_compile_options(${BOARD_TARGET} PUBLIC
    -ffreestanding
    -std=gnu99
    -mno-thumb-interwork
    -Wno-float-equal
    -Wno-unused-parameter
    -Wno-error=array-bounds
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/include
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_F1C100S)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/sunxi/dcd_sunxi_musb.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(${TARGET} PUBLIC
      gcc
      )
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      "LINKER:--defsym=__bss_end__=__bss_end"
      "LINKER:--defsym=__bss_start__=__bss_start"
      "LINKER:--defsym=end=__bss_end"
      --specs=nosys.specs --specs=nano.specs
      -nostartfiles
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      )
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
  family_flash_jlink(${TARGET})
endfunction()
