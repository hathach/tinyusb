include_guard()

if (NOT BOARD)
  message(FATAL_ERROR "BOARD not specified")
endif ()

set(NRFX_DIR ${TOP}/hw/mcu/nordic/nrfx)
set(CMSIS_DIR ${TOP}/lib/CMSIS_5)

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# toolchain set up
if (MCU_VARIANT STREQUAL "nrf5340_application")
  set(CMAKE_SYSTEM_PROCESSOR cortex-m33 CACHE INTERNAL "System Processor")
  set(JLINK_DEVICE nrf5340_xxaa_app)
else ()
  set(CMAKE_SYSTEM_PROCESSOR cortex-m4 CACHE INTERNAL "System Processor")
  set(JLINK_DEVICE ${MCU_VARIANT}_xxaa)
endif ()

set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS NRF5X CACHE INTERNAL "")


#------------------------------------
# BOARD_TARGET
#------------------------------------
# only need to be built ONCE for all examples
function(add_board_target BOARD_TARGET)
  if (NOT TARGET ${BOARD_TARGET})
    add_library(${BOARD_TARGET} STATIC
      # driver
      ${NRFX_DIR}/helpers/nrfx_flag32_allocator.c
      ${NRFX_DIR}/drivers/src/nrfx_gpiote.c
      ${NRFX_DIR}/drivers/src/nrfx_power.c
      ${NRFX_DIR}/drivers/src/nrfx_spim.c
      ${NRFX_DIR}/drivers/src/nrfx_uarte.c
      # mcu
      ${NRFX_DIR}/mdk/system_${MCU_VARIANT}.c
      )
    target_compile_definitions(${BOARD_TARGET} PUBLIC CONFIG_GPIO_AS_PINRESET)

    if (MCU_VARIANT STREQUAL "nrf52840")
      target_compile_definitions(${BOARD_TARGET} PUBLIC NRF52840_XXAA)
    elseif (MCU_VARIANT STREQUAL "nrf52833")
      target_compile_definitions(${BOARD_TARGET} PUBLIC NRF52833_XXAA)
    elseif (MCU_VARIANT STREQUAL "nrf5340_application")
      target_compile_definitions(${BOARD_TARGET} PUBLIC NRF5340_XXAA NRF5340_XXAA_APPLICATION)
    endif ()

    if (TRACE_ETM STREQUAL "1")
      # ENABLE_TRACE will cause system_nrf5x.c to set up ETM trace
      target_compile_definitions(${BOARD_TARGET} PUBLIC ENABLE_TRACE)
    endif ()

    target_include_directories(${BOARD_TARGET} PUBLIC
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
      ${NRFX_DIR}
      ${NRFX_DIR}/mdk
      ${NRFX_DIR}/hal
      ${NRFX_DIR}/drivers/include
      ${NRFX_DIR}/drivers/src
      ${CMSIS_DIR}/CMSIS/Core/Include
      )

    update_board(${BOARD_TARGET})

    if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
      set(LD_FILE_GNU ${NRFX_DIR}/mdk/${MCU_VARIANT}_xxaa.ld)
    endif ()

    if (NOT DEFINED STARTUP_FILE_${CMAKE_C_COMPILER_ID})
      set(STARTUP_FILE_GNU ${NRFX_DIR}/mdk/gcc_startup_${MCU_VARIANT}.S)
    endif ()

    target_sources(${BOARD_TARGET} PUBLIC
      ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
      )

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_link_options(${BOARD_TARGET} PUBLIC
        # linker file
        "LINKER:--script=${LD_FILE_GNU}"
        -L${NRFX_DIR}/mdk
        --specs=nosys.specs --specs=nano.specs
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
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_NRF5X ${RTOS})
  target_sources(${TARGET}-tinyusb PUBLIC
    ${TOP}/src/portable/nordic/nrf5x/dcd_nrf5x.c
    )
  target_link_libraries(${TARGET}-tinyusb PUBLIC board_${BOARD})

  # Link dependencies
  target_link_libraries(${TARGET} PUBLIC board_${BOARD} ${TARGET}-tinyusb)

  # Flashing
  family_flash_jlink(${TARGET})
endfunction()
