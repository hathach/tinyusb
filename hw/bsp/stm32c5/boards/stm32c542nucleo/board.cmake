set(MCU_VARIANT stm32c542xx)
set(JLINK_DEVICE stm32c542rc)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/../../linker/stm32c542xc_flash.ld)
set(LD_FILE_IAR ${CMAKE_CURRENT_LIST_DIR}/../../linker/stm32c542xc_flash.icf)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32C542xx
    HSE_VALUE=24000000
    HSE_STARTUP_TIMEOUT=100
    )
endfunction()
