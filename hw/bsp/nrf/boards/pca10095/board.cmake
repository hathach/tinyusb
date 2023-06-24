set(MCU_VARIANT nrf5340_application)
set(LD_FILE_GNU ${NRFX_DIR}/mdk/nrf5340_xxaa_application.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    NRF5340_XXAA
    NRF5340_XXAA_APPLICATION
    )
  target_sources(${TARGET} PRIVATE
    ${NRFX_DIR}/drivers/src/nrfx_usbreg.c
    )
endfunction()
