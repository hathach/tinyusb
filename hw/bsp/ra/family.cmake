include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(CMSIS_DIR ${TOP}/lib/CMSIS_5)
set(FSP_RA ${TOP}/hw/mcu/renesas/fsp/ra/fsp)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)
#set(FREERTOS_PORT A_CUSTOM_PORT CACHE INTERNAL "")

set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS RAXXX ${MCU_VARIANT} CACHE INTERNAL "")

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (NOT TARGET ${BOARD_TARGET})
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
      ${FSP_RA}/src/bsp/mcu/all/bsp_rom_registers.c
      ${FSP_RA}/src/bsp/mcu/all/bsp_sbrk.c
      ${FSP_RA}/src/bsp/mcu/all/bsp_security.c
      ${FSP_RA}/src/r_ioport/r_ioport.c
      )

    target_compile_options(${BOARD_TARGET} PUBLIC
      -ffreestanding
      )
    target_include_directories(${BOARD_TARGET} PUBLIC
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/fsp_cfg
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}/fsp_cfg/bsp
      ${CMSIS_DIR}/CMSIS/Core/Include
      ${FSP_RA}/inc
      ${FSP_RA}/inc/api
      ${FSP_RA}/inc/instances
      ${FSP_RA}/src/bsp/cmsis/Device/RENESAS/Include
      ${FSP_RA}/src/bsp/mcu/all
      ${FSP_RA}/src/bsp/mcu/${MCU_VARIANT}
      )

    update_board(${BOARD_TARGET})

    if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
      set(LD_FILE_GNU ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/gcc/${MCU_VARIANT}.ld)
    endif ()

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_link_options(${BOARD_TARGET} PUBLIC
        # linker file
        "LINKER:--script=${LD_FILE_GNU}"
        -L${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/gcc
        -nostartfiles
        # nanolib
        --specs=nano.specs
        --specs=nosys.specs
        )
    elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
      target_link_options(${BOARD_TARGET} PUBLIC
        "LINKER:--config=${LD_FILE_IAR}"
        )
    endif ()
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
    )

#  # RA has custom freertos port
#  if (NOT TARGET freertos_kernel_port)
#    add_library(freertos_kernel_port STATIC)
#    target_sources(freertos_kernel_port PUBLIC ${FSP_RA}/src/rm_freertos_port/port.c)
#    target_include_directories(freertos_kernel_port PUBLIC ${FSP_RA}/src/rm_freertos_port)
#
#    target_link_libraries(freertos_kernel_port PUBLIC freertos_kernel)
#  endif ()

    # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_RAXXX ${RTOS})
  target_sources(${TARGET}-tinyusb PUBLIC
    ${TOP}/src/portable/renesas/rusb2/dcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/hcd_rusb2.c
    ${TOP}/src/portable/renesas/rusb2/rusb2_common.c
    )
  target_link_libraries(${TARGET}-tinyusb PUBLIC board_${BOARD})

  # Link dependencies
  target_link_libraries(${TARGET} PUBLIC board_${BOARD} ${TARGET}-tinyusb)

  # Flashing
  family_flash_jlink(${TARGET})

  if (DEFINED DFU_UTIL_VID_PID)
    family_add_bin_hex(${TARGET})
    family_flash_dfu_util(${TARGET} ${DFU_UTIL_VID_PID})
  endif ()
endfunction()
