set(MCU_VARIANT stm32c5a3xx)
set(JLINK_DEVICE stm32c5a3zg)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/../../linker/stm32c5a3xg_flash.ld)
set(LD_FILE_IAR ${CMAKE_CURRENT_LIST_DIR}/../../linker/stm32c5a3xg_flash.icf)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32C5A3xx
    HSE_VALUE=48000000
    HSE_STARTUP_TIMEOUT=100
    )
endfunction()
