include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

if (TOOLCHAIN STREQUAL "clang")
  message(FATAL_ERROR "Renesas Synergy SSP does not build with Clang; use TOOLCHAIN=gcc")
endif ()

set(CMSIS_DIR ${TOP}/lib/CMSIS_6)
set(RENESAS_SSP_PATH "${TOP}/hw/mcu/renesas/synergy/ssp" CACHE PATH
  "Path to a licensed Renesas Synergy Software Package (SSP)")

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

if (NOT EXISTS "${RENESAS_SSP_PATH}/inc/bsp/bsp_api.h" OR
    NOT EXISTS "${RENESAS_SSP_PATH}/src/bsp/mcu/all/bsp_common.c" OR
    NOT EXISTS "${RENESAS_SSP_PATH}/src/bsp/cmsis/Device/RENESAS/${MCU_VARIANT_UPPERCASE}/Source/startup_${MCU_VARIANT_UPPERCASE}.c")
  message(FATAL_ERROR
    "Renesas Synergy SSP was not found or is incomplete at '${RENESAS_SSP_PATH}'. "
    "Install SSP under its license terms, then set RENESAS_SSP_PATH to the SSP root "
    "(the directory containing inc/ and src/).")
endif ()

set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS SYNERGY ${MCU_VARIANT} CACHE INTERNAL "")

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
if (NOT DEFINED LD_FILE_GNU)
set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/script/r7fs124773a01cfm.ld)
endif ()

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${RENESAS_SSP_PATH}/src/bsp/cmsis/Device/RENESAS/${MCU_VARIANT_UPPERCASE}/Source/startup_${MCU_VARIANT_UPPERCASE}.c
    ${RENESAS_SSP_PATH}/src/bsp/cmsis/Device/RENESAS/${MCU_VARIANT_UPPERCASE}/Source/system_${MCU_VARIANT_UPPERCASE}.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/all/bsp_common.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/all/bsp_delay.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/all/bsp_irq.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/all/bsp_locking.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/all/bsp_register_protection.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/all/bsp_sbrk.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/bsp_cache.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/bsp_clocks.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/bsp_feature.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/bsp_group_irq.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/bsp_module_stop.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/bsp_rom_registers.c
    ${RENESAS_SSP_PATH}/src/driver/r_cgc/r_cgc.c
    ${RENESAS_SSP_PATH}/src/driver/r_elc/r_elc.c
    ${RENESAS_SSP_PATH}/src/driver/r_ioport/r_ioport.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/synergy_gen/common_data.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/synergy_gen/pin_data.c
    )

  target_compile_options(${BOARD_TARGET} PUBLIC
    -ffreestanding
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/synergy_cfg/ssp_cfg/bsp
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/synergy_cfg/ssp_cfg/driver
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/synergy_gen
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${RENESAS_SSP_PATH}/inc
    ${RENESAS_SSP_PATH}/inc/bsp
    ${RENESAS_SSP_PATH}/inc/driver/api
    ${RENESAS_SSP_PATH}/inc/driver/instances
    ${RENESAS_SSP_PATH}/src/bsp/cmsis/Device/RENESAS/${MCU_VARIANT_UPPERCASE}/Include
    ${RENESAS_SSP_PATH}/src/bsp/mcu/all
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    BOARD_TUD_RHPORT=${RHPORT_DEVICE}
    BOARD_TUD_MAX_SPEED=${RHPORT_DEVICE_SPEED}
    BOARD_TUH_RHPORT=${RHPORT_HOST}
    BOARD_TUH_MAX_SPEED=${RHPORT_HOST_SPEED}
    )

  target_link_libraries(${BOARD_TARGET} PUBLIC
    ${RENESAS_SSP_PATH}/src/driver/r_fmi/libs/libfmi_cm0_${MCU_VARIANT}_gcc.a
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/libfmi_R7FS124773A01CFM_gcc.a
  )
  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
# Don't include options setting in .bin file since it creates an unnecessary large file due to padding
function(family_add_bin_hex TARGET)
  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Obinary --only-section .text --only-section .data --only-section .rodata
            --only-section .bss $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.hex
    VERBATIM)
endfunction()

function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_SYNERGY)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/renesas/rusb2/dcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/hcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/rusb2_common.c
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/bsp_rom_registers.c
    )

  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      # linker file
      "LINKER:--script=${LD_FILE_GNU}"
      -L${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/script
      -Wl,--defsym=end=__bss_end__
      -nostartfiles
      --specs=nano.specs --specs=nosys.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/bsp_rom_registers.c PROPERTIES COMPILE_FLAGS "-Wno-undef")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes -Wno-undef")
  endif ()

  # Flashing
  family_flash_jlink(${TARGET})
  family_add_bin_hex(${TARGET})

  if (DEFINED DFU_UTIL_VID_PID)
    family_flash_dfu_util(${TARGET} ${DFU_UTIL_VID_PID})
  endif ()
endfunction()
