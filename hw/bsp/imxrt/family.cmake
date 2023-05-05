# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m7 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/../../../examples/cmake/toolchain/arm_${TOOLCHAIN}.cmake)


function(family_configure_target TARGET)
  if (NOT BOARD)
    message(FATAL_ERROR "BOARD not specified")
  endif ()

  # set output name to .elf
  set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME ${TARGET}.elf)

  # TOP is absolute path to root directory of TinyUSB git repo
  set(TOP "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../..")
  get_filename_component(TOP "${TOP}" REALPATH)

  set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
  set(DEPS_SUBMODULES ${SDK_DIR})

  # define BSP target
  add_library(bsp STATIC
    )

  # include board specific cmake
  include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board.cmake)

  target_compile_definitions(bsp PUBLIC
    CFG_TUSB_MCU=OPT_MCU_MIMXRT
    __ARMVFP__=0
    __ARMFPV5__=0
    XIP_EXTERNAL_FLASH=1
    XIP_BOOT_HEADER_ENABLE=1
    )

  target_link_options(${TARGET} PUBLIC
    --specs=nosys.specs
    --specs=nano.specs
    )

  target_sources(bsp PUBLIC
    # TinyUSB
#    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
#    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
#    ${TOP}/src/portable/ehci/ehci.c
    # BSP
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${SDK_DIR}/drivers/common/fsl_common.c
    ${SDK_DIR}/drivers/igpio/fsl_gpio.c
    ${SDK_DIR}/drivers/lpuart/fsl_lpuart.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/system_${MCU_VARIANT}.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/xip/fsl_flexspi_nor_boot.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/project_template/clock_config.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_clock.c
    )

  if (TOOLCHAIN STREQUAL "gcc")
    target_sources(bsp PUBLIC
      ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT}.S
      )

    target_link_options(bsp PUBLIC
      "LINKER:--script=${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_VARIANT}xxxxx_flexspi_nor.ld"
      )
  else ()
    # TODO support IAR
  endif ()

  target_include_directories(bsp PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${SDK_DIR}/CMSIS/Include
    ${SDK_DIR}/devices/${MCU_VARIANT}
    ${SDK_DIR}/devices/${MCU_VARIANT}/project_template
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers
    ${SDK_DIR}/drivers/common
    ${SDK_DIR}/drivers/igpio
    ${SDK_DIR}/drivers/lpuart
    )

  if(NOT TARGET tinyusb_config)
    message(FATAL_ERROR "tinyusb_config target not found")
  endif()

  target_compile_definitions(tinyusb_config INTERFACE
    CFG_TUSB_MCU=OPT_MCU_MIMXRT
    )

  # include tinyusb CMakeList.txt for tinyusb target
  add_subdirectory(${TOP}/src ${CMAKE_CURRENT_BINARY_DIR}/tinyusb)

  add_library(tinyusb_port STATIC
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
    )


  target_link_libraries(${TARGET} PUBLIC
    tinyusb
    bsp
    )
endfunction()


function(family_add_freertos_config TARGET)
  add_library(freertos_config INTERFACE)

  # add path to FreeRTOSConfig.h
  target_include_directories(freertos_config SYSTEM INTERFACE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FreeRTOSConfig
    )

  # select freertos port
  if (TOOLCHAIN STREQUAL "gcc")
    set(FREERTOS_PORT "GCC_ARM_CM7" CACHE INTERNAL "")
  else ()
    # TODO support IAR
  endif ()
endfunction()

function(family_configure_device_example TARGET)
  family_configure_target(${TARGET})
endfunction()
