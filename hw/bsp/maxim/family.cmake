include_guard()

# stub: overridden by board.cmake if needed
function(sign_image TARGET_IN)
endfunction()

set(MSDK_LIB ${TOP}/hw/mcu/analog/msdk/Libraries)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

string(TOUPPER ${MAX_DEVICE} MAX_DEVICE_UPPER)
cmake_print_variables(MAX_DEVICE MAX_DEVICE_UPPER)

set(JLINK_DEVICE ${MAX_DEVICE})
set(OPENOCD_OPTION "-f interface/cmsis-dap.cfg -f target/${MAX_DEVICE}.cfg")

set(FAMILY_MCUS ${MAX_DEVICE_UPPER} CACHE INTERNAL "")

if (${MAX_DEVICE} STREQUAL "max32650")
  set(PERIPH_ID 10)
  set(PERIPH_SUFFIX "me")
elseif (${MAX_DEVICE} STREQUAL "max32665" OR ${MAX_DEVICE} STREQUAL "max32666")
  set(PERIPH_ID 14)
  set(PERIPH_SUFFIX "me")
elseif (${MAX_DEVICE} STREQUAL "max32690")
  set(PERIPH_ID 18)
  set(PERIPH_SUFFIX "me")
elseif (${MAX_DEVICE} STREQUAL "max78002")
  set(PERIPH_ID 87)
  set(PERIPH_SUFFIX "ai")
else()
  message(FATAL_ERROR "Unsupported MAX device: ${MAX_DEVICE}")
