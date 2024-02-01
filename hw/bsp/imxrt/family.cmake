include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)
set(MCU_VARIANT_WITH_CORE ${MCU_VARIANT}${MCU_CORE})

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m7 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS MIMXRT1XXX CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  add_library(${BOARD_TARGET} STATIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board/clock_config.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board/pin_mux.c
    ${SDK_DIR}/drivers/common/fsl_common.c
    ${SDK_DIR}/drivers/common/fsl_common_arm.c
    ${SDK_DIR}/drivers/igpio/fsl_gpio.c
    ${SDK_DIR}/drivers/lpspi/fsl_lpspi.c
    ${SDK_DIR}/drivers/lpuart/fsl_lpuart.c
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
    __ARMVFP__=0
    __ARMFPV5__=0
    XIP_EXTERNAL_FLASH=1
    XIP_BOOT_HEADER_ENABLE=1
    )
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
    )

  update_board(${BOARD_TARGET})

  # LD_FILE and STARTUP_FILE can be defined in board.cmake
  if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
    set(LD_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_VARIANT}xxxxx${MCU_CORE}_flexspi_nor.ld)
    #set(LD_FILE_IAR ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_VARIANT}xxxxx_flexspi_nor.ld)
  endif ()

  if (NOT DEFINED STARTUP_FILE_${CMAKE_C_COMPILER_ID})
    set(STARTUP_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT_WITH_CORE}.S)
    #set(STARTUP_FILE_IAR ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT_WITH_CORE}.S)
  endif ()

  target_sources(${BOARD_TARGET} PUBLIC
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      # nanolib
      --specs=nosys.specs
      --specs=nano.specs
      # force linker to look for these symbols
      -Wl,-uimage_vector_table
      -Wl,-ug_boot_data
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
  family_add_tinyusb(${TARGET} OPT_MCU_MIMXRT1XXX ${RTOS})
  target_sources(${TARGET}-tinyusb PRIVATE
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
    )
  target_link_libraries(${TARGET}-tinyusb PUBLIC board_${BOARD})

  # Link dependencies
  target_link_libraries(${TARGET} PUBLIC board_${BOARD} ${TARGET}-tinyusb)

  # Flashing
  family_flash_jlink(${TARGET})
  #family_flash_nxplink(${TARGET})
  #family_flash_pyocd(${TARGET})
endfunction()
