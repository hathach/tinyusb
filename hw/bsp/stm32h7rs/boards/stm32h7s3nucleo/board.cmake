set(MCU_VARIANT stm32h7s3xx)
set(JLINK_DEVICE stm32h7s3l8)

set(LD_FILE_Clang ${LD_FILE_GNU})
set(RHPORT_DEVICE 1)

# Run from RAM, image on the MX25UW25645G octal flash (XSPIM_P2); -DXSPI_BOOT=0 for internal flash only
if (NOT DEFINED XSPI_BOOT)
  set(XSPI_BOOT 1)
endif ()
if (XSPI_BOOT)
  set(STM32_PROGRAMMER_EXTLOADER MX25UW25645G_NUCLEO-H7S3L8.stldr)
endif ()
set(RHPORT_HOST 1)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H7S3xx
    )
  target_sources(${TARGET} PUBLIC
    ${ST_TCPP0203}/tcpp0203.c
    ${ST_TCPP0203}/tcpp0203_reg.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${ST_TCPP0203}
    )
endfunction()