endif()

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  # Startup & Linker script
  set(STARTUP_FILE_GNU ${MSDK_LIB}/CMSIS/Device/Maxim/${MAX_DEVICE_UPPER}/Source/GCC/startup_${MAX_DEVICE}.S)
  set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

  if (NOT DEFINED LD_FILE_GNU)
    set(LD_FILE_GNU ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/${MAX_DEVICE}.ld)
  endif ()
  set(LD_FILE_Clang ${LD_FILE_GNU})

  # Common
  add_library(${BOARD_TARGET} STATIC
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    ${MSDK_LIB}/CMSIS/Device/Maxim/${MAX_DEVICE_UPPER}/Source/heap.c
    ${MSDK_LIB}/CMSIS/Device/Maxim/${MAX_DEVICE_UPPER}/Source/system_${MAX_DEVICE}.c
    ${MSDK_LIB}/PeriphDrivers/Source/SYS/mxc_assert.c
    ${MSDK_LIB}/PeriphDrivers/Source/SYS/mxc_delay.c
    ${MSDK_LIB}/PeriphDrivers/Source/SYS/mxc_lock.c
    ${MSDK_LIB}/PeriphDrivers/Source/SYS/nvic_table.c
    ${MSDK_LIB}/PeriphDrivers/Source/SYS/pins_${PERIPH_SUFFIX}${PERIPH_ID}.c
    ${MSDK_LIB}/PeriphDrivers/Source/SYS/sys_${PERIPH_SUFFIX}${PERIPH_ID}.c
    ${MSDK_LIB}/PeriphDrivers/Source/FLC/flc_common.c
    ${MSDK_LIB}/PeriphDrivers/Source/FLC/flc_${PERIPH_SUFFIX}${PERIPH_ID}.c
    ${MSDK_LIB}/PeriphDrivers/Source/FLC/flc_reva.c
    ${MSDK_LIB}/PeriphDrivers/Source/GPIO/gpio_common.c
    ${MSDK_LIB}/PeriphDrivers/Source/GPIO/gpio_${PERIPH_SUFFIX}${PERIPH_ID}.c
    ${MSDK_LIB}/PeriphDrivers/Source/GPIO/gpio_reva.c
    ${MSDK_LIB}/PeriphDrivers/Source/ICC/icc_${PERIPH_SUFFIX}${PERIPH_ID}.c
    ${MSDK_LIB}/PeriphDrivers/Source/ICC/icc_reva.c
    ${MSDK_LIB}/PeriphDrivers/Source/UART/uart_common.c
    ${MSDK_LIB}/PeriphDrivers/Source/UART/uart_${PERIPH_SUFFIX}${PERIPH_ID}.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${MSDK_LIB}/CMSIS/5.9.0/Core/Include
    ${MSDK_LIB}/CMSIS/Device/Maxim/${MAX_DEVICE_UPPER}/Include
    ${MSDK_LIB}/PeriphDrivers/Include/${MAX_DEVICE_UPPER}
    ${MSDK_LIB}/PeriphDrivers/Source/SYS
    ${MSDK_LIB}/PeriphDrivers/Source/GPIO
    ${MSDK_LIB}/PeriphDrivers/Source/ICC
    ${MSDK_LIB}/PeriphDrivers/Source/FLC
    ${MSDK_LIB}/PeriphDrivers/Source/UART
    )

  # device specific
  if (${MAX_DEVICE} STREQUAL "max32650" OR
    ${MAX_DEVICE} STREQUAL "max32665" OR ${MAX_DEVICE} STREQUAL "max32666")
    target_sources(${BOARD_TARGET} PRIVATE
      ${MSDK_LIB}/PeriphDrivers/Source/ICC/icc_common.c
      ${MSDK_LIB}/PeriphDrivers/Source/TPU/tpu_${PERIPH_SUFFIX}${PERIPH_ID}.c
      ${MSDK_LIB}/PeriphDrivers/Source/TPU/tpu_reva.c
      ${MSDK_LIB}/PeriphDrivers/Source/UART/uart_reva.c
      )
    target_include_directories(${BOARD_TARGET} PUBLIC
      ${MSDK_LIB}/PeriphDrivers/Source/TPU
      )
  elseif (${MAX_DEVICE} STREQUAL "max32690")
    target_sources(${BOARD_TARGET} PRIVATE
      ${MSDK_LIB}/PeriphDrivers/Source/CTB/ctb_${PERIPH_SUFFIX}${PERIPH_ID}.c
      ${MSDK_LIB}/PeriphDrivers/Source/CTB/ctb_reva.c
      ${MSDK_LIB}/PeriphDrivers/Source/CTB/ctb_common.c
      ${MSDK_LIB}/PeriphDrivers/Source/UART/uart_revb.c
      )
    target_include_directories(${BOARD_TARGET} PUBLIC
      ${MSDK_LIB}/PeriphDrivers/Source/CTB
      )
  elseif (${MAX_DEVICE} STREQUAL "max78002")
    target_sources(${BOARD_TARGET} PRIVATE
      ${MSDK_LIB}/PeriphDrivers/Source/AES/aes_${PERIPH_SUFFIX}${PERIPH_ID}.c
      ${MSDK_LIB}/PeriphDrivers/Source/AES/aes_revb.c
      ${MSDK_LIB}/PeriphDrivers/Source/TRNG/trng_${PERIPH_SUFFIX}${PERIPH_ID}.c
      ${MSDK_LIB}/PeriphDrivers/Source/TRNG/trng_revb.c
      ${MSDK_LIB}/PeriphDrivers/Source/UART/uart_revb.c
      )
    target_include_directories(${BOARD_TARGET} PUBLIC
      ${MSDK_LIB}/PeriphDrivers/Source/AES
      ${MSDK_LIB}/PeriphDrivers/Source/TRNG
      )
  else()
    message(FATAL_ERROR "Unsupported MAX device: ${MAX_DEVICE}")
  endif()

  target_compile_definitions(${BOARD_TARGET} PUBLIC
    TARGET=${MAX_DEVICE_UPPER}
    TARGET_REV=0x4131
    MXC_ASSERT_ENABLE
    ${MAX_DEVICE_UPPER}
    IAR_PRAGMAS=0
    MAX_PERIPH_ID=${PERIPH_ID}
    BOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
    )
  target_compile_options(${BOARD_TARGET} PRIVATE
    -Wno-error=strict-prototypes
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
  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_${MAX_DEVICE_UPPER})
  target_sources(${TARGET} PUBLIC
    ${TOP}/src/portable/mentor/musb/dcd_musb.c
    )
  target_compile_options(${TARGET} PRIVATE
    -Wno-error=strict-prototypes
    )
  target_link_libraries(${TARGET} PUBLIC board_${BOARD})

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})

  sign_image(${TARGET}) # for secured device such as max32651
  family_flash_openocd_adi(${TARGET})
endfunction()
