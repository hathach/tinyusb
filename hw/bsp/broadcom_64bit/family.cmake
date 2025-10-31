include_guard()

set(SDK_DIR ${TOP}/hw/mcu/broadcom)
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/aarch64_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS BCM2711 BCM2835 CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${SDK_DIR}/broadcom/link8.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
set(STARTUP_FILE_GNU ${SDK_DIR}/broadcom/boot8.s)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/broadcom/gen/interrupt_handlers.c
    ${SDK_DIR}/broadcom/gpio.c
    ${SDK_DIR}/broadcom/interrupts.c
    ${SDK_DIR}/broadcom/mmu.c
    ${SDK_DIR}/broadcom/caches.c
    ${SDK_DIR}/broadcom/vcmailbox.c
    )
  target_compile_options(${BOARD_TARGET} PUBLIC
    -O0
    -ffreestanding
    -mgeneral-regs-only
    -fno-exceptions
    -std=c17
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    BCM_VERSION=${BCM_VERSION}
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}
    ${CMSIS_5}/CMSIS/Core_A/Include
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_BCM${BCM_VERSION})

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/synopsys/dwc2/dcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/hcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/dwc2_common.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      "LINKER:--entry=_start"
      --specs=nosys.specs
      -nostartfiles
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
      "LINKER:--entry=_start"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")
  endif ()
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
