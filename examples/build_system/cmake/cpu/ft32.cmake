if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -fvar-tracking
    -fvar-tracking-assignments
    )

elseif (TOOLCHAIN STREQUAL "clang")
  message(FATAL_ERROR "Clang is not supported for this target")

elseif (TOOLCHAIN STREQUAL "iar")
  message(FATAL_ERROR "IAR is not supported for this target")

endif ()
