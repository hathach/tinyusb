set(MCU_FAMILY RT1050)
set(MCU_VARIANT MIMXRT1052)

set(JLINK_DEVICE MIMXRT1052xxxxB)
set(PYOCD_TARGET mimxrt1050)
set(NXPLINK_DEVICE MIMXRT1052xxxxB:EVK-MIMXRT1050)

if (NOT DEFINED RHPORT_HOST)
  set(RHPORT_HOST 1)
endif ()

function(update_board TARGET)
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/evkbimxrt1050_flexspi_nor_config.c
    )
  target_compile_definitions(${TARGET} PUBLIC
    CPU_MIMXRT1052DVL6B
    )
endfunction()
