include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(CMSIS_DIR ${TOP}/lib/CMSIS_6)
set(FSP_RA ${TOP}/hw/mcu/renesas/fsp/ra/fsp)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)
#set(FREERTOS_PORT A_CUSTOM_PORT CACHE INTERNAL "")

set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS RAXXX ${MCU_VARIANT} CACHE INTERNAL "")

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
set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/script/fsp.ld)
endif ()

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${FSP_RA}/src/bsp/cmsis/Device/RENESAS/Source/startup.c
    ${FSP_RA}/src/bsp/cmsis/Device/RENESAS/Source/system.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_clocks.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_common.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_delay.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_group_irq.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_guard.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_io.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_irq.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_register_protection.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_sbrk.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_security.c
    ${FSP_RA}/src/r_ioport/r_ioport.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/ra_gen/common_data.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/ra_gen/pin_data.c
    )

  target_compile_options(${BOARD_TARGET} PUBLIC
    -ffreestanding
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/ra_cfg/fsp_cfg
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/ra_cfg/fsp_cfg/bsp
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/ra_gen
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${FSP_RA}/inc
    ${FSP_RA}/inc/api
    ${FSP_RA}/inc/instances
    ${FSP_RA}/src/bsp/cmsis/Device/RENESAS/Include
    ${FSP_RA}/src/bsp/mcu/all
    ${FSP_RA}/src/bsp/mcu/${MCU_VARIANT}
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
  family_add_tinyusb(${TARGET} OPT_MCU_RAXXX)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${FSP_RA}/src/bsp/mcu/all/bsp_rom_registers.c
    ${TOP}/src/portable/renesas/rusb2/dcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/hcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/rusb2_common.c
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
    set_source_files_properties(${FSP_RA}/src/bsp/mcu/all/bsp_rom_registers.c PROPERTIES COMPILE_FLAGS "-Wno-undef")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")
  endif ()

  # Flashing
  family_flash_jlink(${TARGET})
  family_add_bin_hex(${TARGET})

  if (DEFINED DFU_UTIL_VID_PID)
    family_flash_dfu_util(${TARGET} ${DFU_UTIL_VID_PID})
  endif ()
endfunction()
