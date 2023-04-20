# toolchain set up, include before project()
if (NOT TARGET ${PROJECT})
  set(CMAKE_SYSTEM_PROCESSOR cortex-m7 CACHE INTERNAL "System Processor")
  set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/../../../cmake/toolchain/arm_${TOOLCHAIN}.cmake)
else ()
  if (NOT BOARD)
    message(FATAL_ERROR "BOARD not specified")
  endif ()

  set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
  set(DEPS_SUBMODULES ${SDK_DIR})

  # include basic family CMake functionality
  #set(FAMILY_MCUS RP2040)

  include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

  target_compile_definitions(${PROJECT} PUBLIC
    CFG_TUSB_MCU=OPT_MCU_MIMXRT
    __ARMVFP__=0
    __ARMFPV5__=0
    XIP_EXTERNAL_FLASH=1
    XIP_BOOT_HEADER_ENABLE=1
    )

  target_link_options(${PROJECT} PUBLIC
    --specs=nosys.specs
    --specs=nano.specs
    )

  target_sources(${PROJECT} PUBLIC
    # TinyUSB
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
    # BSP
    ${CMAKE_CURRENT_LIST_DIR}/family.c
    ${SDK_DIR}/drivers/common/fsl_common.c
    ${SDK_DIR}/drivers/igpio/fsl_gpio.c
    ${SDK_DIR}/drivers/lpuart/fsl_lpuart.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/system_${MCU_VARIANT}.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/xip/fsl_flexspi_nor_boot.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/project_template/clock_config.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_clock.c
    )

  if (TOOLCHAIN STREQUAL "gcc")
    target_sources(${PROJECT} PUBLIC
      ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT}.S
      )

    target_link_options(${PROJECT} PUBLIC
      "LINKER:--script=${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_VARIANT}xxxxx_flexspi_nor.ld"
      )
  endif ()

  target_include_directories(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}
    ${SDK_DIR}/CMSIS/Include
    ${SDK_DIR}/devices/${MCU_VARIANT}
    ${SDK_DIR}/devices/${MCU_VARIANT}/project_template
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers
    ${SDK_DIR}/drivers/common
    ${SDK_DIR}/drivers/igpio
    ${SDK_DIR}/drivers/lpuart
    )
endif ()
