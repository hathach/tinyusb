set(MCU_VARIANT stm32h533xx)
set(JLINK_DEVICE stm32h533re)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H533xx
    HSE_VALUE=24000000
    )
  target_compile_definitions(${BOARD_TARGET} PUBLIC
    CFG_EXAMPLE_VIDEO_READONLY
    )
endfunction()
