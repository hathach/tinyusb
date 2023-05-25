include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

# TOP is path to root directory
set(TOP ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m33 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/tools/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC55XX CACHE INTERNAL "")

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)


#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
set(BOARD_TARGET board_${BOARD})
if (NOT TARGET ${BOARD_TARGET})
  add_library(${BOARD_TARGET} STATIC
    # external driver
    #lib/sct_neopixel/sct_neopixel.c

    # driver
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_gpio.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_common_arm.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_lpuart.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_lpflexcomm.c
    # mcu
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_clock.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_reset.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/system_${MCU_CORE}.c
    )
#  target_compile_definitions(${BOARD_TARGET} PUBLIC
#    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    # driver
    # mcu
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${SDK_DIR}/devices/${MCU_VARIANT}
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers
    )
  update_board(${BOARD_TARGET})

  if (TOOLCHAIN STREQUAL "gcc")
    target_sources(${BOARD_TARGET} PUBLIC
      ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_CORE}.S
      )
    cmake_print_variables(CMAKE_CURRENT_BINARY_DIR)
    target_link_options(${BOARD_TARGET} PUBLIC
      # linker file
      "LINKER:--script=${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_CORE}_flash.ld"
      # nanolib
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
function(family_configure_example TARGET)
  family_support_configure_common(${TARGET})

  #---------- Port Specific ----------
  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PUBLIC
    # TinyUSB Port
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
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

  #---------- TinyUSB ----------
  # tinyusb target is built for each example since it depends on example's tusb_config.h
  set(TINYUSB_TARGET_PREFIX ${TARGET}-)
  add_library(${TARGET}-tinyusb_config INTERFACE)

  target_include_directories(${TARGET}-tinyusb_config INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
  target_compile_definitions(${TARGET}-tinyusb_config INTERFACE
    CFG_TUSB_MCU=OPT_MCU_MCXN9
    )

  # tinyusb's CMakeList.txt
  add_subdirectory(${TOP}/src ${CMAKE_CURRENT_BINARY_DIR}/tinyusb)

  # Link dependencies
  target_link_libraries(${TARGET} PUBLIC ${BOARD_TARGET} ${TARGET}-tinyusb)

  # group target (not yet supported by clion)
  set_target_properties(${TARGET}-tinyusb ${TARGET}-tinyusb_config
    PROPERTIES FOLDER ${TARGET}_sub
    )

  #---------- Flash ----------
  # use MCUXpresso GUI Flash Tool to flash the elf

#  set(REDLINK_EXE /usr/local/LinkServer/binaries/crt_emu_cm_redlink)
#  add_custom_target(${TARGET}-redlink
#    DEPENDS ${TARGET}
#    COMMAND ${REDLINK_EXE} --flash-load-exec $<TARGET_FILE:${TARGET}> --vendor NXP -p MCXN947 --bootromstall
#    0x50000040 -CoreIndex=0 --flash-driver= -x ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/flash --flash-dir
#    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/flash --flash-hashing
#    )

  #family_flash_jlink(${TARGET})
  #family_flash_nxplink(${TARGET})
  #family_flash_pyocd(${TARGET})
endfunction()


function(family_add_freertos TARGET)
  # freertos_config
  if (NOT TARGET freertos_config)
    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FreeRTOSConfig
      )
  endif()

  ## freertos
  if (NOT TARGET freertos_kernel)
    add_subdirectory(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../lib/FreeRTOS-Kernel ${CMAKE_CURRENT_BINARY_DIR}/freertos_kernel)
  endif ()

  # Add FreeRTOS option to tinyusb_config
  target_compile_definitions(${TARGET}-tinyusb_config INTERFACE
    CFG_TUSB_OS=OPT_OS_FREERTOS
    )
  # link tinyusb with freeRTOS kernel
  target_link_libraries(${TARGET}-tinyusb PUBLIC
    freertos_kernel
    )
  target_link_libraries(${TARGET} PUBLIC
    freertos_kernel
    )
endfunction()

function(family_configure_device_example TARGET)
  family_configure_example(${TARGET})
endfunction()

function(family_configure_host_example TARGET)
  family_configure_example(${TARGET})
endfunction()

function(family_configure_dual_usb_example TARGET)
  family_configure_example(${TARGET})
endfunction()
