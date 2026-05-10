include_guard()

set(ST_FAMILY f4)
set(ST_PREFIX stm32${ST_FAMILY}xx)

set(ST_HAL_DRIVER ${TOP}/hw/mcu/st/stm32${ST_FAMILY}xx_hal_driver)
set(ST_CMSIS ${TOP}/hw/mcu/st/cmsis_device_${ST_FAMILY})
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS STM32F4 CACHE INTERNAL "")

# ----------------------
# Port & Speed Selection
# ----------------------
if (NOT DEFINED RHPORT_DEVICE)
  set(RHPORT_DEVICE 0)
endif ()
if (NOT DEFINED RHPORT_HOST)
  set(RHPORT_HOST 0)
endif ()

if (NOT DEFINED RHPORT_SPEED)
  # Most F7 does not has built-in HS PHY
  set(RHPORT_SPEED OPT_MODE_FULL_SPEED OPT_MODE_FULL_SPEED)
endif ()
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
set(STARTUP_FILE_GNU ${ST_CMSIS}/Source/Templates/gcc/startup_${MCU_VARIANT}.s)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
set(STARTUP_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/startup_${MCU_VARIANT}.s)
set(LD_FILE_Clang ${LD_FILE_GNU})
set(LD_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/linker/${MCU_VARIANT}_flash.icf)

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${ST_CMSIS}/Source/Templates/system_${ST_PREFIX}.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_cortex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_dma.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_gpio.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_pwr_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_rcc.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_rcc_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_uart.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMSIS_5}/CMSIS/Core/Include
    ${ST_CMSIS}/Include
    ${ST_HAL_DRIVER}/Inc
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    BOARD_TUD_RHPORT=${RHPORT_DEVICE}
    BOARD_TUD_MAX_SPEED=${RHPORT_DEVICE_SPEED}
    BOARD_TUH_RHPORT=${RHPORT_HOST}
    BOARD_TUH_MAX_SPEED=${RHPORT_HOST_SPEED}
    )

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_STM32F4)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/synopsys/dwc2/dcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/hcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/dwc2_common.c
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
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
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
  family_flash_stlink(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()

#------------------------------------
# Optional Ethernet support (HAL_ETH + LAN8742 PHY + lwIP netif)
#------------------------------------
# Pull HAL_ETH, the LAN8742 PHY driver and the lwIP netif glue into
# TARGET, plus the board-specific MspInit (RMII pin map). Boards
# that wire an Ethernet PHY provide
# hw/bsp/stm32f4/boards/${BOARD}/board_eth.c defining
# HAL_ETH_MspInit; family_add_eth fails if that file is missing.
#
# The caller must already have lwIP set up on TARGET's include path
# (the netif glue includes lwip/netif.h and lwip/timeouts.h).
function(family_add_eth TARGET)
  set(BOARD_ETH_SRC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/board_eth.c)
  if (NOT EXISTS ${BOARD_ETH_SRC})
    message(FATAL_ERROR
      "family_add_eth: ${BOARD} has no board_eth.c. "
      "Boards that wire an Ethernet PHY must provide one alongside "
      "their board.h with the HAL_ETH MspInit pin map.")
  endif ()

  set(ETH_LWIP_DIR ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/eth_lwip)

  target_compile_definitions(${TARGET} PUBLIC HAL_ETH_MODULE_ENABLED)
  target_sources(${TARGET} PUBLIC
    ${BOARD_ETH_SRC}
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_eth.c
    ${TOP}/hw/mcu/st/stm32_lan8742/lan8742.c
    ${ETH_LWIP_DIR}/ethernetif.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${TOP}/hw/mcu/st/stm32_lan8742
    ${ETH_LWIP_DIR}
    )
  # Vendored sources don't pass tinyusb's strict warning baseline.
  set_source_files_properties(
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_eth.c
    ${TOP}/hw/mcu/st/stm32_lan8742/lan8742.c
    ${ETH_LWIP_DIR}/ethernetif.c
    ${BOARD_ETH_SRC}
    PROPERTIES COMPILE_FLAGS
    "-Wno-error=conversion -Wno-error=sign-conversion -Wno-error=sign-compare -Wno-error=unused-parameter -Wno-error=cast-align -Wno-error=redundant-decls -Wno-error=missing-prototypes")
endfunction()
