set(JLINK_DEVICE ATSAME70N19B)
set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/same70n19b_flash.ld)

# N19B and Q21B share the same vector table / startup code
set(STARTUP_FILE_GNU ${TOP}/hw/mcu/microchip/same70/same70b/gcc/gcc/startup_same70q21b.c)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    __SAME70N19B__
    )
endfunction()
