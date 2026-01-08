set(MCU_VARIANT nrf54h20)

function(update_board TARGET)
  # temporarily, 54h20 has multiple sram sections
  target_compile_definitions(${TARGET} PUBLIC
    CFG_EXAMPLE_VIDEO_READONLY
    )
  target_sources(${TARGET} PRIVATE
#    ${NRFX_PATH}/drivers/src/nrfx_usbreg.c
    )
endfunction()
