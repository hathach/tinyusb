set(MCU_VARIANT stm32f407xx)
set(JLINK_DEVICE stm32f407ve)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32F407VETx_FLASH.ld)
set(LD_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/linker/${MCU_VARIANT}_flash.icf)

set(STARTUP_FILE_GNU ${ST_CMSIS}/Source/Templates/gcc/startup_${MCU_VARIANT}.s)
set(STARTUP_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/startup_${MCU_VARIANT}.s)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32F407xx
    HSE_VALUE=8000000
    CORE_CM4
          BOARD_TUD_RHPORT=0
          BOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
    )
endfunction()
