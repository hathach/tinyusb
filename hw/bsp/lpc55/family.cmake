include_guard()

set(MCUX_DIR ${TOP}/hw/mcu/nxp/mcuxsdk-core)
set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-devices-lpc)
set(CMSIS_DIR ${TOP}/lib/CMSIS_6)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m33 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC55 CACHE INTERNAL "")

# ----------------------
# Port & Speed Selection
# ----------------------

# default device port to USB1 highspeed, host to USB0 fullspeed
if (NOT DEFINED RHPORT_DEVICE)
  set(RHPORT_DEVICE 1)
endif ()
if (NOT DEFINED RHPORT_HOST)
  set(RHPORT_HOST 1)
endif ()

# port 0 is fullspeed, port 1 is highspeed
set(RHPORT_SPEED OPT_MODE_FULL_SPEED OPT_MODE_HIGH_SPEED)

if (NOT DEFINED RHPORT_DEVICE_SPEED)
  list(GET RHPORT_SPEED ${RHPORT_DEVICE} RHPORT_DEVICE_SPEED)
endif ()
if (NOT DEFINED RHPORT_HOST_SPEED)
  list(GET RHPORT_SPEED ${RHPORT_HOST} RHPORT_HOST_SPEED)
endif ()

cmake_print_variables(RHPORT_DEVICE RHPORT_DEVICE_SPEED RHPORT_HOST RHPORT_HOST_SPEED)

#------------------------------------
# Startup & Linker script
#------------------------------------
if (NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${SDK_DIR}/LPC5500/${MCU_VARIANT}/gcc/${MCU_CORE}_flash.ld)
endif ()
set(LD_FILE_Clang ${LD_FILE_GNU})

if (NOT DEFINED STARTUP_FILE_GNU)
  set(STARTUP_FILE_GNU ${SDK_DIR}/LPC5500/${MCU_VARIANT}/gcc/startup_${MCU_CORE}.S)
endif ()
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

if (NOT DEFINED LD_FILE_IAR)
  set(LD_FILE_IAR ${SDK_DIR}/LPC5500/${MCU_VARIANT}/iar/${MCU_CORE}_flash.icf)
endif ()

if (NOT DEFINED STARTUP_FILE_IAR)
  set(STARTUP_FILE_IAR ${SDK_DIR}/LPC5500/${MCU_VARIANT}/iar/startup_${MCU_CORE}.s)
endif ()

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    # driver
    ${MCUX_DIR}/drivers/lpc_gpio/fsl_gpio.c
    ${MCUX_DIR}/drivers/common/fsl_common_arm.c
    ${MCUX_DIR}/drivers/flexcomm/fsl_flexcomm.c
    ${MCUX_DIR}/drivers/flexcomm/usart/fsl_usart.c
    # mcu
    ${SDK_DIR}/LPC5500/${MCU_VARIANT}/system_${MCU_CORE}.c
    ${SDK_DIR}/LPC5500/${MCU_VARIANT}/drivers/fsl_clock.c
    ${SDK_DIR}/LPC5500/${MCU_VARIANT}/drivers/fsl_power.c
    ${SDK_DIR}/LPC5500/${MCU_VARIANT}/drivers/fsl_reset.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${TOP}/lib/sct_neopixel
    # driver
    ${MCUX_DIR}/drivers/common
    ${MCUX_DIR}/drivers/flexcomm
    ${MCUX_DIR}/drivers/flexcomm/usart
    ${MCUX_DIR}/drivers/lpc_iocon
    ${MCUX_DIR}/drivers/lpc_gpio
    ${MCUX_DIR}/drivers/sctimer
    # mcu
    ${SDK_DIR}/LPC5500/${MCU_VARIANT}
    ${SDK_DIR}/LPC5500/${MCU_VARIANT}/drivers
    ${SDK_DIR}/LPC5500/periph
    ${CMSIS_DIR}/CMSIS/Core/Include
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    CFG_TUSB_MEM_ALIGN=TU_ATTR_ALIGNED\(64\)
    BOARD_TUD_RHPORT=${RHPORT_DEVICE}
    BOARD_TUD_MAX_SPEED=${RHPORT_DEVICE_SPEED}
    BOARD_TUH_RHPORT=${RHPORT_HOST}
    BOARD_TUH_MAX_SPEED=${RHPORT_HOST_SPEED}
    __STARTUP_CLEAR_BSS
    )

  # Port 0 is Fullspeed, Port 1 is Highspeed. Port1 controller can only access USB_SRAM
  if (RHPORT_DEVICE EQUAL 1)
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      [=[CFG_TUD_MEM_SECTION=__attribute__((section("m_usb_global")))]=]
      )
  endif ()
  if (RHPORT_HOST EQUAL 1)
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      [=[CFG_TUH_MEM_SECTION=__attribute__((section("m_usb_global")))]=]
      CFG_TUH_USBIP_IP3516=1
      )
  endif ()

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_LPC55)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/lib/sct_neopixel/sct_neopixel.c
    ${TOP}/src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )

  if (RHPORT_HOST EQUAL 0)
    target_sources(${TARGET} PUBLIC
      ${TOP}/src/portable/ohci/ohci.c
      )
  elseif (RHPORT_HOST EQUAL 1)
    target_sources(${TARGET} PUBLIC
      ${TOP}/src/portable/nxp/lpc_ip3516/hcd_lpc_ip3516.c
      )
  endif ()

  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      --specs=nosys.specs --specs=nano.specs
      -nostartfiles
      "LINKER:--defsym=__stack_size__=0x1000"
      "LINKER:--defsym=__heap_size__=0"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      "LINKER:--config_def=__stack_size__=0x1000"
      "LINKER:--config_def=__heap_size__=0"
      )
  endif ()

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
      PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")
    set_source_files_properties(${TOP}/lib/sct_neopixel/sct_neopixel.c
      PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes -Wno-unused-parameter")
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
