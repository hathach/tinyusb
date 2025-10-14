set(EFM32_FAMILY efm32gg12b)
set(EFM32_MCU EFM32GG12B810F1024GM64)
set(JLINK_DEVICE ${EFM32_MCU})

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    )
endfunction()
