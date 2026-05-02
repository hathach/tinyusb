include_guard()

set(APM32_FAMILY apm32f0xx)
set(APM32_SDK ${TOP}/hw/mcu/geehy/APM32F0xx_SDK_V1.8.6/Libraries)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m0plus CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS APM32F0XX CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
set(STARTUP_FILE_GNU ${APM32_SDK}/Device/Geehy/APM32F0xx/Source/gcc/startup_apm32f072.S)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${APM32_SDK}/Device/Geehy/APM32F0xx/Source/gcc/gcc_${MCU_LINKER_NAME}.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${APM32_SDK}/Device/Geehy/APM32F0xx/Source/system_apm32f0xx.c
    ${APM32_SDK}/APM32F0xx_StdPeriphDriver/src/apm32f0xx_gpio.c
    ${APM32_SDK}/APM32F0xx_StdPeriphDriver/src/apm32f0xx_misc.c
    ${APM32_SDK}/APM32F0xx_StdPeriphDriver/src/apm32f0xx_rcm.c
    ${APM32_SDK}/APM32F0xx_StdPeriphDriver/src/apm32f0xx_crs.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${APM32_SDK}/CMSIS/Include
    ${APM32_SDK}/Device/Geehy/APM32F0xx/Include
    ${APM32_SDK}/APM32F0xx_StdPeriphDriver/inc
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_APM32F0XX)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
    ${TOP}/src/portable/st/stm32_fsdev/fsdev_common.c
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
  endif ()
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
