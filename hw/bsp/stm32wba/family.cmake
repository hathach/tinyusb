include_guard()

set(ST_FAMILY wba)
set(ST_PREFIX stm32${ST_FAMILY}xx)

set(ST_HAL_DRIVER ${TOP}/hw/mcu/st/stm32${ST_FAMILY}xx_hal_driver)
set(ST_CMSIS ${TOP}/hw/mcu/st/cmsis-device-${ST_FAMILY})
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m33 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS STM32WBA CACHE INTERNAL "")

# ----------------------
# Port & Speed Selection
# ----------------------
set(RHPORT_DEVICE 0)
set(RHPORT_HOST 0)

# WBA65/64/62 has built-in HS PHY
set(RHPORT_DEVICE_SPEED OPT_MODE_HIGH_SPEED)
set(RHPORT_HOST_SPEED OPT_MODE_HIGH_SPEED)

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif()

  # STM32WBA HAL uses uppercase MCU_VARIANT (excluding the x's) for linking and lowercase MCU_VARIANT for startup.
  string(TOUPPER "${MCU_VARIANT}" UPPERCASE_MCU_VARIANT)
  string(REGEX REPLACE "X" "x" UPPERCASE_MCU_VARIANT "${UPPERCASE_MCU_VARIANT}")

  # Startup & Linker script
  set(STARTUP_FILE_GNU ${ST_CMSIS}/Source/Templates/gcc/startup_${MCU_VARIANT}.s)
  set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})
  set(STARTUP_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/startup_${MCU_VARIANT}.s)

  set(LD_FILE_GNU ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/${UPPERCASE_MCU_VARIANT}_FLASH_ns.ld)
  set(LD_FILE_Clang ${LD_FILE_GNU})
  set(LD_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/linker/${MCU_VARIANT}_flash_ns.icf)

  add_library(${BOARD_TARGET} STATIC
    ${ST_CMSIS}/Source/Templates/system_${ST_PREFIX}.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_cortex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_icache.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_pwr.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_pwr_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_rcc.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_rcc_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_uart.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_gpio.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_pcd.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_pcd_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_ll_usb.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMSIS_5}/CMSIS/Core/Include
    ${ST_CMSIS}/Include
    ${ST_HAL_DRIVER}/Inc
    )

  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
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

  target_compile_definitions(${TARGET} PUBLIC
    CFG_TUSB_MCU=OPT_MCU_STM32WBA
    )

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
  family_add_tinyusb(${TARGET} OPT_MCU_STM32WBA)
  target_sources(${TARGET} PUBLIC
    ${TOP}/src/portable/synopsys/dwc2/dcd_dwc2.c
	${TOP}/src/portable/synopsys/dwc2/hcd_dwc2.c
	${TOP}/src/portable/synopsys/dwc2/dwc2_common.c
    )
  target_link_libraries(${TARGET} PUBLIC board_${BOARD})

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_stlink(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
