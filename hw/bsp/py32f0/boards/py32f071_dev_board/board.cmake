set(PYOCD_TARGET py32f071ex8)
set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/py32f071xb.ld)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    PY32F071xB
    CFG_EXAMPLE_MSC_DUAL_READONLY
    CFG_EXAMPLE_VIDEO_READONLY
    )
endfunction()
