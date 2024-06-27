if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mcpu=arm926ej-s
    -ffreestanding
    )
  # set(FREERTOS_PORT GCC_ARM_CM0 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    --target=arm-none-eabi
    -mcpu=arm926ej-s
    -mfpu=none
    -mfloat-abi=soft
    -ffreestanding
    )
  #set(FREERTOS_PORT GCC_ARM_CM0 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  message(FATAL_ERROR "IAR not supported")

endif ()
