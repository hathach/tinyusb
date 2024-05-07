set(MCU_VARIANT MIMXRT1176)
set(MCU_CORE _cm7)

set(JLINK_DEVICE MIMXRT1176xxxA_M7)
set(PYOCD_TARGET mimxrt1170_cm7)
set(NXPLINK_DEVICE MIMXRT1176xxxxx:MIMXRT1170-EVK)

function(update_board TARGET)
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/evkbmimxrt1170_flexspi_nor_config.c
    )
  target_compile_definitions(${TARGET} PUBLIC
    CPU_MIMXRT1176DVMAA_cm7
    BOARD_TUD_RHPORT=0
    BOARD_TUH_RHPORT=1
    )
endfunction()
