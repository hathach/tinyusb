include_guard()

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

set(SDK_DIR ${TOP}/hw/mcu/microchip/${SAM_FAMILY})
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS SAMD51 SAME54 CACHE INTERNAL "")
set(OPENOCD_OPTION "-f interface/cmsis-dap.cfg -c \"transport select swd\" -c \"set CHIPNAME samd51\" -f target/atsame5x.cfg")

#------------------------------------
# Startup & Linker script
#------------------------------------
set(LD_FILE_Clang ${LD_FILE_GNU})
set(STARTUP_FILE_GNU ${SDK_DIR}/gcc/gcc/startup_${SAM_FAMILY}.c)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/gcc/system_${SAM_FAMILY}.c
    ${SDK_DIR}/hal/src/hal_atomic.c
    ${SDK_DIR}/hpl/gclk/hpl_gclk.c
    ${SDK_DIR}/hpl/mclk/hpl_mclk.c
    ${SDK_DIR}/hpl/osc32kctrl/hpl_osc32kctrl.c
    ${SDK_DIR}/hpl/oscctrl/hpl_oscctrl.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}
    ${SDK_DIR}/config
    ${SDK_DIR}/include
    ${SDK_DIR}/hal/include
    ${SDK_DIR}/hal/utils/include
    ${SDK_DIR}/hpl/port
    ${SDK_DIR}/hri
    ${CMSIS_5}/CMSIS/Core/Include
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_SAMD51)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/microchip/samd/dcd_samd.c
    ${TOP}/src/portable/microchip/samd/hcd_samd.c
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
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
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
  #family_flash_openocd(${TARGET})
endfunction()
