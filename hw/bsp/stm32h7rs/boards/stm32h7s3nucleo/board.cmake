set(MCU_VARIANT stm32h7s3xx)
set(JLINK_DEVICE stm32h7s3xx)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H7S3xx
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
