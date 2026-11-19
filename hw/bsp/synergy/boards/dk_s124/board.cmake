set(CMAKE_SYSTEM_CPU cortex-m0 CACHE INTERNAL "System Processor")
set(MCU_VARIANT s124)
set(MCU_VARIANT_UPPERCASE S124)

set(JLINK_DEVICE R7FS124773A01CFM)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/script/r7fs124773a01cfm.ld)

function(update_board TARGET)
  target_link_libraries(${TARGET} PUBLIC
    ${RENESAS_SSP_PATH}/src/driver/r_fmi/libs/libfmi_cm0_${MCU_VARIANT}_gcc.a
    ${RENESAS_SSP_PATH}/src/bsp/mcu/${MCU_VARIANT}/libfmi_R7FS124773A01CFM_gcc.a
    )

  # 16 KB RAM: keep example disks and video frames in flash
  target_compile_definitions(${TARGET} PUBLIC
    CFG_EXAMPLE_MSC_READONLY
    CFG_EXAMPLE_VIDEO_READONLY
    )
endfunction()
