include_guard()

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

set(SDK_DIR ${TOP}/hw/mcu/nuvoton/nuc126)
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m0 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)
set(OPENOCD_OPTION "-f interface/nulink.cfg -f target/numicroM0.cfg")

set(FAMILY_MCUS NUC126 CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  set(LD_FILE_Clang ${LD_FILE_GNU})
  if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
    message(FATAL_ERROR "LD_FILE_${CMAKE_C_COMPILER_ID} not defined")
  endif ()

  set(STARTUP_FILE_GNU ${SDK_DIR}/Device/Nuvoton/NUC126/Source/GCC/startup_NUC126.S)
  set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/Device/Nuvoton/NUC126/Source/system_NUC126.c
    ${SDK_DIR}/StdDriver/src/clk.c
    ${SDK_DIR}/StdDriver/src/crc.c
    ${SDK_DIR}/StdDriver/src/gpio.c
    ${SDK_DIR}/StdDriver/src/rtc.c
    ${SDK_DIR}/StdDriver/src/sys.c
    ${SDK_DIR}/StdDriver/src/timer.c
    ${SDK_DIR}/StdDriver/src/uart.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/Device/Nuvoton/NUC126/Include
    ${SDK_DIR}/StdDriver/inc
    ${SDK_DIR}/CMSIS/Include
  )

  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __ARM_FEATURE_DSP=0
    USE_ASSERT=0
    CFG_EXAMPLE_VIDEO_READONLY
    __CORTEX_SC=0
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

  #---------- Port Specific ----------
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_NUC126)
  target_sources(${TARGET} PUBLIC
    ${TOP}/src/portable/nuvoton/nuc121/dcd_nuc121.c
    )
  target_link_libraries(${TARGET} PUBLIC board_${BOARD})

  family_flash_openocd_nuvoton(${TARGET})
endfunction()
