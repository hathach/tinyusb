set(MCU_VARIANT stm32n657xx)
set(JLINK_DEVICE stm32n6xx)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32N657XX_LRUN.ld)
set(LD_FILE_Clang ${LD_FILE_GNU})

set(STARTUP_FILE_GNU ${ST_CMSIS}/Source/Templates/gcc/startup_${MCU_VARIANT}.s)

function(update_board TARGET)

  target_compile_definitions(${TARGET} PUBLIC
    STM32N6xx
    SEGGER_RTT_SECTION="noncacheable_buffer"
    BUFFER_SIZE_UP=0x3000
    )

  target_sources(${TARGET} PUBLIC
    # BSP
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tcpp0203/tcpp0203.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tcpp0203/tcpp0203_reg.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tcpp0203
    )
endfunction()
