include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(SDK_DIR ${TOP}/hw/mcu/nxp/mcux-sdk)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m33 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/tools/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC55XX CACHE INTERNAL "")

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
      # external driver
      #lib/sct_neopixel/sct_neopixel.c

      # driver
      ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_gpio.c
      ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_common_arm.c
      ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_lpuart.c
      ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_lpflexcomm.c
      # mcu
      ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_clock.c
      ${SDK_DIR}/devices/${MCU_VARIANT}/drivers/fsl_reset.c
      ${SDK_DIR}/devices/${MCU_VARIANT}/system_${MCU_CORE}.c
      )
    #  target_compile_definitions(${BOARD_TARGET} PUBLIC
    #    )
    target_include_directories(${BOARD_TARGET} PUBLIC
      # driver
      # mcu
      ${CMSIS_DIR}/CMSIS/Core/Include
      ${SDK_DIR}/devices/${MCU_VARIANT}
      ${SDK_DIR}/devices/${MCU_VARIANT}/drivers
      )

    update_board(${BOARD_TARGET})

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_sources(${BOARD_TARGET} PUBLIC
        ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_CORE}.S
        )
      target_link_options(${BOARD_TARGET} PUBLIC
        # linker file
        "LINKER:--script=${SDK_DIR}/devices/${MCU_VARIANT}/gcc/${MCU_CORE}_flash.ld"
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
  family_add_tinyusb(${TARGET} OPT_MCU_MCXN9)

  # Link dependencies
  target_link_libraries(${TARGET} PUBLIC board_${BOARD} ${TARGET}-tinyusb)

  # Flashing
  family_flash_jlink(${TARGET})
  #family_flash_nxplink(${TARGET})
  #family_flash_pyocd(${TARGET})
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
