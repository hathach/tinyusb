set(MCU_VARIANT stm32h573i-dk)
set(JLINK_DEVICE stm32h573i-dk)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32H573I-DK_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H573xx
    )
endfunction()
