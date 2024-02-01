set(MCU_VARIANT stm32l476xx)
set(JLINK_DEVICE stm32l476rg)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32L476RGTx_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32L476xx
    )
endfunction()
