set(MCU_VARIANT stm32u083xx)
set(JLINK_DEVICE stm32u083mc)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32U083MCTx_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32U083xx
    )
endfunction()
