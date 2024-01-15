include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC54 CACHE INTERNAL "")

if (NOT DEFINED PORT)
  set(PORT 0)
endif()

# Host port will be the other port if available
set(HOST_PORT $<NOT:${PORT}>)

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif()

  add_library(${BOARD_TARGET} STATIC
    # driver
    ${SDK_DIR}/drivers/lpc_gpio/fsl_gpio.c
    ${SDK_DIR}/drivers/common/fsl_common_arm.c
    ${SDK_DIR}/drivers/flexcomm/fsl_flexcomm.c
    ${SDK_DIR}/drivers/flexcomm/fsl_usart.c
    # mcu
    ${SDK_DIR}/devices/${MCU_VARIANT}/system_${MCU_CORE}.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_clock.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_power.c
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_reset.c
    )

  target_compile_definitions(${BOARD_TARGET} PUBLIC
    CFG_TUSB_MEM_ALIGN=TU_ATTR_ALIGNED\(64\)
    BOARD_TUD_RHPORT=${PORT}
    BOARD_TUH_RHPORT=${HOST_PORT}
    )
  # Port 0 is Fullspeed, Port 1 is Highspeed. Port1 controller can only access USB_SRAM
  if (PORT EQUAL 1)
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      BOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
      BOARD_TUH_MAX_SPEED=OPT_MODE_FULL_SPEED
      CFG_TUD_MEM_SECTION=__attribute__\(\(section\(\"m_usb_global\"\)\)\)
      )
  else ()
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      BOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
      BOARD_TUH_MAX_SPEED=OPT_MODE_HIGH_SPEED
      CFG_TUH_MEM_SECTION=__attribute__\(\(section\(\"m_usb_global\"\)\)\)
      #CFG_TUD_MEM_SECTION=__attribute__\(\(section\(\"m_usb_global\"\)\)\)
      )
  endif ()

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${TOP}/lib/sct_neopixel
    # driver
    ${SDK_DIR}/drivers/common
    ${SDK_DIR}/drivers/flexcomm
    ${SDK_DIR}/drivers/lpc_iocon
    ${SDK_DIR}/drivers/lpc_gpio
    ${SDK_DIR}/drivers/lpuart
    ${SDK_DIR}/drivers/sctimer
    # mcu
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${SDK_DIR}/devices/${MCU_VARIANT}
    ${SDK_DIR}/devices/${MCU_VARIANT}/drivers
    )

  update_board(${BOARD_TARGET})

  if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
    set(LD_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_CORE}_flash.ld)
  endif ()

  if (NOT DEFINED STARTUP_FILE_${CMAKE_C_COMPILER_ID})
    set(STARTUP_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_CORE}.S)
  endif ()

  target_sources(${BOARD_TARGET} PUBLIC
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${BOARD_TARGET} PUBLIC
      # linker file
      "LINKER:--script=${LD_FILE_GNU}"
      # nanolib
      --specs=nosys.specs
      --specs=nano.specs
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

  # https://github.com/gsteiert/sct_neopixel/pull/1
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(${TOP}/lib/sct_neopixel/sct_neopixel.c PROPERTIES
      COMPILE_FLAGS "-Wno-unused-parameter")
  endif ()

  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_LPC54 ${RTOS})
  target_sources(${TARGET}-tinyusb PUBLIC
    ${TOP}/src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c
    )
  target_link_libraries(${TARGET}-tinyusb PUBLIC board_${BOARD})

  # Link dependencies
  target_link_libraries(${TARGET} PUBLIC board_${BOARD} ${TARGET}-tinyusb)

  # Flashing
  family_flash_jlink(${TARGET})
  #family_flash_nxplink(${TARGET})
  #family_flash_pyocd(${TARGET})
endfunction()
