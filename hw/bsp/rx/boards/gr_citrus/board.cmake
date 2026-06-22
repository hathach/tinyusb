set(MCU_VARIANT rx63n)
set(MCU_FAMILY RX63X)

set(CMAKE_SYSTEM_CPU rx610 CACHE INTERNAL "System Processor")

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/r5f5631fd.ld)

set(JLINK_DEVICE R5F5631F)
set(JLINK_IF JTAG)

set(BOARD_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/gr_citrus.c
  )

function(update_board TARGET)
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )
endfunction()
