set(CMAKE_SYSTEM_PROCESSOR cortex-m33 CACHE INTERNAL "System Processor")
set(MCU_VARIANT ra6m5)

set(JLINK_DEVICE R7FA6M5BH)

# default to PORT1 Highspeed
if (NOT DEFINED PORT)
set(PORT 1)
endif()

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    BOARD_TUD_RHPORT=${PORT}
    # port 0 is fullspeed, port 1 is highspeed
    BOARD_TUD_MAX_SPEED=$<IF:${PORT},OPT_MODE_HIGH_SPEED,OPT_MODE_FULL_SPEED>
    )
endfunction()
