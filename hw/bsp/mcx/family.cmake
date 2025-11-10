include_guard()

set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
if (MCU_VARIANT STREQUAL "MCXA153")
  set(CMAKE_SYSTEM_CPU cortex-m33-nodsp-nofp CACHE INTERNAL "System Processor")
  set(FAMILY_MCUS MCXA15 CACHE INTERNAL "")
elseif (MCU_VARIANT STREQUAL "MCXA156")
    set(CMAKE_SYSTEM_CPU cortex-m33 CACHE INTERNAL "System Processor")
    set(FAMILY_MCUS MCXA15 CACHE INTERNAL "")
elseif (MCU_VARIANT STREQUAL "MCXN947")
  set(CMAKE_SYSTEM_CPU cortex-m33 CACHE INTERNAL "System Processor")
  set(FAMILY_MCUS MCXN9 CACHE INTERNAL "")
else()
  message(FATAL_ERROR "MCU_VARIANT not supported")
endif()

set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_CORE}_flash.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED STARTUP_FILE_GNU)
set(STARTUP_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_CORE}.S)
endif()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    # driver
    ${SDK_DIR}/drivers/gpio/fsl_gpio.c
    ${SDK_DIR}/drivers/common/fsl_common_arm.c
    ${SDK_DIR}/drivers/lpuart/fsl_lpuart.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/drivers/spc/fsl_spc.c
    # mcu
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_clock.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_reset.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/system_${MCU_CORE}.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${SDK_DIR}/drivers/gpio/
    ${SDK_DIR}/drivers/lpuart
    ${SDK_DIR}/drivers/common
    ${SDK_DIR}/drivers/port
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/drivers/spc
    ${SDK_DIR}/devices/${MCU_VARIANT}
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers
    )

  if (${FAMILY_MCUS} STREQUAL "MCXN9")
    target_sources(${BOARD_TARGET} PRIVATE
      ${SDK_DIR}/drivers/lpflexcomm/fsl_lpflexcomm.c
      )
    target_include_directories(${BOARD_TARGET} PUBLIC
      ${SDK_DIR}/drivers/lpflexcomm
      )
  elseif(${FAMILY_MCUS} STREQUAL "MCXA15")
  endif()

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  if (${FAMILY_MCUS} STREQUAL "MCXN9")
    family_add_tinyusb(${TARGET} OPT_MCU_MCXN9)
  elseif(${FAMILY_MCUS} STREQUAL "MCXA15")
    family_add_tinyusb(${TARGET} OPT_MCU_MCXA15)
  endif()

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/chipidea/$<IF:${PORT},ci_hs/dcd_ci_hs.c,ci_fs/dcd_ci_fs.c>
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
      #-nostartfiles
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
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
  #family_flash_nxplink(${TARGET})
  #family_flash_pyocd(${TARGET})
endfunction()
