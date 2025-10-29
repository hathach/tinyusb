set(JLINK_DEVICE SAME70Q21B)
set(LD_FILE_GNU ${TOP}/hw/mcu/microchip/same70/same70b/gcc/gcc/same70q21b_flash.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    __SAME70Q21B__
    )
endfunction()
