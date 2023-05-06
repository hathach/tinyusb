if (TARGET _imxrt_family_inclusion_marker)
  return()
endif ()

add_library(_imxrt_family_inclusion_marker INTERFACE)

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m7 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/../../../examples/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)


#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
set(BOARD_TARGET board_${BOARD})
if (NOT TARGET ${BOARD_TARGET})
  # TOP is path to root directory
  set(TOP "${CMAKE_CURRENT_LIST_DIR}/../../..")

  set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
  set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/drivers/common/fsl_common.c
    ${SDK_DIR}/drivers/igpio/fsl_gpio.c
    ${SDK_DIR}/drivers/lpuart/fsl_lpuart.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/system_${MCU_VARIANT}.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/xip/fsl_flexspi_nor_boot.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/project_template/clock_config.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_clock.c
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    CFG_TUSB_MCU=OPT_MCU_MIMXRT
    __ARMVFP__=0
    __ARMFPV5__=0
    XIP_EXTERNAL_FLASH=1
    XIP_BOOT_HEADER_ENABLE=1
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${SDK_DIR}/devices/${MCU_VARIANT}
    ${SDK_DIR}/devices/${MCU_VARIANT}/project_template
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers
    ${SDK_DIR}/drivers/common
    ${SDK_DIR}/drivers/igpio
    ${SDK_DIR}/drivers/lpuart
    )
  update_board(${BOARD_TARGET})

  if (TOOLCHAIN STREQUAL "gcc")
    target_sources(${BOARD_TARGET} PUBLIC
      ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT}.S
      )
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_VARIANT}xxxxx_flexspi_nor.ld"
      --specs=nosys.specs
      --specs=nano.specs
      )
  else ()
    # TODO support IAR
  endif ()
endif () # BOARD_TARGET

#------------------------------------
# Functions
#------------------------------------
function(family_configure_target TARGET)
  # set output name to .elf
  set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME ${TARGET}.elf)

  # TOP is path to root directory
  set(TOP "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../..")

  #---------- BSP_TARGET ----------
  # BSP_TARGET is built for each example since it depends on example's tusb_config.h
  set(BSP_TARGET "${TARGET}_bsp_${BOARD}")
  add_library(${BSP_TARGET} STATIC
    # TinyUSB
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
    # BSP
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    )
  target_include_directories(${BSP_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  #---------- TinyUSB ----------
  # tinyusb target is built for each example since it depends on example's tusb_config.h
  set(TINYUSB_TARGET_PREFIX ${TARGET})
  add_library(${TARGET}_tinyusb_config INTERFACE)

  target_include_directories(${TARGET}_tinyusb_config INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
  target_compile_definitions(${TARGET}_tinyusb_config INTERFACE
    CFG_TUSB_MCU=OPT_MCU_MIMXRT
    )

  # tinyusb's CMakeList.txt
  add_subdirectory(${TOP}/src ${CMAKE_CURRENT_BINARY_DIR}/tinyusb)

  # Link dependencies
  target_link_libraries(${BSP_TARGET} PUBLIC ${BOARD_TARGET} ${TARGET}_tinyusb)
  target_link_libraries(${TARGET} PUBLIC ${BSP_TARGET} ${TARGET}_tinyusb)
endfunction()


function(family_configure_freertos_example TARGET)
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
