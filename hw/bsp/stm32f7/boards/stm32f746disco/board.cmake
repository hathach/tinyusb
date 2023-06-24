set(MCU_VARIANT stm32f746xx)
set(JLINK_DEVICE stm32f746xx)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32F746ZGTx_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32F746xx
    HSE_VALUE=25000000
    # default to PORT 1 High Speed
    BOARD_TUD_RHPORT=1
    BOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
    )
endfunction()
