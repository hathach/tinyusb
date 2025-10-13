#set(MCU_VARIANT nrf5340_application)
set(MCU_VARIANT nrf5340)

function(update_board TARGET)
  target_sources(${TARGET} PRIVATE
    ${NRFX_PATH}/drivers/src/nrfx_usbreg.c
    )
endfunction()
