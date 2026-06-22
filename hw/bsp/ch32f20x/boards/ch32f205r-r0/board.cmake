set(MCU_VARIANT D8C)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    CH32F20x_D8C
    )
endfunction()
