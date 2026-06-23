include_guard()

set(ST_FAMILY c5)
set(ST_PREFIX stm32${ST_FAMILY}xx)

set(ST_DRIVER ${TOP}/hw/mcu/st/stm32${ST_FAMILY}xx-drivers)
set(ST_CMSIS ${TOP}/hw/mcu/st/stm32${ST_FAMILY}xx-dfp)
set(CMSIS_6 ${TOP}/lib/CMSIS_6)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m33 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS STM32C5 CACHE INTERNAL "")
set(OPENOCD_OPTION "-f interface/stlink.cfg -f target/stm32c5x.cfg")

#------------------------------------
# Startup & Linker script
#------------------------------------
set(STARTUP_FILE ${ST_CMSIS}/Source/startup_${MCU_VARIANT}.c)
set(LD_FILE_Clang ${LD_FILE_GNU})

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${ST_CMSIS}/Source/Templates/system_${ST_PREFIX}.c
    ${ST_DRIVER}/hal/${ST_PREFIX}_hal.c
    ${ST_DRIVER}/hal/${ST_PREFIX}_hal_cortex.c
    ${ST_DRIVER}/hal/${ST_PREFIX}_hal_flash_itf.c
    ${ST_DRIVER}/hal/${ST_PREFIX}_hal_pwr.c
    ${ST_DRIVER}/hal/${ST_PREFIX}_hal_rcc.c
    ${ST_DRIVER}/hal/${ST_PREFIX}_hal_gpio.c
    ${ST_DRIVER}/hal/${ST_PREFIX}_hal_uart.c
    ${ST_DRIVER}/hal/${ST_PREFIX}_hal_dma.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMSIS_6}/CMSIS/Core/Include
    ${ST_CMSIS}/Include
    ${ST_DRIVER}/hal
    ${ST_DRIVER}/ll
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${BOARD_TARGET} PUBLIC -Wno-redundant-decls)
  endif ()

  if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      __STACK_LIMIT=__StackLimit
      __INITIAL_SP=__StackTop
    )
  endif ()

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_STM32C5)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
    ${TOP}/src/portable/st/stm32_fsdev/hcd_stm32_fsdev.c
    ${TOP}/src/portable/st/stm32_fsdev/fsdev_common.c
    ${STARTUP_FILE}
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

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")
  endif ()

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
  family_flash_stlink(${TARGET})
  #family_flash_openocd(${TARGET})
endfunction()
