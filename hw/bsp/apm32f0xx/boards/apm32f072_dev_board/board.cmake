set(MCU_VARIANT APM32F072xB)
set(MCU_LINKER_NAME APM32F07xxB)

set(JLINK_DEVICE APM32F072RB)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC ${MCU_VARIANT})
endfunction()
