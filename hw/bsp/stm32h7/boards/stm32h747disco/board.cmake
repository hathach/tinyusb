set(MCU_VARIANT stm32h747xx)
set(JLINK_DEVICE stm32h747xi_m7)

set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/../../linker/${MCU_VARIANT}_flash_CM7.ld)
set(LD_FILE_IAR ${ST_CMSIS}/Source/Templates/iar/linker/${MCU_VARIANT}_flash_CM7.icf)

set(RHPORT_SPEED OPT_MODE_FULL_SPEED OPT_MODE_HIGH_SPEED)

# device default to PORT 1 High Speed
if (NOT DEFINED RHPORT_DEVICE)
  set(RHPORT_DEVICE 1)
endif()
if (NOT DEFINED RHPORT_HOST)
  set(RHPORT_HOST 1)
endif()

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    STM32H747xx
    HSE_VALUE=25000000
    CORE_CM7
    )
endfunction()
