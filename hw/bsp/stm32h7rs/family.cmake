include_guard()

set(ST_FAMILY h7rs)
set(ST_PREFIX stm32${ST_FAMILY}xx)

set(ST_HAL_DRIVER ${TOP}/hw/mcu/st/stm32${ST_FAMILY}xx_hal_driver)
set(ST_CMSIS ${TOP}/hw/mcu/st/cmsis_device_${ST_FAMILY})
set(ST_TCPP0203 ${TOP}/hw/mcu/st/stm32-tcpp0203)
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m7 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS STM32H7RS CACHE INTERNAL "")

# ----------------------
# Port & Speed Selection
# ----------------------
if (NOT DEFINED RHPORT_DEVICE)
  set(RHPORT_DEVICE 1)
endif ()
if (NOT DEFINED RHPORT_HOST)
  set(RHPORT_HOST 0)
endif ()

if (NOT DEFINED RHPORT_SPEED)
  set(RHPORT_SPEED OPT_MODE_FULL_SPEED OPT_MODE_HIGH_SPEED)
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

# XSPI_BOOT (board.cmake): internal flash holds only the vector table and boot code, the image is
# stored on the XSPI2 (XSPIM_P2) flash and copied to AXI SRAM at reset, see xspi_boot.c
if (XSPI_BOOT)
  set(LD_SUFFIX xspi)
else ()
  set(LD_SUFFIX flash)
endif ()
if(NOT DEFINED LD_FILE_GNU)
  set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/linker/${MCU_VARIANT}_${LD_SUFFIX}.ld)
endif()
set(LD_FILE_Clang ${LD_FILE_GNU})
if(NOT DEFINED LD_FILE_IAR)
  if (XSPI_BOOT)
    set(LD_FILE_IAR ${CMAKE_CURRENT_LIST_DIR}/linker/${MCU_VARIANT}_xspi.icf)
  else ()
    set(LD_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/linker/${MCU_VARIANT}_flash.icf)
  endif ()
endif()

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
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_pwr.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_i2c.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_pwr_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_rcc.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_rcc_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_uart.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_uart_ex.c
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
    SEGGER_RTT_SECTION=\"noncacheable_buffer\"
    )
  # RTT up buffer, it lives in the noncacheable section (1 KB, 4 KB with XSPI_BOOT). Defined once:
  # iasmarm rejects a second -D of it with another value, e.g. from CFLAGS_CLI
  if (NOT CFLAGS_CLI MATCHES "BUFFER_SIZE_UP=")
    if (NOT DEFINED BUFFER_SIZE_UP)
      set(BUFFER_SIZE_UP 0x300)
    endif ()
    target_compile_definitions(${BOARD_TARGET} PUBLIC BUFFER_SIZE_UP=${BUFFER_SIZE_UP})
  endif ()
  if (XSPI_BOOT)
    target_compile_definitions(${BOARD_TARGET} PUBLIC XSPI_BOOT=1)
  endif ()

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_STM32H7RS ${RTOS})

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/synopsys/dwc2/dcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/hcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/dwc2_common.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  if (XSPI_BOOT)
    # not in the board library: its strong Reset_Handler must override the startup's weak one
    target_sources(${TARGET} PUBLIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/xspi_boot.c)
  endif ()
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
  if (XSPI_BOOT)
    # a single .bin would span 0x08000000-0x70000000: one per flash instead
    if (CMAKE_C_COMPILER_ID STREQUAL "IAR")
      add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --ihex $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.hex
        VERBATIM)
    else ()
      add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} -Obinary -j .isr_vector -j .boot $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
        COMMAND ${CMAKE_OBJCOPY} -Obinary -j .text -j .rodata -j .ARM.extab -j .ARM -j .preinit_array -j .init_array
                -j .fini_array -j .data $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}-xspi.bin
        VERBATIM)
    endif ()
  else ()
    family_add_bin_hex(${TARGET})
  endif ()
  family_flash_stlink(${TARGET})
  family_flash_jlink(${TARGET})
endfunction()
