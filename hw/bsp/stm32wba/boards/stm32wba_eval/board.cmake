set(MCU_VARIANT stm32wba65xx)
set(JLINK_DEVICE STM32WB65RI)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/stm32wb65RIVX_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32WB65xx
    )
endfunction()
