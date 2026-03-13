set(MCU_VARIANT K32L2B31A)
set(MCU_CORE ${MCU_VARIANT})

set(JLINK_DEVICE K32L2B31xxxxA)
set(PYOCD_TARGET K32L2B)

set(LD_FILE_GNU ${SDK_DIR}/K32L/${MCU_VARIANT}/gcc/K32L2B31xxxxA_flash.ld)

function(update_board TARGET)
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clock_config.c
    )
  target_compile_definitions(${TARGET} PUBLIC
    CPU_K32L2B31VLH0A
    )
  target_include_directories(${TARGET} PUBLIC
    ${SDK_DIR}/K32L/periph2
    )
endfunction()
