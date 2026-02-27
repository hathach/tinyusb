set(JLINK_DEVICE SAME70N19B)
set(LD_FILE_GNU ${TOP}/hw/mcu/microchip/same70/same70b/gcc/gcc/same70n19b_flash.ld)
set(LD_FILE_IAR ${TOP}/hw/mcu/microchip/same70/same70b/iar/config/linker/Microchip/atsame70n19b/flash.icf)

set(STARTUP_FILE_GNU ${TOP}/hw/mcu/microchip/same70/same70b/gcc/gcc/startup_same70n19b.c)
set(STARTUP_FILE_IAR ${TOP}/hw/mcu/microchip/same70/same70b/iar/iar/startup_same70n19b.c)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    __SAME70N19B__
    )
endfunction()
