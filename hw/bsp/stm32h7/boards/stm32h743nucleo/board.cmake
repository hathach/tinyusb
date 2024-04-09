set(MCU_VARIANT stm32h743xx)
set(JLINK_DEVICE stm32h743xi)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/../../linker/${MCU_VARIANT}_flash.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H743xx
    HSE_VALUE=8000000
    # default to PORT 0
    BOARD_TUD_RHPORT=0
    BOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
    )
endfunction()
