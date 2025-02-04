set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(MCU_VARIANT ra4m1)
set(JLINK_DEVICE R7FA4M1AB)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    CFG_EXAMPLE_VIDEO_READONLY
    )
endfunction()
