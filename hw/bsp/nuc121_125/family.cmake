include_guard()

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

set(SDK_DIR ${TOP}/hw/mcu/nuvoton/nuc121_125)
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m0 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)
set(OPENOCD_OPTION "-f interface/nulink.cfg -f target/numicroM0.cfg")

set(FAMILY_MCUS NUC121 CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  set(LD_FILE_Clang ${LD_FILE_GNU})
  if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
    message(FATAL_ERROR "LD_FILE_${CMAKE_C_COMPILER_ID} not defined")
  endif ()

  set(STARTUP_FILE_GNU ${SDK_DIR}/Device/Nuvoton/NUC121/Source/GCC/startup_NUC121.S)
  set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

  # Common sources for all NUC12x
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/Device/Nuvoton/NUC121/Source/system_NUC121.c
    ${SDK_DIR}/StdDriver/src/clk.c
    ${SDK_DIR}/StdDriver/src/gpio.c
    ${SDK_DIR}/StdDriver/src/fmc.c
    ${SDK_DIR}/StdDriver/src/sys.c
    ${SDK_DIR}/StdDriver/src/timer.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
  )

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/Device/Nuvoton/NUC121/Include
    ${SDK_DIR}/StdDriver/inc
    ${SDK_DIR}/CMSIS/Include
  )

  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __ARM_FEATURE_DSP=0
    USE_ASSERT=0
    CFG_EXAMPLE_MSC_READONLY
  )
  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
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
endfunction()


#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})

  # Board target
  add_board_target(board_${BOARD})

  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PUBLIC
    # BSP
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    )
  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_NUC121)
  target_sources(${TARGET} PUBLIC
    ${TOP}/src/portable/nuvoton/nuc121/dcd_nuc121.c
    )
  target_link_libraries(${TARGET} PUBLIC board_${BOARD})

  family_flash_openocd_nuvoton(${TARGET})
endfunction()
