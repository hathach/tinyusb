if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -march=rv32imac_zicsr
    -mabi=ilp32
    )
  set(FREERTOS_PORT GCC_RISC_V CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    -march=rv32imac_zicsr
    -mabi=ilp32
    )
  set(FREERTOS_PORT GCC_RISC_V CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  message(FATAL_ERROR "IAR not supported")
  set(FREERTOS_PORT IAR_RISC_V CACHE INTERNAL "")
endif ()
