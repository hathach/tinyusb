#set(MCU_VARIANT MIMXRT1011)
set(JLINK_DEVICE STM32G0B1RE)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/STM32G0B1RETx_FLASH.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32G0B1xx
    #HSE_VALUE=8000000U
    )
endfunction()
