set(SERIAL /dev/ttyUSB0 CACHE STRING "Serial port for flashing")

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    )
endfunction()
