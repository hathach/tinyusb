set(MCU_VARIANT stm32u083xx)
set(JLINK_DEVICE stm32u083mc)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32U083MCTx_FLASH.ld)
set(LD_FILE_IAR ${CMAKE_CURRENT_LIST_DIR}/stm32u083xx_flash.icf)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32U083xx
    CFG_EXAMPLE_VIDEO_READONLY
    )
endfunction()
