set(MCU_VARIANT AT32F403ACGU7)
set(JLINK_DEVICE ${MCU_VARIANT})

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/AT32F403AxG_FLASH.ld)
set(LD_FILE_IAR ${AT_SDK_LIB}/cmsis/cm4/device_support/startup/iar/linker/AT32F403AxG.icf)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC ${MCU_VARIANT})
endfunction()
