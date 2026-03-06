set(MCU_VARIANT LPC54608)
set(MCU_CORE LPC54608)

set(JLINK_DEVICE LPC54608J512)
set(PYOCD_TARGET LPC54608)
set(NXPLINK_DEVICE LPC54608:LPCXpresso54608)

set(LD_FILE_GNU ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/LPC54608J512_flash.ld)

# Device port default to PORT1 Highspeed
if (NOT DEFINED RHPORT_DEVICE)
  set(RHPORT_DEVICE 1)
endif()
if (NOT DEFINED RHPORT_HOST)
  set(RHPORT_HOST 0)
endif()

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    CPU_LPC54608J512ET180
    )
  target_link_libraries(${TARGET} PUBLIC
    ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/libpower_hardabi.a
    )
endfunction()
