include_guard()

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

if (NOT DEFINED MCU_VARIANT)
  set(MCU_VARIANT PY32F071)
endif ()
string(TOLOWER ${MCU_VARIANT} PY32_SERIES_LOWER)
set(PY32_SDK_NAME ${MCU_VARIANT}_Firmware)
set(PY32_TEMPLATE ${MCU_VARIANT}xx_Templates)
set(PY32_STARTUP startup_${PY32_SERIES_LOWER}xx.s)
set(PY32_SYSTEM_SOURCE system_${PY32_SERIES_LOWER}.c)
set(PY32_HAL_PREFIX ${PY32_SERIES_LOWER})

set(PY32_SDK ${TOP}/hw/mcu/puya/${PY32_SDK_NAME})
set(PY32_HAL ${PY32_SDK}/Drivers/${MCU_VARIANT}_HAL_Driver)
set(PY32_CMSIS ${PY32_SDK}/Drivers/CMSIS/Device/${MCU_VARIANT})

set(CMAKE_SYSTEM_CPU cortex-m0plus CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS PY32F0 CACHE INTERNAL "")

set(STARTUP_FILE_GNU ${PY32_SDK}/Templates/${PY32_TEMPLATE}/EIDE/${PY32_STARTUP})
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
set(LD_FILE_Clang ${LD_FILE_GNU})

function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${PY32_SDK}/Templates/${PY32_TEMPLATE}/Src/${PY32_SYSTEM_SOURCE}
    ${PY32_HAL}/Src/${PY32_HAL_PREFIX}_hal.c
    ${PY32_HAL}/Src/${PY32_HAL_PREFIX}_hal_cortex.c
    ${PY32_HAL}/Src/${PY32_HAL_PREFIX}_hal_flash.c
    ${PY32_HAL}/Src/${PY32_HAL_PREFIX}_hal_gpio.c
    ${PY32_HAL}/Src/${PY32_HAL_PREFIX}_hal_pwr.c
    ${PY32_HAL}/Src/${PY32_HAL_PREFIX}_hal_rcc.c
    ${PY32_HAL}/Src/${PY32_HAL_PREFIX}_hal_rcc_ex.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${PY32_SDK}/Drivers/CMSIS/Include
    ${PY32_CMSIS}/Include
    ${PY32_HAL}/Inc
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )
  target_compile_definitions(${BOARD_TARGET} PRIVATE
    USE_HAL_DRIVER
    )
  update_board(${BOARD_TARGET})
endfunction()

function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_PY32F0)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/mentor/musb/dcd_musb.c
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
    message(FATAL_ERROR "Clang is not supported")
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    message(FATAL_ERROR "IAR is not supported")
  endif ()

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")
  endif ()
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  family_add_bin_hex(${TARGET})
  family_flash_pyocd(${TARGET})
endfunction()
