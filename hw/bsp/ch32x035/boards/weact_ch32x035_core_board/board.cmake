set(LD_FLASH_SIZE 62K)
set(LD_RAM_SIZE 20K)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    SYSCLK_FREQ_48MHz_HSI=48000000
    CFG_EXAMPLE_MSC_DUAL_READONLY
    )
endfunction()
