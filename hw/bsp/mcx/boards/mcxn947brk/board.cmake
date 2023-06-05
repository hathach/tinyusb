set(MCU_VARIANT MCXN947)
set(MCU_CORE MCXN947_cm33_core0)

set(JLINK_DEVICE MCXN947_M33_0)
set(PYOCD_TARGET MCXN947)
set(NXPLINK_DEVICE MCXN947:MCXN947)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    CPU_MCXN947VDF_cm33_core0
    # port 1 is highspeed
    BOARD_TUD_RHPORT=1
    )
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clock_config.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/pin_mux.c
    )
endfunction()
