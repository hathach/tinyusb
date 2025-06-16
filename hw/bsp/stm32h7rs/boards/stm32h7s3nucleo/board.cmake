set(MCU_VARIANT stm32h7s3xx)
set(JLINK_DEVICE stm32h7s3xx)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/stm32h7s3xx_flash.ld)
set(LD_FILE_Clang ${LD_FILE_GNU})
set(LD_FILE_IAR ${CMAKE_CURRENT_LIST_DIR}/stm32h7s3xx_flash.icf)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H7S3xx
    )
  target_sources(${TARGET} PUBLIC
    ${ST_TCPP0203}/tcpp0203.c
    ${ST_TCPP0203}/tcpp0203_reg.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${ST_TCPP0203}
    )
endfunction()
