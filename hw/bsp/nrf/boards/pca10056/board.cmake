set(MCU_VARIANT nrf52840)
set(LD_FILE_GNU ${NRFX_DIR}/mdk/nrf52840_xxaa.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    NRF52840_XXAA
    )
endfunction()
