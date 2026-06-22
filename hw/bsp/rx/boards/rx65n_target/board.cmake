set(MCU_VARIANT rx65n)
set(MCU_FAMILY RX65X)

set(CMAKE_SYSTEM_CPU rx64m CACHE INTERNAL "System Processor")

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/r5f565ne.ld)

set(JLINK_DEVICE R5F565NE)
set(JLINK_IF JTAG)

set(RFP_DEVICE rx65x)
set(RFP_TOOL e2l)

set(BOARD_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/rx65n_target.c
  )

function(update_board TARGET)
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )
endfunction()
