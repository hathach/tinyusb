set(LD_FLASH_SIZE 128K)
set(LD_RAM_SIZE 32K)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    LWIP_HIGH_THROUGHPUT=0
    )
endfunction()
