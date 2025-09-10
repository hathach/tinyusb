set(MCU_VARIANT stm32wba65xx)
set(JLINK_DEVICE STM32WBA65RI)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32WBA65xx
    )
endfunction()
