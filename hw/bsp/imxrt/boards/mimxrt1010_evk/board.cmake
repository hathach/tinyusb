set(MCU_VARIANT MIMXRT1011)

function(update_board TARGET)
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/evkmimxrt1010_flexspi_nor_config.c
    )
  target_compile_definitions(${TARGET} PUBLIC
    CPU_MIMXRT1011DAE5A
    CFG_EXAMPLE_VIDEO_READONLY
    )
endfunction()
