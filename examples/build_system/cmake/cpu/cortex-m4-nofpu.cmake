if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m4
    -mfloat-abi=soft
    )
  if (NOT DEFINED FREERTOS_PORT)
    set(FREERTOS_PORT GCC_ARM_CM3 CACHE INTERNAL "")
  endif ()

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    --target=arm-none-eabi
    -mcpu=cortex-m4
    )
  if (NOT DEFINED FREERTOS_PORT)
    set(FREERTOS_PORT GCC_ARM_CM3 CACHE INTERNAL "")
  endif ()

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m4
    --fpu none
    )

  if (NOT DEFINED FREERTOS_PORT)
    set(FREERTOS_PORT IAR_ARM_CM3 CACHE INTERNAL "")
  endif ()

endif ()
