include_guard()

set(APM32_FAMILY apm32f10x)
set(APM32_SDK ${TOP}/hw/mcu/geehy/APM32F10x_SDK/Libraries)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m3 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

# set(FAMILY_MCUS APM32F103 APM32F107 CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
# F103: startup_apm32f103xb.S (medium density)
# F107: startup_apm32f107xc.S (connectivity line)
set(STARTUP_FILE_GNU ${APM32_SDK}/Device/Geehy/APM32F10x/Source/gcc/startup_${MCU_VARIANT}.S)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${APM32_SDK}/Device/Geehy/APM32F10x/Source/gcc/${MCU_LINKER_NAME}_flash.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${APM32_SDK}/Device/Geehy/APM32F10x/Source/system_apm32f10x.c
    ${APM32_SDK}/APM32F10x_StdPeriphDriver/src/apm32f10x_gpio.c
    ${APM32_SDK}/APM32F10x_StdPeriphDriver/src/apm32f10x_misc.c
    ${APM32_SDK}/APM32F10x_StdPeriphDriver/src/apm32f10x_rcm.c
    ${APM32_SDK}/APM32F10x_StdPeriphDriver/src/apm32f10x_usart.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${APM32_SDK}/CMSIS/Include
    ${APM32_SDK}/Device/Geehy/APM32F10x/Include
    ${APM32_SDK}/APM32F10x_StdPeriphDriver/inc
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})

  if (BOARD STREQUAL "apm32f103_dev_board")
    family_add_tinyusb(${TARGET} OPT_MCU_APM32F103)
  elseif (BOARD STREQUAL "apm32f107_dev_board")
    family_add_tinyusb(${TARGET} OPT_MCU_APM32F107)
  endif ()

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )

  if (BOARD STREQUAL "apm32f103_dev_board")
    target_sources(${TARGET} PUBLIC
      ${TOP}/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
      ${TOP}/src/portable/st/stm32_fsdev/fsdev_common.c
      )
  elseif (BOARD STREQUAL "apm32f107_dev_board")
    target_sources(${TARGET} PUBLIC
      ${TOP}/src/portable/synopsys/dwc2/dcd_dwc2.c
      ${TOP}/src/portable/synopsys/dwc2/hcd_dwc2.c
      ${TOP}/src/portable/synopsys/dwc2/dwc2_common.c
      )
  endif ()

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
