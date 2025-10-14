include_guard()

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

set(SDK_DIR ${TOP}/hw/mcu/microchip/same70)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m7 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS SAMX7X CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  set(STARTUP_FILE_GNU ${SDK_DIR}/same70b/gcc/gcc/startup_same70q21b.c)
  set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

  if (NOT DEFINED LD_FILE_Clang)
    set(LD_FILE_Clang ${LD_FILE_GNU})
  endif ()

  if (NOT DEFINED LD_FILE_IAR)
    set(LD_FILE_IAR ${LD_FILE_GNU})
  endif ()

  if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
    message(FATAL_ERROR "LD_FILE_${CMAKE_C_COMPILER_ID} not defined")
  endif ()

  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/same70b/gcc/system_same70q21b.c
    ${SDK_DIR}/hpl/core/hpl_init.c
    ${SDK_DIR}/hpl/usart/hpl_usart.c
    ${SDK_DIR}/hpl/pmc/hpl_pmc.c
    ${SDK_DIR}/hal/src/hal_usart_async.c
    ${SDK_DIR}/hal/src/hal_io.c
    ${SDK_DIR}/hal/src/hal_atomic.c
    ${SDK_DIR}/hal/utils/src/utils_ringbuffer.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}
    ${SDK_DIR}/config
    ${SDK_DIR}/same70b/include
    ${SDK_DIR}/hal/include
    ${SDK_DIR}/hal/utils/include
    ${SDK_DIR}/hpl/core
    ${SDK_DIR}/hpl/pio
    ${SDK_DIR}/hpl/pmc
    ${SDK_DIR}/hri
    ${SDK_DIR}/CMSIS/Core/Include
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()

  target_compile_options(${BOARD_TARGET} PUBLIC
    -Wno-error=unused-parameter
    -Wno-error=cast-align
    -Wno-error=redundant-decls
    -Wno-error=cast-qual
    )
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})

  add_board_target(board_${BOARD})

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    )

  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  family_add_tinyusb(${TARGET} OPT_MCU_SAMX7X)
  target_sources(${TARGET} PUBLIC
    ${TOP}/src/portable/microchip/samx7x/dcd_samx7x.c
    )

  target_link_libraries(${TARGET} PUBLIC board_${BOARD})
  target_compile_options(${TARGET} PUBLIC
    -Wno-error=unused-parameter
    -Wno-error=cast-align
    -Wno-error=redundant-decls
    -Wno-error=cast-qual
    )

  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
