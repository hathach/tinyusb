set(NUC_SERIES nuc121)
set(JLINK_DEVICE NUC121SC2AE)
set(LD_FILE_GNU ${CMAKE_CURRENT_LIST_DIR}/nuc121_flash.ld)

# Extra StdDriver sources for NUC121
set(BOARD_SOURCES
  ${SDK_DIR}/StdDriver/src/fmc.c
  ${SDK_DIR}/StdDriver/src/sys.c
  ${SDK_DIR}/StdDriver/src/timer.c
)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    )
endfunction()
