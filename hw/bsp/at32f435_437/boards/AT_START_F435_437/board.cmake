set(MCU_VARIANT AT32F437ZMT7)
set(MCU_LINKER_NAME AT32F437xM)

set(JLINK_DEVICE ${MCU_VARIANT})

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC ${MCU_VARIANT})
endfunction()
