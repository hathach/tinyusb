set(MCU_VARIANT nrf54lm20a_enga)
set(JLINK_DEVICE NRF54LM20A_M33)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    CFG_EXAMPLE_VIDEO_READONLY
    )
  target_sources(${TARGET} PRIVATE
#    ${NRFX_PATH}/drivers/src/nrfx_usbreg.c
    )
endfunction()
