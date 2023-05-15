if (TARGET _nrf_family_inclusion_marker)
  return()
endif ()

add_library(_nrf_family_inclusion_marker INTERFACE)

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/../../../examples/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS NRF5X CACHE INTERNAL "")

# TOP is path to root directory
set(TOP "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(NRFX_DIR ${TOP}/hw/mcu/nordic/nrfx)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)
set(JLINK_DEVICE $(MCU_VARIANT)_xxaa)

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
set(BOARD_TARGET board_${BOARD})
if (NOT TARGET ${BOARD_TARGET})
  add_library(${BOARD_TARGET} STATIC
    # driver
    ${NRFX_DIR}/drivers/src/nrfx_power.c
    ${NRFX_DIR}/drivers/src/nrfx_uarte.c
    # mcu
    ${NRFX_DIR}/mdk/system_${MCU_VARIANT}.c
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    CONFIG_GPIO_AS_PINRESET
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${NRFX_DIR}/../ # hw/mcu/nordic: remove later
    # driver
    ${NRFX_DIR}
    ${NRFX_DIR}/mdk
    ${NRFX_DIR}/hal
    ${NRFX_DIR}/drivers/include
    ${NRFX_DIR}/drivers/src
    ${CMSIS_DIR}/CMSIS/Core/Include
    )
  update_board(${BOARD_TARGET})

  if (NOT DEFINED LD_FILE_${TOOLCHAIN})
    set(LD_FILE_gcc ${NRFX_DIR}/mdk/${MCU_VARIANT}_xxaa.ld)
  endif ()

  if (TOOLCHAIN STREQUAL "gcc")
    target_sources(${BOARD_TARGET} PUBLIC
      ${NRFX_DIR}/mdk/gcc_startup_${MCU_VARIANT}.S
      )
    target_link_options(${BOARD_TARGET} PUBLIC
      # linker file
      "LINKER:--script=${LD_FILE_gcc}"
      -L${NRFX_DIR}/mdk
      # link map
      "LINKER:-Map=$<IF:$<BOOL:$<TARGET_PROPERTY:OUTPUT_NAME>>,$<TARGET_PROPERTY:OUTPUT_NAME>,$<TARGET_PROPERTY:NAME>>${CMAKE_EXECUTABLE_SUFFIX}.map"
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
function(family_configure_target TARGET)
  # set output name to .elf
  set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME ${TARGET}.elf)

  # TOP is path to root directory
  set(TOP "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../..")

  #---------- Port Specific ----------
  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PUBLIC
    # TinyUSB Port
    ${TOP}/src/portable/nordic/nrf5x/dcd_nrf5x.c
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
    CFG_TUSB_MCU=OPT_MCU_NRF5X
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
  # Flash using pyocd
  add_custom_target(${TARGET}-pyocd
    COMMAND pyocd flash -t ${PYOCD_TARGET} $<TARGET_FILE:${TARGET}>
    )

  # Flash using NXP LinkServer (redlink)
  # https://www.nxp.com/design/software/development-software/mcuxpresso-software-and-tools-/linkserver-for-microcontrollers:LINKERSERVER
  # LinkServer has a bug that can only execute with full path otherwise it throws:
  # realpath error: No such file or directory
  execute_process(COMMAND which LinkServer OUTPUT_VARIABLE LINKSERVER_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
  add_custom_target(${TARGET}-nxplink
    COMMAND ${LINKSERVER_PATH} flash ${NXPLINK_DEVICE} load $<TARGET_FILE:${TARGET}>
    )

endfunction()


function(family_add_freertos TARGET)
  # freertos_config
  add_subdirectory(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FreeRTOSConfig ${CMAKE_CURRENT_BINARY_DIR}/freertos_config)

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
  family_configure_target(${TARGET})
endfunction()

function(family_configure_host_example TARGET)
  family_configure_target(${TARGET})
endfunction()

function(family_configure_dual_usb_example TARGET)
  family_configure_target(${TARGET})
endfunction()
