set(MCU_SUB_VARIANT 129)

set(JLINK_DEVICE TM4C1294NCPDT)
set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/tm4c1294nc.ld)
set(LD_FILE_IAR ${CMAKE_CURRENT_LIST_DIR}/TM4C1294NC.icf)

set(OPENOCD_OPTION "-f board/ti_ek-tm4c1294xl.cfg")

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    TM4C1294NCPDT
    )
endfunction()
