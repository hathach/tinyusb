include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(CMSIS_DIR ${TOP}/lib/CMSIS_6)
set(SSP ${TOP}/hw/mcu/renesas/synergy/ssp)

# include board specific
include(${TOP}/${BOARD_PATH}/board.cmake)

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
set(LD_FILE_GNU ${TOP}/${BOARD_PATH}/script/r7fs124773a01cfm.ld)
endif ()

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SSP}/src/bsp/cmsis/Device/RENESAS/${MCU_VARIANT_UPPERCASE}/Source/startup_${MCU_VARIANT_UPPERCASE}.c
    ${SSP}/src/bsp/cmsis/Device/RENESAS/${MCU_VARIANT_UPPERCASE}/Source/system_${MCU_VARIANT_UPPERCASE}.c
    ${SSP}/src/bsp/mcu/all/bsp_common.c
    ${SSP}/src/bsp/mcu/all/bsp_delay.c
    ${SSP}/src/bsp/mcu/all/bsp_irq.c
    ${SSP}/src/bsp/mcu/all/bsp_locking.c
    ${SSP}/src/bsp/mcu/all/bsp_register_protection.c
    ${SSP}/src/bsp/mcu/all/bsp_sbrk.c
	  ${SSP}/src/bsp/mcu/${MCU_VARIANT}/bsp_cache.c
	  ${SSP}/src/bsp/mcu/${MCU_VARIANT}/bsp_clocks.c
	  ${SSP}/src/bsp/mcu/${MCU_VARIANT}/bsp_feature.c
	  ${SSP}/src/bsp/mcu/${MCU_VARIANT}/bsp_group_irq.c
	  ${SSP}/src/bsp/mcu/${MCU_VARIANT}/bsp_module_stop.c
	  ${SSP}/src/bsp/mcu/${MCU_VARIANT}/bsp_rom_registers.c
	  ${SSP}/src/driver/r_cgc/r_cgc.c
	  ${SSP}/src/driver/r_elc/r_elc.c
    ${SSP}/src/driver/r_ioport/r_ioport.c
    ${TOP}/${BOARD_PATH}/synergy_gen/common_data.c
    ${TOP}/${BOARD_PATH}/synergy_gen/pin_data.c
    )

  target_compile_options(${BOARD_TARGET} PUBLIC
    -ffreestanding
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${TOP}/${BOARD_PATH}
    ${TOP}/${BOARD_PATH}/synergy_cfg/ssp_cfg/bsp
    ${TOP}/${BOARD_PATH}/synergy_cfg/ssp_cfg/driver
    ${TOP}/${BOARD_PATH}/synergy_gen
    ${CMSIS_DIR}/CMSIS/Core/Include
    ${SSP}/inc
    ${SSP}/inc/bsp
    ${SSP}/inc/driver/api
    ${SSP}/inc/driver/instances
    ${SSP}/src/bsp/cmsis/Device/RENESAS/${MCU_VARIANT_UPPERCASE}/Include
    ${SSP}/src/bsp/mcu/all
    ${SSP}/src/bsp/mcu/${MCU_VARIANT}
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    BOARD_TUD_RHPORT=${RHPORT_DEVICE}
    BOARD_TUD_MAX_SPEED=${RHPORT_DEVICE_SPEED}
    BOARD_TUH_RHPORT=${RHPORT_HOST}
    BOARD_TUH_MAX_SPEED=${RHPORT_HOST_SPEED}
    )

  target_link_libraries(${BOARD_TARGET} PUBLIC
    ${SSP}/src/driver/r_fmi/libs/libfmi_cm0_${MCU_VARIANT}_gcc.a
    ${SSP}/src/bsp/mcu/${MCU_VARIANT}/libfmi_R7FS124773A01CFM_gcc.a
  )
  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_SYNERGY)

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/renesas/rusb2/dcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/hcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/rusb2_common.c
	  ${SSP}/src/bsp/mcu/${MCU_VARIANT}/bsp_rom_registers.c
    )

  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      # linker file
      "LINKER:--script=${LD_FILE_GNU}"
      -L${TOP}/${BOARD_PATH}/script
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
    set_source_files_properties(${SSP}/src/bsp/mcu/${MCU_VARIANT}/bsp_rom_registers.c PROPERTIES COMPILE_FLAGS "-Wno-undef")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes -Wno-undef")
  endif ()

  # Flashing
  family_flash_jlink(${TARGET})
  family_add_bin_hex(${TARGET})

  if (DEFINED DFU_UTIL_VID_PID)
    family_flash_dfu_util(${TARGET} ${DFU_UTIL_VID_PID})
  endif ()
endfunction()
