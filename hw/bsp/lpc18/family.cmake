include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(SDK_DIR ${TOP}/hw/mcu/nxp/lpcopen/lpc18xx/lpc_chip_18xx)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m3 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/tools/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC18XX CACHE INTERNAL "")

# enable LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if (IPO_SUPPORTED)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()


#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (NOT TARGET ${BOARD_TARGET})
    add_library(${BOARD_TARGET} STATIC
      ${SDK_DIR}/../gcc/cr_startup_lpc18xx.c
      ${SDK_DIR}/src/chip_18xx_43xx.c
      ${SDK_DIR}/src/clock_18xx_43xx.c
      ${SDK_DIR}/src/gpio_18xx_43xx.c
      ${SDK_DIR}/src/sysinit_18xx_43xx.c
      ${SDK_DIR}/src/uart_18xx_43xx.c
      )
    target_compile_options(${BOARD_TARGET} PUBLIC
      -nostdlib
      )
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      __USE_LPCOPEN
      CORE_M3
      )
    target_include_directories(${BOARD_TARGET} PUBLIC
      ${SDK_DIR}/inc
      ${SDK_DIR}/inc/config_18xx
      )

    update_board(${BOARD_TARGET})

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_link_options(${BOARD_TARGET} PUBLIC
        "LINKER:--script=${LD_FILE_GNU}"
        # nanolib
        --specs=nosys.specs
        --specs=nano.specs
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
function(family_configure_example TARGET)
  family_configure_common(${TARGET})

  # Board target
  add_board_target(board_${BOARD})

  #---------- Port Specific ----------
  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PUBLIC
    # TinyUSB Port
    ${TOP}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
    ${TOP}/src/portable/chipidea/ci_hs/hcd_ci_hs.c
    ${TOP}/src/portable/ehci/ehci.c
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

  # Add TinyUSB
  family_add_tinyusb(${TARGET} OPT_MCU_LPC18XX)

  # Link dependencies
  target_link_libraries(${TARGET} PUBLIC board_${BOARD} ${TARGET}-tinyusb)

  # Flashing
  family_flash_jlink(${TARGET})
  #family_flash_nxplink(${TARGET})
endfunction()


function(family_configure_device_example TARGET)
  family_configure_example(${TARGET})
endfunction()

function(family_configure_host_example TARGET)
  family_configure_example(${TARGET})
endfunction()

function(family_configure_dual_usb_example TARGET)
  family_configure_example(${TARGET})
endfunction()
