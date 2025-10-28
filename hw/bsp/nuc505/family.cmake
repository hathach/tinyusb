include_guard()

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

set(SDK_DIR ${TOP}/hw/mcu/nuvoton/nuc505)
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)
set(OPENOCD_OPTION "-f interface/nulink.cfg -f target/numicroM4.cfg")

set(FAMILY_MCUS NUC505 CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
set(LD_FILE_Clang ${LD_FILE_GNU})
set(STARTUP_FILE_GNU ${SDK_DIR}/Device/Nuvoton/NUC505Series/Source/GCC/startup_NUC505Series.S)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/Device/Nuvoton/NUC505Series/Source/system_NUC505Series.c
    ${SDK_DIR}/StdDriver/src/adc.c
    ${SDK_DIR}/StdDriver/src/clk.c
    ${SDK_DIR}/StdDriver/src/gpio.c
    ${SDK_DIR}/StdDriver/src/i2c.c
    ${SDK_DIR}/StdDriver/src/i2s.c
    ${SDK_DIR}/StdDriver/src/pwm.c
    ${SDK_DIR}/StdDriver/src/rtc.c
    ${SDK_DIR}/StdDriver/src/spi.c
    ${SDK_DIR}/StdDriver/src/spim.c
    ${SDK_DIR}/StdDriver/src/sys.c
    ${SDK_DIR}/StdDriver/src/timer.c
    ${SDK_DIR}/StdDriver/src/uart.c
    ${SDK_DIR}/StdDriver/src/wdt.c
    ${SDK_DIR}/StdDriver/src/wwdt.c
    )

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/Device/Nuvoton/NUC505Series/Include
    ${SDK_DIR}/StdDriver/inc
    ${SDK_DIR}/CMSIS/Include
  )

  update_board(${BOARD_TARGET})
endfunction()


function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_NUC505)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/nuvoton/nuc505/dcd_nuc505.c
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
      --specs=nosys.specs --specs=nano.specs
      )
    target_compile_options(${TARGET} PRIVATE -Wno-redundant-decls)

  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC "LINKER:--script=${LD_FILE_Clang}")
    target_compile_options(${TARGET} PRIVATE -Wno-redundant-decls)

  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC "LINKER:--config=${LD_FILE_IAR}")
  endif ()

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES
      COMPILE_FLAGS "-Wno-missing-prototypes -Wno-redundant-decls")
  endif ()
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  family_flash_openocd_nuvoton(${TARGET})
endfunction()
