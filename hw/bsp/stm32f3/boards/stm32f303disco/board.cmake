set(MCU_VARIANT stm32f303xc)
set(JLINK_DEVICE stm32f303vc)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32F303VCTx_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32F303xC
    )
endfunction()
