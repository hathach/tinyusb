include_guard()

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

set(CMSIS_DIR ${TOP}/lib/CMSIS_5)
if (LPC_FAMILY STREQUAL 11xx)
  set(SDK_DIR ${TOP}/hw/mcu/nxp/lpcopen/lpc11uxx/lpc_chip_11uxx)
else()
  set(SDK_DIR ${TOP}/hw/mcu/nxp/lpcopen/lpc${LPC_FAMILY}/lpc_chip_${LPC_FAMILY})
endif()

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m0plus CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC11UXX CACHE INTERNAL "")


#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} STATIC
    ${SDK_DIR}/../gcc/cr_startup_lpc${LPC_FAMILY}.c
    ${SDK_DIR}/src/chip_${LPC_FAMILY}.c
    ${SDK_DIR}/src/clock_${LPC_FAMILY}.c
    ${SDK_DIR}/src/iap.c
    ${SDK_DIR}/src/iocon_${LPC_FAMILY}.c
    ${SDK_DIR}/src/sysinit_${LPC_FAMILY}.c
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __USE_LPCOPEN
    __VTOR_PRESENT=0
    CORE_M0
    CORE_M0PLUS
    CFG_TUSB_MEM_ALIGN=TU_ATTR_ALIGNED\(64\)
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}/inc
    ${CMSIS_DIR}/CMSIS/Core/Include
    )

  update_board(${BOARD_TARGET})

  set_target_properties(${BOARD_TARGET} PROPERTIES COMPILE_FLAGS "-Wno-incompatible-pointer-types")
endfunction()


#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_LPC11UXX)

  #---------- Port Specific ----------
  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c
    )
  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${TARGET} PUBLIC
      -nostdlib
      -Wno-error=incompatible-pointer-types
      )
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
  #family_flash_nxplink(${TARGET})
endfunction()
