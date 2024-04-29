set(CMAKE_SYSTEM_PROCESSOR cortex-m33 CACHE INTERNAL "System Processor")
set(MCU_VARIANT ra6m5)

set(JLINK_DEVICE R7FA6M5BH)
set(DFU_UTIL_VID_PID 2341:0368)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/${BOARD}.ld)

# Device port default to PORT1 Highspeed
if (NOT DEFINED PORT)
set(PORT 1)
endif()

# Host port will be the other port
set(HOST_PORT $<NOT:${PORT}>)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    BOARD_TUD_RHPORT=${PORT}
    BOARD_TUH_RHPORT=${HOST_PORT}
    # port 0 is fullspeed, port 1 is highspeed
    BOARD_TUD_MAX_SPEED=$<IF:${PORT},OPT_MODE_HIGH_SPEED,OPT_MODE_FULL_SPEED>
    BOARD_TUH_MAX_SPEED=$<IF:${HOST_PORT},OPT_MODE_HIGH_SPEED,OPT_MODE_FULL_SPEED>
    )
endfunction()
