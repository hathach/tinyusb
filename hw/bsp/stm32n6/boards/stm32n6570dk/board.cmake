set(MCU_VARIANT stm32n657xx)
set(JLINK_DEVICE stm32n6xx)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32N657XX_AXISRAM2_fsbl.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32N657xx
    )
  target_sources(${TARGET} PUBLIC
    ${ST_TCPP0203}/tcpp0203.c
    ${ST_TCPP0203}/tcpp0203_reg.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${ST_TCPP0203}
    )
endfunction()
