set(CMAKE_SYSTEM_PROCESSOR cortex-m33 CACHE INTERNAL "System Processor")
set(MCU_VARIANT ra4m3)

set(JLINK_DEVICE R7FA4M3AF)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/ra4m3_ek.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    )
  target_sources(${TARGET} PRIVATE
    )
  target_include_directories(${BOARD_TARGET} PUBLIC
    )
endfunction()
