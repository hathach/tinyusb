set(MCU_VARIANT D6)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    CH32V20x_D6
    )
endfunction()
