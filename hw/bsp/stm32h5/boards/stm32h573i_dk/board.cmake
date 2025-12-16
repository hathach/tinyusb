set(MCU_VARIANT stm32h573xx)
set(JLINK_DEVICE stm32h573ii)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H573xx
    )
  target_sources(${TARGET} PUBLIC
    ${ST_TCPP0203}/tcpp0203.c
    ${ST_TCPP0203}/tcpp0203_reg.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${ST_TCPP0203}
    )
endfunction()
