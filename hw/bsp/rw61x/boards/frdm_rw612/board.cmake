set(MCU_VARIANT RW612)
set(MCU_CORE    RW612)

set(JLINK_DEVICE ${MCU_VARIANT})
set(PYOCD_TARGET rw612eta2i)

set(BOARD_DIR ${SDK_DIR}/boards/frdmrw612)

set(PORT 1)


function(update_board BOARD_TARGET)
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    CPU_RW612ETA2I
    BOARD_TUD_RHPORT=${PORT}
    BOARD_TUH_RHPORT=${PORT}
    BOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
    BOOT_HEADER_ENABLE=1
    )

  target_sources(${BOARD_TARGET} PUBLIC
    ${BOARD_DIR}/flash_config/flash_config.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clock_config.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/pin_mux.c
    )

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${BOARD_DIR}/flash_config
    )
endfunction()
