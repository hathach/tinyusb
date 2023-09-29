set(MCU_VARIANT stm32h750xx)
set(JLINK_DEVICE stm32h750xb_m7)

set(LD_FILE_GNU ${ST_CMSIS}/Source/Templates/gcc/linker/${MCU_VARIANT}_flash_CM7.ld)
set(LD_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/linker/${MCU_VARIANT}_flash_CM7.icf)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H750xx
    HSE_VALUE=25000000
    CORE_CM7
    # default to PORT 0
    BOARD_TUD_RHPORT=0
    BOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
    )
endfunction()
