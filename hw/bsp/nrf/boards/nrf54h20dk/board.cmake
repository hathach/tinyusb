set(MCU_VARIANT nrf54h20)

function(update_board TARGET)
  # 32 KB primary RAM is too tight for memory-heavy examples (e.g. video YUY2
  # framebuf). TODO: route static .bss to RAM00 (512 KB) and drop this.
  target_compile_definitions(${TARGET} PUBLIC
    CFG_EXAMPLE_VIDEO_READONLY
    )
  target_sources(${TARGET} PRIVATE
#    ${NRFX_PATH}/drivers/src/nrfx_usbreg.c
    )
endfunction()
