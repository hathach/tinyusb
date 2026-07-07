set(LD_FLASH_SIZE 448K)
set(LD_RAM_SIZE 16K)

function(update_board TARGET)
  target_compile_definitions(${TARGET} PUBLIC
    # 16KB RAM cannot hold two 8KB RAM disks: use read-only disks in flash
    CFG_EXAMPLE_MSC_DUAL_READONLY
    )
endfunction()
