set(CMAKE_SYSTEM_PROCESSOR cortex-m33 CACHE INTERNAL "System Processor")
set(MCU_VARIANT ra6m5)

set(JLINK_DEVICE R7FA6M5BH)

function(update_board TARGET)
#  target_compile_definitions(${TARGET} PUBLIC)
#  target_sources(${TARGET} PRIVATE)
#  target_include_directories(${BOARD_TARGET} PUBLIC)
endfunction()
