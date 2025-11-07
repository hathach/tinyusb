include_guard()

set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)
set(MCU_VARIANT_WITH_CORE ${MCU_VARIANT}${MCU_CORE})

# toolchain set up
if (NOT DEFINED CMAKE_SYSTEM_CPU)
  set(CMAKE_SYSTEM_CPU cortex-m7 CACHE INTERNAL "System Processor")
endif ()
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS MIMXRT1XXX CACHE INTERNAL "")

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_VARIANT}xxxxx${MCU_CORE}_flexspi_nor.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED LD_FILE_IAR)
set(LD_FILE_IAR ${SDK_DIR}/devices/${MCU_VARIANT}/iar/${MCU_VARIANT}xxxxx${MCU_CORE}_flexspi_nor.icf)
endif ()

set(STARTUP_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT_WITH_CORE}.S)
set(STARTUP_FILE_IAR ${SDK_DIR}/devices/${MCU_VARIANT}/iar/startup_${MCU_VARIANT_WITH_CORE}.s)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board/clock_config.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board/pin_mux.c
    ${SDK_DIR}/drivers/common/fsl_common.c
    ${SDK_DIR}/drivers/common/fsl_common_arm.c
    ${SDK_DIR}/drivers/igpio/fsl_gpio.c
    ${SDK_DIR}/drivers/lpspi/fsl_lpspi.c
    ${SDK_DIR}/drivers/lpuart/fsl_lpuart.c
    ${SDK_DIR}/drivers/ocotp/fsl_ocotp.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/system_${MCU_VARIANT_WITH_CORE}.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/xip/fsl_flexspi_nor_boot.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_clock.c
    )

  # Optional drivers: only available for some mcus: rt1160, rt1170
  set(OPTIONAL_DRIVER fsl_dcdc.c fsl_pmu.c fsl_anatop_ai.c)
  foreach(FILE IN LISTS OPTIONAL_DRIVER)
    if(EXISTS ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/${FILE})
      target_sources(${BOARD_TARGET} PRIVATE ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/${FILE})
    endif()
  endforeach()

  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __STARTUP_CLEAR_BSS
    [=[CFG_TUSB_MEM_SECTION=__attribute__((section("NonCacheable")))]=]
    )

  if (NOT M4 STREQUAL "1")
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      XIP_EXTERNAL_FLASH=1
      XIP_BOOT_HEADER_ENABLE=1
      )
  endif ()

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${SDK_DIR}/devices/${MCU_VARIANT}
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers
    #${SDK_DIR}/drivers/adc_12b1msps_sar
    ${SDK_DIR}/drivers/common
    ${SDK_DIR}/drivers/igpio
    ${SDK_DIR}/drivers/lpspi
    ${SDK_DIR}/drivers/lpuart
    ${SDK_DIR}/drivers/ocotp
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_MIMXRT1XXX)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
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
      # force linker to look for these symbols
      -Wl,-uimage_vector_table
      -Wl,-ug_boot_data
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -Wl,-uimage_vector_table
      -Wl,-ug_boot_data
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
