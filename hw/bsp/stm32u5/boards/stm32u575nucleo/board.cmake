set(MCU_VARIANT stm32u575xx)
set(JLINK_DEVICE stm32u575zi)

set(LD_FILE_GNU ${ST_CMSIS}/Source/Templates/gcc/linker/STM32U575xx_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32U575xx
    )
endfunction()
