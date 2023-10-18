set(MCU_VARIANT stm32u5a5xx)
set(JLINK_DEVICE stm32u5a5zj)

set(LD_FILE_GNU ${ST_CMSIS}/Source/Templates/gcc/linker/STM32U5A9xx_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32U5A5xx
    )
endfunction()
