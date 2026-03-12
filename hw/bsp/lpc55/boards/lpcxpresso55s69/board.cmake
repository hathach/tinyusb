set(MCU_VARIANT LPC55S69)
set(MCU_CORE LPC55S69_cm33_core0)

set(JLINK_DEVICE LPC55S69_M33_0)
set(JLINK_OPTION "-USB 000727648789")

set(PYOCD_TARGET LPC55S69)
set(NXPLINK_DEVICE LPC55S69:LPCXpresso55S69)

# device highspeed, host fullspeed
set(RHPORT_DEVICE 1)
set(RHPORT_HOST 0)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    CPU_LPC55S69JBD100_cm33_core0
    )
endfunction()
