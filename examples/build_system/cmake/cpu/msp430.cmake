if (TOOLCHAIN STREQUAL "gcc")
  set(FREERTOS_PORT GCC_MSP430F449 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  message(FATAL_ERROR "Clang is not supported for this target")

elseif (TOOLCHAIN STREQUAL "iar")
  set(FREERTOS_PORT IAR_MSP430 CACHE INTERNAL "")

endif ()
