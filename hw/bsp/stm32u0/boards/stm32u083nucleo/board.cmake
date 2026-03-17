set(MCU_VARIANT stm32u083xx)
set(JLINK_DEVICE stm32u083rc)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32U083xx
    CFG_EXAMPLE_VIDEO_READONLY
    )
endfunction()
