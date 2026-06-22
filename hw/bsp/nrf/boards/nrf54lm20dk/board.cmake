set(MCU_VARIANT nrf54lm20a_enga)
set(JLINK_DEVICE NRF54LM20A_M33)

function(update_board TARGET)
  # No board-specific overrides needed — primary 256 KB RAM is plenty for
  # memory-heavy examples (video YUY2 framebuf etc.).
endfunction()
