set(MCU_VARIANT nrf5340_application)
set(LD_FILE_GNU ${NRFX_DIR}/mdk/nrf5340_xxaa_application.ld)

# enable max3421 host driver for this board
set(MAX3421_HOST 1)

function(update_board TARGET)
  target_sources(${TARGET} PRIVATE
    ${NRFX_DIR}/drivers/src/nrfx_usbreg.c
    )
endfunction()
