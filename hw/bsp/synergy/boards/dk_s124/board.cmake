set(CMAKE_SYSTEM_CPU cortex-m0 CACHE INTERNAL "System Processor")
set(MCU_VARIANT s124)
set(MCU_VARIANT_UPPERCASE S124)

set(JLINK_DEVICE R7FS124773A01CFM)

function(update_board TARGET)
  # 16 KB RAM: keep example disks and video frames in flash
  target_compile_definitions(${TARGET} PUBLIC
    CFG_EXAMPLE_MSC_READONLY
    CFG_EXAMPLE_VIDEO_READONLY
    )
endfunction()
