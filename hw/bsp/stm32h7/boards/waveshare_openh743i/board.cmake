set(MCU_VARIANT stm32h743xx)
set(JLINK_DEVICE stm32h743xi)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/../../linker/${MCU_VARIANT}_flash.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H743xx
    HSE_VALUE=8000000
    HAL_TIM_MODULE_ENABLED
    # default to PORT 1 High Speed
    BOARD_TUD_RHPORT=1
    BOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
    )
  target_sources(${TARGET} PUBLIC
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_tim.c
    ${ST_HAL_DRIVER}/Src/${ST_PREFIX}_hal_tim_ex.c
    )
endfunction()
