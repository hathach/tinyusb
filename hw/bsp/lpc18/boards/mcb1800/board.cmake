set(MCU_VARIANT LPC1857)

set(JLINK_DEVICE LPC1857)
set(PYOCD_TARGET LPC1857)
set(NXPLINK_DEVICE LPC1857:MCB1857)

set(LD_FILE_gcc ${CMAKE_CURRENT_LIST_DIR}/lpc1857.ld)

function(update_board TARGET)
  # nothing to do
endfunction()
