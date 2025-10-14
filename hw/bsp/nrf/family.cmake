include_guard()

set(NRFX_PATH ${TOP}/hw/mcu/nordic/nrfx)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# include board specific, for zephyr BOARD_ALIAS may be used instead
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake OPTIONAL RESULT_VARIABLE board_cmake_included)
if (NOT board_cmake_included)
  include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD_ALIAS}/board.cmake)
endif ()

# toolchain set up
if (MCU_VARIANT STREQUAL nrf5340 OR MCU_VARIANT STREQUAL nrf54h20)
  set(CMAKE_SYSTEM_CPU cortex-m33 CACHE INTERNAL "System Processor")
  set(JLINK_DEVICE ${MCU_VARIANT}_xxaa_app)
else ()
  set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
  set(JLINK_DEVICE ${MCU_VARIANT}_xxaa)
endif ()

if (MCU_VARIANT STREQUAL "nrf54h20")
  set(FAMILY_MCUS NRF54 CACHE INTERNAL "")
else ()
  set(FAMILY_MCUS NRF5X CACHE INTERNAL "")
endif ()

set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  add_library(${BOARD_TARGET} STATIC
    ${NRFX_PATH}/helpers/nrfx_flag32_allocator.c
    ${NRFX_PATH}/drivers/src/nrfx_gpiote.c
    ${NRFX_PATH}/drivers/src/nrfx_power.c
    ${NRFX_PATH}/drivers/src/nrfx_spim.c
    ${NRFX_PATH}/drivers/src/nrfx_uarte.c
    ${NRFX_PATH}/soc/nrfx_atomic.c
    )

  if (MCU_VARIANT STREQUAL nrf54h20)
    set(LD_FILE_GNU_DEFAULT ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/${MCU_VARIANT}_xxaa_application.ld)
    target_sources(${BOARD_TARGET} PRIVATE
      ${NRFX_PATH}/mdk/system_nrf54h.c
      ${NRFX_PATH}/mdk/gcc_startup_${MCU_VARIANT}_application.S
    )
  elseif (MCU_VARIANT STREQUAL nrf5340)
    set(LD_FILE_GNU_DEFAULT ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/${MCU_VARIANT}_xxaa_application.ld)
    target_sources(${BOARD_TARGET} PRIVATE
      ${NRFX_PATH}/mdk/system_${MCU_VARIANT}_application.c
      ${NRFX_PATH}/mdk/gcc_startup_${MCU_VARIANT}_application.S
      ${NRFX_PATH}/drivers/src/nrfx_usbreg.c
      )
    target_compile_definitions(${BOARD_TARGET} PUBLIC NRF5340_XXAA_APPLICATION)
  else()
    set(LD_FILE_GNU_DEFAULT ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/linker/${MCU_VARIANT}_xxaa.ld)
    target_sources(${BOARD_TARGET} PRIVATE
      ${NRFX_PATH}/mdk/system_${MCU_VARIANT}.c
      ${NRFX_PATH}/mdk/gcc_startup_${MCU_VARIANT}.S
      )
  endif ()

  if (NOT DEFINED LD_FILE_GNU)
    set(LD_FILE_GNU ${LD_FILE_GNU_DEFAULT})
  endif ()

  string(TOUPPER ${MCU_VARIANT} MCU_VARIANT_UPPER)
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    __STARTUP_CLEAR_BSS
    CONFIG_GPIO_AS_PINRESET
    ${MCU_VARIANT_UPPER}_XXAA
    NRF_APPLICATION
    )

  if (TRACE_ETM STREQUAL "1")
    # ENABLE_TRACE will cause system_nrf5x.c to set up ETM trace
    target_compile_definitions(${BOARD_TARGET} PUBLIC ENABLE_TRACE)
  endif ()

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/nrfx_config
    ${NRFX_PATH}
    ${NRFX_PATH}/mdk
    ${NRFX_PATH}/drivers/include
    ${CMSIS_DIR}/CMSIS/Core/Include
    )

  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -L${NRFX_PATH}/mdk
      --specs=nosys.specs --specs=nano.specs
      -nostartfiles
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -L${NRFX_PATH}/mdk
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${BOARD_TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()
endfunction()


#------------------------------------
# Functions
#------------------------------------

#function(family_flash_adafruit_nrfutil TARGET)
#  add_custom_target(${TARGET}-adafruit-nrfutil
#    DEPENDS ${TARGET}
#    COMMAND adafruit-nrfutil --verbose dfu serial --package $^ -p /dev/ttyACM0 -b 115200 --singlebank --touch 1200
#    )
#endfunction()


function(family_configure_example TARGET RTOS)
  # Board target
  if (NOT RTOS STREQUAL zephyr)
    add_board_target(board_${BOARD})
    target_link_libraries(${TARGET} PUBLIC board_${BOARD})
  endif ()

  family_configure_common(${TARGET} ${RTOS})

  #---------- Port Specific ----------
  # These files are built for each example since it depends on example's tusb_config.h
  target_sources(${TARGET} PRIVATE
    # BSP
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    )
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes")
  endif ()

  target_include_directories(${TARGET} PUBLIC
    # family, hw, board
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )
  if (RTOS STREQUAL zephyr AND DEFINED BOARD_ALIAS AND NOT BOARD STREQUAL BOARD_ALIAS)
    target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD_ALIAS})
  endif ()

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_${FAMILY_MCUS})
  target_sources(${TARGET} PRIVATE
    ${TOP}/src/portable/nordic/nrf5x/dcd_nrf5x.c
    ${TOP}/src/portable/synopsys/dwc2/dcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/hcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/dwc2_common.c
    )

  # Flashing
  #  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
  #  family_flash_adafruit_nrfutil(${TARGET})
endfunction()
