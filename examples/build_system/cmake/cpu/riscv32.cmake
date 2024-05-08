if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -march=rv32imac
    -mabi=ilp32
    )
  # set(FREERTOS_PORT GCC_ARM_CM0 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    -march=rv32imac
    -mabi=ilp32
    )
  #set(FREERTOS_PORT GCC_ARM_CM0 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  message(FATAL_ERROR "IAR not supported")

endif ()
