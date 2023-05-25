include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

# TOP is path to root directory
set(TOP "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(SDK_DIR ${TOP}/hw/mcu/nxp/lpcopen/lpc18xx/lpc_chip_18xx)

# toolchain set up
set(CMAKE_SYSTEM_PROCESSOR cortex-m3 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/tools/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS LPC18XX CACHE INTERNAL "")

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)


#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
set(BOARD_TARGET board_${BOARD})
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

  if (NOT DEFINED LD_FILE_${TOOLCHAIN})
    MESSAGE(FATAL_ERROR "LD_FILE_${TOOLCHAIN} not defined")
  endif ()

  if (TOOLCHAIN STREQUAL "gcc")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_gcc}"
      "LINKER:-Map=$<IF:$<BOOL:$<TARGET_PROPERTY:OUTPUT_NAME>>,$<TARGET_PROPERTY:OUTPUT_NAME>,$<TARGET_PROPERTY:NAME>>${CMAKE_EXECUTABLE_SUFFIX}.map"
      # nanolib
      --specs=nosys.specs
      --specs=nano.specs
      )
  else ()
    # TODO support IAR
  endif ()
endif () # BOARD_TARGET


#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET)
  family_support_configure_common(${TARGET})

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

  #---------- TinyUSB ----------
  # tinyusb target is built for each example since it depends on example's tusb_config.h
  set(TINYUSB_TARGET_PREFIX ${TARGET}-)
  add_library(${TARGET}-tinyusb_config INTERFACE)

  target_include_directories(${TARGET}-tinyusb_config INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
  target_compile_definitions(${TARGET}-tinyusb_config INTERFACE
    CFG_TUSB_MCU=OPT_MCU_LPC18XX
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
  family_flash_jlink(${TARGET})
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
