set(JLINK_DEVICE ATSAME70N19B)
# N19B shares Q21B's startup code but not its MEMORY sizes (512K/256K vs 2048K/384K),
# and the SDK ships no N19B linker script, so the board carries its own.
set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/same70n19b_flash.ld)
set(STARTUP_FILE_GNU ${TOP}/hw/mcu/microchip/same70/same70b/gcc/gcc/startup_same70q21b.c)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    __SAME70N19B__
    )
endfunction()
