set(MCU_VARIANT XMC4500)

set(JLINK_DEVICE XMC4500-1024)
set(LD_FILE_GNU ${SDK_DIR}/CMSIS/Infineon/COMPONENT_${MCU_VARIANT}/Source/TOOLCHAIN_GCC_ARM/XMC4500x1024.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    XMC4500_F100x1024
    )
endfunction()
