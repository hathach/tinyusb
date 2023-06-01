include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(ST_FAMILY g0)
set(ST_PREFIX stm32${ST_FAMILY}xx)

set(ST_HAL_DRIVER ${TOP}/hw/mcu/st/stm32${ST_FAMILY}xx_hal_driver)
set(ST_CMSIS ${TOP}/hw/mcu/st/cmsis_device_${ST_FAMILY})
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# enable LTO
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/tools/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS STM32G0 CACHE INTERNAL "")

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)


#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
set(BOARD_TARGET board_${BOARD})
if (NOT TARGET ${BOARD_TARGET})
  add_library(${BOARD_TARGET} STATIC
    ${ST_CMSIS}/Source/Templates/system_${ST_PREFIX}.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_cortex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_pwr_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_rcc.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_rcc_ex.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_gpio.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_uart.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_uart_ex.c
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMSIS_5}/CMSIS/Core/Include
    ${ST_CMSIS}/Include
    ${ST_HAL_DRIVER}/Inc
    )
  target_compile_options(${BOARD_TARGET} PUBLIC
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    )
  update_board(${BOARD_TARGET})

  target_sources(${BOARD_TARGET} PUBLIC
    ${STARTUP_FILE_${TOOLCHAIN}}
    )

  if (TOOLCHAIN STREQUAL "gcc")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_gcc}"
      -nostartfiles
      # nanolib
      --specs=nosys.specs
      --specs=nano.specs
      )
  else ()
    # TODO support IAR
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_iar}"
      )
  endif ()
endif () # BOARD_TARGET


#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET)
  family_configure_common(${TARGET})

  #---------- Port Specific ----------
  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PUBLIC
    # TinyUSB Port
    ${TOP}/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
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

  #---------- TinyUSB ----------
  # tinyusb target is built for each example since it depends on example's tusb_config.h
  set(TINYUSB_TARGET_PREFIX ${TARGET}-)
  add_library(${TARGET}-tinyusb_config INTERFACE)

  target_include_directories(${TARGET}-tinyusb_config INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
  target_compile_definitions(${TARGET}-tinyusb_config INTERFACE
    CFG_TUSB_MCU=OPT_MCU_STM32G0
    )

  # tinyusb's CMakeList.txt
  add_subdirectory(${TOP}/src ${CMAKE_CURRENT_BINARY_DIR}/tinyusb)

  # Link dependencies
  target_link_libraries(${TARGET} PUBLIC ${BOARD_TARGET} ${TARGET}-tinyusb)

  # group target (not yet supported by clion)
  set_target_properties(${TARGET}-tinyusb ${TARGET}-tinyusb_config
    PROPERTIES FOLDER ${TARGET}_sub
    )

  #---------- Flash ----------
  family_flash_stlink(${TARGET})
  #family_flash_jlink(${TARGET})
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
