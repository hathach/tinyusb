include_guard()

set(MCUX_CORE ${TOP}/hw/mcu/nxp/mcuxsdk-core)
set(MCUX_DEVICES ${TOP}/hw/mcu/nxp/mcux-devices-rt)
set(CMSIS_DIR ${TOP}/lib/CMSIS_6)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)
set(MCU_VARIANT_WITH_CORE ${MCU_VARIANT}${MCU_CORE})

# toolchain set up
if (NOT DEFINED CMAKE_SYSTEM_CPU)
  set(CMAKE_SYSTEM_CPU cortex-m7 CACHE INTERNAL "System Processor")
endif ()
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS MIMXRT1XXX CACHE INTERNAL "")

# XIP boot files: some devices reference RT1052's xip (see each device's xip/CMakeLists.txt)
if (MCU_FAMILY STREQUAL "RT1064")
  set(XIP_DIR ${MCUX_DEVICES}/RT1064/MIMXRT1064/xip)
elseif (MCU_FAMILY STREQUAL "RT1170")
  set(XIP_DIR ${MCUX_DEVICES}/RT1170/MIMXRT1176/xip)
else()
  # RT1010, RT1015, RT1020, RT1050, RT1060 all use RT1052's xip
  set(XIP_DIR ${MCUX_DEVICES}/RT1050/MIMXRT1052/xip)
endif()

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/gcc/${MCU_VARIANT}xxxxx${MCU_CORE}_flexspi_nor.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})
if (NOT DEFINED LD_FILE_IAR)
set(LD_FILE_IAR ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/iar/${MCU_VARIANT}xxxxx${MCU_CORE}_flexspi_nor.icf)
endif ()

set(STARTUP_FILE_GNU ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT_WITH_CORE}.S)
set(STARTUP_FILE_IAR ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/iar/startup_${MCU_VARIANT_WITH_CORE}.s)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board/clock_config.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board/pin_mux.c
    # mcuxsdk-core drivers
    ${MCUX_CORE}/drivers/common/fsl_common.c
    ${MCUX_CORE}/drivers/common/fsl_common_arm.c
    ${MCUX_CORE}/drivers/igpio/fsl_gpio.c
    ${MCUX_CORE}/drivers/lpspi/fsl_lpspi.c
    ${MCUX_CORE}/drivers/lpuart/fsl_lpuart.c
    ${MCUX_CORE}/drivers/ocotp/fsl_ocotp.c
    # device specific
    ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/system_${MCU_VARIANT_WITH_CORE}.c
    ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/drivers/fsl_clock.c
    )

  # Additional drivers in subdirectories (RT1170 power/anatop_ai)
  if (MCU_FAMILY STREQUAL "RT1170")
    target_sources(${BOARD_TARGET} PRIVATE
      ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/drivers/power/fsl_dcdc.c
      ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/drivers/power/fsl_pmu.c
      ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/drivers/anatop_ai/fsl_anatop_ai.c
      )
  endif()

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
    # device specific
    ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}
    ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/drivers
    ${MCUX_DEVICES}/${MCU_FAMILY}/periph
    # mcuxsdk-core drivers
    ${MCUX_CORE}/drivers/common
    ${MCUX_CORE}/drivers/igpio
    ${MCUX_CORE}/drivers/lpspi
    ${MCUX_CORE}/drivers/lpuart
    ${MCUX_CORE}/drivers/ocotp
    )

  # Include power/anatop_ai driver directories if they exist
  foreach(SUBDIR power anatop_ai)
    if(EXISTS ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/drivers/${SUBDIR})
      target_include_directories(${BOARD_TARGET} PUBLIC
        ${MCUX_DEVICES}/${MCU_FAMILY}/${MCU_VARIANT}/drivers/${SUBDIR}
        )
    endif()
  endforeach()

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_MIMXRT1XXX)

  target_sources(${TARGET} PRIVATE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
    ${XIP_DIR}/fsl_flexspi_nor_boot.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${XIP_DIR}
    )
  target_compile_definitions(${TARGET} PUBLIC
    __START=main # required with -nostartfiles
    __STARTUP_CLEAR_BSS
    [=[CFG_TUSB_MEM_SECTION=__attribute__((section("NonCacheable")))]=]
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PRIVATE "LINKER:--script=${LD_FILE_GNU}")
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PRIVATE "LINKER:--config=${LD_FILE_IAR}")
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
